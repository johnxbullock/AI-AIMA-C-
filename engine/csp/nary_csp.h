#pragma once

/*  N-ary Constraint Satisfaction Problems and solvers.         (Chapter 6)
 *
 *  Port of the NaryCSP / ACSolver / ACSearchSolver portions of
 *  aima-python/csp.py to C++17 / Qt 6.5.
 *
 *  An N-ary CSP generalises binary CSP: constraints can involve
 *  any number of variables (not just pairs).
 *
 *  Template parameters:
 *    Var — variable type   (typically QString)
 *    Val — value type      (e.g. int, QChar, QString)
 */

#include "../search/problem.h"
#include "../search/search.h"          // depthFirstTreeSearch
#include <QVector>
#include <QHash>
#include <QSet>
#include <QPair>
#include <functional>
#include <optional>
#include <algorithm>
#include <set>


// ============================================================================
//  NaryConstraint — scope (tuple of variables) + condition function
// ============================================================================
template<typename Var, typename Val>
struct NaryConstraint
{
    QVector<Var> scope;
    // condition is called with values in scope order
    std::function<bool(const QVector<Val> &)> condition;
    QString name;   // for debugging (optional)

    // Check whether the constraint holds under the given (possibly partial)
    // assignment.  Only evaluates if every scope variable is assigned.
    bool holds(const QHash<Var, Val> &assignment) const
    {
        QVector<Val> vals;
        vals.reserve(scope.size());
        for (const Var &v : scope) {
            auto it = assignment.find(v);
            if (it == assignment.end())
                return true;   // can't evaluate — treat as satisfied
            vals.append(it.value());
        }
        return condition(vals);
    }

    // Overload: check using set-valued domains (for GAC).
    // Evaluates with the first (only) element of each set-domain.
    bool holdsWithSingletons(const QHash<Var, QSet<Val>> &domains) const
    {
        QVector<Val> vals;
        vals.reserve(scope.size());
        for (const Var &v : scope) {
            auto it = domains.find(v);
            if (it == domains.end() || it.value().size() != 1)
                return true;
            vals.append(*it.value().begin());
        }
        return condition(vals);
    }
};


// ============================================================================
//  NaryCSP — an n-ary constraint satisfaction problem
// ============================================================================
template<typename Var, typename Val>
class NaryCSP
{
public:
    QHash<Var, QSet<Val>>                m_domains;
    QVector<NaryConstraint<Var, Val>>    m_constraints;
    QSet<Var>                            m_variables;
    QHash<Var, QSet<int>>                m_varToConstraint; // var → constraint indices

    NaryCSP(const QHash<Var, QSet<Val>> &domains,
            const QVector<NaryConstraint<Var, Val>> &constraints)
        : m_domains(domains)
        , m_constraints(constraints)
        , m_variables([&](){
                          const auto k = domains.keys();
                          QSet<Var> s;
                          s.reserve(k.size());
                          for (int i = 0; i < k.size(); ++i) s.insert(k[i]);
                          return s;
                      }())
    {
        for (int i = 0; i < m_constraints.size(); ++i)
            for (const Var &v : m_constraints[i].scope)
                m_varToConstraint[v].insert(i);
    }

    // Check whether all evaluable constraints are satisfied.
    bool consistent(const QHash<Var, Val> &assignment) const
    {
        for (const auto &con : m_constraints) {
            bool allAssigned = true;
            for (const Var &v : con.scope)
                if (!assignment.contains(v)) { allAssigned = false; break; }
            if (allAssigned && !con.holds(assignment))
                return false;
        }
        return true;
    }

    void display(const QHash<Var, Val> &assignment = {}) const
    {
        qDebug() << assignment;
    }
};


// ============================================================================
//  ACSolver — Generalized Arc Consistency + Domain Splitting
// ============================================================================
template<typename Var, typename Val>
class ACSolver
{
public:
    explicit ACSolver(NaryCSP<Var, Val> &csp) : m_csp(csp) {}

    // ---------- GAC (Generalized Arc Consistency) ---------------------------
    // Returns (consistent, reduced_domains, checks).
    struct GacResult {
        bool                     consistent;
        QHash<Var, QSet<Val>>    domains;
        int                      checks;
    };

    GacResult gac(QHash<Var, QSet<Val>> origDomains = {},
                  QSet<QPair<Var, int>> toDo = {})
    {
        if (origDomains.isEmpty())
            origDomains = m_csp.m_domains;
        if (toDo.isEmpty()) {
            for (int ci = 0; ci < m_csp.m_constraints.size(); ++ci)
                for (const Var &v : m_csp.m_constraints[ci].scope)
                    toDo.insert({v, ci});
        }
        QHash<Var, QSet<Val>> domains = origDomains;
        int checks = 0;

        while (!toDo.isEmpty()) {
            auto it = toDo.begin();
            auto [var, ci] = *it;
            toDo.erase(it);

            const auto &con = m_csp.m_constraints[ci];
            QVector<Var> otherVars;
            for (const Var &ov : con.scope)
                if (!(ov == var))
                    otherVars.append(ov);

            QSet<Val> newDomain;
            if (otherVars.isEmpty()) {
                // Unary constraint
                for (const Val &val : domains.value(var)) {
                    ++checks;
                    QHash<Var, Val> env;
                    env[var] = val;
                    if (con.holds(env))
                        newDomain.insert(val);
                }
            } else if (otherVars.size() == 1) {
                // Binary constraint
                Var other = otherVars[0];
                for (const Val &val : domains.value(var)) {
                    for (const Val &otherVal : domains.value(other)) {
                        ++checks;
                        QHash<Var, Val> env;
                        env[var] = val;
                        env[other] = otherVal;
                        if (con.holds(env)) {
                            newDomain.insert(val);
                            break;
                        }
                    }
                }
            } else {
                // General case
                for (const Val &val : domains.value(var)) {
                    QHash<Var, Val> env;
                    env[var] = val;
                    bool h;
                    std::tie(h, checks) = anyHolds(domains, con, env, otherVars, 0, checks);
                    if (h)
                        newDomain.insert(val);
                }
            }

            if (newDomain != domains.value(var)) {
                domains[var] = newDomain;
                if (newDomain.isEmpty())
                    return {false, domains, checks};
                // Add affected (var, constraint) pairs
                auto add = newToDo(var, ci);
                toDo |= add;
            }
        }
        return {true, domains, checks};
    }

    // ---------- Domain Splitting solver -------------------------------------
    // Returns a solution or std::nullopt.
    std::optional<QHash<Var, Val>>
    domainSplitting(QHash<Var, QSet<Val>> domains = {},
                    QSet<QPair<Var, int>> toDo = {})
    {
        if (domains.isEmpty())
            domains = m_csp.m_domains;
        auto [consistent, newDomains, checks] = gac(domains, toDo);
        Q_UNUSED(checks);
        if (!consistent)
            return std::nullopt;

        // Check if all domains are singletons
        bool allSingle = true;
        for (auto it = newDomains.cbegin(); it != newDomains.cend(); ++it) {
            if (it.value().size() != 1) { allSingle = false; break; }
        }
        if (allSingle) {
            QHash<Var, Val> result;
            for (auto it = newDomains.cbegin(); it != newDomains.cend(); ++it)
                result[it.key()] = *it.value().begin();
            return result;
        }

        // Find a variable with domain size > 1 and split
        Var splitVar{};
        for (const Var &v : m_csp.m_variables) {
            if (newDomains.value(v).size() > 1) {
                splitVar = v;
                break;
            }
        }

        auto [dom1, dom2] = partitionDomain(newDomains.value(splitVar));
        QHash<Var, QSet<Val>> doms1 = newDomains;
        doms1[splitVar] = dom1;
        QHash<Var, QSet<Val>> doms2 = newDomains;
        doms2[splitVar] = dom2;
        auto splitToDo = newToDo(splitVar, -1);

        auto r1 = domainSplitting(doms1, splitToDo);
        if (r1) return r1;
        return domainSplitting(doms2, splitToDo);
    }

    // ---------- Helpers (public for ACSearchSolver) -------------------------

    // Return new (var, constraint) pairs to process after changing var.
    // constraintIdx == -1 means "all constraints on var".
    QSet<QPair<Var, int>> newToDo(const Var &var, int constraintIdx) const
    {
        QSet<QPair<Var, int>> result;
        for (int ci : m_csp.m_varToConstraint.value(var)) {
            if (ci == constraintIdx) continue;
            for (const Var &nvar : m_csp.m_constraints[ci].scope)
                if (!(nvar == var))
                    result.insert({nvar, ci});
        }
        return result;
    }

private:
    NaryCSP<Var, Val> &m_csp;

    // Check if constraint holds for some extension of env over other_vars[ind:]
    QPair<bool, int> anyHolds(const QHash<Var, QSet<Val>> &domains,
                              const NaryConstraint<Var, Val> &con,
                              QHash<Var, Val> &env,
                              const QVector<Var> &otherVars,
                              int ind, int checks)
    {
        if (ind == otherVars.size())
            return {con.holds(env), checks + 1};
        Var v = otherVars[ind];
        for (const Val &val : domains.value(v)) {
            env[v] = val;
            auto [holds, c] = anyHolds(domains, con, env, otherVars, ind + 1, checks);
            checks = c;
            if (holds)
                return {true, checks};
        }
        return {false, checks};
    }

    // Split a domain into two roughly equal halves.
    static QPair<QSet<Val>, QSet<Val>> partitionDomain(const QSet<Val> &dom)
    {
        QVector<Val> elems(dom.begin(), dom.end());
        int split = elems.size() / 2;
        QSet<Val> d1(elems.begin(), elems.begin() + split);
        QSet<Val> d2(elems.begin() + split, elems.end());
        return {d1, d2};
    }
};


// ============================================================================
//  ACSearchSolver — wraps NaryCSP as a search Problem for DFS
// ============================================================================
template<typename Var, typename Val>
class ACSearchSolver
    : public Problem<QHash<Var, QSet<Val>>, QHash<Var, QSet<Val>>>
{
    using Domains = QHash<Var, QSet<Val>>;
public:
    ACSearchSolver(NaryCSP<Var, Val> &csp)
        : Problem<Domains, Domains>(Domains{})
        , m_solver(csp)
    {
        auto [consistent, domains, checks] = m_solver.gac();
        Q_UNUSED(checks);
        if (!consistent)
            throw std::runtime_error("ACSearchSolver: CSP is inconsistent");
        this->initial = domains;
    }

    bool goalTest(const Domains &node) const override
    {
        for (auto it = node.cbegin(); it != node.cend(); ++it)
            if (it.value().size() != 1)
                return false;
        return true;
    }

    QVector<Domains> actions(const Domains &state) const override
    {
        // Find a variable with domain > 1
        Var splitVar{};
        bool found = false;
        for (auto it = state.cbegin(); it != state.cend(); ++it) {
            if (it.value().size() > 1) {
                splitVar = it.key();
                found = true;
                break;
            }
        }
        QVector<Domains> neighbors;
        if (!found) return neighbors;

        auto [dom1, dom2] = partitionDomain(state.value(splitVar));
        auto toDo = m_solver.newToDo(splitVar, -1);
        for (const auto &dom : {dom1, dom2}) {
            Domains newDomains = state;
            newDomains[splitVar] = dom;
            auto [consistent, consDoms, checks] = m_solver.gac(newDomains, toDo);
            Q_UNUSED(checks);
            if (consistent)
                neighbors.append(consDoms);
        }
        return neighbors;
    }

    Domains result(const Domains &/*state*/, const Domains &action) const override
    {
        return action;
    }

    // Extract single-valued solution from a goal state.
    static QHash<Var, Val> extractSolution(const Domains &goalState)
    {
        QHash<Var, Val> sol;
        for (auto it = goalState.cbegin(); it != goalState.cend(); ++it)
            sol[it.key()] = *it.value().begin();
        return sol;
    }

private:
    mutable ACSolver<Var, Val> m_solver;

    static QPair<QSet<Val>, QSet<Val>> partitionDomain(const QSet<Val> &dom)
    {
        QVector<Val> elems(dom.begin(), dom.end());
        int split = elems.size() / 2;
        QSet<Val> d1(elems.begin(), elems.begin() + split);
        QSet<Val> d2(elems.begin() + split, elems.end());
        return {d1, d2};
    }
};


// ============================================================================
//  Convenience free functions
// ============================================================================

// Arc consistency with domain splitting interface.
template<typename Var, typename Val>
std::optional<QHash<Var, Val>> acSolver(NaryCSP<Var, Val> &csp)
{
    ACSolver<Var, Val> solver(csp);
    return solver.domainSplitting();
}

// Arc consistency with DFS search interface.
template<typename Var, typename Val>
std::optional<QHash<Var, Val>> acSearchSolver(NaryCSP<Var, Val> &csp)
{
    using Domains = QHash<Var, QSet<Val>>;
    try {
        ACSearchSolver<Var, Val> problem(csp);
        auto node = depthFirstTreeSearch(problem);
        if (node)
            return ACSearchSolver<Var, Val>::extractSolution(node->state);
    } catch (...) {
        // CSP is inconsistent
    }
    return std::nullopt;
}


// ============================================================================
//  Constraint factory helpers
// ============================================================================

// All-different constraint: true if all values are distinct.
template<typename Var, typename Val>
NaryConstraint<Var, Val> allDiffConstraint(const QVector<Var> &vars)
{
    return {vars,
            [](const QVector<Val> &vals) -> bool {
                QSet<Val> s(vals.begin(), vals.end());
                return s.size() == vals.size();
            },
            QStringLiteral("allDiff")};
}

// Sum constraint: true if sum of values equals n.
template<typename Var, typename Val>
NaryConstraint<Var, Val> sumConstraint(const QVector<Var> &vars, Val n)
{
    return {vars,
            [n](const QVector<Val> &vals) -> bool {
                Val total{};
                for (const Val &v : vals) total = total + v;
                return total == n;
            },
            QStringLiteral("sum==") + QString::number(static_cast<int>(n))};
}

// Equality constraint: true if the single variable equals val.
template<typename Var, typename Val>
NaryConstraint<Var, Val> isConstraint(const Var &var, Val target)
{
    return {{var},
            [target](const QVector<Val> &vals) -> bool {
                return vals[0] == target;
            },
            QStringLiteral("is==") + QString::number(static_cast<int>(target))};
}

// Not-equal constraint: true if the single variable does not equal val.
template<typename Var, typename Val>
NaryConstraint<Var, Val> neConstraint(const Var &var, Val target)
{
    return {{var},
            [target](const QVector<Val> &vals) -> bool {
                return !(vals[0] == target);
            },
            QStringLiteral("ne!=") + QString::number(static_cast<int>(target))};
}

// Equality between two variables.
template<typename Var, typename Val>
NaryConstraint<Var, Val> eqConstraint(const Var &v1, const Var &v2)
{
    return {{v1, v2},
            [](const QVector<Val> &vals) -> bool {
                return vals[0] == vals[1];
            },
            QStringLiteral("eq")};
}

// Adjacent constraint: |v1 - v2| == 1   (requires Val to support subtraction and abs)
template<typename Var, typename Val>
NaryConstraint<Var, Val> adjacentConstraint(const Var &v1, const Var &v2)
{
    return {{v1, v2},
            [](const QVector<Val> &vals) -> bool {
                return std::abs(vals[0] - vals[1]) == 1;
            },
            QStringLiteral("adjacent")};
}
