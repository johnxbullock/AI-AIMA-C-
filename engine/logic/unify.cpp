#include "unify.h"
#include "utils.h"
#include <atomic>

// ---- substExpr -------------------------------------------------------------

Expr substExpr(const Substitution &s, const Expr &x) {
    if (isVariable(x))
        return s.value(x, x);
    if (x.args().isEmpty())
        return x;
    QVector<Expr> newArgs;
    newArgs.reserve(x.args().size());
    for (const Expr &arg : x.args())
        newArgs.append(substExpr(s, arg));
    return Expr(x.op(), newArgs);
}

// ---- occurCheck ------------------------------------------------------------

bool occurCheck(const Expr &var, const Expr &x, const Substitution &s) {
    if (var == x) return true;
    if (isVariable(x) && s.contains(x))
        return occurCheck(var, s.value(x), s);
    for (const Expr &arg : x.args())
        if (occurCheck(var, arg, s)) return true;
    return false;
}

// ---- unifyVariable ---------------------------------------------------------

static std::optional<Substitution> unifyVariable(const Expr &var, const Expr &x,
                                                  const Substitution &s) {
    if (s.contains(var)) return unifyMM(s.value(var), x, s);
    if (isVariable(x) && s.contains(x)) return unifyMM(var, s.value(x), s);
    if (occurCheck(var, x, s)) return std::nullopt;
    Substitution s2 = s;
    s2[var] = x;
    return s2;
}

// ---- unifyMM ---------------------------------------------------------------

std::optional<Substitution> unifyMM(const Expr &x, const Expr &y,
                                     const Substitution &s) {
    if (x == y)          return s;
    if (isVariable(x))   return unifyVariable(x, y, s);
    if (isVariable(y))   return unifyVariable(y, x, s);

    // Compound Exprs: match op and unify args pairwise
    if (!x.args().isEmpty() && x.op() == y.op()
            && x.args().size() == y.args().size()) {
        Substitution s2 = s;
        for (int i = 0; i < x.args().size(); ++i) {
            auto r = unifyMM(x.args()[i], y.args()[i], s2);
            if (!r) return std::nullopt;
            s2 = *r;
        }
        return s2;
    }
    return std::nullopt;
}

// ---- standardizeVariables --------------------------------------------------

static std::atomic<int> g_varCounter{0};

static Expr standardizeVariablesHelper(const Expr &sentence,
                                        QHash<Expr, Expr> &mapping) {
    if (!sentence.args().isEmpty()) {
        QVector<Expr> newArgs;
        newArgs.reserve(sentence.args().size());
        for (const Expr &arg : sentence.args())
            newArgs.append(standardizeVariablesHelper(arg, mapping));
        return Expr(sentence.op(), newArgs);
    }
    if (!isVariable(sentence))
        return sentence;  // constant — keep as-is
    if (!mapping.contains(sentence)) {
        int id = g_varCounter.fetch_add(1, std::memory_order_relaxed);
        mapping[sentence] = Expr(QString("v_%1").arg(id));
    }
    return mapping[sentence];
}

Expr standardizeVariables(const Expr &sentence) {
    QHash<Expr, Expr> mapping;
    return standardizeVariablesHelper(sentence, mapping);
}
