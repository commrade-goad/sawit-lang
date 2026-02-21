#ifndef AST_H
#define AST_H

#include <stdlib.h>
#include <stdint.h>
#include "lexer.h"
#include "arena.h"
#include "utils.h"

typedef enum {
    EXPR_LITERAL_INT,
    EXPR_LITERAL_FLOAT,
    EXPR_LITERAL_STRING,
    EXPR_IDENTIFIER,
    EXPR_UNARY_OP,      // something like -5 +2 !a ~a
    EXPR_BINARY_OP,     // lhs [SOMETHING] rhs
    EXPR_ASSIGN,
    EXPR_FUNCTION,
    EXPR_CALL,
    EXPR_INDEX,
} ExprType;

typedef enum {
    STMT_EXPR,     // expression as statement: a + 5;
    STMT_LET,      // let a = expr;
    STMT_CONST,    // BGCOLOR :: "red";
    STMT_RET,      // return
    STMT_IF,       // if (a) {} else {}
    STMT_FOR,      // for (init; cond; inc) body
    STMT_BLOCK,    // { stmt1; stmt2; }
    STMT_DEFER,
    STMT_ENUM_DEF,
    STMT_STRUCT_DEF,
} StmtType;

typedef struct Expr Expr;
typedef struct Stmt Stmt;
typedef struct Type Type;

typedef struct {
    Stmt   **items;
    size_t count;
    size_t capacity;
} Statements;

typedef struct {
    char *name;
    Type *type;
    SrcLoc loc;
    void *resolved_symbol;
} Param;

typedef struct {
    Param* items;
    size_t count;
    size_t capacity;
} Params;

typedef struct {
    Expr **items;
    size_t count;
    size_t capacity;
} Args;

typedef struct {
    char *name;
    Expr *value;
} EnumVariant;

typedef struct {
    EnumVariant *items;
    size_t count;
    size_t capacity;
} EnumVariants;

typedef struct {
    char *name;
    Type *type;
    Expr *value;
} StructureMember;

typedef struct {
    StructureMember *items;
    size_t count;
    size_t capacity;
} Structure;

typedef struct {
    Tokens *tokens;
    size_t current;
    Arena *arena;
} Parser;

typedef enum {
    TYPE_NAME,      // int, MyStruct, Foo
    TYPE_POINTER,   // *T
    TYPE_ARRAY,     // T[]
    TYPE_FUNCTION,  // fn(T1, T2) -> T3
    TYPE_ENUM,
    TYPE_STRUCT,
    TYPE_VARIADIC,
    TYPE_CVARIADIC,
} TypeKind;

struct Type {
    TypeKind kind;
    SrcLoc loc;

    union {
        // int, MyStruct
        struct {
            char *name;
        } named;

        // *T
        struct {
            Type *base;
        } pointer;

        // T[]
        struct {
            Type *element;
            Expr *size;
        } array;

        // fn(T1, T2) -> T3
        struct {
            Type *ret;
            Params params;
        } function;

        struct {
            EnumVariants *variants;
        } enum_type;

        struct {
            Structure *members;
        } struct_type;

        struct {
            Type *var_type;
        } variadic;
    } as;
};

struct Stmt {
    StmtType type;
    SrcLoc loc;
    void *resolved_symbol; // save the symbol from pass 2 here

    union {
        // Expression statement
        struct {
            Expr *expr;
        } expr;

        // let a = expr;
        struct {
            char *name;
            Type *type;
            Expr *value;
            // @TODO: in the future refactor this to a proper flag not just bool like this!
            bool extern_symbol;
        } let;

        struct {
            Stmt *callback;
        } defer;

        struct {
            char *name;
            Type *type;
            Expr *value;
        } const_stmt;

        // if (cond) then_b else else_b
        struct {
            Expr *condition;
            Stmt *then_b;
            Stmt *else_b;
        } if_stmt;

        // for (init; cond; inc) body
        struct {
            Stmt *init;      // can be NULL
            Expr *condition; // can be NULL
            Expr *increment; // can be NULL
            Stmt *body;
            void *created_scope; // save scope here
        } for_stmt;

        struct {
            char *name;
            EnumVariants variants;
        } enum_def;

        // @TODO: this is a really bad type name update that later
        struct {
            char *name;
            Structure members;
        } struct_def;

        // { ... }
        struct {
            Statements statements;
            void *created_scope; // save scope here
        } block;

    } as;
};

struct Expr {
    ExprType type;
    SrcLoc loc;
    void *resolved_symbol; // save the symbol from pass 2 here
    Type *resolved_type; // save the resolved type from pass 2 here

    union {
        // Literals
        uint64_t uint_val;
        double float_val;
        char *identifier;

        // Unary Op (e.g., -5, !flag, ~bits)
        struct {
            int op; // The token type (T_MIN, T_NOT, T_BIT_NOT, etc)
            Expr *right;
        } unary;

        // a[stuff]
        struct {
            Expr *object;
            Expr *index;
        } index;

        // a = expr;
        struct {
            Expr *target;
            Expr *value;
        } assign;

        // function
        struct {
            Type *ret;
            Params params;
            Stmt *body;   // must be block
        } function;

        // function call
        struct {
            Expr *callee;
            Args args;
        } call;

        // Binary Op (e.g., 5 + 5, a == b, x && y)
        struct {
            int op; // The token type (T_PLUS, T_MIN, T_EQ, T_AND, etc)
            Expr *left;
            Expr *right;
        } binary;
    } as;
};

Expr *make_expr(ExprType type, Arena *a);
Stmt *make_stmt(StmtType type, Arena *a);
Type *make_type(Arena *a, TypeKind kind);
bool make_ast(Arena *a, Statements *stmts, Tokens *t);
void print_stmt(Stmt *s, int indent);

#endif // AST_H
