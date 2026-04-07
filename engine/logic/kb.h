#pragma once

#include "expr.h"
#include <QHash>
#include <QVector>
#include <optional>
#include <functional>

// A substitution maps Expr variables to Expr values  (e.g. {x: Cain})
using Substitution = QHash<Expr, Expr>;

// A model maps propositional symbols to truth values
using Model = QHash<Expr, bool>;

// ============================================================================
// Propositional logic evaluation
// ============================================================================

// Evaluate exp under model. Returns nullopt when the model is incomplete.
std::optional<bool> plTrue(const Expr &exp, const Model &model = {});

// ============================================================================
// Truth-table entailment  [AIMA Figure 7.10]
// ============================================================================

bool ttEntails(const Expr &kb, const Expr &alpha);
bool ttCheckAll(const Expr &kb, const Expr &alpha,
                QVector<Expr> symbols, Model model);
bool ttTrue(const Expr &s);   // is s a tautology?

// ============================================================================
// Resolution  [AIMA Figure 7.12]
// ============================================================================

bool         plResolution(const QVector<Expr> &clauses, const Expr &alpha);
QVector<Expr> plResolve(const Expr &ci, const Expr &cj);

// ============================================================================
// Forward-chaining  [AIMA Figure 7.15]
// ============================================================================

bool plFcEntails(const QVector<Expr> &clauses, const Expr &q);

// ============================================================================
// KB  — abstract base class
// ============================================================================

class KB {
public:
    KB() = default;
    virtual ~KB() = default;

    virtual void tell(const Expr &sentence) = 0;
    virtual QVector<Substitution> askGenerator(const Expr &query) = 0;
    virtual void retract(const Expr &sentence) = 0;

    // Returns the first substitution from askGenerator, or an empty optional.
    std::optional<Substitution> ask(const Expr &query);
};

// ============================================================================
// PropKB  — propositional logic KB (inefficient, no indexing)
// ============================================================================

class PropKB : public KB {
public:
    PropKB() = default;
    explicit PropKB(const Expr &sentence);

    void tell(const Expr &sentence) override;
    QVector<Substitution> askGenerator(const Expr &query) override;
    bool askIfTrue(const Expr &query);
    void retract(const Expr &sentence) override;

    QVector<Expr> clauses;
};

// ============================================================================
// PropDefiniteKB  — KB of propositional definite clauses
// ============================================================================

class PropDefiniteKB : public PropKB {
public:
    PropDefiniteKB() = default;
    explicit PropDefiniteKB(const Expr &sentence);

    void tell(const Expr &sentence) override;
    QVector<Substitution> askGenerator(const Expr &query) override;
    void retract(const Expr &sentence) override;

    // Return clauses whose premise contains p
    QVector<Expr> clausesWithPremise(const Expr &p) const;
};

// ============================================================================
// KBAgentProgram  [AIMA Figure 7.1]
// ============================================================================

// Returns a program function: given a percept Expr, returns an action Expr.
std::function<Expr(const Expr &)> makeKBAgentProgram(KB &kb);
