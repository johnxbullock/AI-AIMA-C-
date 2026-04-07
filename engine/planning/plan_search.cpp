#include "plan_search.h"

// ============================================================================
// ForwardPlan
// ============================================================================

ForwardPlan::ForwardPlan(const PlanningProblem &pp)
    : Problem<Expr, Action>(
          associate(QStringLiteral("&"), pp.kb().clauses),   // initial = conjunction of facts
          associate(QStringLiteral("&"), pp.goals()))        // goal    = conjunction of goals
    , m_pp(pp)
    , m_expandedActions(pp.expandActions()) // we expand to get the full set of actions in the constructor?
{}

/*
 * Get the actions that can be performed in this state?
 */
QVector<Action> ForwardPlan::actions(const Expr &state) const {
    QVector<Expr> stateFacts = conjuncts(state); // get the ?
    QVector<Action> result;  //
    for (const Action &action : m_expandedActions) {
        bool ok = true;
        for (const Expr &pre : action.precond())
            if (!stateFacts.contains(pre)) { ok = false; break; }
        if (ok)
            result.append(action);
    }
    return result;
}

Expr ForwardPlan::result(const Expr &state, const Action &action) const {
    // act() applies the ground effects: adds positive facts, retracts their negations
    FolKB kb = action.act(conjuncts(state), action.args());
    return associate(QStringLiteral("&"), kb.clauses);
}

bool ForwardPlan::goalTest(const Expr &state) const {
    QVector<Expr> stateFacts = conjuncts(state);
    for (const Expr &goal : m_pp.goals())
        if (!stateFacts.contains(goal)) return false;
    return true;
}

double ForwardPlan::h(const Node<Expr, Action> &) const {
    // TODO: ignore-delete-lists heuristic via GraphPlan (not yet implemented)
    return 0.0;
}

// ============================================================================
// BackwardPlan
// ============================================================================

Expr BackwardPlan::negateClause(const Expr &clause) {
    if (clause.op().startsWith(QStringLiteral("Not")))
        return Expr(clause.op().mid(3), clause.args());          // NotAt → At
    return Expr(QStringLiteral("Not") + clause.op(), clause.args()); // At → NotAt
}

BackwardPlan::BackwardPlan(const PlanningProblem &pp)
    : Problem<Expr, Action>(
          associate(QStringLiteral("&"), pp.goals()),            // initial = planning goals
          associate(QStringLiteral("&"), pp.kb().clauses))       // goal    = planning initial state
    , m_pp(pp)
    , m_expandedActions(pp.expandActions())
{}

QVector<Action> BackwardPlan::actions(const Expr &subgoal) const {
    QVector<Expr> sg = conjuncts(subgoal);
    QVector<Action> result;

    for (const Action &action : m_expandedActions) {
        // 1. Action achieves at least one subgoal literal
        bool achievesSome = false;
        for (const Expr &e : action.effect())
            if (sg.contains(e)) { achievesSome = true; break; }
        if (!achievesSome) continue;

        // 2. No effect negates a subgoal literal
        //    (i.e., negate(effect) is not in the subgoal)
        bool deletesGoal = false;
        for (const Expr &e : action.effect())
            if (sg.contains(negateClause(e))) { deletesGoal = true; break; }
        if (deletesGoal) continue;

        // 3. Preconditions are consistent with the subgoal:
        //    No precondition p has negate(p) in the subgoal unless the action
        //    itself produces negate(p) (which would be contradictory anyway).
        bool inconsistent = false;
        for (const Expr &p : action.precond()) {
            Expr negP = negateClause(p);
            if (sg.contains(negP) && !action.effect().contains(negP)) {
                inconsistent = true; break;
            }
        }
        if (!inconsistent)
            result.append(action);
    }
    return result;
}

Expr BackwardPlan::result(const Expr &subgoal, const Action &action) const {
    // g' = (g − effects(a)) ∪ precond(a)
    QSet<Expr> g(conjuncts(subgoal).begin(), conjuncts(subgoal).end());
    for (const Expr &e : action.effect())  g.remove(e);
    for (const Expr &p : action.precond()) g.insert(p);
    return associate(QStringLiteral("&"), QVector<Expr>(g.begin(), g.end()));
}

bool BackwardPlan::goalTest(const Expr &subgoal) const {
    // Subgoal is fully satisfied when every fact it requires is in the initial state
    QVector<Expr> initialFacts = conjuncts(*m_goal);  // m_goal = associate(&, pp.initial)
    for (const Expr &g : conjuncts(subgoal))
        if (!initialFacts.contains(g)) return false;
    return true;
}

double BackwardPlan::h(const Node<Expr, Action> &) const {
    return 0.0;
}
