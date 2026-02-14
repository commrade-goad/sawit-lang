#ifndef SEMANTIC_H
#define SEMANTIC_H

#define NOB_STRIP_PREFIX
#include "nob.h"
#include "arena.h"
#include "utils.h"
#include "ast.h"
#include <stdbool.h>

typedef enum {
    TYPE_S8,
    TYPE_S16,
    TYPE_S32,
    TYPE_S64,
    TYPE_U8,
    TYPE_U16,
    TYPE_U32,
    TYPE_U64,
    TYPE_F32,
    TYPE_F64,
    TYPE_BOOL,     // alias for s8, 0=false, other=true
    TYPE_VOID,     // for functions that don't return
    TYPE_POINTER,  // pointer to another type
    TYPE_STRUCT,   // user-defined struct
    TYPE_ENUM,     // user-defined enum
    TYPE_UNKNOWN,  // for error cases
} TypeKind;

typedef struct Type {
    TypeKind kind;
    char *name;  // original name string
    struct Type *pointee;  // for TYPE_POINTER: what it points to
    void *user_data;  // for TYPE_STRUCT/TYPE_ENUM: definition data
} Type;

Type *make_type(TypeKind kind, const char *name, Arena *a);
Type *type_from_string(const char *name, Arena *a);
const char *type_to_string(Type *t);
bool types_equal(Type *a, Type *b);

typedef enum {
    SYM_VAR,
    SYM_FUNC,
    SYM_TYPE,
    SYM_STRUCT,
    SYM_ENUM,
} Symbol_Kind;

typedef struct {
    char *name;
    Symbol_Kind kind;
    Type *type;
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
    int in_loop;  // counter for nested loops
    
    // Template/generic type tracking
    Statements *template_defs;       // Store generic struct definitions
    Statements *generated_instances; // Store generated concrete types
} Semantic;

void enter_scope(Semantic *s);
void leave_scope(Semantic *s);

bool define_symbol(Semantic *s, Symbol symbol);
Symbol *lookup_symbol(Semantic *s, const char *name);

Type *infer_expr_type(Semantic *s, Expr *expr);
bool check_block_has_return(Stmt *block);

// Template/generic instantiation
Stmt *instantiate_template(Semantic *s, Stmt *template_def, TypeParams *type_args);
char *build_instantiated_name(const char *base_name, TypeParams *type_args, Arena *a);

void semantic_check(Semantic *s, Statements *p);

#endif /* SEMANTIC_H */
