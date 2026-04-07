#include "expr.h"
#include <QHashFunctions>
#include <stdexcept>

// --- Construction ---

Expr::Expr(const QString &op, QVector<Expr> args)
    : m_op(op), m_args(std::move(args)) {}

QString Expr::op() const { return m_op; }
QVector<Expr> Expr::args() const { return m_args; }

// --- Unary operators ---

Expr Expr::operator-() const { return Expr(QStringLiteral("-"), {*this}); } // derefernce the current object
Expr Expr::operator+() const { return Expr(QStringLiteral("+"), {*this}); }
Expr Expr::operator~() const { return Expr(QStringLiteral("~"), {*this}); }

// --- Binary operators ---

Expr operator+(const Expr &lhs, const Expr &rhs) { return Expr(QStringLiteral("+"),  {lhs, rhs}); }
Expr operator-(const Expr &lhs, const Expr &rhs) { return Expr(QStringLiteral("-"),  {lhs, rhs}); }
Expr operator*(const Expr &lhs, const Expr &rhs) { return Expr(QStringLiteral("*"),  {lhs, rhs}); }
Expr operator/(const Expr &lhs, const Expr &rhs) { return Expr(QStringLiteral("/"),  {lhs, rhs}); }
Expr operator%(const Expr &lhs, const Expr &rhs) { return Expr(QStringLiteral("%"),  {lhs, rhs}); }
Expr operator&(const Expr &lhs, const Expr &rhs) { return Expr(QStringLiteral("&"),  {lhs, rhs}); }
Expr operator|(const Expr &lhs, const Expr &rhs) { return Expr(QStringLiteral("|"),  {lhs, rhs}); }
Expr operator^(const Expr &lhs, const Expr &rhs) { return Expr(QStringLiteral("^"),  {lhs, rhs}); }
Expr operator>>(const Expr &lhs, const Expr &rhs) { return Expr(QStringLiteral(">>"), {lhs, rhs}); }
Expr operator<<(const Expr &lhs, const Expr &rhs) { return Expr(QStringLiteral("<<"), {lhs, rhs}); }

// Named functions for operators without C++ equivalents
Expr pow(const Expr &lhs, const Expr &rhs)      { return Expr(QStringLiteral("**"), {lhs, rhs}); }
Expr floordiv(const Expr &lhs, const Expr &rhs) { return Expr(QStringLiteral("//"), {lhs, rhs}); }
Expr matmul(const Expr &lhs, const Expr &rhs)   { return Expr(QStringLiteral("@"),  {lhs, rhs}); }

// --- Call operator ---

Expr Expr::operator()(QVector<Expr> args) const {
    Q_ASSERT_X(m_args.isEmpty(), "Expr::operator()",
               "Can only call a Symbol, not a compound Expr");
    return Expr(m_op, std::move(args));
}

// --- Comparison ---

bool Expr::operator==(const Expr &other) const {
    return m_op == other.m_op && m_args == other.m_args;
}

bool Expr::operator<(const Expr &other) const {
    return toString() < other.toString();
}

// --- Hashing ---

uint qHash(const Expr &expr, uint seed) {
    uint h = qHash(expr.op(), seed);
    for (const Expr &arg : expr.args())
        h ^= qHash(arg, seed);
    return h;
}

// --- String representation ---

QString Expr::toString() const {
    QStringList argStrings;
    argStrings.reserve(m_args.size());
    for (const Expr &arg : m_args)
        argStrings << arg.toString();

    // Check if op is a valid identifier (letters, digits, underscores, starts with letter/underscore)
    bool isIdentifier = !m_op.isEmpty()
        && (m_op[0].isLetter() || m_op[0] == QLatin1Char('_'));
    if (isIdentifier) {
        for (int i = 1; i < m_op.size() && isIdentifier; ++i)
            isIdentifier = m_op[i].isLetterOrNumber() || m_op[i] == QLatin1Char('_');
    }

    if (isIdentifier) {
        // f(x, y) or just f
        if (argStrings.isEmpty())
            return m_op;
        return QStringLiteral("%1(%2)").arg(m_op, argStrings.join(QStringLiteral(", ")));
    } else if (argStrings.size() == 1) {
        // Unary: -x
        return m_op + argStrings.first();
    } else {
        // Binary: (x + y)
        QString sep = QStringLiteral(" %1 ").arg(m_op);
        return QStringLiteral("(%1)").arg(argStrings.join(sep));
    }
}

QDebug operator<<(QDebug dbg, const Expr &expr) {
    QDebugStateSaver saver(dbg);
    dbg.noquote() << expr.toString();
    return dbg;
}

// ============================================================================
// PartialExpr
// ============================================================================

PartialExpr::PartialExpr(const QString &op, const Expr &lhs)
    : m_op(op), m_lhs(lhs) {}

Expr PartialExpr::operator|(const Expr &rhs) const {
    return Expr(m_op, {m_lhs, rhs});
}

QString PartialExpr::toString() const {
    return QString("PartialExpr('%1', %2)").arg(m_op, m_lhs.toString());
}

QDebug operator<<(QDebug dbg, const PartialExpr &p) {
    QDebugStateSaver saver(dbg);
    dbg.noquote() << p.toString();
    return dbg;
}

// Completes the Expr | InfixOp("==>") chain
PartialExpr operator|(const Expr &lhs, const InfixOp &op) {
    return PartialExpr(op.op, lhs);
}

// ============================================================================
// expr() — recursive descent parser
// ============================================================================

namespace {

// ---- Tokenizer -------------------------------------------------------------

struct Token {
    enum Type {
        Ident,
        LParen, RParen,
        Comma,
        Tilde,          // ~
        Amp,            // &
        Pipe,           // |
        Caret,          // ^
        Implies,        // ==>
        ImpliedBy,      // <==
        Iff,            // <=>
        End,
    };
    Type    type;
    QString value;
};

class Lexer {
public:
    explicit Lexer(const QString &input) : m_input(input), m_pos(0) {}

    Token next() {
        // Skip whitespace
        while (m_pos < m_input.size() && m_input[m_pos].isSpace())
            ++m_pos;

        if (m_pos >= m_input.size())
            return {Token::End, {}};

        const QChar c = m_input[m_pos];

        // Identifier
        if (c.isLetter() || c == QLatin1Char('_')) {
            int start = m_pos;
            while (m_pos < m_input.size() &&
                   (m_input[m_pos].isLetterOrNumber() || m_input[m_pos] == QLatin1Char('_')))
                ++m_pos;
            return {Token::Ident, m_input.mid(start, m_pos - start)};
        }

        // Multi-character operators: ==>, <==, <=>
        auto peek = [&](int offset) -> QChar {
            int i = m_pos + offset;
            return (i < m_input.size()) ? m_input[i] : QChar(0);
        };

        if (c == QLatin1Char('=') && peek(1) == QLatin1Char('=') && peek(2) == QLatin1Char('>')) {
            m_pos += 3; return {Token::Implies, QStringLiteral("==>")};
        }
        if (c == QLatin1Char('<') && peek(1) == QLatin1Char('=') && peek(2) == QLatin1Char('=')) {
            m_pos += 3; return {Token::ImpliedBy, QStringLiteral("<==")};
        }
        if (c == QLatin1Char('<') && peek(1) == QLatin1Char('=') && peek(2) == QLatin1Char('>')) {
            m_pos += 3; return {Token::Iff, QStringLiteral("<=>")};
        }

        // Single-character operators
        ++m_pos;
        switch (c.toLatin1()) {
        case '(': return {Token::LParen,  QStringLiteral("(")};
        case ')': return {Token::RParen,  QStringLiteral(")")};
        case ',': return {Token::Comma,   QStringLiteral(",")};
        case '~': return {Token::Tilde,   QStringLiteral("~")};
        case '&': return {Token::Amp,     QStringLiteral("&")};
        case '|': return {Token::Pipe,    QStringLiteral("|")};
        case '^': return {Token::Caret,   QStringLiteral("^")};
        default:
            throw std::runtime_error(
                QString("Unexpected character '%1' in expression").arg(c).toStdString());
        }
    }

private:
    QString m_input;
    int     m_pos;
};

// ---- Parser ----------------------------------------------------------------
// Precedence (low → high):  <=>  |  ==>/<==  |  |  |  &  |  ~  |  atom

class Parser {
public:
    explicit Parser(const QString &input) : m_lexer(input) { consume(); }

    Expr parse() {
        Expr result = parseBiconditional();
        expect(Token::End);
        return result;
    }

private:
    Lexer m_lexer;
    Token m_current;

    void consume() { m_current = m_lexer.next(); }

    void expect(Token::Type t) {
        if (m_current.type != t)
            throw std::runtime_error("Unexpected token in expression");
        consume();
    }

    // <=>
    Expr parseBiconditional() {
        Expr lhs = parseImplication();
        if (m_current.type == Token::Iff) {
            consume();
            return Expr(QStringLiteral("<=>"), {lhs, parseBiconditional()});
        }
        return lhs;
    }

    // ==>  <==
    Expr parseImplication() {
        Expr lhs = parseOr();
        if (m_current.type == Token::Implies) {
            consume();
            return Expr(QStringLiteral("==>"), {lhs, parseImplication()});
        }
        if (m_current.type == Token::ImpliedBy) {
            consume();
            return Expr(QStringLiteral("<=="), {lhs, parseImplication()});
        }
        return lhs;
    }

    // |
    Expr parseOr() {
        Expr lhs = parseAnd();
        while (m_current.type == Token::Pipe) {
            consume();
            lhs = Expr(QStringLiteral("|"), {lhs, parseAnd()});
        }
        return lhs;
    }

    // &
    Expr parseAnd() {
        Expr lhs = parseNot();
        while (m_current.type == Token::Amp) {
            consume();
            lhs = Expr(QStringLiteral("&"), {lhs, parseNot()});
        }
        return lhs;
    }

    // ~
    Expr parseNot() {
        if (m_current.type == Token::Tilde) {
            consume();
            return Expr(QStringLiteral("~"), {parseNot()});
        }
        return parseAtom();
    }

    // identifier, identifier(args), or (expr)
    Expr parseAtom() {
        if (m_current.type == Token::LParen) {
            consume();
            Expr inner = parseBiconditional();
            expect(Token::RParen);
            return inner;
        }
        if (m_current.type == Token::Ident) {
            QString name = m_current.value;
            consume();
            // Function call: f(a, b, ...)
            if (m_current.type == Token::LParen) {
                consume();
                QVector<Expr> args;
                if (m_current.type != Token::RParen) {
                    args << parseBiconditional();
                    while (m_current.type == Token::Comma) {
                        consume();
                        args << parseBiconditional();
                    }
                }
                expect(Token::RParen);
                return Expr(name, args);
            }
            return Expr(name); // Symbol
        }
        throw std::runtime_error("Expected an atom in expression");
    }
};

} // anonymous namespace

// ---- Public API ------------------------------------------------------------

Expr expr(const QString &x) {
    return Parser(x).parse();
}
