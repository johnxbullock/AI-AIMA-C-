#include "dpll.h"
#include "cnf.h"
#include <algorithm>
#include <cmath>

// ---- Internal helpers ------------------------------------------------------

static Model extend(Model model, const Expr &var, bool val) {
    model[var] = val;
    return model;
}

// ============================================================================
// minClauses
// ============================================================================

QVector<Expr> minClauses(const QVector<Expr> &clauses) {
    if (clauses.isEmpty()) return {};
    int minLen = INT_MAX;
    for (const Expr &c : clauses)
        minLen = qMin(minLen, c.args().size());
    int threshold = qMax(minLen, 2);
    QVector<Expr> result;
    for (const Expr &c : clauses)
        if (c.args().size() == threshold) result.append(c);
    return result;
}

// ============================================================================
// Unit propagation helpers
// ============================================================================

QPair<Expr, bool> inspectLiteralLocal(const Expr &literal) {
    if (literal.op() == QStringLiteral("~"))
        return {literal.args()[0], false};
    return {literal, true};
}

QPair<Expr, bool> unitClauseAssign(const Expr &clause, const Model &model) {
    Expr P; bool value = false;
    for (const Expr &literal : disjuncts(clause)) {
        auto [sym, positive] = inspectLiteral(literal);
        if (model.contains(sym)) {
            if (model.value(sym) == positive)
                return {Expr(), false};  // clause already True
        } else if (!P.op().isEmpty()) {
            return {Expr(), false};      // more than one unbound literal
        } else {
            P = sym; value = positive;
        }
    }
    return {P, value};
}

QPair<Expr, bool> findUnitClause(const QVector<Expr> &clauses, const Model &model) {
    for (const Expr &c : clauses) {
        auto [P, value] = unitClauseAssign(c, model);
        if (!P.op().isEmpty()) return {P, value};
    }
    return {Expr(), false};
}

QPair<Expr, bool> findPureSymbol(const QVector<Expr> &symbols,
                                  const QVector<Expr> &clauses) {
    for (const Expr &s : symbols) {
        bool foundPos = false, foundNeg = false;
        for (const Expr &c : clauses) {
            const QVector<Expr> dis = disjuncts(c);
            if (!foundPos && dis.contains(s))  foundPos = true;
            if (!foundNeg && dis.contains(~s)) foundNeg = true;
            if (foundPos && foundNeg) break;
        }
        if (foundPos != foundNeg) return {s, foundPos};
    }
    return {Expr(), false};
}

// ============================================================================
// DPLL core
// ============================================================================

std::optional<Model> dpll(const QVector<Expr> &clauses, QVector<Expr> symbols,
                           Model model, const BranchingHeuristic &heuristic) {
    QVector<Expr> unknown;
    for (const Expr &c : clauses) {
        auto val = plTrue(c, model);
        if (val.has_value() && !val.value()) return std::nullopt;
        if (!val.has_value()) unknown.append(c);
    }
    if (unknown.isEmpty()) return model;

    // Pure symbol elimination
    auto [P1, v1] = findPureSymbol(symbols, unknown);
    if (!P1.op().isEmpty()) {
        symbols.removeOne(P1);
        return dpll(clauses, symbols, extend(model, P1, v1), heuristic);
    }

    // Unit clause propagation
    auto [P2, v2] = findUnitClause(clauses, model);
    if (!P2.op().isEmpty()) {
        symbols.removeOne(P2);
        return dpll(clauses, symbols, extend(model, P2, v2), heuristic);
    }

    // Branching
    auto [P, value] = heuristic(symbols, unknown);
    QVector<Expr> rest = symbols;
    rest.removeOne(P);

    auto r = dpll(clauses, rest, extend(model, P, value), heuristic);
    if (r) return r;
    return dpll(clauses, rest, extend(model, P, !value), heuristic);
}

std::optional<Model> dpllSatisfiable(const Expr &s,
                                      const BranchingHeuristic &heuristic) {
    QVector<Expr> clauses = conjuncts(toCnf(s));
    QSet<Expr>    symSet  = propSymbols(s);
    QVector<Expr> syms(symSet.begin(), symSet.end());

    BranchingHeuristic h = heuristic ? heuristic : noBranchingHeuristic;
    return dpll(clauses, syms, {}, h);
}

// ============================================================================
// Branching heuristics
// ============================================================================

QPair<Expr, bool> noBranchingHeuristic(const QVector<Expr> &symbols,
                                        const QVector<Expr> &) {
    return {symbols.first(), true};
}

// ---- MOMS ------------------------------------------------------------------

QPair<Expr, bool> moms(const QVector<Expr> &symbols, const QVector<Expr> &clauses) {
    QHash<Expr, int> scores;
    for (const Expr &c : minClauses(clauses))
        for (const Expr &l : propSymbols(c))
            ++scores[l];
    auto P = *std::max_element(symbols.begin(), symbols.end(),
        [&](const Expr &a, const Expr &b) { return scores.value(a) < scores.value(b); });
    return {P, true};
}

// ---- MOMS-F ----------------------------------------------------------------

QPair<Expr, bool> momsf(const QVector<Expr> &symbols, const QVector<Expr> &clauses,
                         int k) {
    QHash<Expr, int> scores;
    for (const Expr &c : minClauses(clauses))
        for (const Expr &l : disjuncts(c))
            ++scores[l];
    auto P = *std::max_element(symbols.begin(), symbols.end(),
        [&](const Expr &a, const Expr &b) {
            auto scoreA = (scores.value(a) + scores.value(~a)) * (1 << k)
                         + scores.value(a) * scores.value(~a);
            auto scoreB = (scores.value(b) + scores.value(~b)) * (1 << k)
                         + scores.value(b) * scores.value(~b);
            return scoreA < scoreB;
        });
    return {P, scores.value(P) >= scores.value(~P)};
}

// ---- POSIT -----------------------------------------------------------------

QPair<Expr, bool> posit(const QVector<Expr> &symbols, const QVector<Expr> &clauses) {
    QHash<Expr, int> scores;
    for (const Expr &c : minClauses(clauses))
        for (const Expr &l : disjuncts(c))
            ++scores[l];
    auto P = *std::max_element(symbols.begin(), symbols.end(),
        [&](const Expr &a, const Expr &b) {
            return (scores.value(a) + scores.value(~a)) < (scores.value(b) + scores.value(~b));
        });
    return {P, scores.value(P) >= scores.value(~P)};
}

// ---- ZM --------------------------------------------------------------------

QPair<Expr, bool> zm(const QVector<Expr> &symbols, const QVector<Expr> &clauses) {
    QHash<Expr, int> scores;
    for (const Expr &c : minClauses(clauses))
        for (const Expr &l : disjuncts(c))
            if (l.op() == QStringLiteral("~")) ++scores[l];
    auto P = *std::max_element(symbols.begin(), symbols.end(),
        [&](const Expr &a, const Expr &b) {
            return scores.value(~a) < scores.value(~b);
        });
    return {P, true};
}

// ---- DLIS ------------------------------------------------------------------

QPair<Expr, bool> dlis(const QVector<Expr> &symbols, const QVector<Expr> &clauses) {
    QHash<Expr, int> scores;
    for (const Expr &c : clauses)
        for (const Expr &l : disjuncts(c))
            ++scores[l];
    auto P = *std::max_element(symbols.begin(), symbols.end(),
        [&](const Expr &a, const Expr &b) { return scores.value(a) < scores.value(b); });
    return {P, scores.value(P) >= scores.value(~P)};
}

// ---- DLCS ------------------------------------------------------------------

QPair<Expr, bool> dlcs(const QVector<Expr> &symbols, const QVector<Expr> &clauses) {
    QHash<Expr, int> scores;
    for (const Expr &c : clauses)
        for (const Expr &l : disjuncts(c))
            ++scores[l];
    auto P = *std::max_element(symbols.begin(), symbols.end(),
        [&](const Expr &a, const Expr &b) {
            return (scores.value(a) + scores.value(~a)) < (scores.value(b) + scores.value(~b));
        });
    return {P, scores.value(P) >= scores.value(~P)};
}

// ---- JW --------------------------------------------------------------------

QPair<Expr, bool> jw(const QVector<Expr> &symbols, const QVector<Expr> &clauses) {
    QHash<Expr, double> scores;
    for (const Expr &c : clauses)
        for (const Expr &l : propSymbols(c))
            scores[l] += std::pow(2.0, -(double)c.args().size());
    auto P = *std::max_element(symbols.begin(), symbols.end(),
        [&](const Expr &a, const Expr &b) { return scores.value(a) < scores.value(b); });
    return {P, true};
}

// ---- JW2 -------------------------------------------------------------------

QPair<Expr, bool> jw2(const QVector<Expr> &symbols, const QVector<Expr> &clauses) {
    QHash<Expr, double> scores;
    for (const Expr &c : clauses)
        for (const Expr &l : disjuncts(c))
            scores[l] += std::pow(2.0, -(double)c.args().size());
    auto P = *std::max_element(symbols.begin(), symbols.end(),
        [&](const Expr &a, const Expr &b) {
            return (scores.value(a) + scores.value(~a)) < (scores.value(b) + scores.value(~b));
        });
    return {P, scores.value(P) >= scores.value(~P)};
}
