#ifndef AST_H
#define AST_H

#include <stdlib.h>
#include <stdint.h>
#include "lexer.h"
#include "arena.h"
#include "utils.h"

typedef enum {
    AST_LITERAL_INT,
    AST_LITERAL_FLOAT,
    AST_LITERAL_STRING,
    AST_LITERAL_NULL,  // null literal
    AST_IDENTIFIER,
    AST_UNARY_OP,      // something like -5 +2 *a so operator to 1 num
    AST_BINARY_OP,     // lhs [SOMETHING] rhs
    AST_ASSIGN,
    AST_FUNCTION,
    AST_FUNCTION_TYPE, // fn(params) -> ret
    AST_CALL,
    AST_BLOCK_EXPR,    // block as expression (must have return)
    AST_CAST,          // cast(type, expr)
    AST_SIZEOF,        // sizeof(type or expr)
    AST_ADDR_OF,       // &expr
    AST_DEREF,         // *expr
    AST_STRUCT_INIT,   // TypeName{field1: val1, ...}
    AST_MEMBER_ACCESS, // expr.member
} ExprType;

typedef enum {
    STMT_EXPR,     // expression as statement: a + 5;
    STMT_LET,      // let a = expr;
    STMT_RET,      // return
    STMT_BLOCK,    // { stmt1; stmt2; }
    STMT_IF,       // if (cond) { ... } else { ... }
    STMT_FOR,      // for (init; cond; inc) { ... }
    STMT_BREAK,    // break;
    STMT_CONTINUE, // continue;
    STMT_STRUCT_DEF, // Name :: type { fields }
    STMT_ENUM_DEF,   // Name :: enum { variants }
} StmtType;

typedef struct Expr Expr;
typedef struct Stmt Stmt;

typedef struct {
    Stmt   **items;
    /* Stmt   *items; */ /* did we need to own this? */
    size_t count;
    size_t capacity;
} Statements;

typedef struct {
    char *name;
    char *type;
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
    char *type;
    bool is_method;      // Is this a function pointer (method)?
    Params method_params; // If method, its parameters
    char *method_return;  // If method, return type
} StructField;

typedef struct {
    StructField *items;
    size_t count;
    size_t capacity;
} StructFields;

typedef struct {
    char *name;
    Expr *value;
} FieldInit;

typedef struct {
    FieldInit *items;
    size_t count;
    size_t capacity;
} FieldInits;

typedef struct {
    char *name;  // Type parameter like "T"
} TypeParam;

typedef struct {
    TypeParam *items;
    size_t count;
    size_t capacity;
} TypeParams;

typedef struct {
    char *name;
    int64_t value;       // tag value
    bool has_value;
    char *data_type;     // For union: type of data this variant holds (can be NULL)
} EnumVariant;

typedef struct {
    EnumVariant *items;
    size_t count;
    size_t capacity;
} EnumVariants;

typedef struct {
    Tokens *tokens;
    size_t current;
    Arena *arena;
} Parser;

struct Stmt {
    StmtType type;
    SrcLoc loc;

    union {
        // Expression statement
        struct {
            Expr *expr;
        } expr;

        // let a = expr;
        struct {
            char *name;
            char *type;
            Expr *value;
        } let;

        // { ... }
        struct {
            Statements statements;
        } block;

        // if (cond) then_branch else else_branch
        struct {
            Expr *condition;
            Stmt *then_branch;
            Stmt *else_branch;  // can be NULL
        } if_stmt;

        // for (init; cond; inc) body
        struct {
            Stmt *init;         // can be NULL
            Expr *condition;    // can be NULL
            Expr *increment;    // can be NULL
            Stmt *body;
        } for_stmt;

        // struct definition: Name :: type<T> { fields } or Name :: type { fields }
        struct {
            char *name;
            TypeParams type_params;  // for generics like <T>
            StructFields fields;
        } struct_def;

        // enum definition: Name :: enum { variants }
        struct {
            char *name;
            EnumVariants variants;
        } enum_def;

    } as;
};

struct Expr {
    ExprType type;
    SrcLoc loc;

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

        // function
        struct {
            char *ret;
            Params params;
            Stmt *body;   // must be block
        } function;

        // function call
        struct {
            Expr *callee;
            Args args;
        } call;

        // Binary Op (e.g., 5 + 5)
        struct {
            int op; // The token type (T_PLUS, T_MIN, etc)
            Expr *left;
            Expr *right;
        } binary;

        // Block as expression (must have return)
        struct {
            Statements statements;
        } block_expr;

        // cast(type, expr)
        struct {
            char *target_type;
            Expr *expr;
        } cast;

        // sizeof(type or expr)
        struct {
            char *type_name;   // if sizeof type
            Expr *expr;        // if sizeof expr
        } size_of;

        // &expr
        struct {
            Expr *expr;
        } addr_of;

        // *expr
        struct {
            Expr *expr;
        } deref;

        // TypeName{field1: val1, ...}
        struct {
            char *type_name;
            TypeParams type_args;  // for generics like Vec<s32>
            FieldInits field_inits;
        } struct_init;

        // expr.member
        struct {
            Expr *object;
            char *member_name;
        } member_access;

        // fn(params) -> ret
        struct {
            Params params;
            char *return_type;
        } function_type;
    } as;
};

Expr *make_expr(ExprType type, Arena *a);
Stmt *make_stmt(StmtType type, Arena *a);
bool make_ast(Arena *a, Statements *stmts, Tokens *t);
void print_stmt(Stmt *s, int indent);

#endif // EXPR_H
