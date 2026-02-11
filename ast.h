#ifndef AST_H
#define AST_H

#include <stdlib.h>
#include <stdint.h>
#include "token.h"

typedef enum {
    AST_LITERAL_INT,
    AST_LITERAL_FLOAT,
    AST_LITERAL_STRING,
    AST_IDENTIFIER,
    AST_UNARY_OP,      // something like -5 +2 *a so operator to 1 num
    AST_BINARY_OP,     // lhs [SOMETHING] rhs
    AST_ASSIGN,
} ExprType;

typedef enum {
    STMT_EXPR,     // expression as statement: a + 5;
    STMT_LET,      // let a = expr;
    STMT_BLOCK,    // { stmt1; stmt2; }
} StmtType;

typedef struct Expr Expr;
typedef struct Stmt Stmt;

typedef struct {
    Stmt   **items;
    /* Stmt   *items; */ /* did we need to own this? */
    size_t count;
    size_t capacity;
} Statements;

struct Stmt {
    StmtType type;

    union {
        // Expression statement
        struct {
            Expr *expr;
        } expr;

        // let a = expr;
        struct {
            char *name;
            Expr *value;
        } let;

        // { ... }
        struct {
            Statements statements;
        } block;

    } as;
};

struct Expr {
    ExprType type;
    union {
        // Literals
        uint64_t uint_val;
        double float_val;
        char *identifier;

        // Unary Op (e.g., -5)
        struct {
            int op; // The token type (T_MIN, etc)
            Expr *right;
        } unary;

        // a = expr;
        struct {
            char *name;
            Expr *value;
        } assign;

        // Binary Op (e.g., 5 + 5)
        struct {
            int op; // The token type (T_PLUS, T_MIN, etc)
            Expr *left;
            Expr *right;
        } binary;
    } as;
};

Expr *make_expr(ExprType type);
Stmt *make_stmt(StmtType type);
void make_ast(Statements *stmts, Tokens *t);
void print_stmt(Stmt *s, int indent);

#endif // EXPR_H
