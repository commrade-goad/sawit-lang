#include "ast.h"
#include "lexer.h"

#define BIGGEST_POWER 40

#define EXPECT_EXIT(p, toktype) \
    do { if(!expect((p), (toktype))) return NULL; } while(0)

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

static bool expect(Parser *p, TokenKind kind) {
    Token *cr = peek(p);
    if (!match(p, kind)) {
        log_error(cr->loc, "Expected token %s, got %s", get_token_str(kind), get_token_str(cr->tk));
        return false;
    }
    return true;
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
    // Logical OR (lowest precedence)
    case T_OR:
        *left_bp = 1;
        *right_bp = 2;
        return 1;

    // Logical AND
    case T_AND:
        *left_bp = 3;
        *right_bp = 4;
        return 1;

    // Bitwise OR
    case T_BIT_OR:
        *left_bp = 5;
        *right_bp = 6;
        return 1;

    // Bitwise XOR
    case T_BIT_XOR:
        *left_bp = 7;
        *right_bp = 8;
        return 1;

    // Bitwise AND
    case T_BIT_AND:
        *left_bp = 9;
        *right_bp = 10;
        return 1;

    // Equality
    case T_EQ:
    case T_NEQ:
        *left_bp = 11;
        *right_bp = 12;
        return 1;

    // Comparison
    case T_LT:
    case T_GT:
    case T_LTE:
    case T_GTE:
        *left_bp = 13;
        *right_bp = 14;
        return 1;

    // Bit shifts
    case T_LSHIFT:
    case T_RSHIFT:
        *left_bp = 15;
        *right_bp = 16;
        return 1;

    // Addition/Subtraction
    case T_PLUS:
    case T_MIN:
        *left_bp = 17;
        *right_bp = 18;
        return 1;

    // Multiplication/Division/Modulo
    case T_STAR:
    case T_DIV:
    case T_MOD:
        *left_bp = 19;
        *right_bp = 20;
        return 1;

    // Assignment (right associative)
    case T_EQUAL:
        *left_bp = 2;
        *right_bp = 1;
        return 1;

    // Function call
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
static Stmt *parse_if(Parser *p, Token *kw);
static Stmt *parse_for(Parser *p, Token *kw);
static Stmt *parse_enum_def(Parser *p, Token *name_tok);

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
    case T_FALSE: {
        lhs = make_expr(AST_LITERAL_INT, p->arena);
        lhs->loc = tok->loc;
        lhs->as.uint_val = 0;
    } break;
    case T_TRUE: {
        lhs = make_expr(AST_LITERAL_INT, p->arena);
        lhs->loc = tok->loc;
        lhs->as.uint_val = 1;
    } break;

    case T_FN: {
        Token *before = peek(p);
        Params params = {0};
        EXPECT_EXIT(p, T_OPARENT);
        if (!check(p, T_CPARENT)) {
            do {
                if (!check(p, T_IDENT)) {
                    log_error(peek(p)->loc, "Expecting the parameter to be identifier not %s.", get_token_str(peek(p)->tk));
                    return NULL;
                }

                Token *name = advance(p);
                Token *typename = NULL;

                if (check(p, T_IDENT)) {
                    typename = peek(p);
                    advance(p);
                } else {
                    log_error(name->loc, "Expecting the parameter type after the name.");
                    return NULL;
                }
                Param p = {
                    .name = name->data.String,
                    .type = typename ? typename->data.String : "comptimecheck"
                };
                da_append(&params, p);
            } while (match(p, T_COMMA));
        }

        if (match(p, T_CPARENT) && check(p, T_ARROW)) {
            advance(p);
            Token *retval = peek(p);

            if (check(p, T_IDENT)) {
                advance(p);
            } else {
                if (!retval) return NULL;
                log_error(retval->loc, "Expected a return value instead got %s", get_token_str(retval->tk));
                return NULL;
            }

            EXPECT_EXIT(p, T_OCPARENT);
            Token *kw = previous(p);
            Stmt *body = parse_block(p, kw);

            lhs = make_expr(AST_FUNCTION, p->arena);
            if (retval) {
                lhs->as.function.ret = retval->data.String;
            } else {
                lhs->as.function.ret = "comptimecheck";
            }
            lhs->as.function.body = body;
            lhs->as.function.params = params;
            lhs->loc = before->loc;
        } else {
            log_error(peek(p)->loc, "Unexpected token in expression %s expected %s", get_token_str(peek(p)->tk), get_token_str(T_ARROW));
            return NULL;
        }
    } break;

    case T_OPARENT: {
        lhs = parse_expression(p, 0);
        if (!lhs) return NULL;
        EXPECT_EXIT(p, T_CPARENT);
    } break;

    // Unary operators
    case T_MIN:
    case T_NOT:
    case T_BIT_NOT: {
        Expr *rhs = parse_expression(p, BIGGEST_POWER);
        if (!rhs) return NULL;

        lhs = make_expr(AST_UNARY_OP, p->arena);
        lhs->loc = tok->loc;
        lhs->as.unary.op = tok->tk;
        lhs->as.unary.right = rhs;
    } break;

    default: {
        Token current_token = p->tokens->items[p->current];
        log_error(current_token.loc, "Unexpected token in expression: %s", get_token_str(current_token.tk));
        return NULL;
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
                    if (!arg) return NULL;
                    da_append(&args, arg);
                } while (match(p, T_COMMA));
            }

            EXPECT_EXIT(p, T_CPARENT);

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
        if (!rhs) return NULL;

        if (op == T_EQUAL) {
            if (lhs->type != AST_IDENTIFIER) {
                log_error(lhs->loc, "Invalid assignment target expected identifier.");
                return NULL;
            }

            Expr *assign = make_expr(AST_ASSIGN, p->arena);
            assign->as.assign.name = lhs->as.identifier;
            assign->as.assign.value = rhs;
            assign->loc = next->loc;

            lhs = assign;
            continue;
        }

        Expr *bin = make_expr(AST_BINARY_OP, p->arena);
        bin->as.binary.op = op;
        bin->as.binary.left = lhs;
        bin->as.binary.right = rhs;
        bin->loc = next->loc;

        lhs = bin;
    }

    return lhs;
}

static Stmt *parse_if(Parser *p, Token *kw) {
    Stmt *stmt = make_stmt(STMT_IF, p->arena);
    stmt->loc = kw->loc;

    EXPECT_EXIT(p, T_OPARENT);
    stmt->as.if_stmt.condition = parse_expression(p, 0);
    if (!stmt->as.if_stmt.condition) return NULL;
    EXPECT_EXIT(p, T_CPARENT);

    stmt->as.if_stmt.then_b = parse_statement(p);
    if (!stmt->as.if_stmt.then_b) return NULL;

    stmt->as.if_stmt.else_b = NULL;
    if (match(p, T_ELSE)) {
        stmt->as.if_stmt.else_b = parse_statement(p);
        if (!stmt->as.if_stmt.else_b) return NULL;
    }

    return stmt;
}

static Stmt *parse_for(Parser *p, Token *kw) {
    Stmt *stmt = make_stmt(STMT_FOR, p->arena);
    stmt->loc = kw->loc;

    EXPECT_EXIT(p, T_OPARENT);

    // Init (optional)
    stmt->as.for_stmt.init = NULL;
    if (!check(p, T_CLOSING)) {
        // Check if it's a let statement
        if (check(p, T_LET)) {
            Token *let_tok = advance(p);
            stmt->as.for_stmt.init = parse_let(p, let_tok);
            if (!stmt->as.for_stmt.init) return NULL;
            // parse_let already consumed the semicolon
        } else {
            // It's an expression statement
            Expr *init_expr = parse_expression(p, 0);
            if (!init_expr) return NULL;

            Stmt *init_stmt = make_stmt(STMT_EXPR, p->arena);
            init_stmt->loc = init_expr->loc;
            init_stmt->as.expr.expr = init_expr;
            stmt->as.for_stmt.init = init_stmt;

            EXPECT_EXIT(p, T_CLOSING);
        }
    } else {
        advance(p); // consume ';'
    }

    // Condition (optional)
    stmt->as.for_stmt.condition = NULL;
    if (!check(p, T_CLOSING)) {
        stmt->as.for_stmt.condition = parse_expression(p, 0);
        if (!stmt->as.for_stmt.condition) return NULL;
    }
    EXPECT_EXIT(p, T_CLOSING);

    // Increment (optional)
    stmt->as.for_stmt.increment = NULL;
    if (!check(p, T_CPARENT)) {
        stmt->as.for_stmt.increment = parse_expression(p, 0);
        if (!stmt->as.for_stmt.increment) return NULL;
    }

    EXPECT_EXIT(p, T_CPARENT);

    // Body
    stmt->as.for_stmt.body = parse_statement(p);
    if (!stmt->as.for_stmt.body) return NULL;

    return stmt;
}

static Stmt *parse_enum_def(Parser *p, Token *name_tok) {
    Stmt *stmt = make_stmt(STMT_ENUM_DEF, p->arena);
    stmt->loc = name_tok->loc;
    stmt->as.enum_def.name = name_tok->data.String;
    stmt->as.enum_def.variants = (EnumVariants){0};

    // Expect :: enum { ... }
    EXPECT_EXIT(p, T_DCOLON);
    EXPECT_EXIT(p, T_ENUM);
    EXPECT_EXIT(p, T_OCPARENT);

    int64_t current_value = 0;

    while (!check(p, T_CCPARENT)) {
        Token *variant_tok = peek(p);
        EXPECT_EXIT(p, T_IDENT);

        EnumVariant variant = {
            .name = variant_tok->data.String,
            .value = current_value++
        };

        if (check(p, T_EQUAL)) {
            advance(p);
            Token *current = peek(p);
            if (current && current->tk == T_NUM) {
                current_value = current->data.Uint64;
                variant.value = current_value++;
                advance(p);
            } else {
                log_error(current->loc, "Expected an integer for the value but got %s", get_token_str(current->tk));
                return NULL;
            }
        }
        da_append(&stmt->as.enum_def.variants, variant);

        if (!match(p, T_CLOSING)) {
            break;
        }
    }

    EXPECT_EXIT(p, T_CCPARENT);
    EXPECT_EXIT(p, T_CLOSING);

    return stmt;
}

static Stmt *parse_return(Parser *p, Token *kw) {
    Stmt *stmt = make_stmt(STMT_RET, p->arena);
    stmt->loc = kw->loc;

    if (!check(p, T_CLOSING)) {
        stmt->as.expr.expr = parse_expression(p, 0);
        if (!stmt->as.expr.expr) return NULL;
    } else {
        stmt->as.expr.expr = NULL;
    }

    EXPECT_EXIT(p, T_CLOSING);
    return stmt;
}

static Stmt *parse_let(Parser *p, Token *kw) {
    Token *name = peek(p);
    EXPECT_EXIT(p, T_IDENT);

    Token *lettype = NULL;
    if (check(p, T_IDENT)) {
        lettype = peek(p);
        EXPECT_EXIT(p, T_IDENT);
    }

    EXPECT_EXIT(p, T_EQUAL);

    Expr *value = parse_expression(p, 0);
    if (!value) return NULL;

    EXPECT_EXIT(p, T_CLOSING);

    Stmt *stmt = make_stmt(STMT_LET, p->arena);
    stmt->loc = kw->loc;
    stmt->as.let.name = name->data.String;
    if (lettype) {
        stmt->as.let.type = lettype->data.String;
    } else {
        stmt->as.let.type = "comptimecheck";
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

    EXPECT_EXIT(p, T_CCPARENT);

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

    if (match(p, T_IF)) {
        Token *kw = previous(p);
        return parse_if(p, kw);
    }

    if (match(p, T_FOR)) {
        Token *kw = previous(p);
        return parse_for(p, kw);
    }

    // Check for enum definition: Name :: enum { ... };
    if (check(p, T_IDENT)) {
        Token *name_tok = peek(p);
        // Peek ahead for ::
        if (p->current + 1 < p->tokens->count &&
            p->tokens->items[p->current + 1].tk == T_DCOLON) {
            advance(p); // consume identifier
            return parse_enum_def(p, name_tok);
        }
    }

    Expr *expr = parse_expression(p, 0);
    if (!expr) return NULL;
    EXPECT_EXIT(p, T_CLOSING);

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

    case AST_LITERAL_STRING:
        printf("STRING(%s)\n", e->as.identifier);
        break;

    case AST_IDENTIFIER:
        printf("IDENT(%s)\n", e->as.identifier);
        break;

    case AST_UNARY_OP:
        printf("UNARY(op=%s)\n", get_token_str(e->as.unary.op));
        print_expr(e->as.unary.right, indent + 1);
        break;

    case AST_BINARY_OP:
        printf("BINARY(op=%s)\n", get_token_str(e->as.binary.op));
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
    case STMT_IF:
        printf("IF\n");
        print_expr(s->as.if_stmt.condition, indent + 1);
        print_indent(indent);
        printf("THEN\n");
        print_stmt(s->as.if_stmt.then_b, indent + 1);
        if (s->as.if_stmt.else_b) {
            print_indent(indent);
            printf("ELSE\n");
            print_stmt(s->as.if_stmt.else_b, indent + 1);
        }
        break;

    case STMT_FOR:
        printf("FOR\n");
        if (s->as.for_stmt.init) {
            print_indent(indent + 1);
            printf("INIT:\n");
            print_stmt(s->as.for_stmt.init, indent + 2);
        }
        if (s->as.for_stmt.condition) {
            print_indent(indent + 1);
            printf("COND:\n");
            print_expr(s->as.for_stmt.condition, indent + 2);
        }
        if (s->as.for_stmt.increment) {
            print_indent(indent + 1);
            printf("INC:\n");
            print_expr(s->as.for_stmt.increment, indent + 2);
        }
        print_indent(indent + 1);
        printf("BODY:\n");
        print_stmt(s->as.for_stmt.body, indent + 2);
        break;

    case STMT_ENUM_DEF:
        printf("ENUM %s\n", s->as.enum_def.name);
        for (size_t i = 0; i < s->as.enum_def.variants.count; i++) {
            print_indent(indent + 1);
            printf("VARIANT(%s = %ld)\n",
                   s->as.enum_def.variants.items[i].name,
                   s->as.enum_def.variants.items[i].value);
        }
        break;

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
        if (stmt == NULL) {
            return false;
        }
        da_append(stmts, stmt);
    }
    return true;
}
