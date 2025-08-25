#ifndef SOLVER_H
#define SOLVER_H

#include "ast.h" // For term*

// Polynomial solver function
// Returns 1 if equal, 0 if not equal, -1 if unable to determine
int solver_poly(term* t);

// Rational solver function
// Returns 1 if equal, 0 if not equal, -1 if unable to determine
int solver_rational(term* t);

#endif // SOLVER_H