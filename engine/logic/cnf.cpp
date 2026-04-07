#include "cnf.h"
#include "utils.h"

// ---- eliminateImplications -------------------------------------------------

Expr eliminateImplications(const Expr &s) {
    // Atoms (symbols or nullary exprs) are unchanged
    if (s.args().isEmpty() || isSymbol(s.op()))
        return s;

    QVector<Expr> args;
    args.reserve(s.args().size());
    for (const Expr &arg : s.args())
        args.append(eliminateImplications(arg)); // recursive implementation

    const Expr &a = args.first();
    const Expr &b = args.last();

    if (s.op() == QStringLiteral("==>"))
        return b | ~a;
    if (s.op() == QStringLiteral("<=="))
        return a | ~b;
    if (s.op() == QStringLiteral("<=>"))
        return (a | ~b) & (b | ~a);
    if (s.op() == QStringLiteral("^")) {
        Q_ASSERT(args.size() == 2);
        return (a & ~b) | (~a & b);
    }
    return Expr(s.op(), args);
}

// ---- moveNotInwards --------------------------------------------------------

Expr moveNotInwards(const Expr &s) {
    if (s.op() == QStringLiteral("~")) {
        const Expr &a = s.args()[0];

        if (a.op() == QStringLiteral("~"))                  // ~~A  =>  A
            return moveNotInwards(a.args()[0]);

        if (a.op() == QStringLiteral("&")) {                // ~(A & B)  =>  ~A | ~B
            QVector<Expr> negated;
            for (const Expr &arg : a.args())
                negated.append(moveNotInwards(~arg));
            return associate(QStringLiteral("|"), negated);
        }

        if (a.op() == QStringLiteral("|")) {                // ~(A | B)  =>  ~A & ~B
            QVector<Expr> negated;
            for (const Expr &arg : a.args())
                negated.append(moveNotInwards(~arg));
            return associate(QStringLiteral("&"), negated);
        }

        return s;  // ~atom — already in normal form
    }

    if (isSymbol(s.op()) || s.args().isEmpty())
        return s;

    QVector<Expr> args;
    args.reserve(s.args().size());
    for (const Expr &arg : s.args())
        args.append(moveNotInwards(arg));
    return Expr(s.op(), args);
}

// ---- distributeAndOverOr ---------------------------------------------------

Expr distributeAndOverOr(const Expr &s) {
    if (s.op() == QStringLiteral("|")) {
        // Flatten any nested |  (A | (B | C))  =>  (A | B | C)
        Expr flat = associate(QStringLiteral("|"), s.args());

        if (flat.op() != QStringLiteral("|"))
            return distributeAndOverOr(flat);   // got a single element after flattening

        const QVector<Expr> &fargs = flat.args();
        if (fargs.isEmpty())
            return ExprFalse;
        if (fargs.size() == 1)
            return distributeAndOverOr(fargs[0]);

        // Find the first conjunction among the disjuncts
        int conjIdx = -1;
        for (int i = 0; i < fargs.size(); ++i) {
            if (fargs[i].op() == QStringLiteral("&")) { conjIdx = i; break; }
        }
        if (conjIdx == -1)
            return flat;    // already a disjunction of literals — done

        const Expr &conj = fargs[conjIdx];

        // Everything except conj becomes the "rest" disjunction
        QVector<Expr> others;
        for (int i = 0; i < fargs.size(); ++i)
            if (i != conjIdx) others.append(fargs[i]);
        Expr rest = associate(QStringLiteral("|"), others);

        // Distribute:  (A & B) | rest  =>  (A | rest) & (B | rest)
        QVector<Expr> distributed;
        for (const Expr &c : conj.args())
            distributed.append(distributeAndOverOr(c | rest));
        return associate(QStringLiteral("&"), distributed);
    }

    if (s.op() == QStringLiteral("&")) {
        QVector<Expr> args;
        args.reserve(s.args().size());
        for (const Expr &arg : s.args())
            args.append(distributeAndOverOr(arg));
        return associate(QStringLiteral("&"), args);
    }

    return s;   // literal or atom
}

// ---- toCnf -----------------------------------------------------------------

Expr toCnf(const Expr &s) {
    Expr e = eliminateImplications(s);
    e = moveNotInwards(e);
    return distributeAndOverOr(e);
}
