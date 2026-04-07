#pragma once

#include "kb.h"
#include "unify.h"
#include <optional>

// ============================================================================
// FolKB — First-Order Logic Knowledge Base
// Uses backward chaining (fol_bc_ask) for ask().
// ============================================================================

class FolKB : public KB {
public:
    FolKB() = default;
    explicit FolKB(const QVector<Expr> &clauses);

    // Add a definite clause to the KB
    void tell(const Expr &sentence) override;

    // Yields all substitutions that make query true via backward chaining
    QVector<Substitution> askGenerator(const Expr &query) override;

    void retract(const Expr &sentence) override;

    // All KB clauses (definite clauses only)
    QVector<Expr> clauses;

    // Returns all clauses that could match the given goal (currently all clauses)
    QVector<Expr> fetchRulesForGoal(const Expr &goal) const;
};

// ============================================================================
// Backward chaining  [AIMA Figure 9.6]
// ============================================================================

QVector<Substitution> folBcAsk(const FolKB &kb, const Expr &query);

// ============================================================================
// Forward chaining  [AIMA Figure 9.3]
// Modifies kb by adding newly inferred clauses.
// Returns all substitutions that unify with alpha.
// ============================================================================

QVector<Substitution> folFcAsk(FolKB &kb, const Expr &alpha);
