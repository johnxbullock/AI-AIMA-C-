#include "graph.h"
#include <algorithm>
#include <functional>
#include <climits>

const Expr ImplicationGraph::ConflictNode = Expr(QStringLiteral("K"));

// ---- Nodes -----------------------------------------------------------------

void ImplicationGraph::addNode(const Expr &node, bool val, int dl) {
    m_attrs[node] = {val, dl};
    if (!m_succ.contains(node)) m_succ[node] = {};
    if (!m_pred.contains(node)) m_pred[node] = {};
}

bool ImplicationGraph::hasNode(const Expr &node) const {
    return m_attrs.contains(node);
}

NodeAttr ImplicationGraph::nodeAttr(const Expr &node) const {
    return m_attrs.value(node);  // returns default {false, 0} if absent
}

// ---- Edges -----------------------------------------------------------------

void ImplicationGraph::addEdge(const Expr &from, const Expr &to,
                                const Expr &antecedent) {
    m_succ[from][to] = {antecedent};
    m_pred[to].insert(from);
    // Ensure entries exist for both endpoints even if not yet added as nodes
    if (!m_succ.contains(to))   m_succ[to]   = {};
    if (!m_pred.contains(from)) m_pred[from] = {};
}

void ImplicationGraph::addEdgesFrom(const QSet<Expr> &sources,
                                     const Expr &target,
                                     const Expr &antecedent) {
    for (const Expr &src : sources)
        addEdge(src, target, antecedent);
}

Expr ImplicationGraph::edgeAntecedent(const Expr &from, const Expr &to) const {
    return m_succ.value(from).value(to).antecedent;
}

// ---- Queries ---------------------------------------------------------------

QSet<Expr> ImplicationGraph::nodes() const {
    return QSet<Expr>(m_attrs.keyBegin(), m_attrs.keyEnd());
}

QSet<Expr> ImplicationGraph::predecessors(const Expr &node) const {
    return m_pred.value(node);
}

QSet<Expr> ImplicationGraph::successors(const Expr &node) const {
    QSet<Expr> result;
    auto it = m_succ.find(node);
    if (it != m_succ.end())
        for (auto jt = it->keyBegin(); jt != it->keyEnd(); ++jt)
            result.insert(*jt);
    return result;
}

int ImplicationGraph::inDegree(const Expr &node) const {
    return m_pred.value(node).size();
}

QSet<Expr> ImplicationGraph::descendants(const Expr &source) const {
    QSet<Expr> visited;
    std::function<void(const Expr &)> dfs = [&](const Expr &node) {
        auto it = m_succ.find(node);
        if (it == m_succ.end()) return;
        for (auto jt = it->keyBegin(); jt != it->keyEnd(); ++jt) {
            if (!visited.contains(*jt)) {
                visited.insert(*jt);
                dfs(*jt);
            }
        }
    };
    dfs(source);
    return visited;
}

// ---- Immediate dominators (Cooper et al. 2001) -----------------------------

QHash<Expr, Expr> ImplicationGraph::immediateDominators(const Expr &start) const {
    // 1. Post-order DFS from start, then reverse to get RPO
    QVector<Expr> rpo;
    QSet<Expr>    visited;
    std::function<void(const Expr &)> postDfs = [&](const Expr &node) {
        visited.insert(node);
        auto it = m_succ.find(node);
        if (it != m_succ.end())
            for (auto jt = it->keyBegin(); jt != it->keyEnd(); ++jt)
                if (!visited.contains(*jt))
                    postDfs(*jt);
        rpo.append(node);
    };
    postDfs(start);
    std::reverse(rpo.begin(), rpo.end());

    // 2. RPO index for each node
    QHash<Expr, int> rpoIdx;
    for (int i = 0; i < rpo.size(); ++i)
        rpoIdx[rpo[i]] = i;

    // 3. Initialise: only the start has a known dominator (itself)
    QHash<Expr, Expr> doms;
    doms[start] = start;

    // intersect: walk up the dominator tree to find the common ancestor
    auto intersect = [&](Expr b1, Expr b2) -> Expr {
        while (b1 != b2) {
            while (rpoIdx.value(b1, INT_MAX) > rpoIdx.value(b2, INT_MAX))
                b1 = doms.value(b1, b1);
            while (rpoIdx.value(b2, INT_MAX) > rpoIdx.value(b1, INT_MAX))
                b2 = doms.value(b2, b2);
        }
        return b1;
    };

    // 4. Iterate until convergence
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 1; i < rpo.size(); ++i) {
            const Expr &n = rpo[i];

            // Only consider predecessors that already have a dominator assigned
            QVector<Expr> processed;
            for (const Expr &p : m_pred.value(n))
                if (doms.contains(p)) processed.append(p);
            if (processed.isEmpty()) continue;

            // Sort by RPO index so the first element is the "earliest" predecessor
            std::sort(processed.begin(), processed.end(),
                [&](const Expr &a, const Expr &b) {
                    return rpoIdx.value(a) < rpoIdx.value(b);
                });

            Expr newIdom = processed[0];
            for (int j = 1; j < processed.size(); ++j)
                newIdom = intersect(processed[j], newIdom);

            if (!doms.contains(n) || doms[n] != newIdom) {
                doms[n] = newIdom;
                changed = true;
            }
        }
    }
    return doms;
}

// ---- Mutation --------------------------------------------------------------

void ImplicationGraph::removeNode(const Expr &node) {
    // Remove outgoing edges
    auto succIt = m_succ.find(node);
    if (succIt != m_succ.end()) {
        for (auto jt = succIt->keyBegin(); jt != succIt->keyEnd(); ++jt)
            m_pred[*jt].remove(node);
        m_succ.erase(succIt);
    }
    // Remove incoming edges
    const QSet<Expr> preds = m_pred.value(node);
    for (const Expr &p : preds)
        m_succ[p].remove(node);
    m_pred.remove(node);

    m_attrs.remove(node);
}

void ImplicationGraph::removeNodes(const QSet<Expr> &nodes) {
    for (const Expr &n : nodes)
        removeNode(n);
}
