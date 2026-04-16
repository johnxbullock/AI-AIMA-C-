#pragma once

#include <QVector>
#include <memory>
#include <optional>
#include <limits>
#include <stdexcept>

// Forward declaration
template<typename S, typename A> class Node;

// ============================================================================
// Problem — abstract base class for a formal search problem
//
// Subclass this and implement actions() and result().
// Override goalTest(), pathCost(), value(), and h() as needed.
//
// Template parameters:
//   S — state type  (must support operator== and qHash for graph search)
//   A — action type
// ============================================================================
template<typename S, typename A>
class Problem {
public:
    S initial;

    // Single-goal constructor
    explicit Problem(const S &initial, std::optional<S> goal = std::nullopt)
        : initial(initial), m_goal(std::move(goal)) {}

    // Multi-goal constructor
    Problem(const S &initial, const QVector<S> &goals)
        : initial(initial), m_goals(goals) {}

    virtual ~Problem() = default;

    // Return all actions applicable in state (must override)
    virtual QVector<A> actions(const S &state) const = 0;

    // Return the state that results from applying action in state (must override)
    virtual S result(const S &state, const A &action) const = 0;

    // Return true if state satisfies the goal condition
    virtual bool goalTest(const S &state) const {
        if (!m_goals.isEmpty())
            return m_goals.contains(state);
        if (m_goal)
            return state == *m_goal;
        return false;
    }

    // Return the cost of the path to state2 from state1 via action, with prior cost c
    virtual double pathCost(double c, const S &, const A &, const S &) const {
        return c + 1.0;
    }

    // For optimization problems: return the value of state (override for hill climbing)
    virtual double value(const S &) const {
        throw std::runtime_error("Problem::value — not implemented");
    }

    // Heuristic estimate of cost from node to the nearest goal (override for A*)
    virtual double h(const Node<S, A> &) const { return 0.0; }

protected:
    std::optional<S> m_goal;
    QVector<S>       m_goals;
};


// ============================================================================
// Node — a node in the search tree
//
// Always create via std::make_shared<Node<S,A>>(...) so that shared_from_this()
// works inside expand() / childNode().
// ============================================================================
template<typename S, typename A>
class Node : public std::enable_shared_from_this<Node<S, A>> {
public:
    using NodePtr = std::shared_ptr<Node<S, A>>;

    S                state;
    NodePtr          parent;
    std::optional<A> action;
    double           pathCost;
    int              depth;

    // Cached evaluation values written by search algorithms
    mutable double _f = std::numeric_limits<double>::quiet_NaN();
    mutable double _h = std::numeric_limits<double>::quiet_NaN();

    explicit Node(const S &state,
                  NodePtr parent = nullptr,
                  std::optional<A> action = std::nullopt,
                  double pathCost = 0.0)
        : state(state)
        , parent(std::move(parent))
        , action(std::move(action))
        , pathCost(pathCost)
        , depth(this->parent ? this->parent->depth + 1 : 0)
    {}

    // Create the child node reached by applying act in this node's state
    NodePtr childNode(const Problem<S, A> &problem, const A &act) const {
        S next = problem.result(state, act); // returns a KB for ForwardPlan
        double cost = problem.pathCost(pathCost, state, act, next);
        return std::make_shared<Node<S, A>>(
            next,
            std::const_pointer_cast<Node<S, A>>(this->shared_from_this()),
            act, // action that produced the node
            cost // cost of applying the action
        );
    }

    // All child nodes reachable in one step
    QVector<NodePtr> expand(const Problem<S, A> &problem) const {
        QVector<NodePtr> children;
        for (const A &act : problem.actions(state))
            children.append(childNode(problem, act));
        return children;
    }

    // Path from root to this node (as a list of nodes)
    QVector<NodePtr> path() const {
        QVector<NodePtr> p;
        auto n = std::const_pointer_cast<Node<S,A>>(this->shared_from_this());
        while (n) {
            p.prepend(n);
            n = n->parent;
        }
        return p;
    }

    // Sequence of actions from root to this node
    QVector<A> solution() const {
        QVector<A> acts;
        for (const auto &n : path().mid(1))
            if (n->action)
                acts.append(*n->action);
        return acts;
    }

    // Nodes with the same state are considered equal
    bool operator==(const Node &other) const { return state == other.state; }
    bool operator<(const Node &other)  const { return pathCost < other.pathCost; }
};


// ============================================================================
// SimpleProblemSolvingAgentProgram  [Figure 3.1]
//
// Subclass and implement updateState, formulateGoal, formulateProblem, search.
//
// Template parameters:
//   S       — state type
//   A       — action type
//   Percept — percept type (defaults to S)
// ============================================================================
template<typename S, typename A, typename Percept = S>
class SimpleProblemSolvingAgentProgram {
public:
    S          state;
    QVector<A> seq;

    explicit SimpleProblemSolvingAgentProgram(const S &initialState = S{})
        : state(initialState) {}

    virtual ~SimpleProblemSolvingAgentProgram() = default;

    std::optional<A> operator()(const Percept &percept) {
        state = updateState(state, percept);
        if (seq.isEmpty()) {
            S goal          = formulateGoal(state);
            auto problem    = formulateProblem(state, goal);
            seq             = search(*problem);
            if (seq.isEmpty())
                return std::nullopt;
        }
        A act = seq.front();
        seq.removeFirst();
        return act;
    }

    virtual S updateState(const S &state, const Percept &percept) = 0;
    virtual S formulateGoal(const S &state) = 0;
    virtual std::shared_ptr<Problem<S, A>> formulateProblem(const S &state,
                                                             const S &goal) = 0;
    virtual QVector<A> search(const Problem<S, A> &problem) = 0;
};
