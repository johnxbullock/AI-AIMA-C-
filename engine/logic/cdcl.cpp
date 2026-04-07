#include "cdcl.h"
#include "cnf.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <functional>

// ============================================================================
// TwoWLClauseDatabase
// ============================================================================

TwoWLClauseDatabase::TwoWLClauseDatabase(const QVector<Expr> &clauses) {
    for (const Expr &c : clauses)
        add(c, {});
}

QVector<Expr> TwoWLClauseDatabase::getClauses() const {
    return QVector<Expr>(m_twl.keyBegin(), m_twl.keyEnd());
}

// ---- Watched literal accessors ---------------------------------------------

Expr TwoWLClauseDatabase::getFirstWatched(const Expr &clause) const {
    const int n = clause.args().size();
    if (n == 2) return clause.args()[0];
    if (n  > 2) return m_twl.value(clause).first;
    return clause;  // unit clause: the clause IS the literal
}

Expr TwoWLClauseDatabase::getSecondWatched(const Expr &clause) const {
    const int n = clause.args().size();
    if (n == 2) return clause.args().last();
    if (n  > 2) return m_twl.value(clause).second;
    return clause;
}

void TwoWLClauseDatabase::setFirstWatched(const Expr &clause, const Expr &w) {
    if (clause.args().size() > 2) m_twl[clause].first  = w;
}

void TwoWLClauseDatabase::setSecondWatched(const Expr &clause, const Expr &w) {
    if (clause.args().size() > 2) m_twl[clause].second = w;
}

// ---- Watch list accessors --------------------------------------------------

QSet<Expr> TwoWLClauseDatabase::getPosWatched(const Expr &l) const {
    return m_watchList.value(l).first;
}

QSet<Expr> TwoWLClauseDatabase::getNegWatched(const Expr &l) const {
    return m_watchList.value(l).second;
}

// ---- Internal helpers ------------------------------------------------------

TwoWLClauseDatabase::WatchPair
TwoWLClauseDatabase::assignWatchingLiterals(const Expr &clause,
                                             const Model &model) const {
    if (clause.args().size() <= 2)
        return {};  // not used — binary/unit clauses handled directly

    if (model.isEmpty()) {
        return {clause.args().first(), clause.args().last()};
    }
    // Find one unassigned and one false literal to watch
    Expr unassigned, falseLit;
    for (const Expr &l : disjuncts(clause)) {
        auto v = plTrue(l, model);
        if (!v.has_value() && unassigned.op().isEmpty())
            unassigned = l;
        else if (v.has_value() && !v.value() && falseLit.op().isEmpty())
            falseLit = l;
    }
    return {unassigned, falseLit};
}

QPair<bool, Expr>
TwoWLClauseDatabase::findNewWatchingLiteral(const Expr &clause,
                                             const Expr &otherWatched,
                                             const Model &model) const {
    if (clause.args().size() > 2) {
        for (const Expr &l : disjuncts(clause)) {
            if (!(l == otherWatched)) {
                auto v = plTrue(l, model);
                if (!v.has_value() || v.value())  // not False
                    return {true, l};
            }
        }
    }
    return {false, Expr()};
}

// ---- add / remove ----------------------------------------------------------

void TwoWLClauseDatabase::add(const Expr &clause, const Model &model) {
    m_twl[clause] = assignWatchingLiterals(clause, model);

    auto [w1, p1] = inspectLiteral(getFirstWatched(clause));
    auto [w2, p2] = inspectLiteral(getSecondWatched(clause));

    if (p1) m_watchList[w1].first.insert(clause);
    else     m_watchList[w1].second.insert(clause);

    if (!(w1 == w2)) {
        if (p2) m_watchList[w2].first.insert(clause);
        else     m_watchList[w2].second.insert(clause);
    }
}

void TwoWLClauseDatabase::remove(const Expr &clause) {
    auto [w1, p1] = inspectLiteral(getFirstWatched(clause));
    auto [w2, p2] = inspectLiteral(getSecondWatched(clause));

    if (p1) m_watchList[w1].first.remove(clause);
    else     m_watchList[w1].second.remove(clause);

    if (!(w1 == w2)) {
        if (p2) m_watchList[w2].first.remove(clause);
        else     m_watchList[w2].second.remove(clause);
    }
    m_twl.remove(clause);
}

// ---- update watched --------------------------------------------------------

// Called when second watched is False: replace second watched with a new literal ≠ first watched
bool TwoWLClauseDatabase::updateFirstWatched(const Expr &clause, const Model &model) {
    auto [found, newW] = findNewWatchingLiteral(clause, getFirstWatched(clause), model);
    if (!found) return false;

    auto [w, p] = inspectLiteral(getSecondWatched(clause));
    if (p) m_watchList[w].first.remove(clause);
    else    m_watchList[w].second.remove(clause);

    setSecondWatched(clause, newW);
    auto [nw, np] = inspectLiteral(newW);
    if (np) m_watchList[nw].first.insert(clause);
    else     m_watchList[nw].second.insert(clause);
    return true;
}

// Called when first watched is False: replace first watched with a new literal ≠ second watched
bool TwoWLClauseDatabase::updateSecondWatched(const Expr &clause, const Model &model) {
    auto [found, newW] = findNewWatchingLiteral(clause, getSecondWatched(clause), model);
    if (!found) return false;

    auto [w, p] = inspectLiteral(getFirstWatched(clause));
    if (p) m_watchList[w].first.remove(clause);
    else    m_watchList[w].second.remove(clause);

    setFirstWatched(clause, newW);
    auto [nw, np] = inspectLiteral(newW);
    if (np) m_watchList[nw].first.insert(clause);
    else     m_watchList[nw].second.insert(clause);
    return true;
}

// ============================================================================
// Restart strategies
// ============================================================================

bool noRestart(int, int, const QVector<int> &, int) { return false; }

bool luby(int, int restarts, const QVector<int> &queueLbd, int, int unit) {
    std::function<int(int)> _luby = [&](int i) -> int {
        for (int k = 1; ; ++k) {
            if (i == (1 << k) - 1) return 1 << (k - 1);
            if ((1 << (k - 1)) <= i && i < (1 << k) - 1)
                return _luby(i - (1 << (k - 1)) + 1);
        }
    };
    return unit * _luby(restarts) == queueLbd.size();
}

bool glucose(int conflicts, int, const QVector<int> &queueLbd, int sumLbd, int x, double k) {
    if (queueLbd.size() < x) return false;
    double avgRecent = (double)std::accumulate(queueLbd.begin(), queueLbd.end(), 0) / queueLbd.size();
    double avgAll    = (double)sumLbd / conflicts;
    return avgRecent * k > avgAll;
}

// ============================================================================
// CDCL helpers
// ============================================================================

Expr plBinaryResolution(const Expr &ci, const Expr &cj) {
    const QVector<Expr> diI = disjuncts(ci);
    const QVector<Expr> diJ = disjuncts(cj);
    for (const Expr &di : diI) {
        for (const Expr &dj : diJ) {
            if (di == ~dj || ~di == dj) {
                return plBinaryResolution(
                    associate(QStringLiteral("|"), removeAll(di, diI)),
                    associate(QStringLiteral("|"), removeAll(dj, diJ)));
            }
        }
    }
    return associate(QStringLiteral("|"), unique(diI + diJ));
}

void assignDecisionLiteral(QSet<Expr> &symbols, Model &model,
                            QHash<Expr, double> &scores,
                            ImplicationGraph &G, int dl) {
    auto P = *std::max_element(symbols.begin(), symbols.end(),
        [&](const Expr &a, const Expr &b) {
            return (scores.value(a) + scores.value(~a))
                 < (scores.value(b) + scores.value(~b));
        });
    bool value = scores.value(P) >= scores.value(~P);
    symbols.remove(P);
    model[P] = value;
    G.addNode(P, value, dl);
}

void backjump(QSet<Expr> &symbols, Model &model, ImplicationGraph &G, int dl) {
    QSet<Expr> toDelete;
    for (const Expr &node : G.nodes())
        if (G.nodeAttr(node).dl > dl) toDelete.insert(node);

    G.removeNodes(toDelete);
    for (const Expr &node : toDelete) {
        model.remove(node);
        symbols.insert(node);
    }
}

bool unitPropagation(TwoWLClauseDatabase &clauses, QSet<Expr> &symbols,
                     Model &model, ImplicationGraph &G, int dl) {
    const Expr K = ImplicationGraph::ConflictNode;

    // Adds a forced assignment to model and graph
    auto doUnitClause = [&](const Expr &watching, const Expr &c) {
        auto [w, p] = inspectLiteral(watching);
        G.addNode(w, p, dl);
        QSet<Expr> srcs = propSymbols(c);
        srcs.remove(w);
        G.addEdgesFrom(srcs, w, c);
        symbols.remove(w);
        model[w] = p;
    };

    // Records a conflict in the graph
    auto doConflictClause = [&](const Expr &c) {
        G.addEdgesFrom(propSymbols(c), K, c);
    };

    // Returns true iff a watched literal of c is now False (needs re-examination)
    auto needsCheck = [&](const Expr &c) -> bool {
        if (model.isEmpty() || clauses.getFirstWatched(c) == clauses.getSecondWatched(c))
            return true;
        auto [w1, _1] = inspectLiteral(clauses.getFirstWatched(c));
        if (model.contains(w1)) {
            bool v = model[w1];
            return v ? clauses.getNegWatched(w1).contains(c)
                     : clauses.getPosWatched(w1).contains(c);
        }
        auto [w2, _2] = inspectLiteral(clauses.getSecondWatched(c));
        if (model.contains(w2)) {
            bool v = model[w2];
            return v ? clauses.getNegWatched(w2).contains(c)
                     : clauses.getPosWatched(w2).contains(c);
        }
        return false;
    };

    auto isTrue  = [](std::optional<bool> v){ return v.has_value() &&  v.value(); };
    auto isFalse = [](std::optional<bool> v){ return v.has_value() && !v.value(); };
    auto isNone  = [](std::optional<bool> v){ return !v.has_value(); };

    while (true) {
        bool bcp = false;
        for (const Expr &c : clauses.getClauses()) {
            if (!needsCheck(c)) continue;

            auto fw = plTrue(clauses.getFirstWatched(c),  model);
            auto sw = plTrue(clauses.getSecondWatched(c), model);
            bool sameW = clauses.getFirstWatched(c) == clauses.getSecondWatched(c);

            if (isNone(fw) && sameW) {
                doUnitClause(clauses.getFirstWatched(c), c);
                bcp = true; break;
            }
            if (isFalse(fw) && !isTrue(sw)) {
                if (clauses.updateSecondWatched(c, model)) {
                    bcp = true;
                } else if (isNone(sw)) {
                    doUnitClause(clauses.getSecondWatched(c), c);
                    bcp = true; break;
                } else {
                    doConflictClause(c); return true;
                }
            } else if (isFalse(sw) && !isTrue(fw)) {
                if (clauses.updateFirstWatched(c, model)) {
                    bcp = true;
                } else if (isNone(fw)) {
                    doUnitClause(clauses.getFirstWatched(c), c);
                    bcp = true; break;
                } else {
                    doConflictClause(c); return true;
                }
            }
        }
        if (!bcp) return false;
    }
}

std::tuple<int, Expr, int> conflictAnalysis(ImplicationGraph &G, int dl) {
    const Expr K = ImplicationGraph::ConflictNode;

    // Conflict clause = antecedent on the first edge into K
    Expr conflictClause;
    for (const Expr &p : G.predecessors(K)) {
        conflictClause = G.edgeAntecedent(p, K);
        break;
    }

    // Decision literal at current dl: no predecessors, dl == current dl
    Expr P;
    for (const Expr &node : G.nodes()) {
        if (!(node == K) && G.nodeAttr(node).dl == dl && G.inDegree(node) == 0) {
            P = node; break;
        }
    }

    Expr firstUip = G.immediateDominators(P).value(K);
    G.removeNode(K);
    QSet<Expr> conflictSide = G.descendants(firstUip);

    while (true) {
        QSet<Expr> intersection = propSymbols(conflictClause) & conflictSide;
        if (intersection.isEmpty()) break;

        for (const Expr &l : intersection) {
            Expr antecedent;
            for (const Expr &p : G.predecessors(l)) {
                antecedent = G.edgeAntecedent(p, l);
                break;
            }
            conflictClause = plBinaryResolution(conflictClause, antecedent);

            QVector<int> lbd;
            for (const Expr &sym : propSymbols(conflictClause))
                if (G.hasNode(sym))
                    lbd.append(G.nodeAttr(sym).dl);

            int countDl = (int)std::count(lbd.begin(), lbd.end(), dl);
            if (countDl == 1 && propSymbols(conflictClause).contains(firstUip)) {
                int backLevel = 0;
                if (lbd.size() > 1) {
                    QVector<int> s = lbd;
                    std::partial_sort(s.begin(), s.begin() + 2, s.end(), std::greater<int>());
                    backLevel = s[1];
                }
                return {backLevel, conflictClause,
                        (int)QSet<int>(lbd.begin(), lbd.end()).size()};
            }
        }
    }
    return {0, conflictClause, 1};
}

// ============================================================================
// CDCL main loop
// ============================================================================

std::optional<Model> cdclSatisfiable(const Expr &s, double vsidsDecay,
                                      const RestartStrategy &restartStrategy) {
    TwoWLClauseDatabase clauses(conjuncts(toCnf(s)));
    QSet<Expr>          symbols = propSymbols(s);
    QHash<Expr, double> scores;
    ImplicationGraph    G;
    Model               model;
    int dl        = 0;
    int conflicts = 0;
    int restarts  = 1;
    int sumLbd    = 0;
    QVector<int> queueLbd;

    while (true) {
        bool conflict = unitPropagation(clauses, symbols, model, G, dl);

        if (conflict) {
            if (dl == 0) return std::nullopt;

            ++conflicts;
            auto [newDl, learn, lbd] = conflictAnalysis(G, dl);
            dl = newDl;
            queueLbd.append(lbd);
            sumLbd += lbd;

            backjump(symbols, model, G, dl);
            clauses.add(learn, model);

            // VSIDS: bump scores for literals in the learned clause, then decay all
            for (const Expr &l : disjuncts(learn))
                scores[l] += 1.0;
            for (auto it = scores.begin(); it != scores.end(); ++it)
                it.value() *= vsidsDecay;

            if (restartStrategy(conflicts, restarts, queueLbd, sumLbd)) {
                backjump(symbols, model, G);
                queueLbd.clear();
                ++restarts;
            }
        } else {
            if (symbols.isEmpty()) return model;
            ++dl;
            assignDecisionLiteral(symbols, model, scores, G, dl);
        }
    }
}
