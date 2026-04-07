#pragma once

#include "action.h"
#include "../logic/fol_kb.h"
#include <QVector>
#include <QString>

// ============================================================================
// PlanningProblem — a STRIPS-style planning problem
//
// initial  — list of ground literals that are true in the initial state
// goals    — list of ground literals that must be true in the goal state
// actions  — list of action schemas (may contain parameter variables)
//
// Example (Spare Tyre problem):
//   PlanningProblem problem(
//       {expr("Tire(Flat)"), expr("Tire(Spare)"), expr("At(Flat, Axle)")},
//       {expr("At(Spare, Axle)")},
//       {removeTire, putOnTire, leaveOvernight}
//   );
// ============================================================================

class PlanningProblem {
public:
    PlanningProblem(const QVector<Expr>  &initial,
                    const QVector<Expr>  &goals,
                    const QVector<Action> &actions);

    // Accessors
    FolKB          kb()      const { return m_kb; }
    QVector<Expr>  goals()   const { return m_goals; }
    QVector<Action> actions() const { return m_actions; }

    // Returns true if all goal literals hold in state
    bool goalTest(const FolKB &state) const;
    bool goalTest(const QVector<Expr> &state) const;

    // Returns a new PlanningProblem with negative effects stripped from all actions
    PlanningProblem relaxed() const;

    // Returns all ground (fully substituted) instances of every action schema.
    // Objects are collected from the initial-state KB clauses.
    QVector<Action> expandActions() const;

private:
    FolKB          m_kb;
    QVector<Expr>  m_goals;
    QVector<Action> m_actions;
};

// ============================================================================
// Free function: goalTest(kb, goals)
// ============================================================================

bool goalTest(const FolKB &kb, const QVector<Expr> &goals);
bool goalTest(const QVector<Expr> &state, const QVector<Expr> &goals);
