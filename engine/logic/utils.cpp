#include "utils.h"

// ---- Identity constants ----------------------------------------------------
const Expr ExprTrue  = Expr(QStringLiteral("True"));
const Expr ExprFalse = Expr(QStringLiteral("False"));

// ---- Common symbol constants -----------------------------------------------
const Expr A = Expr(QStringLiteral("A")), B = Expr(QStringLiteral("B")),
           C = Expr(QStringLiteral("C")), D = Expr(QStringLiteral("D")),
           E = Expr(QStringLiteral("E")), F = Expr(QStringLiteral("F")),
           G = Expr(QStringLiteral("G")), P = Expr(QStringLiteral("P")),
           Q = Expr(QStringLiteral("Q"));
const Expr a = Expr(QStringLiteral("a")), x = Expr(QStringLiteral("x")),
           y = Expr(QStringLiteral("y")), z = Expr(QStringLiteral("z")),
           u = Expr(QStringLiteral("u"));

// ---- Symbol predicates -----------------------------------------------------

bool isSymbol(const QString &s) {
    return !s.isEmpty() && s[0].isLetter();
}

bool isVarSymbol(const QString &s) {
    return isSymbol(s) && s[0].isLower();
}

bool isPropSymbol(const QString &s) {
    return isSymbol(s) && s[0].isUpper();
}

bool isVariable(const Expr &x) {
    return isVarSymbol(x.op()) && x.args().isEmpty();
}

// ---- Subexpressions --------------------------------------------------------

QVector<Expr> subexpressions(const Expr &x) {
    QVector<Expr> result = {x};
    for (const Expr &arg : x.args())
        result += subexpressions(arg);
    return result;
}

QSet<Expr> variables(const Expr &s) {
    QSet<Expr> result;
    for (const Expr &sub : subexpressions(s))
        if (isVariable(sub))
            result.insert(sub);
    return result;
}

// ---- Symbol sets -----------------------------------------------------------

QSet<Expr> propSymbols(const Expr &x) {
    if (isPropSymbol(x.op()))
        return {x};
    QSet<Expr> result;
    for (const Expr &arg : x.args())
        result += propSymbols(arg);
    return result;
}

QSet<Expr> constantSymbols(const Expr &x) {
    if (isPropSymbol(x.op()) && x.args().isEmpty())
        return {x};
    QSet<Expr> result;
    for (const Expr &arg : x.args())
        result += constantSymbols(arg);
    return result;
}

QSet<QPair<QString, int>> predicateSymbols(const Expr &x) {
    if (x.args().isEmpty())
        return {};
    QSet<QPair<QString, int>> result;
    if (isPropSymbol(x.op()))
        result.insert({x.op(), x.args().size()});
    for (const Expr &arg : x.args())
        result += predicateSymbols(arg);
    return result;
}

// ---- Clause operations -----------------------------------------------------

QVector<Expr> dissociate(const QString &op, const QVector<Expr> &args) {
    QVector<Expr> result;
    std::function<void(const QVector<Expr> &)> collect = [&](const QVector<Expr> &subargs) {
        for (const Expr &arg : subargs) {
            if (arg.op() == op)
                collect(arg.args());
            else
                result.append(arg);
        }
    };
    collect(args);
    return result;
}

Expr associate(const QString &op, const QVector<Expr> &args) {
    QVector<Expr> flat = dissociate(op, args);
    if (flat.isEmpty()) {
        if (op == QStringLiteral("&")) return ExprTrue;
        if (op == QStringLiteral("|")) return ExprFalse;
        if (op == QStringLiteral("+")) return Expr(QStringLiteral("0"));
        if (op == QStringLiteral("*")) return Expr(QStringLiteral("1"));
        return Expr(op);
    }
    if (flat.size() == 1)
        return flat.first();
    return Expr(op, flat);
}

QVector<Expr> conjuncts(const Expr &s) {
    return dissociate(QStringLiteral("&"), {s});
}

QVector<Expr> disjuncts(const Expr &s) {
    return dissociate(QStringLiteral("|"), {s});
}

// ---- Definite clause helpers -----------------------------------------------

bool isDefiniteClause(const Expr &s) {
    if (isSymbol(s.op()))
        return true;
    if (s.op() == QStringLiteral("==>")) {
        const QVector<Expr> &args = s.args();
        if (args.size() != 2) return false;
        if (!isSymbol(args[1].op())) return false;
        for (const Expr &arg : conjuncts(args[0]))
            if (!isSymbol(arg.op())) return false;
        return true;
    }
    return false;
}

QPair<QVector<Expr>, Expr> parseDefiniteClause(const Expr &s) {
    Q_ASSERT_X(isDefiniteClause(s), "parseDefiniteClause", "Argument must be a definite clause");
    if (isSymbol(s.op()))
        return {{}, s};
    return {conjuncts(s.args()[0]), s.args()[1]};
}

// ---- Literal helpers -------------------------------------------------------

QPair<Expr, bool> inspectLiteral(const Expr &literal) {
    if (literal.op() == QStringLiteral("~"))
        return {literal.args()[0], false};
    return {literal, true};
}

QVector<Expr> removeAll(const Expr &val, const QVector<Expr> &list) {
    QVector<Expr> result;
    for (const Expr &e : list)
        if (!(e == val)) result.append(e);
    return result;
}

QVector<Expr> unique(const QVector<Expr> &list) {
    QVector<Expr> result;
    for (const Expr &e : list)
        if (!result.contains(e)) result.append(e);
    return result;
}
