#pragma once

/*  CSP (Constraint Satisfaction Problems) problems and solvers.  (Chapter 6)
 *
 *  Port of aima-python/csp.py to C++17 / Qt 6.5.
 *
 *  Template parameters:
 *    Var — variable type   (e.g. QString for map colouring, int for n-Queens)
 *    Val — value type      (e.g. QChar for colours, int for row positions)
 *
 *  Both Var and Val must support: operator==, qHash.
 *  Var must also be usable as a QHash key.
 *
 *  NOTE — Qt 6 debug builds assert when non-const begin()/end() is called on
 *  an implicitly-shared (COW) container.  Every range-for in this file uses
 *  std::as_const() or a const local to guarantee const iteration.
 */

#include "../search/problem.h"
#include <QVector>
#include <QHash>
#include <QSet>
#include <QPair>
#include <QDebug>
#include <functional>
#include <optional>
#include <algorithm>
#include <random>
#include <cmath>
#include <tuple>
#include <utility>   // std::as_const

// ============================================================================
// Utility: argminRandomTie
//
// Return the element of `items` that minimises `key`, breaking ties at random.
// ============================================================================
namespace csp_detail {

inline std::mt19937 &rng()
{
    static thread_local std::mt19937 gen{std::random_device{}()};
    return gen;
}

template<typename T, typename KeyFn>
T argminRandomTie(const QVector<T> &items, KeyFn key)
{
    Q_ASSERT(!items.isEmpty());
    using K = decltype(key(items[0]));
    K bestVal = key(items[0]);
    QVector<T> best;
    best.append(items[0]);
    for (int i = 1; i < items.size(); ++i) {
        K v = key(items[i]);
        if (v < bestVal) {
            bestVal = v;
            best.clear();
            best.append(items[i]);
        } else if (v == bestVal) {
            best.append(items[i]);
        }
    }
    if (best.size() == 1)
        return best[0];
    std::uniform_int_distribution<int> dist(0, best.size() - 1);
    return best[dist(rng())];
}

} // namespace csp_detail


// ============================================================================
//  Forward declarations
// ============================================================================
template<typename Var, typename Val> class CSP;

template<typename Var, typename Val>
QPair<bool, int> ac3(CSP<Var, Val> &csp,
                     QSet<QPair<Var, Var>> queue = {},
                     QVector<QPair<Var, Val>> *removals = nullptr);


// ============================================================================
//  CSP — Finite-domain Constraint Satisfaction Problem            [Chapter 6]
//
//  Inputs:
//    variables   — list of variables
//    domains     — { var : [possible_values] }
//    neighbors   — { var : [neighboring_vars] }
//    constraints — f(A, a, B, b) → true iff constraint satisfied
//
//  Also inherits Problem<State, Action> so it can be used with generic search.
// ============================================================================
template<typename Var, typename Val>
class CSP : public Problem<QHash<Var, Val>, QPair<Var, Val>> {
public:
    using State        = QHash<Var, Val>;
    using Action       = QPair<Var, Val>;
    using Assignment   = QHash<Var, Val>;
    using Removal      = QPair<Var, Val>;
    using Removals     = QVector<Removal>;
    using ConstraintFn = std::function<bool(const Var &, const Val &,
                                            const Var &, const Val &)>;

    // -- Public data (algorithms need direct access, matching Python) --------
    QVector<Var>              m_variables;
    QHash<Var, QVector<Val>>  m_domains;
    QHash<Var, QVector<Var>>  m_neighbors;
    ConstraintFn              m_constraints;
    int                       m_nassigns = 0;

    // -- Constructor ---------------------------------------------------------
    CSP(const QVector<Var> &variables,
        const QHash<Var, QVector<Val>> &domains,
        const QHash<Var, QVector<Var>> &neighbors,
        ConstraintFn constraints)
        : Problem<State, Action>(State{})
        , m_variables(variables.isEmpty()
                          ? [&](){
                                const auto k = domains.keys();
                                QVector<Var> v;
                                v.reserve(k.size());
                                for (int i = 0; i < k.size(); ++i) v.append(k[i]);
                                return v;
                            }()
                          : variables)
        , m_domains(domains)
        , m_neighbors(neighbors)
        , m_constraints(std::move(constraints))
    {}

    virtual ~CSP() = default;

    // -- Assignment bookkeeping ----------------------------------------------

    virtual void assign(const Var &var, const Val &val, Assignment &assignment)
    {
        assignment[var] = val;
        ++m_nassigns;
    }

    virtual void unassign(const Var &var, Assignment &assignment)
    {
        assignment.remove(var);
    }

    // Return the number of conflicts var=val has with other assigned variables
    virtual int nconflicts(const Var &var, const Val &val,
                           const Assignment &assignment) const
    {
        int c = 0;
        const QVector<Var> nb = m_neighbors.value(var);   // const local
        for (int i = 0; i < nb.size(); ++i) {
            auto it = assignment.find(nb[i]);
            if (it != assignment.end() && !m_constraints(var, val, nb[i], it.value()))
                ++c;
        }
        return c;
    }

    virtual void display(const Assignment &assignment) const
    {
        qDebug() << assignment;
    }

    // -- Problem interface (for tree / graph search) -------------------------

    QVector<Action> actions(const State &state) const override
    {
        if (state.size() == m_variables.size())
            return {};
        // First unassigned variable
        Var var{};
        for (int i = 0; i < m_variables.size(); ++i)
            if (!state.contains(m_variables[i])) { var = m_variables[i]; break; }
        QVector<Action> acts;
        const QVector<Val> dom = m_domains.value(var);
        for (int i = 0; i < dom.size(); ++i)
            if (nconflicts(var, dom[i], state) == 0)
                acts.append(Action(var, dom[i]));
        return acts;
    }

    State result(const State &state, const Action &action) const override
    {
        State next = state;
        next[action.first] = action.second;
        return next;
    }

    bool goalTest(const State &state) const override
    {
        if (state.size() != m_variables.size())
            return false;
        for (int i = 0; i < m_variables.size(); ++i)
            if (nconflicts(m_variables[i], state.value(m_variables[i]), state) != 0)
                return false;
        return true;
    }

    // -- Constraint propagation support --------------------------------------

    void supportPruning()
    {
        if (!m_hasCurrDomains) {
            m_currDomains.clear();
            for (int i = 0; i < m_variables.size(); ++i) {
                const Var &v = m_variables[i];
                // Build an independent copy (avoid Qt COW sharing with m_domains)
                const QVector<Val> src = m_domains.value(v);
                QVector<Val> copy;
                copy.reserve(src.size());
                for (int j = 0; j < src.size(); ++j)
                    copy.append(src[j]);
                m_currDomains[v] = std::move(copy);
            }
            m_hasCurrDomains = true;
        }
    }

    // Start accumulating inferences from assuming var=value.
    // Returns the removals list (all other values of var that were pruned).
    Removals suppose(const Var &var, const Val &value)
    {
        supportPruning();
        Removals removals;
        const QVector<Val> snap = m_currDomains.value(var);
        for (int i = 0; i < snap.size(); ++i)
            if (!(snap[i] == value))
                removals.append(Removal(var, snap[i]));
        QVector<Val> single;
        single.reserve(1);
        single.append(value);
        m_currDomains[var] = std::move(single);
        return removals;
    }

    // Rule out var=value.  If removals is non-null, record the removal.
    void prune(const Var &var, const Val &value, Removals *removals)
    {
        // Build a new domain without the pruned value (avoids in-place COW issues)
        const QVector<Val> old = m_currDomains.value(var);
        QVector<Val> updated;
        updated.reserve(old.size());
        for (int i = 0; i < old.size(); ++i)
            if (!(old[i] == value))
                updated.append(old[i]);
        m_currDomains[var] = std::move(updated);
        if (removals)
            removals->append(Removal(var, value));
    }

    // Return all values for var that aren't currently ruled out.
    QVector<Val> choices(const Var &var) const
    {
        return m_hasCurrDomains ? m_currDomains.value(var) : m_domains.value(var);
    }

    // Return the partial assignment implied by current inferences
    // (variables whose domain has been reduced to a single value).
    Assignment inferAssignment()
    {
        supportPruning();
        Assignment a;
        for (int i = 0; i < m_variables.size(); ++i) {
            const QVector<Val> dom = m_currDomains.value(m_variables[i]);
            if (dom.size() == 1)
                a[m_variables[i]] = dom[0];
        }
        return a;
    }

    // Undo a supposition and all inferences from it.
    void restore(const Removals &removals)
    {
        for (int i = 0; i < removals.size(); ++i) {
            const Var &var = removals[i].first;
            const Val &val = removals[i].second;
            // Rebuild to avoid COW mutation issues
            QVector<Val> dom = m_currDomains.value(var);
            QVector<Val> updated;
            updated.reserve(dom.size() + 1);
            for (int j = 0; j < dom.size(); ++j)
                updated.append(dom[j]);
            updated.append(val);
            m_currDomains[var] = std::move(updated);
        }
    }

    // Return variables in current assignment that are in conflict.
    QVector<Var> conflictedVars(const Assignment &current) const
    {
        QVector<Var> result;
        for (int i = 0; i < m_variables.size(); ++i)
            if (nconflicts(m_variables[i], current.value(m_variables[i]), current) > 0)
                result.append(m_variables[i]);
        return result;
    }

    // Direct access to curr_domains (for AC algorithms)
    bool hasCurrDomains() const { return m_hasCurrDomains; }
    QHash<Var, QVector<Val>> &currDomains() { return m_currDomains; }
    const QHash<Var, QVector<Val>> &currDomains() const { return m_currDomains; }

private:
    bool                       m_hasCurrDomains = false;
    QHash<Var, QVector<Val>>   m_currDomains;
};


// ============================================================================
//  Constraint Propagation with AC-3                         [Figure 6.3]
// ============================================================================

namespace csp_detail {

// Return true if we removed a value from Xi's domain.
template<typename Var, typename Val>
QPair<bool, int> revise(CSP<Var, Val> &csp, const Var &Xi, const Var &Xj,
                        QVector<QPair<Var, Val>> *removals, int checks = 0)
{
    bool revised = false;
    const QVector<Val> domXi = csp.currDomains().value(Xi);
    for (int i = 0; i < domXi.size(); ++i) {
        const Val &x = domXi[i];
        bool conflict = true;
        const QVector<Val> domXj = csp.currDomains().value(Xj);
        for (int j = 0; j < domXj.size(); ++j) {
            ++checks;
            if (csp.m_constraints(Xi, x, Xj, domXj[j])) {
                conflict = false;
                break;
            }
        }
        if (conflict) {
            csp.prune(Xi, x, removals);
            revised = true;
        }
    }
    return {revised, checks};
}

} // namespace csp_detail


// AC-3: returns (consistent, checks).
// If queue is empty, initialises with all arcs.
template<typename Var, typename Val>
QPair<bool, int> ac3(CSP<Var, Val> &csp,
                     QSet<QPair<Var, Var>> queue,
                     QVector<QPair<Var, Val>> *removals)
{
    if (queue.isEmpty()) {
        for (int i = 0; i < csp.m_variables.size(); ++i) {
            const Var &Xi = csp.m_variables[i];
            const QVector<Var> nb = csp.m_neighbors.value(Xi);
            for (int j = 0; j < nb.size(); ++j)
                queue.insert(QPair<Var,Var>(Xi, nb[j]));
        }
    }

    csp.supportPruning();
    int checks = 0;
    while (!queue.isEmpty()) {
        auto it = queue.begin();
        QPair<Var,Var> arc = *it;
        queue.erase(it);
        Var Xi = arc.first;
        Var Xj = arc.second;

        auto rv = csp_detail::revise(csp, Xi, Xj, removals, checks);
        bool revised = rv.first;
        checks = rv.second;
        if (revised) {
            if (csp.currDomains().value(Xi).isEmpty())
                return {false, checks};
            const QVector<Var> nb = csp.m_neighbors.value(Xi);
            for (int k = 0; k < nb.size(); ++k)
                if (!(nb[k] == Xj))
                    queue.insert(QPair<Var,Var>(nb[k], Xi));
        }
    }
    return {true, checks};
}


// ============================================================================
//  Constraint Propagation with AC-3b  (double-support improvement)
// ============================================================================

namespace csp_detail {

template<typename Var, typename Val>
std::tuple<QSet<Val>, QSet<Val>, QSet<Val>, int>
partition(CSP<Var, Val> &csp, const Var &Xi, const Var &Xj, int checks = 0)
{
    QSet<Val> Si_p, Sj_p;
    const QVector<Val> domXj = csp.currDomains().value(Xj);
    QSet<Val> Sj_u(domXj.begin(), domXj.end());

    const QVector<Val> domXi = csp.currDomains().value(Xi);
    for (int i = 0; i < domXi.size(); ++i) {
        const Val &vi_u = domXi[i];
        bool conflict = true;
        QSet<Val> tryFirst = Sj_u - Sj_p;
        for (auto jt = tryFirst.constBegin(); jt != tryFirst.constEnd(); ++jt) {
            ++checks;
            if (csp.m_constraints(Xi, vi_u, Xj, *jt)) {
                conflict = false;
                Si_p.insert(vi_u);
                Sj_p.insert(*jt);
                break;
            }
        }
        if (conflict) {
            for (auto jt = Sj_p.constBegin(); jt != Sj_p.constEnd(); ++jt) {
                ++checks;
                if (csp.m_constraints(Xi, vi_u, Xj, *jt)) {
                    conflict = false;
                    Si_p.insert(vi_u);
                    break;
                }
            }
        }
    }
    return {Si_p, Sj_p, Sj_u - Sj_p, checks};
}

} // namespace csp_detail


template<typename Var, typename Val>
QPair<bool, int> ac3b(CSP<Var, Val> &csp,
                      QSet<QPair<Var, Var>> queue = {},
                      QVector<QPair<Var, Val>> *removals = nullptr)
{
    if (queue.isEmpty()) {
        for (int i = 0; i < csp.m_variables.size(); ++i) {
            const Var &Xi = csp.m_variables[i];
            const QVector<Var> nb = csp.m_neighbors.value(Xi);
            for (int j = 0; j < nb.size(); ++j)
                queue.insert(QPair<Var,Var>(Xi, nb[j]));
        }
    }

    csp.supportPruning();
    int checks = 0;
    while (!queue.isEmpty()) {
        auto it = queue.begin();
        QPair<Var,Var> arc = *it;
        queue.erase(it);
        Var Xi = arc.first;
        Var Xj = arc.second;

        auto [Si_p, Sj_p, Sj_u, c] =
            csp_detail::partition(csp, Xi, Xj, checks);
        checks = c;

        if (Si_p.isEmpty())
            return {false, checks};

        // Prune Xi values not in Si_p
        bool revised = false;
        const QVector<Val> domXi = csp.currDomains().value(Xi);
        QSet<Val> domXiSet(domXi.begin(), domXi.end());
        QSet<Val> toPrune = domXiSet - Si_p;
        for (auto pt = toPrune.constBegin(); pt != toPrune.constEnd(); ++pt) {
            csp.prune(Xi, *pt, removals);
            revised = true;
        }
        if (revised) {
            const QVector<Var> nb = csp.m_neighbors.value(Xi);
            for (int k = 0; k < nb.size(); ++k)
                if (!(nb[k] == Xj))
                    queue.insert(QPair<Var,Var>(nb[k], Xi));
        }

        // Process reverse arc (Xj, Xi) if it's in the queue
        if (queue.contains(QPair<Var,Var>(Xj, Xi))) {
            queue.remove(QPair<Var,Var>(Xj, Xi));
            for (auto ut = Sj_u.constBegin(); ut != Sj_u.constEnd(); ++ut) {
                for (auto st = Si_p.constBegin(); st != Si_p.constEnd(); ++st) {
                    ++checks;
                    if (csp.m_constraints(Xj, *ut, Xi, *st)) {
                        Sj_p.insert(*ut);
                        break;
                    }
                }
            }
            revised = false;
            const QVector<Val> domXj = csp.currDomains().value(Xj);
            QSet<Val> domXjSet(domXj.begin(), domXj.end());
            QSet<Val> toPruneJ = domXjSet - Sj_p;
            for (auto pt = toPruneJ.constBegin(); pt != toPruneJ.constEnd(); ++pt) {
                csp.prune(Xj, *pt, removals);
                revised = true;
            }
            if (revised) {
                const QVector<Var> nb = csp.m_neighbors.value(Xj);
                for (int k = 0; k < nb.size(); ++k)
                    if (!(nb[k] == Xi))
                        queue.insert(QPair<Var,Var>(nb[k], Xj));
            }
        }
    }
    return {true, checks};
}


// ============================================================================
//  Constraint Propagation with AC-4  (fine-grained arc consistency)
// ============================================================================

template<typename Var, typename Val>
QPair<bool, int> ac4(CSP<Var, Val> &csp,
                     QSet<QPair<Var, Var>> queue = {},
                     QVector<QPair<Var, Val>> *removals = nullptr)
{
    if (queue.isEmpty()) {
        for (int i = 0; i < csp.m_variables.size(); ++i) {
            const Var &Xi = csp.m_variables[i];
            const QVector<Var> nb = csp.m_neighbors.value(Xi);
            for (int j = 0; j < nb.size(); ++j)
                queue.insert(QPair<Var,Var>(Xi, nb[j]));
        }
    }

    csp.supportPruning();

    using SupportKey = std::tuple<Var, Val, Var>;
    QHash<SupportKey, int> supportCounter;
    QHash<QPair<Var, Val>, QSet<QPair<Var, Val>>> supported;
    QVector<QPair<Var, Val>> unsupported;
    int checks = 0;

    while (!queue.isEmpty()) {
        auto it = queue.begin();
        QPair<Var,Var> arc = *it;
        queue.erase(it);
        Var Xi = arc.first;
        Var Xj = arc.second;

        bool revised = false;
        const QVector<Val> domXi = csp.currDomains().value(Xi);
        for (int i = 0; i < domXi.size(); ++i) {
            const Val &x = domXi[i];
            const QVector<Val> domXj = csp.currDomains().value(Xj);
            for (int j = 0; j < domXj.size(); ++j) {
                ++checks;
                if (csp.m_constraints(Xi, x, Xj, domXj[j])) {
                    supportCounter[SupportKey(Xi, x, Xj)] += 1;
                    supported[QPair<Var,Val>(Xj, domXj[j])].insert(QPair<Var,Val>(Xi, x));
                }
            }
            if (supportCounter.value(SupportKey(Xi, x, Xj), 0) == 0) {
                csp.prune(Xi, x, removals);
                revised = true;
                unsupported.append(QPair<Var,Val>(Xi, x));
            }
        }
        if (revised && csp.currDomains().value(Xi).isEmpty())
            return {false, checks};
    }

    while (!unsupported.isEmpty()) {
        QPair<Var,Val> pair = unsupported.takeLast();
        Var Xj = pair.first;
        Val y  = pair.second;
        const QSet<QPair<Var,Val>> sup = supported.value(QPair<Var,Val>(Xj, y));
        QVector<QPair<Var,Val>> supList(sup.begin(), sup.end());
        for (int s = 0; s < supList.size(); ++s) {
            Var Xi = supList[s].first;
            Val x  = supList[s].second;
            if (csp.currDomains().value(Xi).contains(x)) {
                SupportKey key(Xi, x, Xj);
                supportCounter[key] -= 1;
                if (supportCounter.value(key, 0) == 0) {
                    csp.prune(Xi, x, removals);
                    unsupported.append(QPair<Var,Val>(Xi, x));
                    if (csp.currDomains().value(Xi).isEmpty())
                        return {false, checks};
                }
            }
        }
    }
    return {true, checks};
}


// ============================================================================
//  CSP Backtracking Search                                  [Figure 6.5]
// ============================================================================

// -- Variable ordering -------------------------------------------------------

template<typename Var, typename Val>
Var firstUnassignedVariable(const QHash<Var, Val> &assignment,
                            const CSP<Var, Val> &csp)
{
    for (int i = 0; i < csp.m_variables.size(); ++i)
        if (!assignment.contains(csp.m_variables[i]))
            return csp.m_variables[i];
    Q_UNREACHABLE();
}

template<typename Var, typename Val>
int numLegalValues(const CSP<Var, Val> &csp, const Var &var,
                   const QHash<Var, Val> &assignment)
{
    if (csp.hasCurrDomains())
        return csp.currDomains().value(var).size();
    int c = 0;
    const QVector<Val> dom = csp.m_domains.value(var);
    for (int i = 0; i < dom.size(); ++i)
        if (csp.nconflicts(var, dom[i], assignment) == 0)
            ++c;
    return c;
}

template<typename Var, typename Val>
Var mrv(const QHash<Var, Val> &assignment, const CSP<Var, Val> &csp)
{
    QVector<Var> unassigned;
    for (int i = 0; i < csp.m_variables.size(); ++i)
        if (!assignment.contains(csp.m_variables[i]))
            unassigned.append(csp.m_variables[i]);
    return csp_detail::argminRandomTie(unassigned,
        [&](const Var &var) { return numLegalValues(csp, var, assignment); });
}

// -- Value ordering ----------------------------------------------------------

template<typename Var, typename Val>
QVector<Val> unorderedDomainValues(const Var &var,
                                   const QHash<Var, Val> &/*assignment*/,
                                   const CSP<Var, Val> &csp)
{
    return csp.choices(var);
}

template<typename Var, typename Val>
QVector<Val> lcv(const Var &var,
                 const QHash<Var, Val> &assignment,
                 const CSP<Var, Val> &csp)
{
    QVector<Val> vals = csp.choices(var);
    std::sort(vals.begin(), vals.end(),
              [&](const Val &a, const Val &b) {
                  return csp.nconflicts(var, a, assignment)
                       < csp.nconflicts(var, b, assignment);
              });
    return vals;
}

// -- Inference ---------------------------------------------------------------

template<typename Var, typename Val>
bool noInference(CSP<Var, Val> &/*csp*/, const Var &/*var*/, const Val &/*value*/,
                 const QHash<Var, Val> &/*assignment*/,
                 QVector<QPair<Var, Val>> &/*removals*/)
{
    return true;
}

template<typename Var, typename Val>
bool forwardChecking(CSP<Var, Val> &csp, const Var &var, const Val &value,
                     const QHash<Var, Val> &assignment,
                     QVector<QPair<Var, Val>> &removals)
{
    csp.supportPruning();
    const QVector<Var> nb = csp.m_neighbors.value(var);
    for (int i = 0; i < nb.size(); ++i) {
        const Var &B = nb[i];
        if (!assignment.contains(B)) {
            const QVector<Val> domB = csp.currDomains().value(B);
            for (int j = 0; j < domB.size(); ++j)
                if (!csp.m_constraints(var, value, B, domB[j]))
                    csp.prune(B, domB[j], &removals);
            if (csp.currDomains().value(B).isEmpty())
                return false;
        }
    }
    return true;
}

// Maintain arc consistency (MAC): run AC-3 on arcs affected by var's assignment.
// Fixes Python bug where mac() returned (bool, checks) tuple (always truthy).
template<typename Var, typename Val>
bool mac(CSP<Var, Val> &csp, const Var &var, const Val &/*value*/,
         const QHash<Var, Val> &/*assignment*/,
         QVector<QPair<Var, Val>> &removals)
{
    QSet<QPair<Var, Var>> arcs;
    const QVector<Var> nb = csp.m_neighbors.value(var);
    for (int i = 0; i < nb.size(); ++i)
        arcs.insert(QPair<Var,Var>(nb[i], var));
    auto result = ac3(csp, arcs, &removals);
    return result.first;
}

// -- The search, proper ------------------------------------------------------

template<typename Var, typename Val>
using SelectVarFn = std::function<Var(const QHash<Var, Val> &, const CSP<Var, Val> &)>;

template<typename Var, typename Val>
using OrderValsFn = std::function<QVector<Val>(const Var &, const QHash<Var, Val> &, const CSP<Var, Val> &)>;

template<typename Var, typename Val>
using InferenceFn = std::function<bool(CSP<Var, Val> &, const Var &, const Val &,
                                       const QHash<Var, Val> &,
                                       QVector<QPair<Var, Val>> &)>;


namespace csp_detail {

template<typename Var, typename Val>
std::optional<QHash<Var, Val>>
backtrack(CSP<Var, Val> &csp,
          QHash<Var, Val> &assignment,
          const SelectVarFn<Var, Val> &selectVar,
          const OrderValsFn<Var, Val> &orderVals,
          const InferenceFn<Var, Val> &inference)
{
    if (assignment.size() == csp.m_variables.size())
        return assignment;

    Var var = selectVar(assignment, csp);
    const QVector<Val> values = orderVals(var, assignment, csp);  // const local
    for (int i = 0; i < values.size(); ++i) {
        const Val &value = values[i];
        if (csp.nconflicts(var, value, assignment) == 0) {
            csp.assign(var, value, assignment);
            auto removals = csp.suppose(var, value);
            if (inference(csp, var, value, assignment, removals)) {
                auto result = backtrack(csp, assignment, selectVar, orderVals, inference);
                if (result)
                    return result;
            }
            csp.restore(removals);
        }
    }
    csp.unassign(var, assignment);
    return std::nullopt;
}

} // namespace csp_detail


template<typename Var, typename Val>
std::optional<QHash<Var, Val>>
backtrackingSearch(CSP<Var, Val> &csp,
                   SelectVarFn<Var, Val> selectVar = firstUnassignedVariable<Var, Val>,
                   OrderValsFn<Var, Val> orderVals = unorderedDomainValues<Var, Val>,
                   InferenceFn<Var, Val> inference  = noInference<Var, Val>)
{
    QHash<Var, Val> assignment;
    auto result = csp_detail::backtrack(csp, assignment, selectVar, orderVals, inference);
    Q_ASSERT(!result || csp.goalTest(*result));
    return result;
}


// ============================================================================
//  Min-conflicts Hill Climbing search for CSPs              [Section 6.4]
// ============================================================================

template<typename Var, typename Val>
Val minConflictsValue(const CSP<Var, Val> &csp, const Var &var,
                      const QHash<Var, Val> &current)
{
    return csp_detail::argminRandomTie(csp.m_domains.value(var),
        [&](const Val &val) { return csp.nconflicts(var, val, current); });
}

template<typename Var, typename Val>
std::optional<QHash<Var, Val>>
minConflicts(CSP<Var, Val> &csp, int maxSteps = 100000)
{
    QHash<Var, Val> current;
    for (int i = 0; i < csp.m_variables.size(); ++i) {
        Val val = minConflictsValue(csp, csp.m_variables[i], current);
        csp.assign(csp.m_variables[i], val, current);
    }
    for (int step = 0; step < maxSteps; ++step) {
        QVector<Var> conflicted = csp.conflictedVars(current);
        if (conflicted.isEmpty())
            return current;
        std::uniform_int_distribution<int> dist(0, conflicted.size() - 1);
        Var var = conflicted[dist(csp_detail::rng())];
        Val val = minConflictsValue(csp, var, current);
        csp.assign(var, val, current);
    }
    return std::nullopt;
}


// ============================================================================
//  Tree CSP Solver                                          [Figure 6.11]
// ============================================================================

namespace csp_detail {

template<typename Var>
void buildTopological(const Var &node,
                      const std::optional<Var> &parent,
                      const QHash<Var, QVector<Var>> &neighbors,
                      QHash<Var, bool> &visited,
                      QVector<Var> &stack,
                      QHash<Var, Var> &parents)
{
    visited[node] = true;
    const QVector<Var> nb = neighbors.value(node);
    for (int i = 0; i < nb.size(); ++i)
        if (!visited.value(nb[i], false))
            buildTopological(nb[i], node, neighbors, visited, stack, parents);
    if (parent)
        parents[node] = *parent;
    stack.prepend(node);
}

template<typename Var>
QPair<QVector<Var>, QHash<Var, Var>>
topologicalSort(const QHash<Var, QVector<Var>> &neighbors,
                const Var &root)
{
    QHash<Var, bool> visited;
    QVector<Var> stack;
    QHash<Var, Var> parents;
    buildTopological(root, std::nullopt, neighbors, visited, stack, parents);
    return {stack, parents};
}

template<typename Var, typename Val>
bool makeArcConsistent(const Var &Xj, const Var &Xk, CSP<Var, Val> &csp)
{
    const QVector<Val> domXj = csp.m_domains.value(Xj);
    for (int i = 0; i < domXj.size(); ++i) {
        bool keep = false;
        const QVector<Val> domXk = csp.m_domains.value(Xk);
        for (int j = 0; j < domXk.size(); ++j) {
            if (csp.m_constraints(Xj, domXj[i], Xk, domXk[j])) {
                keep = true;
                break;
            }
        }
        if (!keep)
            csp.prune(Xj, domXj[i], nullptr);
    }
    return !csp.currDomains().value(Xj).isEmpty();
}

template<typename Var, typename Val>
std::optional<Val> assignValue(const Var &Xj, const Var &Xk,
                               CSP<Var, Val> &csp,
                               const QHash<Var, Val> &assignment)
{
    Val parentVal = assignment.value(Xj);
    const QVector<Val> dom = csp.currDomains().value(Xk);
    for (int i = 0; i < dom.size(); ++i)
        if (csp.m_constraints(Xj, parentVal, Xk, dom[i]))
            return dom[i];
    return std::nullopt;
}

} // namespace csp_detail


template<typename Var, typename Val>
std::optional<QHash<Var, Val>> treeCspSolver(CSP<Var, Val> &csp)
{
    QHash<Var, Val> assignment;
    Var root = csp.m_variables[0];
    auto topo = csp_detail::topologicalSort(csp.m_neighbors, root);
    QVector<Var> X = topo.first;
    QHash<Var, Var> parent = topo.second;

    csp.supportPruning();

    for (int i = X.size() - 1; i >= 1; --i) {
        const Var &Xj = X[i];
        if (!csp_detail::makeArcConsistent(parent.value(Xj), Xj, csp))
            return std::nullopt;
    }

    assignment[root] = csp.currDomains().value(root).first();
    for (int i = 1; i < X.size(); ++i) {
        const Var &Xi = X[i];
        auto val = csp_detail::assignValue(parent.value(Xi), Xi, csp, assignment);
        if (!val)
            return std::nullopt;
        assignment[Xi] = *val;
    }
    return assignment;
}
