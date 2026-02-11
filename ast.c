#include "ast.h"
#include "token.h"

#define BIGGEST_POWER 30

typedef struct {
    Tokens *tokens;
    size_t current;
    Arena *arena;
} Parser;

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
        perr("%s:%lu:%lu: Expected token %d, got %d", cr->loc.name, cr->loc.line, cr->loc.col, kind, cr->tk);
        exit(1);
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

    default:
        return 0;
    }
}

static Expr *parse_expression(Parser *p, int min_bp) {
    Expr *lhs;

    // ----- Prefix -----
    Token *tok = advance(p);

    switch (tok->tk) {

    case T_NUM: {
        lhs = make_expr(AST_LITERAL_INT, p->arena);
        lhs->as.uint_val = tok->data.Uint64;
    } break;

    case T_FLO: {
        lhs = make_expr(AST_LITERAL_FLOAT, p->arena);
        lhs->as.float_val = tok->data.F64;
    } break;

    case T_STR: {
        lhs = make_expr(AST_LITERAL_STRING, p->arena);
        lhs->as.identifier = tok->data.String;
    } break;
    case T_IDENT: {
        lhs = make_expr(AST_IDENTIFIER, p->arena);
        lhs->as.identifier = tok->data.String;
    } break;

    case T_OPARENT: {
        lhs = parse_expression(p, 0);
        expect(p, T_CPARENT);
    } break;

    case T_MIN: { // unary minus
        Expr *rhs = parse_expression(p, BIGGEST_POWER);

        lhs = make_expr(AST_UNARY_OP, p->arena);
        lhs->as.unary.op = T_MIN;
        lhs->as.unary.right = rhs;
    } break;

    default:
        fprintf(stderr, "Unexpected token in expression\n");
        exit(1);
    }

    // ----- Infix Loop -----
    while (1) {
        Token *next = peek(p);
        int left_bp, right_bp;
        if (!infix_binding_power(next->tk, &left_bp, &right_bp))
            break;

        if (left_bp < min_bp)
            break;

        TokenKind op = next->tk;
        advance(p);

        Expr *rhs = parse_expression(p, right_bp);

        if (op == T_EQUAL) {
            if (lhs->type != AST_IDENTIFIER) {
                fprintf(stderr, "Invalid assignment target\n");
                exit(1);
            }

            Expr *assign = make_expr(AST_ASSIGN, p->arena);
            assign->as.assign.name = lhs->as.identifier;
            assign->as.assign.value = rhs;

            lhs = assign;
            continue;
        }

        Expr *bin = make_expr(AST_BINARY_OP, p->arena);
        bin->as.binary.op = next->tk;
        bin->as.binary.left = lhs;
        bin->as.binary.right = rhs;

        lhs = bin;
    }

    return lhs;
}

 /* C BULLSHIT */
static Stmt *parse_statement(Parser *p);
static Stmt *parse_let(Parser *p);
static Stmt *parse_block(Parser *p);

static Stmt *parse_let(Parser *p) {
    Token *name = peek(p);
    expect(p, T_IDENT);

    expect(p, T_EQUAL);

    Expr *value = parse_expression(p, 0);

    expect(p, T_CLOSING);

    Stmt *stmt = make_stmt(STMT_LET, p->arena);
    stmt->loc = name->loc;
    stmt->as.let.name = name->data.String;
    stmt->as.let.value = value;

    return stmt;
}

static Stmt *parse_block(Parser *p) {
    Stmt *block = make_stmt(STMT_BLOCK, p->arena);

    block->as.block.statements.items = NULL;
    block->as.block.statements.count = 0;
    block->as.block.statements.capacity = 0;

    while (!check(p, T_CCPARENT) && !is_at_end(p)) {
        Stmt *stmt = parse_statement(p);
        da_append(&block->as.block.statements, stmt);
    }

    expect(p, T_CCPARENT);

    return block;
}

static Stmt *parse_statement(Parser *p) {
    if (match(p, T_LET)) {
        return parse_let(p);
    }

    if (match(p, T_OCPARENT)) {
        return parse_block(p);
    }

    Expr *expr = parse_expression(p, 0);
    expect(p, T_CLOSING);

    Stmt *stmt = make_stmt(STMT_EXPR, p->arena);
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
    }
}

void print_stmt(Stmt *s, int indent) {
    print_indent(indent);

    switch (s->type) {
    case STMT_LET:
        printf("LET %s\n", s->as.let.name);
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

void make_ast(Arena *a, Statements *stmts, Tokens *t) {
    Parser p = {0};
    p.tokens = t;
    p.current = 0;
    p.arena = a;

    while (!is_at_end(&p)) {
        Stmt *stmt = parse_statement(&p);
        da_append(stmts, stmt);
    }
}
