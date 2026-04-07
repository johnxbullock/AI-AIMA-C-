#pragma once

#include "kb.h"
#include "graph.h"
#include "utils.h"
#include <functional>
#include <optional>
#include <tuple>

// ============================================================================
// TwoWLClauseDatabase — Two Watched Literals clause database
// ============================================================================

class TwoWLClauseDatabase {
public:
    explicit TwoWLClauseDatabase(const QVector<Expr> &clauses);

    QVector<Expr> getClauses() const;

    Expr getFirstWatched(const Expr &clause) const;
    Expr getSecondWatched(const Expr &clause) const;
    void setFirstWatched(const Expr &clause, const Expr &watching);
    void setSecondWatched(const Expr &clause, const Expr &watching);

    // Clauses that watch literal l in the positive sense (l itself is watched)
    QSet<Expr> getPosWatched(const Expr &l) const;
    // Clauses that watch literal l in the negative sense (~l is watched)
    QSet<Expr> getNegWatched(const Expr &l) const;

    void add(const Expr &clause, const Model &model);
    void remove(const Expr &clause);

    // Called when second watched is False: find replacement ≠ first watched
    // Returns true and updates the data structure if a replacement was found.
    bool updateFirstWatched(const Expr &clause, const Model &model);

    // Called when first watched is False: find replacement ≠ second watched
    // Returns true and updates the data structure if a replacement was found.
    bool updateSecondWatched(const Expr &clause, const Model &model);

private:
    using WatchPair = QPair<Expr, Expr>;

    QHash<Expr, WatchPair>                      m_twl;        // clause -> (w1, w2)
    QHash<Expr, QPair<QSet<Expr>, QSet<Expr>>>  m_watchList;  // symbol -> (pos, neg)

    WatchPair           assignWatchingLiterals(const Expr &clause,
                                               const Model &model) const;
    QPair<bool, Expr>   findNewWatchingLiteral(const Expr &clause,
                                               const Expr &otherWatched,
                                               const Model &model) const;
};

// ============================================================================
// Restart strategies
// ============================================================================

using RestartStrategy = std::function<bool(int, int, const QVector<int> &, int)>;

bool noRestart(int conflicts, int restarts, const QVector<int> &queueLbd, int sumLbd);

// Luby restart sequence scaled by unit (default 512)
bool luby(int conflicts, int restarts, const QVector<int> &queueLbd, int sumLbd,
          int unit = 512);

// Glucose restart: restart when recent average LBD > k * overall average
bool glucose(int conflicts, int restarts, const QVector<int> &queueLbd, int sumLbd,
             int x = 100, double k = 0.7);

// ============================================================================
// CDCL
// ============================================================================

std::optional<Model> cdclSatisfiable(const Expr &s,
                                      double vsidsDecay = 0.95,
                                      const RestartStrategy &restartStrategy = noRestart);

// ---- Internal helpers (exposed for testing) --------------------------------

Expr plBinaryResolution(const Expr &ci, const Expr &cj);

void assignDecisionLiteral(QSet<Expr> &symbols, Model &model,
                            QHash<Expr, double> &scores,
                            ImplicationGraph &G, int dl);

// Returns true if a conflict was detected (conflict node K added to G)
bool unitPropagation(TwoWLClauseDatabase &clauses, QSet<Expr> &symbols,
                     Model &model, ImplicationGraph &G, int dl);

// Returns {backtrack_level, learned_clause, lbd}
std::tuple<int, Expr, int> conflictAnalysis(ImplicationGraph &G, int dl);

// Remove all assignments at decision level > dl
void backjump(QSet<Expr> &symbols, Model &model, ImplicationGraph &G, int dl = 0);
