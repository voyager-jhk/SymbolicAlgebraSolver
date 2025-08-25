#include "solver.h"
#include "polynomial.h"
#include "parser_api.h"   // For parse_term_from_str, print_term
#include "pattern_match.h" // For match function, match_init
#include "ast.h"           // For term structure, enums like term_type, oper_type, pred_type
#include "parser.h"        // For token definitions like VAR, NUM, EQ etc.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h> // For fabs

#ifndef EPSILON
#define EPSILON 1e-9    // Define EPSILON here for floating-point comparisons
#endif

// Checks if a polynomial equation (LHS = RHS) holds true.
// Returns 1 if true, 0 if false, -1 if cannot determine or error.
int solver_poly(term* t) {
    if (!t) {
        fprintf(stderr, "Error (solver_poly): Null input term.\n");
        return -1;
    }

    // The input 't' should represent an equality: Apply(Apply(Eq, lhs), rhs)
    if (t->type != Apply || !t->content.Apply.left || !t->content.Apply.right) {
        fprintf(stderr, "Error (solver_poly): Input term is not a top-level Apply node or has null children.\n");
        return -1;
    }

    term* first_apply_node = t->content.Apply.left;
    term* rhs_ast = t->content.Apply.right;

    if (first_apply_node->type != Apply || !first_apply_node->content.Apply.left || !first_apply_node->content.Apply.right) {
        fprintf(stderr, "Error (solver_poly): Input term structure is not Apply(Apply(Op,LHS),RHS).\n");
        return -1;
    }

    term* eq_operator_term = first_apply_node->content.Apply.left;
    term* lhs_ast = first_apply_node->content.Apply.right;

    if (eq_operator_term->type != Pred || eq_operator_term->content.Pred != Eq) {
        fprintf(stderr, "Error (solver_poly): Input expression is not an equality (Eq) predicate.\n");
        return -1;
    }

    polynomial* poly_lhs = convert_ast_to_poly(lhs_ast);
    polynomial* poly_rhs = convert_ast_to_poly(rhs_ast);

    if (!poly_lhs || !poly_rhs) {
        // This case means one or both sides could not be converted to a polynomial
        // (e.g., contains sin(x) or other non-polynomial forms).
        // This is an "unable to determine" scenario.
        poly_free(poly_lhs); // poly_free handles NULL safely
        poly_free(poly_rhs);
        return -1;
    }

    polynomial* diff_poly = poly_subtract(poly_lhs, poly_rhs);

    poly_free(poly_lhs);
    poly_free(poly_rhs);

    if (!diff_poly) { // Should not happen if poly_subtract is robust
        fprintf(stderr, "Error (solver_poly): poly_subtract returned NULL.\n");
        return -1;
    }

    int result;
    if (poly_is_zero(diff_poly)) {
        result = 1; // LHS = RHS is true
    } else if (poly_is_constant(diff_poly)) {
        // If diff_poly is a non-zero constant (e.g., 5), then LHS = RHS is false.
        result = 0;
    } else {
        // If diff_poly is not zero and contains variables (e.g., a - d + c),
        // we cannot determine if it's always zero or not.
        result = -1;
    }

    poly_free(diff_poly);
    return result;
}

// Checks if a rational equation (p1/p2 = p3/p4) holds true.
// Returns 1 if true, 0 if false, -1 if cannot determine or error (including division by zero).
int solver_rational(term* t) {
    if (!t) {
        fprintf(stderr, "Error (solver_rational): Null input term.\n");
        return -1;
    }

    // Expected structure: Apply(Apply(Eq, LHS_Frac), RHS_Frac)
    if (t->type != Apply || !t->content.Apply.left || !t->content.Apply.right) {
        fprintf(stderr, "Error (solver_rational): Input term is not a top-level Apply or has null children.\n");
        return -1;
    }

    term* eq_apply_node = t->content.Apply.left;
    term* rhs_frac_ast = t->content.Apply.right;

    if (eq_apply_node->type != Apply || !eq_apply_node->content.Apply.left || !eq_apply_node->content.Apply.right) {
         fprintf(stderr, "Error (solver_rational): Input structure not Apply(Apply(Op,LHS),RHS).\n");
        return -1;
    }

    term* eq_op_term = eq_apply_node->content.Apply.left;
    term* lhs_frac_ast = eq_apply_node->content.Apply.right;

    if (eq_op_term->type != Pred || eq_op_term->content.Pred != Eq) {
        fprintf(stderr, "Error (solver_rational): Input is not an equality (Eq) predicate.\n");
        return -1;
    }

    // Parse LHS_Frac (p1/p2). Expected structure: Apply(Apply(Div, p1), p2)
    if (lhs_frac_ast->type != Apply || !lhs_frac_ast->content.Apply.left || !lhs_frac_ast->content.Apply.right) {
        fprintf(stderr, "Error (solver_rational): LHS fraction is not an Apply node or has null children.\n");
        return -1;
    }
    term* lhs_apply_div_p1_node = lhs_frac_ast->content.Apply.left;
    term* p2_ast = lhs_frac_ast->content.Apply.right;

    if (lhs_apply_div_p1_node->type != Apply || !lhs_apply_div_p1_node->content.Apply.left || !lhs_apply_div_p1_node->content.Apply.right) {
        fprintf(stderr, "Error (solver_rational): LHS Apply(Div,p1) structure is not Apply or has null children.\n");
        return -1;
    }
    term* lhs_div_op_node = lhs_apply_div_p1_node->content.Apply.left;
    term* p1_ast = lhs_apply_div_p1_node->content.Apply.right;

    if (lhs_div_op_node->type != Oper || lhs_div_op_node->content.Oper != Div) {
        fprintf(stderr, "Error (solver_rational): LHS of rational expression is not a division (Div operator not found).\n");
        return -1;
    }

    // Parse RHS_Frac (p3/p4). Expected structure: Apply(Apply(Div, p3), p4)
    if (rhs_frac_ast->type != Apply || !rhs_frac_ast->content.Apply.left || !rhs_frac_ast->content.Apply.right) {
        fprintf(stderr, "Error (solver_rational): RHS fraction is not an Apply node or has null children.\n");
        return -1;
    }
    term* rhs_apply_div_p3_node = rhs_frac_ast->content.Apply.left;
    term* p4_ast = rhs_frac_ast->content.Apply.right;

    if (rhs_apply_div_p3_node->type != Apply || !rhs_apply_div_p3_node->content.Apply.left || !rhs_apply_div_p3_node->content.Apply.right) {
        fprintf(stderr, "Error (solver_rational): RHS Apply(Div,p3) structure is not Apply or has null children.\n");
        return -1;
    }
    term* rhs_div_op_node = rhs_apply_div_p3_node->content.Apply.left;
    term* p3_ast = rhs_apply_div_p3_node->content.Apply.right;

    if (rhs_div_op_node->type != Oper || rhs_div_op_node->content.Oper != Div) {
        fprintf(stderr, "Error (solver_rational): RHS of rational expression is not a division (Div operator not found).\n");
        return -1;
    }

    // Convert p1, p2, p3, p4 to polynomials
    polynomial* poly_p1 = convert_ast_to_poly(p1_ast);
    polynomial* poly_p2 = convert_ast_to_poly(p2_ast);
    polynomial* poly_p3 = convert_ast_to_poly(p3_ast);
    polynomial* poly_p4 = convert_ast_to_poly(p4_ast);

    if (!poly_p1 || !poly_p2 || !poly_p3 || !poly_p4) {
        poly_free(poly_p1); poly_free(poly_p2);
        poly_free(poly_p3); poly_free(poly_p4);
        return -1; // Error in conversion, unable to determine
    }

    // Check for division by zero (p2 != 0 and p4 != 0)
    // poly_is_zero already checks for canonical zero polynomial (0 terms)
    // or a constant zero term after simplification.
    if (poly_is_zero(poly_p2) || poly_is_zero(poly_p4)) {
        fprintf(stderr, "Warning (solver_rational): Division by zero polynomial detected.\n");
        poly_free(poly_p1); poly_free(poly_p2);
        poly_free(poly_p3); poly_free(poly_p4);
        return -1; // Division by zero
    }

    // The equation p1/p2 = p3/p4 is equivalent to p1*p4 = p3*p2
    polynomial* lhs_cross_mult = poly_multiply(poly_p1, poly_p4);
    polynomial* rhs_cross_mult = poly_multiply(poly_p3, poly_p2);

    poly_free(poly_p1); poly_free(poly_p2);
    poly_free(poly_p3); poly_free(poly_p4);

    if (!lhs_cross_mult || !rhs_cross_mult) {
        poly_free(lhs_cross_mult); poly_free(rhs_cross_mult);
        fprintf(stderr, "Error (solver_rational): Cross-multiplication failed.\n");
        return -1;
    }

    polynomial* diff_cross_mult = poly_subtract(lhs_cross_mult, rhs_cross_mult);

    poly_free(lhs_cross_mult); poly_free(rhs_cross_mult);

    if (!diff_cross_mult) { // Should not happen
         fprintf(stderr, "Error (solver_rational): Subtraction of cross-multiplied terms failed.\n");
        return -1;
    }

    int result_val;
    if (poly_is_zero(diff_cross_mult)) {
        result_val = 1; // p1*p4 = p3*p2 is true
    } else {
        result_val = 0; // p1*p4 = p3*p2 is false
    }

    poly_free(diff_cross_mult);
    return result_val;
}