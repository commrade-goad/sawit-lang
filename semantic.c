#include "semantic.h"
/*
bool semantic_check_pass_one(Semantic *s, Statements *st) {
    enter_scope(s); // global scope

    for (size_t i = 0; i < st->count; i++) {
        Stmt *stmt = st->items[i];

        switch (stmt->kind) {
        case STMT_FUNCTION: {
            Symbol sym = {0};
            sym.name = stmt->function.name;
            sym.kind = SYMBOL_FUNCTION;
            sym.sig  = build_function_signature(stmt);

            if (!define_symbol(s, sym)) {
                report_error(...);
                return false;
            }

            break;
        }

        case STMT_GLOBAL_VAR: {
            Symbol sym = {0};
            sym.name = stmt->var.name;
            sym.kind = SYMBOL_GLOBAL_VAR;
            sym.type = resolve_type(stmt->var.type);

            if (!define_symbol(s, sym)) {
                report_error(...);
                return false;
            }

            break;
        }

        case STMT_STRUCT: {
            Symbol sym = {0};
            sym.name = stmt->struct_decl.name;
            sym.kind = SYMBOL_STRUCT;

            if (!define_symbol(s, sym)) {
                report_error(...);
                return false;
            }

            break;
        }

        default:
            break;
        }
    }

    return true;
}
*/

bool semantic_check_pass_one(Semantic *s,  Statements *st) {
    enter_scope(s);

    for (size_t i = 0; i < st->count; i++) {
        Stmt *current = st->items[i];
        (void)current;
    }
    return false;
}

// @NOTE: this is create the new scope to be wary of that!
void enter_scope(Semantic *s) {
    Scope *scope = (Scope *)arena_alloc(s->arena, sizeof(Scope));
    scope->symbols = (Symbols){0};
    scope->parent = s->current_scope;
    s->current_scope = scope;
}

void leave_scope(Semantic *s) {
    // @NOTE: didnt check if the current scope is the global be wary of that
    s->current_scope = s->current_scope->parent;
}

bool define_symbol(Semantic *s, Symbol symbol) {
    Scope *scope = s->current_scope;
    // @NOTE: allow shadowing so if the symbol exist on the parent node then allow it!
    // @NOTE: i dont know if its the best idea but thats fine for now.
    // @TODO: if this getting bigger will be replaced using hashmap.
    for (size_t i = 0; i < scope->symbols.count; i++) {
        Symbol current = scope->symbols.items[i];
        if (strcmp(symbol.name, current.name) == 0) {
            // @TODO: report error that it is redifined symbol
            return false;
        }
    }
    da_append(&scope->symbols, symbol);
    return true;
}

Symbol *lookup_symbol(Semantic *s, const char *name) {
    if (!s || !name) return NULL;

    // Search from current scope up to global
    for (Scope *scope = s->current_scope; scope != NULL; scope = scope->parent) {
        for (size_t i = 0; i < scope->symbols.count; i++) {
            if (strcmp(scope->symbols.items[i].name, name) == 0) {
                return &scope->symbols.items[i];
            }
        }
    }

    return NULL;
}
