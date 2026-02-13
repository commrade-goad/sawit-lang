#include "ast.h"
#include "lexer.h"

#define BIGGEST_POWER 40
#define LOC_FORMAT "%s:%lu:%lu: "
#define LOC_DATA(data) data->loc.name, data->loc.line, data->loc.col

static Token *peek(Parser *p) {
    return &p->tokens->items[p->current];
}

static Token *previous(Parser *p) {
    return &p->tokens->items[p->current - 1];
}

static bool is_at_end(Parser *p) {
    return peek(p)->tk == T_EOF;
}

static Token *advance(Parser *p) {
    if (!is_at_end(p)) p->current++;
    return previous(p);
}

static bool check(Parser *p, TokenKind kind) {
    if (is_at_end(p)) return false;
    return peek(p)->tk == kind;
}

static bool match(Parser *p, TokenKind kind) {
    if (check(p, kind)) {
        advance(p);
        return true;
    }
    return false;
}

static void expect(Parser *p, TokenKind kind) {
    Token *cr = peek(p);
    if (!match(p, kind)) {
        char *stuff = temp_sprintf(LOC_FORMAT"Expected token %s, got %s", LOC_DATA(cr), get_token_str(kind), get_token_str(cr->tk));
        da_append(&p->errors, strdup(stuff));
    }
}

Expr *make_expr(ExprType type, Arena *a) {
    Expr *n = (Expr *)arena_alloc(a, sizeof(Expr));
    n->type = type;
    return n;
}

Stmt *make_stmt(StmtType type, Arena *a) {
    Stmt *n = (Stmt *)arena_alloc(a, sizeof(Stmt));
    n->type = type;
    return n;
}

static int infix_binding_power(TokenKind tk, int *left_bp, int *right_bp) {
    switch (tk) {
    case T_PLUS:
    case T_MIN:
        *left_bp = 10;
        *right_bp = 11;
        return 1;

    case T_STAR:
    case T_DIV:
        *left_bp = 20;
        *right_bp = 21;
        return 1;

    case T_EQUAL:
        *left_bp = 5;
        *right_bp = 4;
        return 1;

    case T_OPARENT:
        *left_bp = 30;
        *right_bp = 31;
        return 1;

    default:
        return 0;
    }
}

static Stmt *parse_statement(Parser *p);
static Stmt *parse_return(Parser *p, Token *kw);
static Stmt *parse_let(Parser *p, Token *kw);
static Stmt *parse_block(Parser *p, Token *kw);

static Expr *parse_expression(Parser *p, int min_bp) {
    Expr *lhs;

    // Prefix
    Token *tok = advance(p);

    switch (tok->tk) {

    case T_NUM: {
        lhs = make_expr(AST_LITERAL_INT, p->arena);
        lhs->loc = tok->loc;
        lhs->as.uint_val = tok->data.Uint64;
    } break;

    case T_FLO: {
        lhs = make_expr(AST_LITERAL_FLOAT, p->arena);
        lhs->loc = tok->loc;
        lhs->as.float_val = tok->data.F64;
    } break;

    case T_STR: {
        lhs = make_expr(AST_LITERAL_STRING, p->arena);
        lhs->loc = tok->loc;
        lhs->as.identifier = tok->data.String;
    } break;
    case T_IDENT: {
        lhs = make_expr(AST_IDENTIFIER, p->arena);
        lhs->loc = tok->loc;
        lhs->as.identifier = tok->data.String;
    } break;

    case T_OPARENT: {
        size_t saved = p->current;
        bool is_param_list = true;
        Params params = {0};
        Token *before = previous(p);

        if (!check(p, T_CPARENT)) {
            do {
                if (!check(p, T_IDENT)) {
                    is_param_list = false;
                    break;
                }

                Token *name = advance(p);
                Token *typename = NULL;

                if (check(p, T_IDENT)) {
                    typename = peek(p);
                    advance(p);
                } else {
                    char *stuff = temp_sprintf(LOC_FORMAT"Expecting the parameter type after the name.", LOC_DATA(name));
                    da_append(&p->errors, strdup(stuff));
                }
                Param p = {
                    .name = name->data.String,
                    .type = typename ? typename->data.String : "any"
                };
                da_append(&params, p);

            } while (match(p, T_COMMA));
        }
        if (is_param_list && match(p, T_CPARENT) && check(p, T_DCOLON)) {
            advance(p); // consume ::

            Token *retval = NULL;

            // consume ident ret
            if (check(p, T_IDENT)) {
                retval = peek(p);
                advance(p);
            }

            expect(p, T_OCPARENT);
            Token *kw = previous(p);
            Stmt *body = parse_block(p, kw);

            lhs = make_expr(AST_FUNCTION, p->arena);
            if (retval) {
                lhs->as.function.ret = retval->data.String;
            } else {
                lhs->as.function.ret = "any";
            }
            lhs->as.function.body = body;
            lhs->as.function.params = params;
            lhs->loc = before->loc;
        } else {
            // rollback
            p->current = saved;

            lhs = parse_expression(p, 0);
            expect(p, T_CPARENT);
        }
    } break;

    case T_MIN: { // unary minus
        Expr *rhs = parse_expression(p, BIGGEST_POWER);

        lhs = make_expr(AST_UNARY_OP, p->arena);
        lhs->loc = tok->loc;
        lhs->as.unary.op = T_MIN;
        lhs->as.unary.right = rhs;
    } break;

    default: {
        Token current_token = p->tokens->items[p->current];

        char *stuff = temp_sprintf(LOC_FORMAT"Unexpected token in expression: %s", LOC_DATA((&current_token)), get_token_str(current_token.tk));
        da_append(&p->errors, strdup(stuff));
    } break;
    }

    // Infix Loop
    while (1) {
        Token *next = peek(p);
        int left_bp, right_bp;
        if (!infix_binding_power(next->tk, &left_bp, &right_bp))
            break;

        if (left_bp < min_bp)
            break;

        if (next->tk == T_OPARENT) {
            Token *before = previous(p);
            advance(p); // consume '('

            Args args = {0};

            if (!check(p, T_CPARENT)) {
                do {
                    Expr *arg = parse_expression(p, 0);
                    da_append(&args, arg);
                } while (match(p, T_COMMA));
            }

            expect(p, T_CPARENT);

            Expr *call = make_expr(AST_CALL, p->arena);
            call->as.call.callee = lhs;
            call->as.call.args = args;
            call->loc = before->loc;

            lhs = call;
            continue;
        }

        TokenKind op = next->tk;
        advance(p);
        Expr *rhs = parse_expression(p, right_bp);

        if (op == T_EQUAL) {
            if (lhs->type != AST_IDENTIFIER) {
                // @TODO: change this to the new stuff
                fprintf(stderr, "Invalid assignment target\n");
                exit(1);
            }

            Expr *assign = make_expr(AST_ASSIGN, p->arena);
            assign->as.assign.name = lhs->as.identifier;
            assign->as.assign.value = rhs;
            assign->loc = next->loc;

            lhs = assign;
            continue;
        }

        Expr *bin = make_expr(AST_BINARY_OP, p->arena);
        bin->as.binary.op = next->tk;
        bin->as.binary.left = lhs;
        bin->as.binary.right = rhs;
        bin->loc = next->loc;

        lhs = bin;
    }

    return lhs;
}

static Stmt *parse_return(Parser *p, Token *kw) {
    Stmt *stmt = make_stmt(STMT_RET, p->arena);
    stmt->loc = kw->loc;

    if (!check(p, T_CLOSING)) {
        stmt->as.expr.expr = parse_expression(p, 0);
    } else {
        stmt->as.expr.expr = NULL;
    }

    expect(p, T_CLOSING);
    return stmt;
}

static Stmt *parse_let(Parser *p, Token *kw) {
    Token *name = peek(p);
    expect(p, T_IDENT);

    Token *lettype = NULL;
    if (check(p, T_IDENT)) {
        lettype = peek(p);
        expect(p, T_IDENT);
    }

    if (!check(p, T_EQUAL)) {
        Token *cr = peek(p);
        advance(p);
        char *stuff = temp_sprintf(LOC_FORMAT"Uninitialized let bindings is not supported!", LOC_DATA(cr));
        da_append(&p->errors, strdup(stuff));
        return NULL;
    }
    expect(p, T_EQUAL);

    Expr *value = parse_expression(p, 0);

    expect(p, T_CLOSING);

    Stmt *stmt = make_stmt(STMT_LET, p->arena);
    stmt->loc = kw->loc;
    stmt->as.let.name = name->data.String;
    if (lettype) {
        stmt->as.let.type = lettype->data.String;
    } else {
        stmt->as.let.type = "any";
    }
    stmt->as.let.value = value;

    return stmt;
}

static Stmt *parse_block(Parser *p, Token *kw) {
    Stmt *block = make_stmt(STMT_BLOCK, p->arena);

    block->as.block.statements.items = NULL;
    block->as.block.statements.count = 0;
    block->as.block.statements.capacity = 0;

    block->loc = kw->loc;

    while (!check(p, T_CCPARENT) && !is_at_end(p)) {
        Stmt *stmt = parse_statement(p);
        da_append(&block->as.block.statements, stmt);
    }

    expect(p, T_CCPARENT);

    return block;
}

static Stmt *parse_statement(Parser *p) {
    if (match(p, T_LET)) {
        Token *kw = previous(p);
        return parse_let(p, kw);
    }

    if (match(p, T_RETURN)) {
        Token *kw = previous(p);
        return parse_return(p, kw);
    }

    if (match(p, T_OCPARENT)) {
        Token *kw = previous(p);
        return parse_block(p, kw);
    }

    Expr *expr = parse_expression(p, 0);
    expect(p, T_CLOSING);

    Stmt *stmt = make_stmt(STMT_EXPR, p->arena);
    stmt->loc = expr->loc;
    stmt->as.expr.expr = expr;
    return stmt;
}

static void print_indent(int n) {
    for (int i = 0; i < n; i++) printf("  ");
}

void print_expr(Expr *e, int indent) {
    if (!e) return;

    print_indent(indent);

    switch (e->type) {
    case AST_LITERAL_INT:
        printf("INT(%lu)\n", e->as.uint_val);
        break;

    case AST_LITERAL_FLOAT:
        printf("FLOAT(%f)\n", e->as.float_val);
        break;

    // @NOTE: this is after the quote so yeah be mindful of this
    case AST_LITERAL_STRING:
        printf("STRING(%s)\n", e->as.identifier);
        break;

    case AST_IDENTIFIER:
        printf("IDENT(%s)\n", e->as.identifier);
        break;

    case AST_UNARY_OP:
        printf("UNARY(op=%d)\n", e->as.unary.op);
        print_expr(e->as.unary.right, indent + 1);
        break;

    case AST_BINARY_OP:
        printf("BINARY(op=%d)\n", e->as.binary.op);
        print_expr(e->as.binary.left, indent + 1);
        print_expr(e->as.binary.right, indent + 1);
        break;
    case AST_ASSIGN:
        printf("ASSIGN(%s)\n", e->as.assign.name);
        print_expr(e->as.assign.value, indent + 1);
        break;

    case AST_FUNCTION:
        printf("FUNCTION(%s)\n", e->as.function.ret);
        for (size_t i = 0; i < e->as.function.params.count; i++) {
            print_indent(indent + 1);
            Param p = e->as.function.params.items[i];
            printf("PARAM(%s: %s)\n", p.name, p.type);
        }
        print_stmt(e->as.function.body, indent + 1);
        break;

    case AST_CALL:
        printf("CALL\n");
        print_expr(e->as.call.callee, indent + 1);
        for (size_t i = 0; i < e->as.call.args.count; i++) {
            print_expr(e->as.call.args.items[i], indent + 1);
        }
        break;
    }
}

void print_stmt(Stmt *s, int indent) {
    print_indent(indent);

    switch (s->type) {
    case STMT_RET:
        printf("RET\n");
        if (s->as.expr.expr) print_expr(s->as.expr.expr, indent + 1);
        break;
    case STMT_LET:
        printf("LET %s: %s\n", s->as.let.name, s->as.let.type);
        print_expr(s->as.let.value, indent + 1);
        break;

    case STMT_EXPR:
        printf("EXPR_STMT\n");
        print_expr(s->as.expr.expr, indent + 1);
        break;

    case STMT_BLOCK:
        printf("BLOCK\n");
        for (size_t i = 0; i < s->as.block.statements.count; i++) {
            print_stmt(s->as.block.statements.items[i], indent + 1);
        }
        break;
    }
}

bool make_ast(Arena *a, Statements *stmts, Tokens *t) {
    Parser p = {0};
    p.tokens = t;
    p.current = 0;
    p.arena = a;

    while (!is_at_end(&p)) {
        Stmt *stmt = parse_statement(&p);
        da_append(stmts, stmt);
    }
    if (p.errors.count > 0) {
        for(size_t i = 0; i < p.errors.count; i++) {
            perr("%s", p.errors.items[i]);
            free(p.errors.items[i]);
        }
        return false;
    }
    return true;
}
