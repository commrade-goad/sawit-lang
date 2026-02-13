#include "semantic.h"
#include "ast.h"
static void analyze_stmt(Semantic *s, Stmt *stmt);
static void analyze_expr(Semantic *s, Expr *expr);

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
    for (size_t i = 0; i < scope->symbols.count; i++) {
        if (strcmp(scope->symbols.items[i].name, symbol.name) == 0) {
            return false;
        }
    }
    da_append(&scope->symbols, symbol);
    return true;
}

Symbol *lookup_symbol(Semantic *s, const char *name) {
    if (!s || !name) return NULL;

    for (Scope *scope = s->current_scope;
         scope != NULL;
         scope = scope->parent) {

        for (size_t i = 0; i < scope->symbols.count; i++) {
            if (strcmp(scope->symbols.items[i].name, name) == 0) {
                return &scope->symbols.items[i];
            }
        }
    }

    return NULL;
}

static void analyze_stmt(Semantic *s, Stmt *stmt) {
    switch (stmt->type) {

    case STMT_LET: {
        if (stmt->as.let.value) {
            analyze_expr(s, stmt->as.let.value);
        }

        Symbol sym = {0};
        sym.name = stmt->as.let.name;
        sym.kind = SYM_VAR;
        sym.type = NULL; // @TODO: do type checkin

        if (!define_symbol(s, sym)) {
            // @TODO: make this prettier
            printf("Semantic error: duplicate variable '%s'\n", sym.name);
            s->had_error = true;
        }

    } break;

    case STMT_BLOCK: {
        enter_scope(s);

        for (size_t i = 0; i < stmt->as.block.statements.count; i++) {
            analyze_stmt(s, stmt->as.block.statements.items[i]);
        }

        leave_scope(s);

    } break;

    case STMT_EXPR:
        analyze_expr(s, stmt->as.expr.expr);
        break;
    // @TODO: handle more stmt

    default:
        break;
    }
}

static void analyze_expr(Semantic *s, Expr *expr) {
    switch (expr->type) {

    case AST_IDENTIFIER: {
        Symbol *sym = lookup_symbol(s, expr->as.identifier);

        if (!sym) {
            printf("Semantic error: undefined identifier '%s'\n",
                   expr->as.identifier);
            s->had_error = true;
        }
    } break;

    case AST_BINARY_OP:
        analyze_expr(s, expr->as.binary.left);
        analyze_expr(s, expr->as.binary.right);
        break;

    case AST_CALL:
        analyze_expr(s, expr->as.call.callee);

        for (size_t i = 0; i < expr->as.call.args.count; i++) {
            analyze_expr(s, expr->as.call.args.items[i]);
        }
        break;

    case AST_FUNCTION: {
        // function introduces new scope
        enter_scope(s);

        // define parameters
        for (size_t i = 0; i < expr->as.function.params.count; i++) {
            Param *p = &expr->as.function.params.items[i];

            Symbol sym = {0};
            sym.name = p->name;
            sym.kind = SYM_VAR;
            sym.type = NULL;

            if (!define_symbol(s, sym)) {
                printf("Semantic error: duplicate parameter '%s'\n", p->name);
                s->had_error = true;
            }
        }

        // analyze body
        analyze_stmt(s, expr->as.function.body);

        leave_scope(s);

    } break;

    default:
        break;
    }
}

void semantic_check(Semantic *s, Statements *p) {
    enter_scope(s); // global scope

    for (size_t i = 0; i < p->count; i++) {
        analyze_stmt(s, p->items[i]);
    }

    leave_scope(s);
}
