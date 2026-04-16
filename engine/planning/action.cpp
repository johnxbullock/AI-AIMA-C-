#include "action.h"
#include "../logic/utils.h"

// ============================================================================
// Constructors
// ============================================================================

Action::Action(const Expr &action,
               const QVector<Expr> &precond,
               const QVector<Expr> &effect,
               const QVector<Expr> &domain)
    : m_name(action.op())
    , m_args(action.args())
    , m_precond(convert(precond))
    , m_effect(convert(effect))
    , m_domain(convert(domain))
{}

Action::Action(const QString &action,
               const QVector<Expr> &precond,
               const QVector<Expr> &effect,
               const QVector<Expr> &domain)
    : Action(expr(action), precond, effect, domain)
{}

// ============================================================================
// convert — replace ~P with NotP
// ============================================================================

QVector<Expr> Action::convert(const QVector<Expr> &clauses) {
    QVector<Expr> result;
    result.reserve(clauses.size());
    for (const Expr &c : clauses) {
        if (c.op() == QStringLiteral("~") && c.args().size() == 1)
            result.append(expr(QStringLiteral("Not") + c.args()[0].op()));
        else
            result.append(c);
    }
    return result;
}

QVector<Expr> Action::convert(const Expr &clause) {
    return convert(QVector<Expr>{clause});
}

// ============================================================================
// toString / operator<<
// ============================================================================

QString Action::toString() const {
    if (m_args.isEmpty())
        return m_name;
    QStringList parts;
    for (const Expr &a : m_args)
        parts.append(a.toString());
    return m_name + QStringLiteral("(") + parts.join(QStringLiteral(", ")) + QStringLiteral(")");
}

QDebug operator<<(QDebug dbg, const Action &a) {
    dbg.noquote() << a.toString();
    return dbg;
}

// ============================================================================
// relaxed — drop negative effects (those whose predicate starts with "Not")
// ============================================================================

Action Action::relaxed() const {
    QVector<Expr> positiveEffects;
    for (const Expr &e : m_effect)
        if (!e.op().startsWith(QStringLiteral("Not")))
            positiveEffects.append(e);

    // Reconstruct action Expr so the constructor can parse it back
    Expr actionExpr(m_name, m_args);
    return Action(actionExpr, m_precond, positiveEffects, m_domain);
}

// ============================================================================
// substitute — replace action's parameter variables with the given args
// ============================================================================

/*!
 * @brief
 * @param e: An expression from the action's pre-condition (a particular pre-condition)
 * @param args: A permutation of ground args.
 *
 */
Expr Action::substitute(const Expr &e, const QVector<Expr> &args) const {
    QVector<Expr> newArgs; // new args that are going to be built up
    newArgs.reserve(e.args().size());
    for (const Expr &eArg : e.args()) // iterate through the args of the pre-condition expression
    {
        bool replaced = false;
        for (int j = 0; j < m_args.size() && j < args.size(); ++j) {
            if (m_args[j] == eArg) {
                newArgs.append(args[j]);
                replaced = true;
                break;
            }
        }
        if (!replaced)
            newArgs.append(eArg);
    }
    return Expr(e.op(), newArgs);
}

// ============================================================================
// checkPrecond
// ============================================================================

bool Action::checkPrecond(const FolKB &kb, const QVector<Expr> &args) const {
    for (const Expr &clause : m_precond)
        if (!kb.clauses.contains(substitute(clause, args)))
            return false;
    return true;
}

bool Action::checkPrecond(const QVector<Expr> &state, const QVector<Expr> &args) const {
    for (const Expr &clause : m_precond)
        if (!state.contains(substitute(clause, args)))
            return false;
    return true;
}

// ============================================================================
// act — apply action effects to a KB
// ============================================================================

static QString negation(const QString &op) {
    if (op.startsWith(QStringLiteral("Not")))
        return op.mid(3);           // NotEaten → Eaten
    return QStringLiteral("Not") + op; // Eaten → NotEaten
}

FolKB Action::act(FolKB kb, const QVector<Expr> &args) const {
    if (!checkPrecond(kb, args))
        throw std::runtime_error("Action::act — preconditions not satisfied for "
                                 + toString().toStdString());
    for (const Expr &clause : m_effect) {
        Expr ground = substitute(clause, args); //ground the effect
        kb.tell(ground); // add the effects of the action to the kb
        // retract the contradicting literal
        Expr contra(negation(ground.op()), ground.args()); // retract
        kb.retract(contra); // retract the negation?
    }
    return kb;
}

FolKB Action::act(const QVector<Expr> &state, const QVector<Expr> &args) const {
    FolKB kb(state);
    return act(kb, args);
}

FolKB Action::operator()(FolKB kb, const QVector<Expr> &args) const {
    return act(std::move(kb), args);
}
