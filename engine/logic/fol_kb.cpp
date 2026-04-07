#include "fol_kb.h"
#include "utils.h"
#include <functional>

// ============================================================================
// FolKB
// ============================================================================

FolKB::FolKB(const QVector<Expr> &clauses) {
    for (const Expr &c : clauses)
        tell(c);
}

void FolKB::tell(const Expr &sentence) {
    if (!isDefiniteClause(sentence))
        throw std::runtime_error("FolKB::tell — not a definite clause: "
                                 + sentence.toString().toStdString());
    clauses.append(sentence);
}

QVector<Substitution> FolKB::askGenerator(const Expr &query) {
    return folBcAsk(*this, query);
}

void FolKB::retract(const Expr &sentence) {
    clauses.removeOne(sentence);
}

QVector<Expr> FolKB::fetchRulesForGoal(const Expr &) const {
    return clauses;  // no indexing — try all rules
}

// ============================================================================
// Backward chaining helpers
// ============================================================================

using SubstCB = std::function<void(const Substitution &)>;

static void folBcAnd(const FolKB &kb,
                     const QVector<Expr> &goals,
                     const std::optional<Substitution> &theta,
                     const SubstCB &cb);

static void folBcOr(const FolKB &kb,
                    const Expr &goal,
                    const Substitution &theta,
                    const SubstCB &cb) {
    for (const Expr &rule : kb.fetchRulesForGoal(goal)) {
        auto [lhs, rhs] = parseDefiniteClause(standardizeVariables(rule));
        folBcAnd(kb, lhs, unifyMM(rhs, goal, theta), cb);
    }
}

static void folBcAnd(const FolKB &kb,
                     const QVector<Expr> &goals,
                     const std::optional<Substitution> &theta,
                     const SubstCB &cb) {
    if (!theta) return;
    if (goals.isEmpty()) { cb(*theta); return; }

    Expr first = substExpr(*theta, goals[0]);
    QVector<Expr> rest = goals.mid(1);

    folBcOr(kb, first, *theta, [&](const Substitution &theta1) {
        folBcAnd(kb, rest, theta1, cb);
    });
}

// ============================================================================
// folBcAsk  [AIMA Figure 9.6]
// ============================================================================

QVector<Substitution> folBcAsk(const FolKB &kb, const Expr &query) {
    QVector<Substitution> results;
    folBcOr(kb, query, {}, [&](const Substitution &s) {
        results.append(s);
    });
    return results;
}

// ============================================================================
// folFcAsk  [AIMA Figure 9.3]
// ============================================================================

// Generate all n-tuples of elements from pool (with repetition)
static void cartesianProduct(const QVector<Expr> &pool, int n,
                              QVector<Expr> current,
                              QVector<QVector<Expr>> &result) {
    if (current.size() == n) { result.append(current); return; }
    for (const Expr &e : pool) {
        QVector<Expr> cur = current;
        cur.append(e);
        cartesianProduct(pool, n, cur, result);
    }
}

QVector<Substitution> folFcAsk(FolKB &kb, const Expr &alpha) {
    QVector<Substitution> results;

    // Collect all constants from existing clauses
    QSet<Expr> constSet;
    for (const Expr &clause : kb.clauses)
        for (const Expr &c : constantSymbols(clause))
            constSet.insert(c);
    QVector<Expr> kbConsts(constSet.begin(), constSet.end());

    // Check if alpha is already directly answered
    for (const Expr &q : kb.clauses) {
        auto phi = unifyMM(q, alpha);
        if (phi) results.append(*phi);
    }

    while (true) {
        QVector<Expr> newFacts;

        for (const Expr &rule : kb.clauses) {
            auto [p, q] = parseDefiniteClause(rule);

            // Collect all variables in the rule's antecedent
            QSet<Expr> varSet;
            for (const Expr &clause : p)
                for (const Expr &v : variables(clause))
                    varSet.insert(v);
            QVector<Expr> queryVars(varSet.begin(), varSet.end());

            // Try all assignments of KB constants to those variables
            QVector<QVector<Expr>> assignments;
            cartesianProduct(kbConsts, queryVars.size(), {}, assignments);

            for (const QVector<Expr> &assignment : assignments) {
                Substitution theta;
                for (int i = 0; i < queryVars.size(); ++i)
                    theta[queryVars[i]] = assignment[i];

                // Check if all preconditions hold
                QVector<Expr> substitutedP;
                for (const Expr &clause : p)
                    substitutedP.append(substExpr(theta, clause));

                bool allHold = true;
                for (const Expr &sc : substitutedP) {
                    if (!kb.clauses.contains(sc)) { allHold = false; break; }
                }
                if (!allHold) continue;

                Expr qPrime = substExpr(theta, q);

                // Add qPrime if not already known
                bool alreadyKnown = false;
                for (const Expr &existing : kb.clauses + newFacts)
                    if (unifyMM(existing, qPrime)) { alreadyKnown = true; break; }

                if (!alreadyKnown) {
                    newFacts.append(qPrime);
                    auto phi = unifyMM(qPrime, alpha);
                    if (phi) results.append(*phi);
                }
            }
        }

        if (newFacts.isEmpty()) break;
        for (const Expr &fact : newFacts)
            kb.tell(fact);
    }

    return results;
}
