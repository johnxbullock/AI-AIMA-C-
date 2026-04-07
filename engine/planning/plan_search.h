#pragma once

#include "planning_problem.h"
#include "../search/problem.h"
#include "../logic/utils.h"
#include <QSet>

// ============================================================================
// ForwardPlan  [Section 10.2.1]
// Forward state-space search.
//
// State  = Expr — a conjunction of ground literals, e.g.  A & B & C
// Action = Action — a fully-ground action instance
//
// The search starts from the conjunction of the initial facts and tries to
// reach a state that satisfies every goal literal.
// ============================================================================
class ForwardPlan : public Problem<Expr, Action> {
public:
    explicit ForwardPlan(const PlanningProblem &pp);

    // All ground actions whose preconditions are satisfied in state
    QVector<Action> actions(const Expr &state) const override;

    // Apply the (ground) action to the current state
    Expr result(const Expr &state, const Action &action) const override;

    // True when every goal literal appears in state
    bool goalTest(const Expr &state) const override;

    // Ignore-delete-lists heuristic (stub — returns 0 until GraphPlan is ready)
    double h(const Node<Expr, Action> &node) const override;

private:
    PlanningProblem m_pp;
    QVector<Action> m_expandedActions;   // all ground instances of every schema
};


// ============================================================================
// BackwardPlan  [Section 10.2.2]
// Backward relevant-states search.
//
// State  = Expr — a conjunction representing the current subgoal
// Action = Action — a relevant ground action instance
//
// The search starts from the conjunction of the goals and tries to reach a
// subgoal that is fully satisfied by the initial state.
// ============================================================================
class BackwardPlan : public Problem<Expr, Action> {
public:
    explicit BackwardPlan(const PlanningProblem &pp);

    // All ground actions relevant to the current subgoal (see AIMA 10.2.2)
    QVector<Action> actions(const Expr &subgoal) const override;

    // Regression: g' = (g − effects(a)) ∪ precond(a)
    Expr result(const Expr &subgoal, const Action &action) const override;

    // True when every subgoal literal is satisfied by the planning problem's initial state
    bool goalTest(const Expr &subgoal) const override;

    // Ignore-delete-lists heuristic (stub)
    double h(const Node<Expr, Action> &node) const override;

private:
    PlanningProblem m_pp;
    QVector<Action> m_expandedActions;

    // Flip the Not-prefix of a literal: At(x) ↔ NotAt(x)
    static Expr negateClause(const Expr &clause);
};
