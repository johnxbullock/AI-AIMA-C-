#pragma once

/*  Example CSP problems from AIMA Chapter 6.
 *
 *  - Map Coloring:  CSP<QString, QChar>   (regions with colour constraints)
 *  - N-Queens:      CSP<int, int>         (columns × rows)
 *
 *  All functions are inline / template so this remains header-only.
 */

#include "csp.h"
#include <QString>
#include <QStringList>
#include <QChar>
#include <QHash>
#include <QVector>
#include <cmath>


// ============================================================================
//  Map Coloring CSP Problems
// ============================================================================

// A constraint saying two neighboring variables must differ in value.
inline bool differentValuesConstraint(const QString &/*A*/, const QChar &a,
                                       const QString &/*B*/, const QChar &b)
{
    return a != b;
}

// Parse a string of the form "X: Y Z; Y: Z" into an adjacency map.
// If you say "X: Y", you don't need "Y: X" — both directions are added.
inline QHash<QString, QVector<QString>> parseNeighbors(const QString &neighbors)
{
    QHash<QString, QVector<QString>> dic;
    const QStringList specs = neighbors.split(QLatin1Char(';'));
    for (const QString &spec : specs) {
        int colonIdx = spec.indexOf(QLatin1Char(':'));
        if (colonIdx < 0) continue;
        QString A = spec.left(colonIdx).trimmed();
        QString rest = spec.mid(colonIdx + 1).trimmed();
        // Ensure A exists in the map even if it has no neighbors
        if (!dic.contains(A))
            dic[A] = {};
        const QStringList bList = rest.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        for (const QString &B : bList) {
            if (!dic[A].contains(B))
                dic[A].append(B);
            if (!dic.contains(B))
                dic[B] = {};
            if (!dic[B].contains(A))
                dic[B].append(A);
        }
    }
    return dic;
}

// Factory: make a CSP for map coloring with the given colours and adjacency.
// `neighborStr` uses the parse_neighbors format: "SA: WA NT Q; NT: WA Q; ..."
inline CSP<QString, QChar>
mapColoringCsp(const QVector<QChar> &colors, const QString &neighborStr)
{
    auto neighbors = parseNeighbors(neighborStr);
    const auto keys = neighbors.keys();
    QVector<QString> variables;
    variables.reserve(keys.size());
    for (int i = 0; i < keys.size(); ++i)
        variables.append(keys[i]);

    // Every variable has the same domain (all colours)
    QHash<QString, QVector<QChar>> domains;
    for (const QString &v : variables)
        domains[v] = colors;

    return CSP<QString, QChar>(variables, domains, neighbors,
                               differentValuesConstraint);
}

// ---- Pre-built instances ---------------------------------------------------

inline CSP<QString, QChar> australiaCsp()
{
    QVector<QChar> colors;
    colors.append(QChar('R'));
    colors.append(QChar('G'));
    colors.append(QChar('B'));
    return mapColoringCsp(colors,
        QStringLiteral("SA: WA NT Q NSW V; NT: WA Q; NSW: Q V; T: "));
}

inline CSP<QString, QChar> usaCsp()
{
    QVector<QChar> colors;
    colors.append(QChar('R'));
    colors.append(QChar('G'));
    colors.append(QChar('B'));
    colors.append(QChar('Y'));
    return mapColoringCsp(colors,
        QStringLiteral(
        "WA: OR ID; OR: ID NV CA; CA: NV AZ; NV: ID UT AZ; ID: MT WY UT;"
        "UT: WY CO AZ; MT: ND SD WY; WY: SD NE CO; CO: NE KA OK NM; NM: OK TX AZ;"
        "ND: MN SD; SD: MN IA NE; NE: IA MO KA; KA: MO OK; OK: MO AR TX;"
        "TX: AR LA; MN: WI IA; IA: WI IL MO; MO: IL KY TN AR; AR: MS TN LA;"
        "LA: MS; WI: MI IL; IL: IN KY; IN: OH KY; MS: TN AL; AL: TN GA FL;"
        "MI: OH IN; OH: PA WV KY; KY: WV VA TN; TN: VA NC GA; GA: NC SC FL;"
        "PA: NY NJ DE MD WV; WV: MD VA; VA: MD DC NC; NC: SC; NY: VT MA CT NJ;"
        "NJ: DE; DE: MD; MD: DC; VT: NH MA; MA: NH RI CT; CT: RI; ME: NH;"
        "HI: ; AK: "));
}

inline CSP<QString, QChar> franceCsp()
{
    QVector<QChar> colors;
    colors.append(QChar('R'));
    colors.append(QChar('G'));
    colors.append(QChar('B'));
    colors.append(QChar('Y'));
    return mapColoringCsp(colors,
        QStringLiteral(
        "AL: LO FC; AQ: MP LI PC; AU: LI CE BO RA LR MP; BO: CE IF CA FC RA "
        "AU; BR: NB PL; CA: IF PI LO FC BO; CE: PL NB NH IF BO AU LI PC; FC: BO "
        "CA LO AL RA; IF: NH PI CA BO CE; LI: PC CE AU MP AQ; LO: CA AL FC; LR: "
        "MP AU RA PA; MP: AQ LI AU LR; NB: NH CE PL BR; NH: PI IF CE NB; NO: "
        "PI; PA: LR RA; PC: PL CE LI AQ; PI: NH NO CA IF; PL: BR NB CE PC; RA: "
        "AU BO FC PA LR"));
}


// ============================================================================
//  N-Queens Problem
// ============================================================================

// Queen constraint: satisfied if A,B are the same variable,
// or they are not in the same row, down diagonal, or up diagonal.
inline bool queensConstraint(const int &A, const int &a,
                              const int &B, const int &b)
{
    return A == B || (a != b && A + a != B + b && A - a != B - b);
}


// NQueensCSP — CSP for n-Queens with O(1) conflict tracking.
//
// Think of placing queens one per column, from left to right.
// Position (x, y) represents (var, val) in the CSP.
//
// The main structures count queens that could conflict:
//   rows[i]   — number of queens in row i
//   downs[i]  — number of queens on the \ diagonal where x+y == i
//   ups[i]    — number of queens on the / diagonal where x-y+n-1 == i
class NQueensCSP : public CSP<int, int>
{
public:
    explicit NQueensCSP(int n)
        : CSP<int, int>(makeVars(n), makeDomains(n), makeNeighbors(n),
                        queensConstraint)
        , m_n(n)
        , m_rows(n, 0)
        , m_ups(2 * n - 1, 0)
        , m_downs(2 * n - 1, 0)
    {}

    int nconflicts(const int &var, const int &val,
                   const Assignment &assignment) const override
    {
        int c = m_rows[val] + m_downs[var + val] + m_ups[var - val + m_n - 1];
        auto it = assignment.find(var);
        if (it != assignment.end() && it.value() == val)
            c -= 3;   // don't count self
        return c;
    }

    void assign(const int &var, const int &val, Assignment &assignment) override
    {
        auto it = assignment.find(var);
        int oldVal = (it != assignment.end()) ? it.value() : -1;
        if (val != oldVal) {
            if (oldVal >= 0)
                recordConflict(var, oldVal, -1);
            recordConflict(var, val, +1);
            CSP<int, int>::assign(var, val, assignment);
        }
    }

    void unassign(const int &var, Assignment &assignment) override
    {
        auto it = assignment.find(var);
        if (it != assignment.end())
            recordConflict(var, it.value(), -1);
        CSP<int, int>::unassign(var, assignment);
    }

    void display(const Assignment &assignment) const override
    {
        for (int row = 0; row < m_n; ++row) {
            QString line;
            for (int col = 0; col < m_n; ++col) {
                auto it = assignment.find(col);
                if (it != assignment.end() && it.value() == row)
                    line += QStringLiteral("Q ");
                else if ((col + row) % 2 == 0)
                    line += QStringLiteral(". ");
                else
                    line += QStringLiteral("- ");
            }
            qDebug().noquote() << line;
        }
    }

private:
    int m_n;
    QVector<int> m_rows;
    QVector<int> m_ups;
    QVector<int> m_downs;

    void recordConflict(int var, int val, int delta)
    {
        m_rows[val]                 += delta;
        m_downs[var + val]          += delta;
        m_ups[var - val + m_n - 1]  += delta;
    }

    // -- Static helpers to build constructor arguments ------------------------
    static QVector<int> makeVars(int n)
    {
        QVector<int> v(n);
        for (int i = 0; i < n; ++i) v[i] = i;
        return v;
    }

    static QHash<int, QVector<int>> makeDomains(int n)
    {
        QVector<int> rows(n);
        for (int i = 0; i < n; ++i) rows[i] = i;
        QHash<int, QVector<int>> d;
        for (int i = 0; i < n; ++i) d[i] = rows;
        return d;
    }

    static QHash<int, QVector<int>> makeNeighbors(int n)
    {
        QVector<int> all(n);
        for (int i = 0; i < n; ++i) all[i] = i;
        QHash<int, QVector<int>> nb;
        for (int i = 0; i < n; ++i) nb[i] = all;  // every column neighbours every column
        return nb;
    }
};
