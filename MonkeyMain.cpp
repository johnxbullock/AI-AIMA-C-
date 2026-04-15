#include <QCoreApplication>
#include <QDebug>
#include "engine/planning/planning_problem.h"
#include "engine/planning/plan_search.h"
#include "engine/search/search.h"
#include "engine/logic/expr.h"
#include "engine/logic/utils.h"

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

    // these need to be determined by the suppliers that give quotes
    QSet<QString> atoms{
        "NeedsHealth",
        "NeedsIP",
        "Life_Liberty",
    };

    // =======================================================================
    // 1.  INITIAL STATE
    //
    // In STRIPS, the initial state is the closed-world assumption: only the
    // predicates listed here are true. Negative predicates must be stated
    // explicitly so precondition checks can find them.
    // =======================================================================
    QVector<Expr> initial = {
        // Sarah currently holds the old inadequate life policy
        expr("PolicyHeld_OldLife"),

        // She does NOT yet hold any of the market products
        expr("NotPolicyHeld_NewLife"),
        expr("NotPolicyHeld_IP"),
        expr("NotPolicyHeld_Bundle"),

        // Her insurance gaps are open: life cover inadequate, no IP at all
        expr("NotLifeGapFilled"),
        expr("NotIPGapFilled"),
    };

    // =======================================================================
    // 2.  GOAL STATE
    //
    // The InsuranceOptimizer's objective: fill both gaps.
    // BudgetOk is enforced structurally through action preconditions below.
    // =======================================================================

    // goal is a vector of expressions?
    QVector<Expr> goals = {
        expr("LifeGapFilled"),
        expr("IPGapFilled"),
    };

    // =======================================================================
    // 3.  ACTION SCHEMAS
    //
    //         in turn retracts PolicyHeld_OldLife from the state.
    Action Go(
        "GO",
        {
            expr("At"),
        },
        {
            expr("At")
        }
        );

    Action dropOldLife(
        "DropOldLife",
        /*precond*/ {
            expr("PolicyHeld_OldLife"),     // must hold it to drop it
        },
        /*effect*/  {
            ~expr("PolicyHeld_OldLife"),    // → NotPolicyHeld_OldLife
            //   retracts PolicyHeld_OldLife
        }
        );

    // --- BuyNewLife ---------------------------------------------------------
    // Buy the Discovery 3,000,000 life policy (R 1,300/month).
    // BUDGET CONSTRAINT: OldLife (R 900) + NewLife (R 1,300) = R 2,200 > R 2,000.
    // Therefore NotPolicyHeld_OldLife is a required precondition — the planner
    // cannot buy this policy until it has dropped OldLife.
    Action buyNewLife(
        "BuyNewLife",
        /*precond*/ {
            expr("NotPolicyHeld_NewLife"),  // must not already hold it
            expr("NotPolicyHeld_OldLife"),  // budget constraint: incompatible with OldLife
        },
        /*effect*/  {
            expr("PolicyHeld_NewLife"),     // retracts NotPolicyHeld_NewLife
            expr("LifeGapFilled"),          // retracts NotLifeGapFilled
        }
        );

    // --- BuyIP --------------------------------------------------------------
    // Buy the Momentum R 30,000/month income-protection policy (R 700/month).
    // R 700 is affordable alongside any single policy in the catalogue,
    // so there is no OldLife budget constraint here.
    Action buyIP(
        "BuyIP",
        /*precond*/ {
            expr("NotPolicyHeld_IP"),       // must not already hold it
        },
        /*effect*/  {
            expr("PolicyHeld_IP"),          // retracts NotPolicyHeld_IP
            expr("IPGapFilled"),            // retracts NotIPGapFilled
        }
        );

    // --- BuyBundle ----------------------------------------------------------
    // Buy the Liberty bundled life+IP policy (R 3,000,000 life + R 30,000 IP,
    // R 2,000/month).  Fills both gaps in one action.
    // BUDGET CONSTRAINT: OldLife (R 900) + Bundle (R 2,000) = R 2,900 > R 2,000.
    // Therefore also requires NotPolicyHeld_OldLife.
    Action buyBundle(
        "BuyBundle",
        /*precond*/ {
            expr("NotPolicyHeld_Bundle"),   // must not already hold it
            expr("NotPolicyHeld_OldLife"),  // budget constraint: incompatible with OldLife
        },
        /*effect*/  {
            expr("PolicyHeld_Bundle"),      // retracts NotPolicyHeld_Bundle
            expr("LifeGapFilled"),          // retracts NotLifeGapFilled
            expr("IPGapFilled"),            // retracts NotIPGapFilled
        }
        );

    // actions are objects - we construct them

    // =======================================================================
    // 4.  CONSTRUCT THE PLANNING PROBLEM
    // =======================================================================

    // initialize the problem with initial state, goals, and vector of actions
    PlanningProblem problem(
        initial,
        goals,
        { dropOldLife, buyNewLife, buyIP, buyBundle }
        );

    // =======================================================================
    // 5.  PRINT SETUP INFORMATION
    // =======================================================================
    printSection("Insurance Optimisation — Classical Planning Demo");
    qInfo().noquote() << "Client:  Sarah, age 35, income R 30,000/month";
    qInfo().noquote() << "Budget:  R 2,000/month for insurance premiums";
    qInfo().noquote() << "Goal:    Fill life gap (R 3M) and IP gap (R 30k/month)";

    printSection("Initial State");
    printState(initial, "True predicates at start:");

    printSection("Goal Predicates");
    for (const Expr &g : goals)
        qInfo().noquote() << "    " + g.toString();

    printSection("Available Actions");
    qInfo().noquote() << "    DropOldLife  — cancel Sanlam Legacy (R 900/month freed)";
    qInfo().noquote() << "    BuyNewLife   — buy Discovery 3M life (R 1,300/month)";
    qInfo().noquote() << "    BuyIP        — buy Momentum IP R30k  (R   700/month)";
    qInfo().noquote() << "    BuyBundle    — buy Liberty life+IP   (R 2,000/month)";
    qInfo().noquote() << "";
    qInfo().noquote() << "  Budget constraints encoded as preconditions:";
    qInfo().noquote() << "    BuyNewLife and BuyBundle both require NotPolicyHeld_OldLife";
    qInfo().noquote() << "    (OldLife + NewLife = R 2,200 > R 2,000; OldLife + Bundle = R 2,900 > R 2,000)";

    // =======================================================================
    // 6.  FORWARD STATE-SPACE SEARCH (BFS)
    //
    // ForwardPlan wraps PlanningProblem as a Problem<Expr, Action>.
    //   State  = conjunction of all currently-true predicates (stored as Expr)
    //   Action = a ground action instance
    //
    // BFS is optimal for unit-cost problems: it finds the plan with the
    // fewest actions.  [AIMA Section 3.4.1]
    // =======================================================================
    printSection("Running ForwardPlan + BFS");

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
