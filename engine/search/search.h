#pragma once

#include "problem.h"
#include <QSet>
#include <QHash>
#include <QDebug>
#include <deque>
#include <queue>
#include <vector>
#include <functional>
#include <memory>
#include <limits>
#include <cmath>

// ============================================================================
// PriorityFrontier — min-priority queue with O(1) state-membership lookup
//
// Used by bestFirstGraphSearch.
// Requires: qHash(S) and operator==(S).
//
// Update semantics (matching Python PriorityQueue):
//   push(node, priority)           — enqueue
//   pop()                          — dequeue minimum-priority node
//   contains(state)                — membership test
//   getCost(state)                 — current best priority for state
//   remove(state)                  — lazy-delete by state
// ============================================================================
template<typename S, typename A>
class PriorityFrontier {
public:
    using NodePtr = std::shared_ptr<Node<S, A>>;

    void push(NodePtr node, double priority) {
        m_heap.push({priority, m_counter++, node});
        m_map[node->state] = priority;
    }

    NodePtr pop() {
        while (!m_heap.empty()) {
            auto entry = m_heap.top();
            m_heap.pop();
            // Skip stale entries (superseded by a lower-cost update or explicit remove)
            auto it = m_map.find(entry.node->state);
            if (it != m_map.end() && it.value() == entry.priority) {
                m_map.erase(it);
                return entry.node;
            }
        }
        return nullptr;
    }

    bool   contains(const S &state) const { return m_map.contains(state); }
    double getCost(const S &state)  const {
        return m_map.value(state, std::numeric_limits<double>::infinity());
    }
    void remove(const S &state)           { m_map.remove(state); }
    bool empty()  const                   { return m_map.isEmpty(); }
    int  size()   const                   { return m_map.size(); }

private:
    struct Entry {
        double    priority;
        long long counter;   // tie-break: FIFO among equal priorities
        NodePtr   node;

        bool operator>(const Entry &o) const {
            if (priority != o.priority) return priority > o.priority;
            return counter > o.counter;
        }
    };

    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> m_heap;
    QHash<S, double> m_map;
    long long m_counter = 0;
};


// ============================================================================
// breadthFirstTreeSearch  [Figure 3.7]
// Explores shallowest nodes first; may loop on cyclic state spaces.
// ============================================================================
template<typename S, typename A>
std::shared_ptr<Node<S, A>> breadthFirstTreeSearch(const Problem<S, A> &problem) {
    using NodePtr = std::shared_ptr<Node<S, A>>;

    std::deque<NodePtr> frontier;
    frontier.push_back(std::make_shared<Node<S, A>>(problem.initial));

    while (!frontier.empty()) {
        NodePtr node = frontier.front();
        frontier.pop_front();
        if (problem.goalTest(node->state))
            return node;
        for (auto &child : node->expand(problem))
            frontier.push_back(child);
    }
    return nullptr;
}


// ============================================================================
// depthFirstTreeSearch  [Figure 3.7]
// Explores deepest nodes first; may loop on cyclic state spaces.
// ============================================================================
template<typename S, typename A>
std::shared_ptr<Node<S, A>> depthFirstTreeSearch(const Problem<S, A> &problem) {
    using NodePtr = std::shared_ptr<Node<S, A>>;

    std::vector<NodePtr> frontier;
    frontier.push_back(std::make_shared<Node<S, A>>(problem.initial));

    while (!frontier.empty()) {
        NodePtr node = frontier.back();
        frontier.pop_back();
        if (problem.goalTest(node->state))
            return node;
        for (auto &child : node->expand(problem))
            frontier.push_back(child);
    }
    return nullptr;
}


// ============================================================================
// depthFirstGraphSearch  [Figure 3.7]
// DFS with a closed set — does not revisit explored states.
// ============================================================================
template<typename S, typename A>
std::shared_ptr<Node<S, A>> depthFirstGraphSearch(const Problem<S, A> &problem) {
    using NodePtr = std::shared_ptr<Node<S, A>>;

    std::vector<NodePtr> frontier;
    QSet<S>              frontierStates;
    QSet<S>              explored;

    frontier.push_back(std::make_shared<Node<S, A>>(problem.initial));
    frontierStates.insert(problem.initial);

    while (!frontier.empty()) {
        NodePtr node = frontier.back();
        frontier.pop_back();
        frontierStates.remove(node->state);

        if (problem.goalTest(node->state))
            return node;

        explored.insert(node->state);
        for (auto &child : node->expand(problem)) {
            if (!explored.contains(child->state) &&
                !frontierStates.contains(child->state)) {
                frontier.push_back(child);
                frontierStates.insert(child->state);
            }
        }
    }
    return nullptr;
}


// ============================================================================
// breadthFirstGraphSearch  [Figure 3.11]
// BFS with a closed set — optimal for unit-cost problems.
// ============================================================================
template<typename S, typename A>
std::shared_ptr<Node<S, A>> breadthFirstGraphSearch(const Problem<S, A> &problem) {
    using NodePtr = std::shared_ptr<Node<S, A>>;

    NodePtr startNode = std::make_shared<Node<S, A>>(problem.initial);
    if (problem.goalTest(startNode->state))
        return startNode;

    std::deque<NodePtr> frontier;
    QSet<S>             frontierStates;
    QSet<S>             explored;

    frontier.push_back(startNode);
    frontierStates.insert(problem.initial);

    while (!frontier.empty()) {
        NodePtr node = frontier.front();
        frontier.pop_front();
        frontierStates.remove(node->state);
        explored.insert(node->state);

        for (auto &child : node->expand(problem)) { // problem provides logic for expanding
            if (!explored.contains(child->state) &&
                !frontierStates.contains(child->state)) {
                if (problem.goalTest(child->state))
                    return child;
                frontier.push_back(child);
                frontierStates.insert(child->state);
            }
        }
    }
    return nullptr;
}


// ============================================================================
// bestFirstGraphSearch
// Search nodes with the lowest f(node) score first.
// f is called once per node and the result is cached in node->_f.
// ============================================================================
template<typename S, typename A, typename F>
std::shared_ptr<Node<S, A>> bestFirstGraphSearch(
    const Problem<S, A> &problem, F f, bool display = false)
{
    using NodePtr = std::shared_ptr<Node<S, A>>;

    // Memoising wrapper: cache computed f value on the node
    auto memoF = [&f](const NodePtr &n) -> double {
        if (!std::isnan(n->_f)) return n->_f;
        n->_f = f(n);
        return n->_f;
    };

    NodePtr startNode = std::make_shared<Node<S, A>>(problem.initial);
    PriorityFrontier<S, A> frontier;
    frontier.push(startNode, memoF(startNode));
    QSet<S> explored;

    while (!frontier.empty()) {
        NodePtr node = frontier.pop();
        if (problem.goalTest(node->state)) {
            if (display)
                qDebug() << explored.size() << "paths expanded,"
                         << frontier.size() << "remain in frontier";
            return node;
        }
        explored.insert(node->state);

        for (auto &child : node->expand(problem)) {
            if (!explored.contains(child->state) &&
                !frontier.contains(child->state)) {
                frontier.push(child, memoF(child));
            } else if (frontier.contains(child->state)) {
                double newCost = memoF(child);
                if (newCost < frontier.getCost(child->state)) {
                    frontier.remove(child->state);
                    frontier.push(child, newCost);
                }
            }
        }
    }
    return nullptr;
}


// ============================================================================
// uniformCostSearch  [Figure 3.14]
// Best-first search with f(n) = g(n) (path cost).
// ============================================================================
template<typename S, typename A>
std::shared_ptr<Node<S, A>> uniformCostSearch(
    const Problem<S, A> &problem, bool display = false)
{
    return bestFirstGraphSearch(
        problem,
        [](const std::shared_ptr<Node<S, A>> &n) { return n->pathCost; },
        display);
}


// ============================================================================
// depthLimitedSearch  [Figure 3.17]
// Returns the goal node, or nullptr if cutoff or failure.
// Use depthLimitedSearchFull to distinguish cutoff from failure.
// ============================================================================
namespace detail {

template<typename S, typename A>
struct DLSResult {
    std::shared_ptr<Node<S, A>> node;  // non-null on success
    bool cutoff = false;               // true on depth cutoff
};

template<typename S, typename A>
DLSResult<S, A> recursiveDLS(
    std::shared_ptr<Node<S, A>> node,
    const Problem<S, A> &problem,
    int limit)
{
    if (problem.goalTest(node->state))
        return {node, false};
    if (limit == 0)
        return {nullptr, true};

    bool cutoffOccurred = false;
    for (auto &child : node->expand(problem)) {
        auto result = recursiveDLS(child, problem, limit - 1);
        if (result.cutoff)
            cutoffOccurred = true;
        else if (result.node)
            return result;
    }
    return {nullptr, cutoffOccurred};
}

} // namespace detail


template<typename S, typename A>
std::shared_ptr<Node<S, A>> depthLimitedSearch(
    const Problem<S, A> &problem, int limit = 50)
{
    return detail::recursiveDLS(
        std::make_shared<Node<S, A>>(problem.initial),
        problem, limit).node;
}


// ============================================================================
// iterativeDeepeningSearch  [Figure 3.18]
// ============================================================================
template<typename S, typename A>
std::shared_ptr<Node<S, A>> iterativeDeepeningSearch(const Problem<S, A> &problem) {
    for (int depth = 0; depth < std::numeric_limits<int>::max(); ++depth) {
        auto result = detail::recursiveDLS(
            std::make_shared<Node<S, A>>(problem.initial),
            problem, depth);
        if (!result.cutoff)
            return result.node;
    }
    return nullptr;
}


// ============================================================================
// astarSearch
// Best-first search with f(n) = g(n) + h(n).
// Pass a custom h, or leave nullptr to use problem.h().
// ============================================================================
template<typename S, typename A>
std::shared_ptr<Node<S, A>> astarSearch(
    const Problem<S, A> &problem,
    std::function<double(const std::shared_ptr<Node<S, A>> &)> h = nullptr,
    bool display = false)
{
    using NodePtr = std::shared_ptr<Node<S, A>>;

    // Memoising h wrapper — caches on node->_h
    auto hMemo = [&](const NodePtr &n) -> double {
        if (!std::isnan(n->_h)) return n->_h;
        n->_h = h ? h(n) : problem.h(*n);
        return n->_h;
    };

    return bestFirstGraphSearch(
        problem,
        [&](const NodePtr &n) { return n->pathCost + hMemo(n); },
        display);
}


// ============================================================================
// greedyBestFirstGraphSearch
// Best-first search with f(n) = h(n).
// ============================================================================
template<typename S, typename A>
std::shared_ptr<Node<S, A>> greedyBestFirstGraphSearch(
    const Problem<S, A> &problem,
    std::function<double(const std::shared_ptr<Node<S, A>> &)> h = nullptr,
    bool display = false)
{
    using NodePtr = std::shared_ptr<Node<S, A>>;

    auto hMemo = [&](const NodePtr &n) -> double {
        if (!std::isnan(n->_h)) return n->_h;
        n->_h = h ? h(n) : problem.h(*n);
        return n->_h;
    };

    return bestFirstGraphSearch(problem, hMemo, display);
}
