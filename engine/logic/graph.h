#pragma once

#include "expr.h"
#include <QHash>
#include <QSet>
#include <QVector>

struct NodeAttr {
    bool val = false;
    int  dl  = 0;
};

// Directed graph for CDCL implication graphs.
// Nodes are Exprs; edges carry an antecedent clause.
class ImplicationGraph {
public:
    // The conflict node added during unit propagation
    static const Expr ConflictNode;   // Expr("K")

    // ---- Nodes -------------------------------------------------------------
    void addNode(const Expr &node, bool val, int dl);
    bool     hasNode(const Expr &node) const;
    NodeAttr nodeAttr(const Expr &node) const;

    // ---- Edges -------------------------------------------------------------
    void addEdge(const Expr &from, const Expr &to, const Expr &antecedent);
    void addEdgesFrom(const QSet<Expr> &sources, const Expr &target,
                      const Expr &antecedent);

    Expr edgeAntecedent(const Expr &from, const Expr &to) const;

    // ---- Queries -----------------------------------------------------------
    QSet<Expr> nodes() const;
    QSet<Expr> predecessors(const Expr &node) const;
    QSet<Expr> successors(const Expr &node) const;
    int        inDegree(const Expr &node) const;

    // All nodes reachable from source, excluding source itself
    QSet<Expr> descendants(const Expr &source) const;

    // Cooper et al. (2001) immediate dominator algorithm.
    // Returns doms where doms[n] = immediate dominator of n from start.
    QHash<Expr, Expr> immediateDominators(const Expr &start) const;

    // ---- Mutation ----------------------------------------------------------
    void removeNode(const Expr &node);
    void removeNodes(const QSet<Expr> &nodes);

private:
    struct EdgeInfo { Expr antecedent; };

    QHash<Expr, QHash<Expr, EdgeInfo>> m_succ;   // node -> {successor -> edge}
    QHash<Expr, QSet<Expr>>            m_pred;   // node -> set of predecessors
    QHash<Expr, NodeAttr>              m_attrs;
};
