#include "semantic.h"

// Forward declarations for the recursive walkers
static bool check_stmt(Semantic *s, Stmt *st);
static bool check_expr(Semantic *s, Expr *e);
static bool check_type(Semantic *s, Type *t);

bool semantic_check_pass_one(Semantic *s,  Statements *st) {
    enter_scope(s);
    s->root_scope = s->current_scope; // Save the root scope we need this for later

    for (size_t i = 0; i < st->count; i++) {
        Stmt *current = st->items[i];

        // @NOTE: there is nof func stuff here because everything is registered as var with a function def inside it.
        switch (current->type) {
        case STMT_ENUM_DEF: {
            Symbol sym = {0};
            sym.loc = current->loc;
            sym.name = current->as.enum_def.name;
            sym.kind = SYM_TYPE;

            // We need to construc the enum type
            Type *newtype = make_type(s->arena, TYPE_ENUM);
            newtype->as.enum_type.variants = &current->as.enum_def.variants;
            sym.declared_type = newtype;
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

            // We need to construc the enum type
            Type *newtype = make_type(s->arena, TYPE_STRUCT);
            newtype->as.struct_type.members = &current->as.struct_def.members;
            sym.declared_type = newtype;
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

bool semantic_check_pass_two(Semantic *s, Statements *st) {
    bool ok = true;
    for (size_t i = 0; i < st->count; i++) {
        if (!check_stmt(s, st->items[i])) {
            ok = false;
        }
    }
    return ok;
}

bool semantic_check_pass_three(Semantic *s, Statements *st) {
    // @TODO: not yet implemented!
    bool ok = true;
    return ok;
}

// ---------------------------------------------------------------------------
// Type checker
// ---------------------------------------------------------------------------

// Verifies that any named type in the annotation actually refers to a known
// SYM_TYPE symbol (struct/enum). Primitive names like "int", "float", etc. are
// not tracked in the symbol table so we skip them – you can add an explicit
// allowlist later if you want stricter checking.
static bool check_type(Semantic *s, Type *t) {
    if (!t) return true;

    switch (t->kind) {
    case TYPE_ENUM:
    case TYPE_STRUCT:
    case TYPE_NAME: {
        Symbol *sym = lookup_symbol(s, t->as.named.name);
        if (!sym) {
            bool found = false;
            for (size_t i = 0; i < KNOWN_DEFAULT_TYPE_LEN; i++) {
                if (strcmp(KNOWN_DEFAULT_TYPE[i], t->as.named.name) == 0) {
                    found = true;
                }
            }
            if (!found) {
                log_error(t->loc, "Unknown type '%s'.", t->as.named.name);
                return false;
            }
            return true;
        }
        if (sym->kind != SYM_TYPE) {
            log_error(t->loc, "'%s' is not a type.", t->as.named.name);
            return false;
        }
    } break;
    case TYPE_POINTER:
        return check_type(s, t->as.pointer.base);
    case TYPE_ARRAY:
        return check_type(s, t->as.array.element);
    case TYPE_FUNCTION: {
        bool ok = check_type(s, t->as.function.ret);
        for (size_t i = 0; i < t->as.function.params.count; i++) {
            if (!check_type(s, t->as.function.params.items[i].type)) ok = false;
        }
        return ok;
    }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Expression walker
// ---------------------------------------------------------------------------

static bool check_expr(Semantic *s, Expr *e) {
    if (!e) return true;
    bool ok = true;

    switch (e->type) {
    // Leaves – nothing to validate
    case EXPR_LITERAL_INT:
    case EXPR_LITERAL_FLOAT:
    case EXPR_LITERAL_STRING:
        break;

    case EXPR_IDENTIFIER: {
        // Bind: look up the identifier in the current scope chain.
        Symbol *sym = lookup_symbol(s, e->as.identifier);
        if (!sym) {
            log_error(e->loc, "Undefined variable '%s'.", e->as.identifier);
            ok = false;
        }
        // @NOTE: once Expr grows a `symbol` field you can write:
        //   e->symbol = sym;
        // here to complete the binding step.
    } break;

    case EXPR_UNARY_OP:
        ok = check_expr(s, e->as.unary.right);
        break;

    case EXPR_BINARY_OP:
        ok  = check_expr(s, e->as.binary.left);
        ok &= check_expr(s, e->as.binary.right);
        break;

    case EXPR_ASSIGN:
        ok  = check_expr(s, e->as.assign.target);
        ok &= check_expr(s, e->as.assign.value);
        break;

    case EXPR_INDEX:
        ok  = check_expr(s, e->as.index.object);
        ok &= check_expr(s, e->as.index.index);
        break;

    case EXPR_CALL: {
        ok = check_expr(s, e->as.call.callee);
        for (size_t i = 0; i < e->as.call.args.count; i++) {
            if (!check_expr(s, e->as.call.args.items[i])) ok = false;
        }
    } break;

    case EXPR_FUNCTION: {
        // A function literal opens its own scope.  Parameters are registered
        // inside that scope so the body can see them.
        enter_scope(s);

        // Validate & register each parameter.
        for (size_t i = 0; i < e->as.function.params.count; i++) {
            Param *p = &e->as.function.params.items[i];

            if (!check_type(s, p->type)) ok = false;

            Symbol sym = {0};
            sym.name         = p->name;
            sym.kind         = SYM_VAR;
            sym.declared_type = p->type;
            sym.loc = p->loc;

            if (!define_symbol(s, sym)) {
                log_error(e->loc, "Duplicate parameter name '%s'.", p->name);
                ok = false;
            }
        }

        // Validate return type annotation.
        if (!check_type(s, e->as.function.ret)) ok = false;

        // Walk the body (must be STMT_BLOCK, but we let check_stmt handle it).
        if (!check_stmt(s, e->as.function.body)) ok = false;

        leave_scope(s);
    } break;
    }

    return ok;
}

// ---------------------------------------------------------------------------
// Statement walker
// ---------------------------------------------------------------------------

static bool check_stmt(Semantic *s, Stmt *st) {
    if (!st) return true;
    bool ok = true;

    switch (st->type) {

    case STMT_EXPR:
        ok = check_expr(s, st->as.expr.expr);
        break;

    case STMT_LET: {
        // Validate the declared type annotation.
        if (!check_type(s, st->as.let.type)) ok = false;

        // Check the initialiser expression first (so the variable itself is
        // not yet visible on its own RHS, preventing `let x = x`).
        if (!check_expr(s, st->as.let.value)) ok = false;

        // At the top-level the symbol was already registered by pass one.
        // Inside nested scopes (function bodies, for-loops …) we register it
        // now so subsequent statements in the same block can see it.
        if (s->current_scope != s->root_scope) {
            Symbol sym = {0};
            sym.loc          = st->loc;
            sym.name         = st->as.let.name;
            sym.kind         = SYM_VAR;
            sym.is_extern    = st->as.let.extern_symbol;
            sym.declared_type = st->as.let.type;

            if (!define_symbol(s, sym)) {
                Symbol *before_def = lookup_symbol(s, sym.name);
                log_error(st->loc, "Redefinition of let variable %s is not allowed.", sym.name);
                log_error(before_def->loc, "let variable defined here.");
                ok = false;
            }
        }
    } break;

    case STMT_CONST: {
        if (!check_type(s, st->as.const_stmt.type)) ok = false;
        if (!check_expr(s, st->as.const_stmt.value)) ok = false;

        // Same as STMT_LET: only register inside nested scopes.
        if (s->current_scope != s->root_scope) {
            Symbol sym = {0};
            sym.loc          = st->loc;
            sym.name         = st->as.const_stmt.name;
            sym.kind         = SYM_CONST;
            sym.declared_type = st->as.const_stmt.type;

            if (!define_symbol(s, sym)) {
                Symbol *before_def = lookup_symbol(s, sym.name);
                log_error(st->loc, "Redefinition of const variable %s is not allowed.", sym.name);
                log_error(before_def->loc, "const variable defined here.");
                ok = false;
            }
        }
    } break;

    case STMT_RET:
        ok = check_expr(s, st->as.expr.expr);
        break;

    case STMT_IF:
        ok  = check_expr(s, st->as.if_stmt.condition);
        ok &= check_stmt(s, st->as.if_stmt.then_b);
        ok &= check_stmt(s, st->as.if_stmt.else_b); // NULL-safe
        break;

    case STMT_FOR: {
        // The for-loop gets its own scope so that the init variable (if any)
        // is scoped to the loop.
        enter_scope(s);
        ok  = check_stmt(s, st->as.for_stmt.init);      // can be NULL
        ok &= check_expr(s, st->as.for_stmt.condition);  // can be NULL
        ok &= check_expr(s, st->as.for_stmt.increment);  // can be NULL
        ok &= check_stmt(s, st->as.for_stmt.body);
        leave_scope(s);
    } break;

    case STMT_BLOCK: {
        // A bare block gets its own scope.
        enter_scope(s);
        for (size_t i = 0; i < st->as.block.statements.count; i++) {
            if (!check_stmt(s, st->as.block.statements.items[i])) ok = false;
        }
        leave_scope(s);
    } break;

    case STMT_DEFER:
        ok = check_stmt(s, st->as.defer.callback);
        break;

    // @TODO:
    // Enum/struct definitions are pure type declarations – their bodies don't
    // contain expressions that need resolving yet.  If you later add default
    // member values to structs, walk them here.
    case STMT_ENUM_DEF: {} break;
    case STMT_STRUCT_DEF: {} break;
    }

    return ok;
}

// ---------------------------------------------------------------------------
// Scope helpers
// ---------------------------------------------------------------------------

// @NOTE: this is create the new scope to be wary of that!
void enter_scope(Semantic *s) {
    Scope *scope = (Scope *)arena_alloc(s->arena, sizeof(Scope));
    scope->symbols = (Symbols){0};
    scope->parent = s->current_scope;
    s->current_scope = scope;
}

void leave_scope(Semantic *s) {
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
