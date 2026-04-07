#pragma once

#include "expr.h"
#include <QHash>
#include <optional>

// A substitution maps variable Exprs to their bound values
using Substitution = QHash<Expr, Expr>;

// Apply substitution s to expression x (recursively replaces variables)
Expr substExpr(const Substitution &s, const Expr &x);

// Returns true if var occurs inside x under substitution s (prevents circular bindings)
bool occurCheck(const Expr &var, const Expr &x, const Substitution &s);

// Unify expressions x and y under existing substitution s.
// Returns an extended substitution if unification succeeds, or nullopt on failure.
std::optional<Substitution> unifyMM(const Expr &x, const Expr &y,
                                     const Substitution &s = {});

// Replace all variables in sentence with fresh uniquely-named variables.
// Call this before each use of a KB rule to avoid variable capture.
Expr standardizeVariables(const Expr &sentence);
