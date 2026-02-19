#ifndef SEMANTIC_H
#define SEMANTIC_H

#define NOB_STRIP_PREFIX
#include "nob.h"
#include "arena.h"
#include "utils.h"
#include "ast.h"
#include <stdbool.h>

typedef struct Type Type;

#define KNOWN_DEFAULT_TYPE_LEN 12
static const char *KNOWN_DEFAULT_TYPE[] = {
    "s8", "s16", "s32", "s64",
    "u8", "u16", "u32", "u64",
    "bool", "char", "null", "VARIADIC"
};

typedef enum {
    SYM_VAR,
    SYM_CONST,
    SYM_TYPE,
} Symbol_Kind;

typedef struct {
    char *name;
    Symbol_Kind kind;
    bool is_extern;      // @NOTE: only used on func
    Type *declared_type;
    SrcLoc loc;
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
    Scope *root_scope;
    Scope *current_scope;
    Errors errors;
} Semantic;

bool semantic_check_pass_one(Semantic *s, Statements *st);
bool semantic_check_pass_two(Semantic *s,  Statements *st);
bool semantic_check_pass_three(Semantic *s, Statements *st);

void enter_scope(Semantic *s);
void leave_scope(Semantic *s);
bool define_symbol(Semantic *s, Symbol symbol);
Symbol *lookup_symbol(Semantic *s, const char *name);

#endif /* SEMANTIC_H */
