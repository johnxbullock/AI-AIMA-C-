#pragma once

#include "kb.h"
#include "utils.h"
#include <functional>
#include <optional>

// Branching heuristic signature: (symbols, unknown_clauses) -> (chosen_symbol, value)
using BranchingHeuristic =
    std::function<QPair<Expr, bool>(const QVector<Expr> &, const QVector<Expr> &)>;

// ---- Branching heuristic helpers -------------------------------------------

// Returns the subset of clauses with the minimum number of args (≥ 2)
QVector<Expr> minClauses(const QVector<Expr> &clauses);

// ---- DPLL entry point ------------------------------------------------------

// Returns a satisfying Model, or nullopt if unsatisfiable.
std::optional<Model> dpllSatisfiable(const Expr &s,
    const BranchingHeuristic &heuristic = {});

// Core DPLL recursion (clauses and symbols passed by value — recursion makes copies)
std::optional<Model> dpll(const QVector<Expr> &clauses, QVector<Expr> symbols,
                           Model model, const BranchingHeuristic &heuristic);

// ---- Unit propagation helpers ----------------------------------------------

// If s appears only positive or only negative in clauses, returns (s, polarity).
// Returns ({}, false) if no pure symbol found.
QPair<Expr, bool> findPureSymbol(const QVector<Expr> &symbols,
                                  const QVector<Expr> &clauses);

// Returns (P, value) from the first unit clause, or ({}, false) if none.
QPair<Expr, bool> findUnitClause(const QVector<Expr> &clauses, const Model &model);

// If clause has exactly one unbound literal under model, returns (symbol, value).
QPair<Expr, bool> unitClauseAssign(const Expr &clause, const Model &model);

// ---- Branching heuristics --------------------------------------------------

QPair<Expr, bool> noBranchingHeuristic(const QVector<Expr> &symbols,
                                        const QVector<Expr> &clauses);

// MOMS — maximum occurrences in minimum-size clauses
QPair<Expr, bool> moms(const QVector<Expr> &symbols, const QVector<Expr> &clauses);

// MOMS-F — weighted MOMS variant; k controls the weight of the combined score
QPair<Expr, bool> momsf(const QVector<Expr> &symbols, const QVector<Expr> &clauses,
                         int k = 0);

// POSIT — Freeman's version of MOMS, returns positive or negative polarity
QPair<Expr, bool> posit(const QVector<Expr> &symbols, const QVector<Expr> &clauses);

// ZM — Zabih & McAllester, counts negative occurrences only
QPair<Expr, bool> zm(const QVector<Expr> &symbols, const QVector<Expr> &clauses);

// DLIS — dynamic largest individual sum
QPair<Expr, bool> dlis(const QVector<Expr> &symbols, const QVector<Expr> &clauses);

// DLCS — dynamic largest combined sum
QPair<Expr, bool> dlcs(const QVector<Expr> &symbols, const QVector<Expr> &clauses);

// JW — Jeroslow-Wang (one-sided)
QPair<Expr, bool> jw(const QVector<Expr> &symbols, const QVector<Expr> &clauses);

// JW2 — Jeroslow-Wang two-sided
QPair<Expr, bool> jw2(const QVector<Expr> &symbols, const QVector<Expr> &clauses);
