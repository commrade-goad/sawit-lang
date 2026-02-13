#ifndef SEMANTIC_H
#define SEMANTIC_H

#define NOB_STRIP_PREFIX
#include "nob.h"
#include "arena.h"
#include <stdbool.h>

typedef struct {
    String_View *items;
    size_t count;
    size_t capacity;
} Errors;

typedef struct Type {
    char *name;  // "s32", "void", "MyStruct", etc.
} Type;

typedef enum {
    SYM_VAR,
    SYM_FUNC,
    SYM_TYPE,
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
    Arena *a;
    Scope *current_scope;
    Errors errors;
    bool had_error;
} Semantic;

void semantic_init(Semantic *s);

void enter_scope(Semantic *s);
void leave_scope(Semantic *s);

bool define_symbol(Semantic *s, Symbol symbol);
Symbol *lookup_symbol(Semantic *s, const char *name);

#endif /* SEMANTIC_H */
