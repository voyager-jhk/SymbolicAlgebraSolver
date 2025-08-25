#include "polynomial.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h> // For fabs, fmod, round, INFINITY
#include "ast.h"  // Make sure ast.h is included for term types and enums

// Define EPSILON for floating point comparisons
#ifndef EPSILON
#define EPSILON 1e-9
#endif

// --- Helper Functions for AST Conversion (Declaration needed in .h if used outside) ---
int poly_is_constant(polynomial* p);
int poly_is_effectively_zero_constant(polynomial* p);
int poly_is_non_negative_integer_constant(polynomial* p);


// --- Helper for qsort: Compares two var_pow for sorting ---
// Sorts alphabetically by base, then by exponent.
int compare_var_pow(const void* a, const void* b) {
    const var_pow* vp1 = (const var_pow*)a;
    const var_pow* vp2 = (const var_pow*)b;
    int cmp = strcmp(vp1->base, vp2->base);
    if (cmp == 0) {
        return vp1->exponent - vp2->exponent;
    }
    return cmp;
}

// --- Helper for qsort: Compares two monomials for sorting and identifying like terms ---
// Returns 0 if like terms, <0 if m1 comes before m2, >0 otherwise.
// Sorts by var_count, then lexicographically by var_pow arrays.
int compare_monomials(const void* a, const void* b) {
    const monomial* m1 = *(const monomial**)a;
    const monomial* m2 = *(const monomial**)b;

    if (m1->var_count != m2->var_count) {
        return m1->var_count - m2->var_count;
    }

    for (int i = 0; i < m1->var_count; ++i) {
        // Ensure vars are sorted within each monomial before comparison
        // This should be guaranteed if monomials are constructed correctly
        int cmp_vp = strcmp(m1->vars[i].base, m2->vars[i].base);
        if (cmp_vp != 0) return cmp_vp;
        if (m1->vars[i].exponent != m2->vars[i].exponent) {
            return m1->vars[i].exponent - m2->vars[i].exponent;
        }
    }
    // If all var_pow are identical, they are like terms.
    return 0;
}

// --- Basic Memory Management ---

// Creates an empty monomial (coefficient 0, no variables)
monomial* create_monomial() {
    monomial* m = (monomial*)malloc(sizeof(monomial));
    if (!m) {
        fprintf(stderr, "Memory allocation failed for monomial\n");
        exit(EXIT_FAILURE);
    }
    m->coeff = 0.0;
    m->vars = NULL;
    m->var_count = 0;
    return m;
}

// Creates a monomial representing a constant value
monomial* create_monomial_from_coeff(double coeff) {
    monomial* m = create_monomial();
    m->coeff = coeff;
    return m;
}


polynomial* poly_create() {
    polynomial* p = (polynomial*)malloc(sizeof(polynomial));
    if (!p) { fprintf(stderr, "Memory allocation failed for polynomial\n"); exit(EXIT_FAILURE); }
    p->terms = NULL;
    p->term_count = 0;
    return p;
}

// Frees memory for a single monomial. Handles duplicated strings for var_pow bases.
void monomial_free(monomial* m) {
    if (m) {
        if (m->vars) {
            for(int i = 0; i < m->var_count; ++i) {
                free(m->vars[i].base); // Free duplicated string
            }
            free(m->vars);
        }
        free(m);
    }
}

// Frees all memory associated with a polynomial.
void poly_free(polynomial* p) {
    if (p) {
        if (p->terms) {
            for (int i = 0; i < p->term_count; ++i) {
                monomial_free(p->terms[i]);
            }
            free(p->terms);
        }
        free(p);
    }
}

// Helper function to create a deep copy of a monomial
monomial* monomial_copy(const monomial* m_orig) {
    if (!m_orig) return NULL;
    monomial* new_m = (monomial*)malloc(sizeof(monomial));
    if (!new_m) { fprintf(stderr, "Memory allocation failed for monomial_copy\n"); exit(EXIT_FAILURE); }
    
    new_m->coeff = m_orig->coeff;
    new_m->var_count = m_orig->var_count;
    
    if (m_orig->var_count > 0) {
        new_m->vars = (var_pow*)malloc(m_orig->var_count * sizeof(var_pow));
        if (!new_m->vars) { 
            fprintf(stderr, "Memory allocation failed for monomial_copy vars\n"); 
            free(new_m);
            exit(EXIT_FAILURE); 
        }
        for (int i = 0; i < m_orig->var_count; ++i) {
            new_m->vars[i].base = strdup(m_orig->vars[i].base); // Duplicate string for base
            if (!new_m->vars[i].base) {
                 fprintf(stderr, "Memory allocation failed for monomial_copy var base strdup\n");
                 // Cleanup partially copied monomial
                 for (int j = 0; j < i; ++j) free(new_m->vars[j].base);
                 free(new_m->vars);
                 free(new_m);
                 exit(EXIT_FAILURE);
            }
            new_m->vars[i].exponent = m_orig->vars[i].exponent;
        }
    } else {
        new_m->vars = NULL;
    }
    return new_m;
}

// Creates a deep copy of a polynomial
polynomial* poly_copy(const polynomial* p_orig) {
    if (!p_orig) return NULL;
    polynomial* new_p = poly_create(); // Initializes terms to NULL and term_count to 0

    if (p_orig->term_count > 0) {
        new_p->terms = (monomial**)malloc(p_orig->term_count * sizeof(monomial*));
        if (!new_p->terms) {
            fprintf(stderr, "Memory allocation failed for poly_copy terms\n");
            poly_free(new_p); // new_p is just an empty shell here
            exit(EXIT_FAILURE);
        }
    } else {
        new_p->terms = NULL; // Explicitly NULL if p_orig->term_count is 0
    }
    
    new_p->term_count = p_orig->term_count;
    for (int i = 0; i < p_orig->term_count; ++i) {
        new_p->terms[i] = monomial_copy(p_orig->terms[i]);
        if (!new_p->terms[i]) {
            fprintf(stderr, "Error during monomial_copy in poly_copy\n");
            // Cleanup partially copied polynomial
            for (int j = 0; j < i; ++j) {
                monomial_free(new_p->terms[j]);
            }
            free(new_p->terms);
            free(new_p); // or poly_free(new_p) if it handles partially filled terms array
            exit(EXIT_FAILURE);
        }
    }
    return new_p;
}


// --- Polynomial Simplification (Canonicalization) ---
// Sorts terms, combines like terms, and removes zero-coefficient terms.
void poly_simplify(polynomial* p) {
    if (!p || p->term_count <= 1) { // No simplification needed for 0 or 1 term if vars are sorted
        // Ensure single monomial's vars are sorted if not already
        if (p && p->term_count == 1 && p->terms[0]->var_count > 1) {
             qsort(p->terms[0]->vars, p->terms[0]->var_count, sizeof(var_pow), compare_var_pow);
        }
        // Remove if the single term is zero
        if (p && p->term_count == 1 && fabs(p->terms[0]->coeff) < EPSILON) {
            monomial_free(p->terms[0]);
            free(p->terms);
            p->terms = NULL;
            p->term_count = 0;
        }
        return;
    }

    // Ensure vars within each monomial are sorted first
    for(int i=0; i < p->term_count; ++i) {
        if (p->terms[i]->var_count > 1) {
            qsort(p->terms[i]->vars, p->terms[i]->var_count, sizeof(var_pow), compare_var_pow);
        }
    }
    
    qsort(p->terms, p->term_count, sizeof(monomial*), compare_monomials);

    monomial** simplified_terms = (monomial**)malloc(p->term_count * sizeof(monomial*));
    if (!simplified_terms && p->term_count > 0) {
        fprintf(stderr, "Memory allocation failed for simplified_terms in poly_simplify\n");
        exit(EXIT_FAILURE);
    }
    int simplified_count = 0;

    if (p->term_count > 0) {
        simplified_terms[0] = p->terms[0]; // Keep the first term
        simplified_count = 1;

        for (int i = 1; i < p->term_count; ++i) {
            // compare_monomials returns 0 for like terms
            if (compare_monomials((const void*)&p->terms[i], (const void*)&simplified_terms[simplified_count - 1]) == 0) {
                simplified_terms[simplified_count - 1]->coeff += p->terms[i]->coeff;
                monomial_free(p->terms[i]); // Free the merged term from the old array
            } else {
                simplified_terms[simplified_count++] = p->terms[i]; // Add as new
            }
        }
    }

    // Filter out terms whose coefficients became zero
    int final_count = 0;
    for (int i = 0; i < simplified_count; ++i) {
        if (fabs(simplified_terms[i]->coeff) > EPSILON) {
            if (final_count != i) { // Avoid self-assignment if no terms were removed yet
                 simplified_terms[final_count] = simplified_terms[i];
            }
            final_count++;
        } else {
            monomial_free(simplified_terms[i]); // Free terms that simplified to zero
        }
    }

    free(p->terms); // Free the old array of monomial pointers

    if (final_count > 0) {
        p->terms = (monomial**)realloc(simplified_terms, final_count * sizeof(monomial*));
        if (!p->terms && final_count > 0) { // Check realloc success
            fprintf(stderr, "Memory allocation failed during poly_simplify realloc\n");
            free(simplified_terms); // simplified_terms might be invalid after failed realloc
            exit(EXIT_FAILURE);
        }
    } else {
        p->terms = NULL;
        free(simplified_terms); // Free the temporary array if nothing is left
    }
    p->term_count = final_count;
}


// Adds a monomial to a polynomial.
// The polynomial takes ownership of the monomial m.
// Assumes m is either newly created or a copy that can be owned.
// Simplification is typically done *after* all desired monomials are added.
void add_monomial_to_poly(polynomial* p, monomial* m_to_add) {
    if (!p || !m_to_add) return;

    // Ensure vars in the monomial to add are sorted (canonical form for the monomial itself)
    if (m_to_add->var_count > 1) {
        qsort(m_to_add->vars, m_to_add->var_count, sizeof(var_pow), compare_var_pow);
    }

    p->term_count++;
    p->terms = (monomial**)realloc(p->terms, p->term_count * sizeof(monomial*));
    if (!p->terms) { 
        fprintf(stderr, "Memory allocation failed for add_monomial_to_poly\n"); 
        // Potentially restore p->term_count and free m_to_add if realloc fails mid-operation
        p->term_count--; 
        monomial_free(m_to_add); // Caller might lose m_to_add if we don't free it
        exit(EXIT_FAILURE); 
    }
    p->terms[p->term_count - 1] = m_to_add;
}

// --- Core Conversion Logic ---
polynomial* convert_ast_to_poly(term* t) {
    if (!t) {
        return poly_create(); // Return zero polynomial for null term
    }

    switch (t->type) {
        case Const: {
            polynomial* p = poly_create();
            monomial* m = create_monomial();
            
            // Assuming t->content.Const is the structure for constants
            // And it has 'type' (const_type enum) and 'content' (the value for Num)
            if (t->content.Const.type == Num) { // Num is an enum from const_type
                m->coeff = (double)t->content.Const.content; // Assuming 'content' holds the numeric value
            } else if (t->content.Const.type == Pos_Infty) { // Assuming Pos_Infty exists
                m->coeff = INFINITY;
            } else if (t->content.Const.type == Neg_Infty) { // Assuming Neg_Infty exists
                m->coeff = -INFINITY;
            } else {
                fprintf(stderr, "Error: Unsupported const_type (%d) for polynomial conversion.\n", t->content.Const.type);
                monomial_free(m); poly_free(p); return NULL;
            }
            add_monomial_to_poly(p, m); // p now owns m
            // poly_simplify(p); // Not strictly needed for single term, but good for consistency
            return p;
        }
        case Var: {
            polynomial* p = poly_create();
            monomial* m = create_monomial();
            m->coeff = 1.0;
            m->var_count = 1;
            m->vars = (var_pow*)malloc(sizeof(var_pow));
            if (!m->vars) {
                perror("Failed to allocate memory for variable in monomial");
                monomial_free(m); poly_free(p); exit(EXIT_FAILURE);
            }
            m->vars[0].base = strdup(t->content.Var); // Assuming t->content.Var is char*
            if(!m->vars[0].base){
                perror("Failed to strdup var name");
                free(m->vars); monomial_free(m); poly_free(p); exit(EXIT_FAILURE);
            }
            m->vars[0].exponent = 1;
            // No need to sort vars for a single variable.
            add_monomial_to_poly(p, m); // p now owns m
            // poly_simplify(p);
            return p;
        }
        case Apply: {
            // AST for binary op: Apply( Apply(Operator, LeftOperand), RightOperand )
            term* first_apply_left = t->content.Apply.left;   // This would be Apply(Operator, LeftOperand)
            term* actual_right_operand_term = t->content.Apply.right; // This is RightOperand

            if (first_apply_left && first_apply_left->type == Apply) {
                term* op_ast_node = first_apply_left->content.Apply.left;  // This is the Operator node
                term* actual_left_operand_term = first_apply_left->content.Apply.right; // This is LeftOperand

                if (op_ast_node && op_ast_node->type == Oper) { // Check if it's an operator
                    oper_type current_op = op_ast_node->content.Oper; // Get the operator type

                    polynomial* p_left = convert_ast_to_poly(actual_left_operand_term);
                    polynomial* p_right = convert_ast_to_poly(actual_right_operand_term);

                    if (!p_left || !p_right) {
                        poly_free(p_left); poly_free(p_right); return NULL;
                    }

                    polynomial* result_poly = NULL;
                    switch (current_op) {
                        case Plus:  result_poly = poly_add(p_left, p_right); break;
                        case Minus: result_poly = poly_subtract(p_left, p_right); break;
                        case Mult:  result_poly = poly_multiply(p_left, p_right); break;
                        case Div:
                            if (poly_is_constant(p_right) && !poly_is_effectively_zero_constant(p_right)) {
                                result_poly = poly_scalar_div(p_left, p_right->terms[0]->coeff);
                            } else {
                                fprintf(stderr, "Warning: Division by non-constant or zero polynomial. Treating as opaque.\n");
                                // Fall through to opaque variable handling if desired, or return NULL
                            }
                            break;
                        case Power:
                            if (poly_is_non_negative_integer_constant(p_right)) {
                                int exponent_val = (int)round(p_right->terms[0]->coeff);
                                if (exponent_val == 0) {
                                    // For x^0 = 1 (assuming x != 0, but for polys this is standard)
                                    // 0^0 is sometimes 1 in this context.
                                    result_poly = poly_create();
                                    add_monomial_to_poly(result_poly, create_monomial_from_coeff(1.0));
                                } else if (exponent_val > 0) {
                                    result_poly = poly_create(); // Start with 1 for multiplication
                                    add_monomial_to_poly(result_poly, create_monomial_from_coeff(1.0));
                                    polynomial* base_copy = poly_copy(p_left); // p_left is the base
                                    for (int i = 0; i < exponent_val; ++i) {
                                        polynomial* temp = poly_multiply(result_poly, base_copy);
                                        poly_free(result_poly); // Free previous result_poly
                                        result_poly = temp;     // Assign new result
                                    }
                                    poly_free(base_copy);
                                } else { // Negative exponent
                                     fprintf(stderr, "Warning: Negative exponent in Power. Treating as opaque.\n");
                                }
                            } else {
                                fprintf(stderr, "Warning: Power with non-integer or non-constant exponent. Treating as opaque.\n");
                            }
                            break;
                        default:
                            fprintf(stderr, "Warning: Unsupported binary operator %d. Treating as opaque.\n", current_op);
                    }

                    poly_free(p_left);  // Free the intermediate polynomials
                    poly_free(p_right);

                    if (result_poly) return result_poly; // If operation was successful
                    // If result_poly is NULL (e.g., unsupported Div/Power), fall through to treat as opaque.
                }
            }
            
            // Lacking term_to_string, we return NULL indicating it's not a convertible polynomial part.
            fprintf(stderr, "Warning: AST node type Apply for term (line) treated as non-polynomial or unhandled structure.\n");
            // print_term(t, stderr); // If you have print_term from parser_api.h
            return NULL;
        }
        // These types are not directly convertible to simple polynomial terms.
        // They might form parts of equations but not terms within a polynomial sum.
        case Oper: // An operator by itself is not a polynomial
        case Pred: 
        case Connect:
        case Binder:
        case Quant:
        case RawQuant:
            fprintf(stderr, "Error: AST node type %d (line) cannot be converted to a polynomial term.\n", t->type);
            return NULL;
        default:
            fprintf(stderr, "Error: Unknown AST term type %d (line) in convert_ast_to_poly.\n", t->type);
            return NULL;
    }
}


// --- Polynomial Operations ---

polynomial* poly_add(polynomial* p1, polynomial* p2) {
    if (!p1 || !p2) { // Handle NULL inputs if necessary, though convert_ast_to_poly should avoid them for valid parts
        if (p1) return poly_copy(p1);
        if (p2) return poly_copy(p2);
        return poly_create(); // Adding two NULLs or a NULL and something
    }
    polynomial* result = poly_create();

    for (int i = 0; i < p1->term_count; ++i) {
        add_monomial_to_poly(result, monomial_copy(p1->terms[i]));
    }
    for (int i = 0; i < p2->term_count; ++i) {
        add_monomial_to_poly(result, monomial_copy(p2->terms[i]));
    }
    poly_simplify(result);
    return result;
}


polynomial* poly_negate(polynomial* p_orig) {
    if (!p_orig) return NULL;
    polynomial* result = poly_copy(p_orig); // Work on a copy
    if (!result) return NULL;

    for (int i = 0; i < result->term_count; ++i) {
        result->terms[i]->coeff *= -1.0;
    }
    // No need to re-simplify if only signs changed, unless a coeff becomes 0.0
    // poly_simplify(result); // Generally safe, but might be redundant if p_orig was simplified
    return result;
}

polynomial* poly_subtract(polynomial* p1, polynomial* p2) {
    if (!p1 || !p2) return NULL; // Or handle more gracefully
    
    polynomial* p2_negated = poly_negate(p2);
    if (!p2_negated) return NULL; // Negation failed

    polynomial* result = poly_add(p1, p2_negated);
    
    poly_free(p2_negated);
    return result; // poly_add already simplifies
}

// Multiplies a monomial by another monomial
monomial* monomial_multiply(const monomial* m1, const monomial* m2) {
    if (!m1 || !m2) return NULL;

    monomial* result = create_monomial();
    result->coeff = m1->coeff * m2->coeff;

    if (fabs(result->coeff) < EPSILON) { // If product coefficient is zero, result is zero monomial
        // No need to process vars if coeff is 0. create_monomial already sets var_count=0
        return result;
    }

    // Combine variables:
    var_pow* temp_vars = (var_pow*)malloc((m1->var_count + m2->var_count) * sizeof(var_pow));
    if (!temp_vars && (m1->var_count + m2->var_count > 0)) {
        fprintf(stderr, "Memory allocation failed for temp_vars in monomial_multiply\n");
        monomial_free(result); exit(EXIT_FAILURE);
    }
    int temp_var_count = 0;

    for (int i = 0; i < m1->var_count; ++i) {
        temp_vars[temp_var_count].base = strdup(m1->vars[i].base);
        if(!temp_vars[temp_var_count].base) { /* error handling */ exit(EXIT_FAILURE); }
        temp_vars[temp_var_count].exponent = m1->vars[i].exponent;
        temp_var_count++;
    }
    for (int i = 0; i < m2->var_count; ++i) {
        temp_vars[temp_var_count].base = strdup(m2->vars[i].base);
         if(!temp_vars[temp_var_count].base) { /* error handling */ exit(EXIT_FAILURE); }
        temp_vars[temp_var_count].exponent = m2->vars[i].exponent;
        temp_var_count++;
    }

    if (temp_var_count > 0) {
        qsort(temp_vars, temp_var_count, sizeof(var_pow), compare_var_pow);

        // Consolidate powers for common bases
        result->vars = (var_pow*)malloc(temp_var_count * sizeof(var_pow)); // Max possible size
        if(!result->vars) { /* error handling */ exit(EXIT_FAILURE); }

        result->vars[0].base = temp_vars[0].base; // Takes ownership of strduped string
        result->vars[0].exponent = temp_vars[0].exponent;
        result->var_count = 1;

        for (int i = 1; i < temp_var_count; ++i) {
            if (strcmp(temp_vars[i].base, result->vars[result->var_count - 1].base) == 0) {
                result->vars[result->var_count - 1].exponent += temp_vars[i].exponent;
                free(temp_vars[i].base); // Free the unused duplicated string
            } else {
                result->vars[result->var_count].base = temp_vars[i].base; // Take ownership
                result->vars[result->var_count].exponent = temp_vars[i].exponent;
                result->var_count++;
            }
        }
        // Resize result->vars to actual size if smaller than temp_var_count
        if (result->var_count < temp_var_count && result->var_count > 0) {
            var_pow* resized_vars = (var_pow*)realloc(result->vars, result->var_count * sizeof(var_pow));
            if (!resized_vars) { /* error handling, though shrinking realloc is usually safe */ }
            else { result->vars = resized_vars; }
        } else if (result->var_count == 0 && result->vars) { // Should not happen if temp_var_count > 0
            free(result->vars);
            result->vars = NULL;
        }
    } else { // No variables
        result->vars = NULL;
        result->var_count = 0;
    }
    free(temp_vars);
    return result;
}


polynomial* poly_multiply(polynomial* p1, polynomial* p2) {
    if (!p1 || !p2) return NULL;
    polynomial* result = poly_create();

    if (poly_is_zero(p1) || poly_is_zero(p2)) {
        return result; // Multiplication by zero polynomial results in zero polynomial
    }

    for (int i = 0; i < p1->term_count; ++i) {
        for (int j = 0; j < p2->term_count; ++j) {
            monomial* product_m = monomial_multiply(p1->terms[i], p2->terms[j]);
            if (product_m) {
                 // Only add if the product monomial is not identically zero (e.g. its coeff became 0)
                if (fabs(product_m->coeff) > EPSILON || product_m->var_count > 0) { // Add if coeff non-zero OR it's 0*vars (which simplify might remove)
                    add_monomial_to_poly(result, product_m); // result takes ownership
                } else {
                    monomial_free(product_m); // product is 0 constant, free it
                }
            }
        }
    }
    poly_simplify(result);
    return result;
}

// Divides all coefficients of a polynomial by a non-zero scalar.
polynomial* poly_scalar_div(polynomial* p_orig, double scalar) {
    if (!p_orig) return NULL;
    if (fabs(scalar) < EPSILON) {
        fprintf(stderr, "Error: Division by zero in poly_scalar_div.\n");
        return NULL; // Cannot divide by zero
    }

    polynomial* result = poly_copy(p_orig);
    if (!result) return NULL;

    for (int i = 0; i < result->term_count; ++i) {
        result->terms[i]->coeff /= scalar;
    }
    poly_simplify(result); // Simplify in case some terms become zero
    return result;
}


// Compare two polynomials for equality (must be in canonical form)
int poly_is_equal(polynomial* p1, polynomial* p2) {
    if (!p1 && !p2) return 1; // Both NULL considered equal (zero)
    if (!p1 || !p2) return 0; // One NULL, one not

    // Ensure both are simplified for a canonical comparison
    // poly_simplify(p1); // Caller should ensure this, or do it here.
    // poly_simplify(p2); // For safety, but can be slow if called repeatedly.

    if (p1->term_count != p2->term_count) {
        return 0;
    }

    // Since poly_simplify sorts terms, we can compare term by term
    for (int i = 0; i < p1->term_count; ++i) {
        const monomial* m1 = p1->terms[i];
        const monomial* m2 = p2->terms[i];

        if (fabs(m1->coeff - m2->coeff) > EPSILON) {
            return 0;
        }
        if (compare_monomials((const void*)&m1, (const void*)&m2) != 0) {
             // This check is more robust if only coefficients differ slightly but vars are identical
             // However, if poly_simplify works correctly and sorts, direct var comparison is also okay.
             // The compare_monomials checks var_count and then var_pow arrays.
             // If coefficients are same (checked above) and compare_monomials is 0, they are identical.
             // The issue is if compare_monomials is used for sorting, it doesn't see coeffs.
             // So, we must compare vars explicitly here after coeffs.
            if (m1->var_count != m2->var_count) return 0;
            for (int v = 0; v < m1->var_count; ++v) {
                if (strcmp(m1->vars[v].base, m2->vars[v].base) != 0 ||
                    m1->vars[v].exponent != m2->vars[v].exponent) {
                    return 0;
                }
            }
        }
    }
    return 1;
}

// Checks if a polynomial is the zero polynomial
int poly_is_zero(polynomial* p) {
    if (!p) return 1; // A NULL polynomial can be treated as zero.
    // After simplification, a zero polynomial has 0 terms.
    if (p->term_count == 0) return true;
    for (int i = 0; i < p->term_count; i++) {
        if (p->terms[i] == NULL) {
            return false;
        }
        if (fabs(p->terms[i]->coeff) > EPSILON) {
            return false;
        }
    }
    return true;
}


// --- Debugging ---
void poly_print(polynomial* p) {
    if (!p) {
        printf("NULL_Polynomial\n");
        return;
    }
    if (p->term_count == 0) {
        printf("0\n");
        return;
    }
    for (int i = 0; i < p->term_count; ++i) {
        monomial* m = p->terms[i];
        
        // Sign handling
        if (m->coeff < 0) {
            printf("%s- ", (i == 0 ? "" : ""));
        } else {
            if (i > 0) {
                printf("+ ");
            }
        }

        double abs_coeff = fabs(m->coeff);
        
        // Print coefficient if it's not 1 (or -1) or if it's a constant term
        if (fabs(abs_coeff - 1.0) > EPSILON || m->var_count == 0) {
             // Check if coefficient is an integer
            if (fabs(abs_coeff - round(abs_coeff)) < EPSILON) {
                printf("%.0f", abs_coeff);
            } else {
                printf("%.2f", abs_coeff); // Print with 2 decimal places for non-integers
            }
        } else if (m->var_count == 0 && fabs(abs_coeff - 1.0) < EPSILON) {
            // Case: constant term is 1 or -1, already handled by sign.
             printf("1"); // Explicitly print 1 if it's just "1" or "-1"
        }


        for (int j = 0; j < m->var_count; ++j) {
            if (j > 0 || (fabs(abs_coeff - 1.0) > EPSILON || m->var_count == 0)) { // Add '*' if coeff printed or not first var
                 if (fabs(abs_coeff - 1.0) < EPSILON && m->var_count > 0 && j==0 && (fabs(abs_coeff - 1.0) > EPSILON || m->var_count == 0) == 0 ) {
                    // no star if coeff is 1 and it's the first var
                 } else {
                    printf("*");
                 }
            } else if ( (fabs(abs_coeff - 1.0) < EPSILON && m->var_count > 0 && j==0 ) && (fabs(abs_coeff - 1.0) > EPSILON || m->var_count == 0) ){
                 printf("*");
            }


            printf("%s", m->vars[j].base);
            if (m->vars[j].exponent != 1) {
                printf("^%d", m->vars[j].exponent);
            }
        }
        if (i < p->term_count - 1) {
            printf(" "); // Space between terms
        }
    }
    printf("\n");
}

// --- Helper Function Implementations for AST Conversion ---
// Checks if polynomial is a single constant term (or zero poly, which is also constant).
int poly_is_constant(polynomial* p) {
    if (!p) return 0; // Or 1 if NULL is considered a zero constant
    if (p->term_count == 0) return 1; // Zero polynomial is constant
    return (p->term_count == 1 && p->terms[0]->var_count == 0);
}

// Checks if polynomial is a constant that is effectively zero.
int poly_is_effectively_zero_constant(polynomial* p) {
    if (!p) return 1; // NULL is like zero
    if (p->term_count == 0) return 1; // Empty is zero
    if (p->term_count == 1 && p->terms[0]->var_count == 0) {
        return fabs(p->terms[0]->coeff) < EPSILON;
    }
    return 0; // Has variables or multiple terms (if not simplified to one zero term)
}

// Checks if polynomial is a constant, non-negative integer.
int poly_is_non_negative_integer_constant(polynomial* p) {
    if (!p) return 0;
    if (p->term_count == 1 && p->terms[0]->var_count == 0) {
        double val = p->terms[0]->coeff;
        // Check if non-negative and if it's an integer (difference from rounded value is small)
        return val >= -EPSILON && fabs(val - round(val)) < EPSILON;
    }
    // A zero polynomial (term_count == 0) could be considered constant 0.
    if (p->term_count == 0) return 1; // 0 is a non-negative integer constant
    return 0;
}
