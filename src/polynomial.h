#ifndef POLYNOMIAL_H
#define POLYNOMIAL_H

#include "ast.h" // For term_type, oper_type, term, constant, etc.
#include <stdbool.h> // For bool type (though not explicitly used in current API)
#include <math.h>    // For INFINITY, fabs, fmod, round if used in header macros (not typical)

// Represents a single variable-power pair, e.g., a^2
typedef struct {
    char *base;     // The base variable name, e.g., "x", "y", or "sin(x)" if treated as opaque
    int exponent;   // The exponent of this variable in a monomial
} var_pow;

// Represents a single monomial, e.g., 5 * a^2 * b
typedef struct {
    double coeff;   // The coefficient
    var_pow* vars;  // A dynamically sized array of variable-power pairs, sorted by base
    int var_count;
} monomial;

// Represents a full polynomial as a sum of monomials
// Terms should be sorted (e.g., lexicographically) for canonical form
typedef struct {
    monomial** terms; // A dynamically sized array of monomial pointers, sorted
    int term_count;
} polynomial;

// --- Function Prototypes for Monomial and Polynomial Operations ---

// Monomial creation and management
monomial* create_monomial(); // Creates an empty monomial (0)
monomial* create_monomial_from_coeff(double coeff); // Creates a constant monomial
void monomial_free(monomial* m);
monomial* monomial_copy(const monomial* m_orig);
monomial* monomial_multiply(const monomial* m1, const monomial* m2);


// Polynomial creation and management
polynomial* poly_create(); // Initializes a new, empty (zero) polynomial
void poly_free(polynomial* p);
polynomial* poly_copy(const polynomial* p_orig); // Deep copies a polynomial

// Adding terms
void add_monomial_to_poly(polynomial* p, monomial* m); // Polynomial takes ownership of m

// Core AST to Polynomial conversion
polynomial* convert_ast_to_poly(term* t);

// Polynomial arithmetic
polynomial* poly_add(polynomial* p1, polynomial* p2);
polynomial* poly_subtract(polynomial* p1, polynomial* p2);
polynomial* poly_multiply(polynomial* p1, polynomial* p2);
polynomial* poly_negate(polynomial* p);
polynomial* poly_scalar_div(polynomial* p, double scalar); // Divides by a scalar

// Comparison and checks
int poly_is_equal(polynomial* p1, polynomial* p2); // Assumes p1, p2 are canonical
int poly_is_zero(polynomial* p);

// Canonicalization
void poly_simplify(polynomial* p); // Sorts terms, combines like terms, removes zeros

// Helpers for sorting (used by qsort, could be static in .c if not needed externally)
int compare_var_pow(const void* a, const void* b);
int compare_monomials(const void* a, const void* b);

// Helpers for AST conversion logic (can be static in .c if only used there)
int poly_is_constant(polynomial* p);
int poly_is_effectively_zero_constant(polynomial* p);
int poly_is_non_negative_integer_constant(polynomial* p);

// Debugging
void poly_print(polynomial* p);

#endif // POLYNOMIAL_H
