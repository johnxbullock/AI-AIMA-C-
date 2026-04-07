#pragma once

#include "expr.h"

// [AIMA p.253] Convert a propositional sentence to Conjunctive Normal Form.
// Returns an Expr of the form  (A | ~B | ...) & (B | C | ...) & ...
Expr toCnf(const Expr &s);

// Step 1+2: replace ==>, <==, <=>, ^ with equivalent &/|/~ forms
Expr eliminateImplications(const Expr &s);

// Step 3: push ~ inward using De Morgan's laws until only literals remain
Expr moveNotInwards(const Expr &s);

// Step 4: distribute & over | to produce a conjunction of clauses
Expr distributeAndOverOr(const Expr &s);
