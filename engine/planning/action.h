#pragma once

#include "../logic/expr.h"
#include "../logic/fol_kb.h"
#include <QVector>
#include <QString>

// ============================================================================
// Action — planning action schema with preconditions and effects
//
// Negative literals are represented by prepending "Not" to the predicate name:
//   ~Eaten(food)  becomes  NotEaten(food)
//
// Example:
//   Action eat("Eat(person, food)",
//              {expr("Human(person)"), expr("Hungry(person)")},
//              {expr("Eaten(food)"), expr("NotHungry(person)")});
// ============================================================================

class Action {
public:
    // Primary constructor — action is an Expr like Expr("Eat", {person, food})
    Action(const Expr &action,
           const QVector<Expr> &precond,
           const QVector<Expr> &effect,
           const QVector<Expr> &domain = {});

    // Convenience constructor — parses action string with expr()
    Action(const QString &action,
           const QVector<Expr> &precond,
           const QVector<Expr> &effect,
           const QVector<Expr> &domain = {});

    // Accessors
    QString        name()   const { return m_name; }
    QVector<Expr>  args()   const { return m_args; }
    QVector<Expr>  precond() const { return m_precond; }
    QVector<Expr>  effect()  const { return m_effect; }
    QVector<Expr>  domain()  const { return m_domain; }

    // String representation: "Name(arg1, arg2, ...)"
    QString toString() const;
    friend QDebug operator<<(QDebug dbg, const Action &a);

    // Returns a new Action with negative effects removed (the relaxed problem)
    Action relaxed() const;

    // Replace variables in e.args using this action's args → given args mapping
    Expr substitute(const Expr &e, const QVector<Expr> &args) const;

    // Returns true if all preconditions hold in kb under the given args binding
    bool checkPrecond(const FolKB &kb, const QVector<Expr> &args) const;
    bool checkPrecond(const QVector<Expr> &state, const QVector<Expr> &args) const;

    // Apply this action to kb under args; returns the resulting KB.
    // Adds effects and retracts contradicting literals.
    FolKB act(FolKB kb, const QVector<Expr> &args) const;
    FolKB act(const QVector<Expr> &state, const QVector<Expr> &args) const;

    // Callable syntax: action(kb, args) == action.act(kb, args)
    FolKB operator()(FolKB kb, const QVector<Expr> &args) const;

    // Convert a clause list: replaces ~P with NotP
    static QVector<Expr> convert(const QVector<Expr> &clauses);
    static QVector<Expr> convert(const Expr &clause);

private:
    QString       m_name; // the name of the action (same as operator), action.op
    QVector<Expr> m_args; // just a QList of Expr objects
    QVector<Expr> m_precond;
    QVector<Expr> m_effect;
    QVector<Expr> m_domain;
};
