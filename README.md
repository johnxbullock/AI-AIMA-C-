# ClassicalPlanning

A C++/Qt port of the propositional logic and classical planning components from
[AIMA (Artificial Intelligence: A Modern Approach)](https://github.com/aimacode/aima-python).

Built with **Qt 6.5**, **C++17**, and **CMake**.

---

## Project structure

```
ClassicalPlanning/
├── main.cpp
├── domain/          — planning domain types (State, Action)
├── engine/          — planning engine
│   ├── PlanningProblem.h/.cpp
│   ├── orchestrator.h/.cpp
│   └── logic/       — propositional logic library (see below)
```

### logic/

| File | Contents |
|---|---|
| `expr.h/.cpp` | `Expr` class, operator overloads, `expr()` string parser, `PartialExpr`, `InfixOp`, `Symbol()` |
| `utils.h/.cpp` | Symbol predicates, `conjuncts`/`disjuncts`/`associate`, `inspectLiteral`, `removeAll`, `unique`, common symbol constants |
| `cnf.h/.cpp` | `toCnf()` pipeline: `eliminateImplications` → `moveNotInwards` → `distributeAndOverOr` |
| `kb.h/.cpp` | `KB`, `PropKB`, `PropDefiniteKB`; inference: `plTrue`, `ttEntails`, `plResolution`, `plFcEntails` |
| `dpll.h/.cpp` | `dpllSatisfiable()`, core `dpll()`, 8 branching heuristics |
| `graph.h/.cpp` | `ImplicationGraph` — directed graph with Cooper et al. (2001) immediate dominators |
| `cdcl.h/.cpp` | `cdclSatisfiable()`, `TwoWLClauseDatabase`, restart strategies |

---

## Usage

### Expressions

```cpp
#include "logic/expr.h"
#include "logic/utils.h"

Expr p = Symbol("P");
Expr q = Symbol("Q");

Expr e1 = p & q;                        // Expr("&", {P, Q})
Expr e2 = p | InfixOp("==>") | q;      // Expr("==>", {P, Q})
Expr e3 = expr("P & Q ==> Q");         // parsed from string
```

### Knowledge base

```cpp
#include "logic/kb.h"
#include "logic/cnf.h"

PropKB kb;
kb.tell(expr("P ==> Q"));
kb.tell(expr("P"));

bool entailed = kb.askIfTrue(expr("Q"));   // true
```

### DPLL

```cpp
#include "logic/dpll.h"

auto model = dpllSatisfiable(expr("P & ~P"));   // nullopt (unsat)
auto model2 = dpllSatisfiable(expr("P | Q"));   // {P: true} or similar
```

#### Branching heuristics

All heuristics have the signature `QPair<Expr, bool>(const QVector<Expr>&, const QVector<Expr>&)`
and can be passed directly as the second argument to `dpllSatisfiable`:

```cpp
dpllSatisfiable(s, moms);
dpllSatisfiable(s, dlcs);
dpllSatisfiable(s, jw2);
```

**Note — `momsf` with non-default `k`:** `momsf` has a third parameter `k` (default `0`).
With `k=0` it can be passed directly. For any other value of `k`, it does not match the
`BranchingHeuristic` signature and must be wrapped in a lambda:

```cpp
// k=0 (default) — passes directly
dpllSatisfiable(s, momsf);

// k=2 — must wrap
dpllSatisfiable(s, [](const QVector<Expr> &syms, const QVector<Expr> &clauses) {
    return momsf(syms, clauses, 2);
});
```

The same pattern applies to `luby` and `glucose` restart strategies for CDCL when
using non-default parameters.

### CDCL

```cpp
#include "logic/cdcl.h"

// Default: no restarts, VSIDS decay = 0.95
auto model = cdclSatisfiable(expr("(P | Q) & (~P | R) & (~Q | ~R)"));

// With Luby restarts (unit=512 is the default, wrap for other values)
auto model2 = cdclSatisfiable(s, 0.95, luby);

// With Glucose restarts at non-default x/k
auto model3 = cdclSatisfiable(s, 0.95,
    [](int c, int r, const QVector<int> &q, int s) {
        return glucose(c, r, q, s, /*x=*/50, /*k=*/0.8);
    });
```

### Infix operators

Custom infix operators (e.g. `==>`, `<==`, `<=>`) can be written two ways:

```cpp
// 1. Via the expr() parser (recommended for readability)
Expr e = expr("P ==> Q");

// 2. Via InfixOp and operator| overloading
Expr e = P | InfixOp("==>") | Q;
```

---

## Building

Open in Qt Creator and build the `Desktop_Qt_6_5_0_MinGW_64_bit-Debug` kit,
or from the command line:

```bash
cmake --build build/Desktop_Qt_6_5_0_MinGW_64_bit-Debug
```

---

## References

- Russell, S. & Norvig, P. — *Artificial Intelligence: A Modern Approach*, 4th ed.
- Cooper, K. et al. — *A Simple, Fast Dominance Algorithm* (2001) — used in `ImplicationGraph::immediateDominators`
- [aima-python](https://github.com/aimacode/aima-python) — original Python source
