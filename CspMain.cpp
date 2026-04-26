#include <QCoreApplication>
#include <QDebug>
#include "engine/csp/csp.h"
#include "engine/csp/csp_problems.h"

// ---------------------------------------------------------------------------
static void printSection(const QString &title)
{
    qInfo().noquote() << "\n========== " + title + " ==========";
}

// ---------------------------------------------------------------------------
// Helper: print a map-coloring assignment
// ---------------------------------------------------------------------------
static void printMapAssignment(const QHash<QString, QChar> &a)
{
    const auto k = a.keys();
    QStringList keys;
    keys.reserve(k.size());
    for (int i = 0; i < k.size(); ++i)
        keys.append(k[i]);
    keys.sort();
    for (const QString &k : keys)
        qInfo().noquote() << QString("  %1 = %2").arg(k, QString(a.value(k)));
}

// ---------------------------------------------------------------------------
// Helper: print an n-queens board
// ---------------------------------------------------------------------------
static void printBoard(int n, const QHash<int, int> &a)
{
    for (int row = 0; row < n; ++row) {
        QString line;
        for (int col = 0; col < n; ++col) {
            auto it = a.find(col);
            if (it != a.end() && it.value() == row)
                line += QStringLiteral(" Q");
            else
                line += QStringLiteral(" .");
        }
        qInfo().noquote() << line;
    }
}

// ---------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // ===== 1. Australia map coloring — plain backtracking ====================
    {
        printSection("Australia Map Coloring — Backtracking");
        auto csp = australiaCsp();
        auto result = backtrackingSearch(csp);
        if (result) {
            qInfo() << "Solution found!  Assignments:" << csp.m_nassigns;
            printMapAssignment(*result);
        } else {
            qInfo() << "No solution.";
        }
    }

    // ===== 2. Australia — backtracking + MRV + forward checking ==============
    {
        printSection("Australia Map Coloring — MRV + Forward Checking");
        auto csp = australiaCsp();
        auto result = backtrackingSearch<QString, QChar>(
            csp,
            mrv<QString, QChar>,
            unorderedDomainValues<QString, QChar>,
            forwardChecking<QString, QChar>);
        if (result) {
            qInfo() << "Solution found!  Assignments:" << csp.m_nassigns;
            printMapAssignment(*result);
        } else {
            qInfo() << "No solution.";
        }
    }

    // ===== 3. Australia — backtracking + MRV + MAC ===========================
    {
        printSection("Australia Map Coloring — MRV + MAC");
        auto csp = australiaCsp();
        auto result = backtrackingSearch<QString, QChar>(
            csp,
            mrv<QString, QChar>,
            unorderedDomainValues<QString, QChar>,
            mac<QString, QChar>);
        if (result) {
            qInfo() << "Solution found!  Assignments:" << csp.m_nassigns;
            printMapAssignment(*result);
        } else {
            qInfo() << "No solution.";
        }
    }

    // ===== 4. 8-Queens — backtracking + MRV + forward checking ===============
    {
        printSection("8-Queens — Backtracking + MRV + Forward Checking");
        NQueensCSP csp(8);
        auto result = backtrackingSearch<int, int>(
            csp,
            mrv<int, int>,
            unorderedDomainValues<int, int>,
            forwardChecking<int, int>);
        if (result) {
            qInfo() << "Solution found!  Assignments:" << csp.m_nassigns;
            printBoard(8, *result);
        } else {
            qInfo() << "No solution.";
        }
    }

    // ===== 5. 8-Queens — min-conflicts =======================================
    {
        printSection("8-Queens — Min-Conflicts");
        NQueensCSP csp(8);
        auto result = minConflicts<int, int>(csp);
        if (result) {
            qInfo() << "Solution found!  Assignments:" << csp.m_nassigns;
            printBoard(8, *result);
        } else {
            qInfo() << "No solution within step limit.";
        }
    }

    return 0;
}
