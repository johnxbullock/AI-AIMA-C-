#include <QCoreApplication>
#include <QDebug>
#include "engine/planning/planning_problem.h"
#include "engine/planning/plan_search.h"
#include "engine/search/search.h"
#include "engine/logic/expr.h"
#include "engine/logic/utils.h"
#include "problems/monkey.h"

// ---------------------------------------------------------------------------
// Helper: print a divider line
// ---------------------------------------------------------------------------
static void printSection(const QString &title)
{
    qInfo().noquote() << "\n--- " + title + " ---";
}

// ---------------------------------------------------------------------------
// Helper: print the state facts in a readable form
// ---------------------------------------------------------------------------
static void printState(const QVector<Expr> &facts, const QString &label)
{
    qInfo().noquote() << label;
    for (const Expr &f : facts)
        qInfo().noquote() << "    " + f.toString();
}


int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    qInfo() << "Running";

    QVector<Expr> initial = initialState();
    QVector<Expr> goals = makeGoals();
    QList<Action> actions = monkeyActions();

    // initialize the problem with initial state, goals, and vector of actions
    PlanningProblem problem(
        initial,
        goals,
        actions
        );


    // algo is initialized as an object
    ForwardPlan fp(problem);
    auto goalNode = breadthFirstGraphSearch(fp); // we find the goal node using a search

    // =======================================================================
    // 7.  PRINT THE RESULT
    // =======================================================================
    if (!goalNode) {
        qInfo().noquote() << "No plan found — the problem may be unsolvable.";
        return 0;
    }

    QVector<Action> plan = goalNode->solution();

    printSection("Plan Found");
    qInfo().noquote() << QString("Steps: %1").arg(plan.size());
    qInfo().noquote() << "";
    for (int i = 0; i < plan.size(); ++i) {
        qInfo().noquote() << QString("  Step %1:  %2").arg(i + 1).arg(plan[i].toString());
    }

    // Trace the state after each step so the reader can follow the logic
    printSection("State Trace");
    QVector<Expr> state = initial;
    qInfo().noquote() << "S0 (initial):";
    for (const Expr &f : state)
        qInfo().noquote() << "    " + f.toString();

    for (int i = 0; i < plan.size(); ++i) {
        const Action &a = plan[i];
        FolKB kb = a.act(state, a.args());
        state = kb.clauses;
        qInfo().noquote() << "";
        qInfo().noquote() << QString("S%1  after  %2:").arg(i + 1).arg(a.toString());
        for (const Expr &f : state)
            qInfo().noquote() << "    " + f.toString();
    }

    // Confirm goal is satisfied
    printSection("Goal Check");
    bool allMet = true;
    for (const Expr &g : goals) {
        bool met = state.contains(g);
        qInfo().noquote() << QString("  %1  %2").arg(met ? "[X]" : "[ ]").arg(g.toString());
        if (!met) allMet = false;
    }
    qInfo().noquote() << "";
    qInfo().noquote() << (allMet ? "All goals satisfied — plan is valid." : "WARNING: some goals not met.");

    return 0;
}
