#include "semantic.h"

bool semantic_check_pass_one(Semantic *s,  Statements *st) {
    enter_scope(s);

    for (size_t i = 0; i < st->count; i++) {
        Stmt *current = st->items[i];

        // @NOTE: there is nof func stuff here because everything is registered as var with a function def inside it.
        switch (current->type) {
        // @TODO: Enum
        case STMT_ENUM_DEF: {
            Symbol sym = {0};
            sym.loc = current->loc;
            sym.name = current->as.enum_def.name;
            sym.kind = SYM_TYPE;
            sym.declared_type = NULL; // @TODO: idk i dont have any struct type so i just give it NULL and if you need the
                                      // type just get the name, but in the future maybe we need new type called type.
            sym.is_extern = false; // @NOTE: maybe will support extern in the future

            if (!define_symbol(s, sym)) {
                Symbol *before_def = lookup_symbol(s, sym.name);
                log_error(current->loc, "Redefinition of enum %s is not allowed.",
                          sym.name);
                log_error(before_def->loc, "enum defined here.");
                continue;
            }
        } break;
        case STMT_STRUCT_DEF: {
            Symbol sym = {0};
            sym.loc = current->loc;
            sym.name = current->as.struct_def.name;
            sym.kind = SYM_TYPE;
            sym.declared_type = NULL; // @TODO: idk i dont have any struct type so i just give it NULL and if you need the
                                      // type just get the name, but in the future maybe we need new type called type.
            sym.is_extern = false; // @NOTE: maybe will support extern in the future

            if (!define_symbol(s, sym)) {
                Symbol *before_def = lookup_symbol(s, sym.name);
                log_error(current->loc, "Redefinition of struct %s is not allowed.",
                          sym.name);
                log_error(before_def->loc, "struct defined here.");
                continue;
            }
        } break;
        case STMT_CONST: {
            Symbol sym = {0};
            sym.loc = current->loc;
            sym.name = current->as.const_stmt.name;
            sym.kind = SYM_CONST;
            sym.is_extern = false; // @NOTE: const cannot be an extern
            sym.declared_type = current->as.const_stmt.type;

            if (!define_symbol(s, sym)) {
                Symbol *before_def = lookup_symbol(s, sym.name);
                log_error(current->loc, "Redefinition of const variable %s is not allowed.",
                          sym.name);
                log_error(before_def->loc, "const variable defined here.");
                continue;
            }
        } break;
        case STMT_LET: {
            Symbol sym = {0};
            sym.loc = current->loc;
            sym.name = current->as.let.name;
            sym.kind = SYM_VAR;
            sym.is_extern = current->as.let.extern_symbol;
            sym.declared_type = current->as.let.type;

            if (!define_symbol(s, sym)) {
                Symbol *before_def = lookup_symbol(s, sym.name);
                log_error(current->loc, "Redefinition of let variable %s is not allowed.",
                          sym.name);
                log_error(before_def->loc, "let variable defined here.");
                continue;
            }
        } break;
        default: {} break;
        }
    }
    return true;
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

    for (Scope *scope = s->current_scope; scope != NULL; scope = scope->parent) {
        for (size_t i = 0; i < scope->symbols.count; i++) {
            if (strcmp(scope->symbols.items[i].name, name) == 0) {
                return &scope->symbols.items[i];
            }
        }
    }

    return NULL;
}
