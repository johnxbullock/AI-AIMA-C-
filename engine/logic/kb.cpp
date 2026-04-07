#include "kb.h"
#include "utils.h"
#include "cnf.h"
#include "expr.h"
#include <stdexcept>

// ============================================================================
// plTrue
// ============================================================================

std::optional<bool> plTrue(const Expr &exp, const Model &model) {
    // Handle identity constants used by associate()
    if (exp == ExprTrue)  return true;
    if (exp == ExprFalse) return false;

    const QString       &op   = exp.op();
    const QVector<Expr> &args = exp.args();

    if (isPropSymbol(op)) {
        auto it = model.find(exp);
        return it != model.end() ? std::optional<bool>(it.value()) : std::nullopt;
    }

    if (op == QStringLiteral("~")) {
        auto p = plTrue(args[0], model);
        return p.has_value() ? std::optional<bool>(!p.value()) : std::nullopt;
    }

    if (op == QStringLiteral("|")) {
        std::optional<bool> result = false;
        for (const Expr &arg : args) {
            auto p = plTrue(arg, model);
            if (p.has_value() && p.value())  return true;
            if (!p.has_value())              result = std::nullopt;
        }
        return result;
    }

    if (op == QStringLiteral("&")) {
        std::optional<bool> result = true;
        for (const Expr &arg : args) {
            auto p = plTrue(arg, model);
            if (p.has_value() && !p.value()) return false;
            if (!p.has_value())              result = std::nullopt;
        }
        return result;
    }

    // Binary operators — need both operands
    Q_ASSERT(args.size() == 2);

    if (op == QStringLiteral("==>"))
        return plTrue(~args[0] | args[1], model);
    if (op == QStringLiteral("<=="))
        return plTrue(args[0] | ~args[1], model);

    auto pt = plTrue(args[0], model);
    if (!pt.has_value()) return std::nullopt;
    auto qt = plTrue(args[1], model);
    if (!qt.has_value()) return std::nullopt;

    if (op == QStringLiteral("<=>")) return pt.value() == qt.value();
    if (op == QStringLiteral("^"))   return pt.value() != qt.value();

    throw std::runtime_error("Illegal operator in logic expression: " + op.toStdString());
}

// ============================================================================
// Truth-table entailment
// ============================================================================

bool ttCheckAll(const Expr &kb, const Expr &alpha,
                QVector<Expr> symbols, Model model) {
    if (symbols.isEmpty()) {
        auto kbVal = plTrue(kb, model);
        if (kbVal.has_value() && kbVal.value()) {
            auto result = plTrue(alpha, model);
            Q_ASSERT(result.has_value());
            return result.value();
        }
        return true;  // model doesn't satisfy KB — not a counter-example
    }

    Expr sym = symbols.takeFirst();

    Model modelTrue  = model; modelTrue[sym]  = true;
    Model modelFalse = model; modelFalse[sym] = false;

    return ttCheckAll(kb, alpha, symbols, modelTrue)
        && ttCheckAll(kb, alpha, symbols, modelFalse);
}

bool ttEntails(const Expr &kb, const Expr &alpha) {
    Q_ASSERT_X(variables(alpha).isEmpty(), "ttEntails",
               "alpha must be a ground (variable-free) sentence");
    QSet<Expr>    symSet = propSymbols(kb & alpha);
    QVector<Expr> syms(symSet.begin(), symSet.end());
    return ttCheckAll(kb, alpha, syms, {});
}

bool ttTrue(const Expr &s) {
    return ttEntails(ExprTrue, s);
}

// ============================================================================
// Resolution
// ============================================================================

QVector<Expr> plResolve(const Expr &ci, const Expr &cj) {
    QVector<Expr> clauses;
    const QVector<Expr> diI = disjuncts(ci);
    const QVector<Expr> diJ = disjuncts(cj);

    for (const Expr &di : diI) {
        for (const Expr &dj : diJ) {
            if (di == ~dj || ~di == dj) {
                // Remove di from diI and dj from diJ, combine, deduplicate
                QVector<Expr> combined;
                for (const Expr &e : diI) if (!(e == di)) combined.append(e);
                for (const Expr &e : diJ) if (!(e == dj)) combined.append(e);

                QVector<Expr> unique;
                for (const Expr &e : combined)
                    if (!unique.contains(e)) unique.append(e);

                clauses.append(associate(QStringLiteral("|"), unique));
            }
        }
    }
    return clauses;
}

bool plResolution(const QVector<Expr> &kbClauses, const Expr &alpha) {
    QVector<Expr> clauses = kbClauses + conjuncts(toCnf(~alpha));
    QSet<Expr>    seen;

    while (true) {
        const int n = clauses.size();
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                for (const Expr &resolvent : plResolve(clauses[i], clauses[j])) {
                    if (resolvent == ExprFalse)
                        return true;   // derived the empty clause — entailed
                    if (!seen.contains(resolvent)) {
                        seen.insert(resolvent);
                        clauses.append(resolvent);
                    }
                }
            }
        }
        if (clauses.size() == n)
            return false;  // no new resolvents — not entailed
    }
}

// ============================================================================
// Forward chaining
// ============================================================================

bool plFcEntails(const QVector<Expr> &clauses, const Expr &q) {
    // count[c] = number of unsatisfied premises for implication c
    QHash<Expr, int> count;
    for (const Expr &c : clauses)
        if (c.op() == QStringLiteral("==>"))
            count[c] = conjuncts(c.args()[0]).size();

    QHash<Expr, bool> inferred;
    QVector<Expr>     agenda;
    for (const Expr &s : clauses)
        if (isPropSymbol(s.op())) agenda.append(s);

    while (!agenda.isEmpty()) {
        Expr p = agenda.takeLast();
        if (p == q) return true;

        if (!inferred.value(p, false)) {
            inferred[p] = true;
            for (const Expr &c : clauses) {
                if (c.op() == QStringLiteral("==>")
                        && conjuncts(c.args()[0]).contains(p)) {
                    if (--count[c] == 0)
                        agenda.append(c.args()[1]);
                }
            }
        }
    }
    return false;
}

// ============================================================================
// KB base class
// ============================================================================

std::optional<Substitution> KB::ask(const Expr &query) {
    QVector<Substitution> results = askGenerator(query);
    if (results.isEmpty()) return std::nullopt;
    return results.first();
}

// ============================================================================
// PropKB
// ============================================================================

PropKB::PropKB(const Expr &sentence) {
    tell(sentence);
}

void PropKB::tell(const Expr &sentence) {
    for (const Expr &clause : conjuncts(toCnf(sentence)))
        clauses.append(clause);
}

QVector<Substitution> PropKB::askGenerator(const Expr &query) {
    if (ttEntails(associate(QStringLiteral("&"), clauses), query))
        return {{}};   // yield the empty substitution
    return {};
}

bool PropKB::askIfTrue(const Expr &query) {
    return !askGenerator(query).isEmpty();
}

void PropKB::retract(const Expr &sentence) {
    for (const Expr &c : conjuncts(toCnf(sentence)))
        clauses.removeOne(c);
}

// ============================================================================
// PropDefiniteKB
// ============================================================================

PropDefiniteKB::PropDefiniteKB(const Expr &sentence) {
    tell(sentence);
}

void PropDefiniteKB::tell(const Expr &sentence) {
    Q_ASSERT_X(isDefiniteClause(sentence), "PropDefiniteKB::tell",
               "Sentence must be a definite clause");
    clauses.append(sentence);
}

QVector<Substitution> PropDefiniteKB::askGenerator(const Expr &query) {
    if (plFcEntails(clauses, query))
        return {{}};
    return {};
}

void PropDefiniteKB::retract(const Expr &sentence) {
    clauses.removeOne(sentence);
}

QVector<Expr> PropDefiniteKB::clausesWithPremise(const Expr &p) const {
    QVector<Expr> result;
    for (const Expr &c : clauses)
        if (c.op() == QStringLiteral("==>") && conjuncts(c.args()[0]).contains(p))
            result.append(c);
    return result;
}

// ============================================================================
// KBAgentProgram
// ============================================================================

namespace {

Expr makePerceptSentence(const Expr &percept, int t) {
    return Expr(QStringLiteral("Percept"), {percept, Expr(QString::number(t))});
}

Expr makeActionQuery(int t) {
    return expr(QString("ShouldDo(action, %1)").arg(t));
}

Expr makeActionSentence(const Substitution &action, int t) {
    Expr actionVar = Expr(QStringLiteral("action"));
    Expr act = action.value(actionVar, Expr(QStringLiteral("NoAction")));
    return Expr(QStringLiteral("Did"), {act, Expr(QString::number(t))});
}

} // anonymous namespace

std::function<Expr(const Expr &)> makeKBAgentProgram(KB &kb) {
    auto step = std::make_shared<int>(0);
    return [&kb, step](const Expr &percept) -> Expr {
        int t = (*step)++;
        kb.tell(makePerceptSentence(percept, t));
        auto action = kb.ask(makeActionQuery(t));
        Substitution sub = action.value_or(Substitution{});
        kb.tell(makeActionSentence(sub, t));
        return sub.value(Expr(QStringLiteral("action")),
                         Expr(QStringLiteral("NoAction")));
    };
}
