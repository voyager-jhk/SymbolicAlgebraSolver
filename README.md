# Symbolic Identity Verifier: Polynomial & Rational Algebra

This is a C-based **Symbolic Algebra Solver** designed to verify the equivalence of polynomial and rational expressions. The project demonstrates core proficiency in **compiler principles** and **computational mathematics**.

---

## 🔬 Core Methodology

The system's goal is to determine if an equation ($A=B$) is an identity by checking if the difference ($A-B$) simplifies to the **zero polynomial**.

### Key Concepts

| **Concept**          | **Description**                                                                                                                                                        |
| -------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Parsing & AST**    | Transforms string input into an Abstract Syntax Tree (AST) for structural representation.                                                                              |
| **Canonicalization** | ASTs are converted into a custom **monomial/polynomial data structure** optimized for algebraic operations (e.g., term merging) and reduced to a unique standard form. |
| **Identity Check**   | If canonical forms match (or the difference is zero), the identity is verified.                                                                                        |

### Feature Summary

- **Polynomial Solver:** Verifies $A=B$ by checking if $A-B=0$.
- **Rational Solver:** Verifies $p_1/p_2 = p_3/p_4$ by checking the polynomial identity $p_1 \cdot p_4 = p_3 \cdot p_2$. Includes zero-denominator detection.
- **Error Handling:** Returns `-1` for invalid or non-algebraic expressions (e.g., trigonometric functions) and division by zero.

---

## ⚙️ Build & Execution

1. **Prerequisite:** GCC Compiler.

2. **Compile:**

   Bash

   ```
   gcc -O3 -g ast.c hashtable.c pattern_match.c parser_api.c parser.c lexer.c polynomial.c solver.c test.c -o test.out
   ```

3. **Run Tests:**

   Bash

   ```
   ./test.out
   ```

## 🛠️ Technical Challenges

| **Challenge**                  | **Solution**                                                                                                                                                                       |
| ------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Inefficient Representation** | Designed a custom, list-based **polynomial** structure for fast term lookup and merging, overcoming AST limitations for algebraic computation.                                     |
| **Floating-Point Precision**   | Used a small **EPSILON ($\epsilon$) tolerance** ($                                                                                                                                 |
| **Zero-Denom Detection**       | Implemented a robust `poly_is_zero` function that **iterates all terms** to confirm all coefficients are zero, ensuring constant `0` is correctly identified as a zero polynomial. |

---

## 📈 Future Work

- Algorithm optimization (e.g., Hash Table-based term merging).
- Extension to support transcendental functions in a formal theory.
- Formal complexity analysis and academic reporting.

## 📄 License

This project is licensed under the [MIT License](https://www.google.com/search?q=LICENSE).
