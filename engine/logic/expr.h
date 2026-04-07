#pragma once

#include <QString>
#include <QVector>
#include <QDebug>

// Forward declarations needed for Expr friend declarations
class PartialExpr;
struct InfixOp;

class Expr {
public:
    Expr() = default;

    // we initialize it with the operator and the args
    explicit Expr(const QString &op, QVector<Expr> args = {});

    QString op() const;
    QVector<Expr> args() const;

    // Unary operators
    Expr operator-() const;
    Expr operator+() const;
    Expr operator~() const;

    // Binary operators (as free friends so both sides participate in overload resolution)
    friend Expr operator+(const Expr &lhs, const Expr &rhs);
    friend Expr operator-(const Expr &lhs, const Expr &rhs);
    friend Expr operator*(const Expr &lhs, const Expr &rhs);
    friend Expr operator/(const Expr &lhs, const Expr &rhs);
    friend Expr operator%(const Expr &lhs, const Expr &rhs);
    friend Expr operator&(const Expr &lhs, const Expr &rhs);
    friend Expr operator|(const Expr &lhs, const Expr &rhs);
    friend PartialExpr operator|(const Expr &lhs, const InfixOp &op);
    friend Expr operator^(const Expr &lhs, const Expr &rhs);
    friend Expr operator>>(const Expr &lhs, const Expr &rhs);
    friend Expr operator<<(const Expr &lhs, const Expr &rhs);

    // No C++ operators for **, //, @ — use free functions
    friend Expr pow(const Expr &lhs, const Expr &rhs);
    friend Expr floordiv(const Expr &lhs, const Expr &rhs);
    friend Expr matmul(const Expr &lhs, const Expr &rhs);

    // Call operator: Symbol("f")(a, b) == Expr("f", {a, b})
    Expr operator()(QVector<Expr> args) const;

    // Comparison
    bool operator==(const Expr &other) const;
    bool operator!=(const Expr &other) const { return !(*this == other); }
    bool operator<(const Expr &other) const;

    // String representation
    QString toString() const;
    friend QDebug operator<<(QDebug dbg, const Expr &expr);

private:
    QString m_op;
    QVector<Expr> m_args; // args are a QList of expr
};

uint qHash(const Expr &expr, uint seed = 0);

// Convenience alias matching the Python code
inline Expr Symbol(const QString &name) { return Expr(name); }

// ---- InfixOp ---------------------------------------------------------------
// Wrapper that enables the  P | InfixOp("==>") | Q  syntax.
// (C++ operator| cannot take a raw string on the right-hand side.)
struct InfixOp {
    explicit InfixOp(const QString &op) : op(op) {}
    QString op;
};

// ---- PartialExpr ------------------------------------------------------------
// Mirrors the Python PartialExpr class.
// Created by  P | InfixOp("==>"),  completed by  partial | Q.
class PartialExpr {
public:
    PartialExpr(const QString &op, const Expr &lhs);

    Expr operator|(const Expr &rhs) const;

    QString toString() const;
    friend QDebug operator<<(QDebug dbg, const PartialExpr &p);

private:
    QString m_op;
    Expr    m_lhs;
};

// ---- expr() parser ----------------------------------------------------------
// Parses a logic expression string into an Expr tree.
// Identifiers are automatically treated as Symbols.
// Supported operators (low to high precedence):
//   <=>   (biconditional)
//   ==>   <==   (implication)
//   |     (or)
//   &     (and)
//   ~     (unary not)
// Example:  expr("P & Q ==> Q")  returns  Expr("==>", {Expr("&",{P,Q}), Q})
Expr expr(const QString &x);
