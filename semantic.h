#ifndef SEMANTIC_H
#define SEMANTIC_H

#define NOB_STRIP_PREFIX
#include "nob.h"
#include "arena.h"
#include "utils.h"
#include "ast.h"
#include <stdbool.h>

typedef enum {
    TYPE_U8,
    TYPE_U16,
    TYPE_U32,
    TYPE_U64,

    TYPE_S8,
    TYPE_S16,
    TYPE_S32,
    TYPE_S64,

    TYPE_F32,
    TYPE_F64,

    TYPE_CHAR,    // alias for u8
    TYPE_BOOL,    // alias for s8
    TYPE_VOID,
    TYPE_POINTER,
    TYPE_FUNCTION,
    TYPE_ARRAY,
    TYPE_STRUCT,  // user-defined struct
    TYPE_ENUM,    // user-defined enum
    TYPE_UNKNOWN,
} TypeKind;

// Forward declaration
typedef struct Type Type;
typedef struct FunctionType FunctionType;

struct FunctionType {
    Type *return_type;
    Type **param_types;    // array of parameter types
    size_t param_count;
};

struct Type {
    TypeKind kind;
    char *name;  // original type name string (e.g., "u32", "MyStruct")

    union {
        // For TYPE_POINTER
        Type *pointee;

        // For TYPE_FUNCTION
        FunctionType *function;

        // For TYPE_ARRAY
        struct {
            Type *element_type;
            size_t size;  // 0 for dynamic arrays
        } array;

        // For user-defined types (structs, enums)
        void *user_data;
    } as;
};

typedef enum {
    SYM_VAR,
    SYM_FUNC,
    SYM_TYPE,
} Symbol_Kind;

typedef struct {
    char *name;
    Symbol_Kind kind;
    Type *type;
    bool is_const;
} Symbol;

typedef struct {
    Symbol *items;
    size_t count;
    size_t capacity;
} Symbols;

typedef struct Scope {
    Symbols symbols;
    struct Scope *parent;
} Scope;

typedef struct {
    Arena *arena;
    Scope *current_scope;
    Errors errors;
    bool had_error;
} Semantic;

// Type creation functions
Type *make_type(TypeKind kind, const char *name, Arena *a);
Type *make_pointer_type(Type *pointee, Arena *a);
Type *make_function_type(Type *return_type, Type **param_types, size_t param_count, Arena *a);
Type *make_array_type(Type *element_type, size_t size, Arena *a);

// Type parsing and lookup
Type *type_from_string(Semantic *s, const char *name);
const char *type_to_string(Type *t);

// Type comparison
bool types_equal(Type *a, Type *b);
bool type_can_assign_to(Type *value_type, Type *target_type, bool is_init);

// Type inference
Type *infer_expr_type(Semantic *s, Expr *expr);

// Scope management
void enter_scope(Semantic *s);
void leave_scope(Semantic *s);

// Symbol table operations
bool define_symbol(Semantic *s, Symbol symbol);
Symbol *lookup_symbol(Semantic *s, const char *name);

// Main semantic analysis entry point
void semantic_check(Semantic *s, Statements *p);

#endif /* SEMANTIC_H */
