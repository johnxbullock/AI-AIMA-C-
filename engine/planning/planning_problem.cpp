#include "planning_problem.h"
#include <QSet>

// ============================================================================
// genPermutations — all r-length ordered selections without repetition
// (equivalent to itertools.permutations(pool, r))
// ============================================================================
static void genPermutationsHelper(const QVector<Expr> &pool, int r,
                                   QVector<Expr> &current,
                                   QVector<bool> &used,
                                   QVector<QVector<Expr>> &result)
{
    // base case
    if (current.size() == r) { result.append(current); return; }

    // go through the full set of objects available
    for (int i = 0; i < pool.size(); ++i) {
        if (!used[i]) // if object i is unused
        {
            used[i] = true; // mark as used
            current.append(pool[i]); // use it
            genPermutationsHelper(pool, r, current, used, result); // repeat until current.size() = r
            current.removeLast();
            used[i] = false; // mark as unused again
        }
    }
}

/*
 * Generate all possible grounded actions given a set of action schemas and a set of objects
 * returns Expr because an Action is an Expr
 * pool - the objects
 * r - number of parameters in the action
 */
static QVector<QVector<Expr>> genPermutations(const QVector<Expr> &pool, int r)
{
    QVector<QVector<Expr>> result;
    if (r == 0) { result.append(QVector<Expr>{}); return result; } // but what is an empty Expr
    QVector<bool> used(pool.size(), false); //
    QVector<Expr> current; // the current grounded action?
    genPermutationsHelper(pool, r, current, used, result);
    return result;
}

// ============================================================================
// PlanningProblem
// ============================================================================

PlanningProblem::PlanningProblem(const QVector<Expr>  &initial,
                                 const QVector<Expr>  &goals,
                                 const QVector<Action> &actions)
    : m_kb(initial)
    , m_goals(goals)
    , m_actions(actions)
{}

bool PlanningProblem::goalTest(const FolKB &state) const {
    return ::goalTest(state, m_goals);
}

bool PlanningProblem::goalTest(const QVector<Expr> &state) const {
    return ::goalTest(state, m_goals);
}

PlanningProblem PlanningProblem::relaxed() const {
    QVector<Action> relaxedActions;
    relaxedActions.reserve(m_actions.size());
    for (const Action &a : m_actions)
        relaxedActions.append(a.relaxed());
    return PlanningProblem(m_kb.clauses, m_goals, relaxedActions);
}

QVector<Action> PlanningProblem::expandActions() const {
    // Collect all constant objects from the initial-state KB
    QSet<Expr> objectSet;
    for (const Expr &clause : m_kb.clauses)
        for (const Expr &arg : clause.args())
            objectSet.insert(arg);
    QVector<Expr> objects(objectSet.begin(), objectSet.end());

    QVector<Action> result;
    for (const Action &action : m_actions) {
        if (action.args().isEmpty()) {
            result.append(action);   // already ground
            continue;
        }
        for (const QVector<Expr> &perm : genPermutations(objects, action.args().size())) {
            // Substitute schema parameters with the concrete objects
            QVector<Expr> groundPrecond, groundEffect;
            for (const Expr &p : action.precond())
                groundPrecond.append(action.substitute(p, perm));
            for (const Expr &e : action.effect())
                groundEffect.append(action.substitute(e, perm));
            Expr groundExpr(action.name(), perm);
            result.append(Action(groundExpr, groundPrecond, groundEffect));
        }
    }
    return result;
}

// ============================================================================
// Free functions
// ============================================================================

bool goalTest(const FolKB &kb, const QVector<Expr> &goals) {
    for (const Expr &g : goals)
        if (!kb.clauses.contains(g))
            return false;
    return true;
}

bool goalTest(const QVector<Expr> &state, const QVector<Expr> &goals) {
    for (const Expr &g : goals)
        if (!state.contains(g))
            return false;
    return true;
}
