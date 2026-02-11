#include "ast.h"
#include "token.h"

typedef struct {
    Tokens *tokens;
    size_t current;
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
    if (!match(p, kind)) {
        fprintf(stderr, "Parse error: expected token %d\n", kind);
        exit(1);
    }
}

Expr *make_expr(ExprType type) {
    Expr *n = malloc(sizeof(Expr));
    n->type = type;
    return n;
}

Stmt *make_stmt(StmtType type) {
    Stmt *n = malloc(sizeof(Stmt));
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
            lhs = make_expr(AST_LITERAL_INT);
            lhs->as.uint_val = tok->data.Uint64;
            break;
        }

        case T_FLO: {
            lhs = make_expr(AST_LITERAL_FLOAT);
            lhs->as.float_val = tok->data.F64;
            break;
        }

        case T_IDENT: {
            lhs = make_expr(AST_IDENTIFIER);
            lhs->as.identifier = tok->data.String;
            break;
        }

        case T_OPARENT: {
            lhs = parse_expression(p, 0);
            expect(p, T_CPARENT);
            break;
        }

        case T_MIN: { // unary minus
            Expr *rhs = parse_expression(p, 30);

            lhs = make_expr(AST_UNARY_OP);
            lhs->as.unary.op = T_MIN;
            lhs->as.unary.right = rhs;
            break;
        }

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

        advance(p);

        Expr *rhs = parse_expression(p, right_bp);

        Expr *bin = make_expr(AST_BINARY_OP);
        bin->as.binary.op = next->tk;
        bin->as.binary.left = lhs;
        bin->as.binary.right = rhs;

        lhs = bin;
    }

    return lhs;
}

static Stmt *parse_let(Parser *p) {
    Token *name = peek(p);
    expect(p, T_IDENT);

    expect(p, T_EQUAL);

    Expr *value = parse_expression(p, 0);

    expect(p, T_CLOSING);

    Stmt *stmt = make_stmt(STMT_LET);
    stmt->as.let.name = name->data.String;
    stmt->as.let.value = value;

    return stmt;
}

static Stmt *parse_statement(Parser *p) {
    if (match(p, T_LET)) {
        return parse_let(p);
    }

    fprintf(stderr, "Unknown statement\n");
    exit(1);
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

void make_ast(Statements *stmts, Tokens *t) {
    Parser p = {0};
    p.tokens = t;
    p.current = 0;

    while (!is_at_end(&p)) {
        Stmt *stmt = parse_statement(&p);
        da_append(stmts, stmt);
    }
}
