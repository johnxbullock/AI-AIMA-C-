#pragma once

#include "expr.h"
#include <QSet>
#include <QVector>
#include <QPair>

// ---- Special identity Exprs (used by associate) ----------------------------
// ExprTrue  is the identity element for & (and as a pl_true shortcut)
// ExprFalse is the identity element for | (and as a pl_true shortcut)
extern const Expr ExprTrue;
extern const Expr ExprFalse;

// ---- Symbol predicates -----------------------------------------------------
bool isSymbol(const QString &s);      // starts with a letter
bool isVarSymbol(const QString &s);   // starts with a lowercase letter
bool isPropSymbol(const QString &s);  // starts with an uppercase letter
bool isVariable(const Expr &x);       // nullary Expr with a lowercase op

// ---- Subexpressions --------------------------------------------------------
QVector<Expr> subexpressions(const Expr &x);
QSet<Expr>    variables(const Expr &s);

// ---- Symbol sets -----------------------------------------------------------
QSet<Expr>                   propSymbols(const Expr &x);
QSet<Expr>                   constantSymbols(const Expr &x);
QSet<QPair<QString, int>>    predicateSymbols(const Expr &x);

// ---- Clause operations -----------------------------------------------------
QVector<Expr> dissociate(const QString &op, const QVector<Expr> &args);
Expr          associate(const QString &op, const QVector<Expr> &args);
QVector<Expr> conjuncts(const Expr &s);
QVector<Expr> disjuncts(const Expr &s);

// ---- Definite clause helpers -----------------------------------------------
bool isDefiniteClause(const Expr &s);
QPair<QVector<Expr>, Expr> parseDefiniteClause(const Expr &s);

// ---- Literal helpers -------------------------------------------------------
// Returns (symbol, positive): P -> (P, true), ~P -> (P, false)
QPair<Expr, bool> inspectLiteral(const Expr &literal);

// Remove ALL occurrences of val from list (preserve order)
QVector<Expr> removeAll(const Expr &val, const QVector<Expr> &list);

// Remove duplicate Exprs while preserving first-occurrence order
QVector<Expr> unique(const QVector<Expr> &list);

// ---- Commonly used symbol constants ----------------------------------------
extern const Expr A, B, C, D, E, F, G, P, Q;        // uppercase (propositions)
extern const Expr a, x, y, z, u;                     // lowercase (variables)
