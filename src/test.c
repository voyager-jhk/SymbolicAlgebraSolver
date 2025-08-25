#include "parser_api.h"
#include "ast.h"
#include "solver.h"
#include "pattern_match.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    match_init(); // IMPORTANT: Initialize the pattern matcher once at the start

    // Test cases for polynomial solver
    printf("--- Polynomial Solver Tests ---\n");
    char *poly_test_cases[] = {
        "a+b+c = c+a+2*b-b",
        "a*b*c + c*d = b*a*c + d*c",
        "a^2 + (c^3)*d = d*(c^3)+a^2",
        "a+b+10 = b+a+5",
        "a+c = d",
        "(2*sinx + a^2)*(x^2) = 2*(x^2)*sinx + (a^2)*(x^2)",
        "2*sinx * cosx = sin(2*x)",
        "(a+b)^2 * (a+b) = a*(a+b)^2 + b*(a+b)^2",
        "(a+b)^3 = a*(a+b)^2 + b*(a+b)^2",
        "d^2 - d^2 + a*b*c = c*b*a",
        "2*a^2 + (c^3)*2*d = (d*(c^3)+a^2)*2",
        "(a+b)*(c+d) = a*c+b*c+a*d+b*d"};

    int expected_poly_results[] = {1, 1, 1, 0, -1, 1, -1, 1, 1, 1, 1, 1};

    for (int i = 0; i < sizeof(poly_test_cases) / sizeof(poly_test_cases[0]); ++i) {
        printf("Testing Poly: %s\n", poly_test_cases[i]);
        term *t = parse_term_from_str(poly_test_cases[i], 0, 0);
        if (t) { // Check if parsing was successful
            int result = solver_poly(t);
            printf("Result: %d (Expected: %d)\n\n", result, expected_poly_results[i]);
            free_term(t); // Free the AST
            t = NULL;     // Good practice
        } else {
            printf("Parsing failed for: %s\n\n", poly_test_cases[i]);
        }
    }

    printf("\n--- Rational Solver Tests ---\n");
    char *rational_test_cases[] = {
        "(3*(a+b))/(2*(b*b-c*c)) = (6*(2*b+a-b))/(4*(b+c)*(b-c))",
        "1/x = 2/x",
        "1/0 = 2/0", 
        "a/(a-a) = b/(b-b)"
    };
    int expected_rational_results[] = {1, 0, -1, -1};

    for (int i = 0; i < sizeof(rational_test_cases) / sizeof(rational_test_cases[0]); ++i) {
        printf("Testing Rational: %s\n", rational_test_cases[i]);
        term *t = parse_term_from_str(rational_test_cases[i], 0, 0);
        if (t) { // Check if parsing was successful
            int result = solver_rational(t);
            printf("Result: %d (Expected: %d)\n\n", result, expected_rational_results[i]);
            free_term(t); // Free the AST
            t = NULL;     // Good practice
        } else {
            printf("Parsing failed for: %s\n\n", rational_test_cases[i]);
        }
    }
    return 0;
}
