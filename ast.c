#include "ast.h"
#include "lexer.h"

#define BIGGEST_POWER 40

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
    case T_OR:
        *left_bp = 2;
        *right_bp = 3;
        return 1;

    case T_AND:
        *left_bp = 4;
        *right_bp = 5;
        return 1;

    case T_BIT_OR:
        *left_bp = 6;
        *right_bp = 7;
        return 1;

    case T_BIT_AND:
        *left_bp = 8;
        *right_bp = 9;
        return 1;

    case T_EQ:
    case T_NEQ:
    case T_LT:
    case T_GT:
    case T_LTE:
    case T_GTE:
        *left_bp = 10;
        *right_bp = 11;
        return 1;

    case T_LSHIFT:
    case T_RSHIFT:
        *left_bp = 11;
        *right_bp = 12;
        return 1;

    case T_PLUS:
    case T_MIN:
        *left_bp = 13;
        *right_bp = 14;
        return 1;

    case T_STAR:
    case T_DIV:
    case T_MOD:
        *left_bp = 15;
        *right_bp = 16;
        return 1;

    case T_DOT:
        *left_bp = 20;
        *right_bp = 21;
        return 1;

    case T_EQUAL:
        *left_bp = 1;
        *right_bp = 0;  // Right associative
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
static Stmt *parse_if(Parser *p, Token *kw);
static Stmt *parse_for(Parser *p, Token *kw);
static Stmt *parse_break(Parser *p, Token *kw);
static Stmt *parse_continue(Parser *p, Token *kw);
static Stmt *parse_struct_def(Parser *p, Token *name_tok);
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

    case T_TRUE: {
        lhs = make_expr(AST_LITERAL_INT, p->arena);
        lhs->loc = tok->loc;
        lhs->as.uint_val = 1;  // true is 1
    } break;

    case T_FALSE: {
        lhs = make_expr(AST_LITERAL_INT, p->arena);
        lhs->loc = tok->loc;
        lhs->as.uint_val = 0;  // false is 0
    } break;

    case T_NULL: {
        lhs = make_expr(AST_LITERAL_NULL, p->arena);
        lhs->loc = tok->loc;
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
        char *name = tok->data.String;
        
        // Check for generic type: TypeName<T1, T2>
        TypeParams type_args = {0};
        if (check(p, T_LT)) {
            advance(p); // consume '<'
            
            do {
                Token *type_arg = peek(p);
                if (!expect(p, T_IDENT)) return NULL;
                
                TypeParam param = { .name = type_arg->data.String };
                da_append(&type_args, param);
                
            } while (match(p, T_COMMA));
            
            if (!expect(p, T_GT)) return NULL;
        }
        
        // Check for struct initialization: TypeName{...}
        if (check(p, T_OCPARENT)) {
            advance(p); // consume '{'
            
            lhs = make_expr(AST_STRUCT_INIT, p->arena);
            lhs->loc = tok->loc;
            lhs->as.struct_init.type_name = name;
            lhs->as.struct_init.type_args = type_args;
            lhs->as.struct_init.field_inits = (FieldInits){0};
            
            // Parse field initializers: field: value, ...
            while (!check(p, T_CCPARENT) && !is_at_end(p)) {
                Token *field_name = peek(p);
                if (!expect(p, T_IDENT)) return NULL;
                
                if (!expect(p, T_COLON)) return NULL;
                
                Expr *field_value = parse_expression(p, 0);
                if (!field_value) return NULL;
                
                FieldInit init = {
                    .name = field_name->data.String,
                    .value = field_value
                };
                da_append(&lhs->as.struct_init.field_inits, init);
                
                if (!match(p, T_COMMA)) {
                    // Allow optional commas
                }
            }
            
            if (!expect(p, T_CCPARENT)) return NULL;
        } else {
            // Regular identifier
            lhs = make_expr(AST_IDENTIFIER, p->arena);
            lhs->loc = tok->loc;
            lhs->as.identifier = name;
            
            // If we parsed type args but no {}, this is a type reference
            // Store type args somehow? For now just ignore them for identifiers
        }
    } break;

    case T_CAST: {
        // cast(type, expr)
        if (!expect(p, T_OPARENT)) return NULL;
        
        Token *type_tok = peek(p);
        if (!expect(p, T_IDENT)) return NULL;
        
        if (!expect(p, T_COMMA)) return NULL;
        
        Expr *expr = parse_expression(p, 0);
        if (!expr) return NULL;
        
        if (!expect(p, T_CPARENT)) return NULL;
        
        lhs = make_expr(AST_CAST, p->arena);
        lhs->loc = tok->loc;
        lhs->as.cast.target_type = type_tok->data.String;
        lhs->as.cast.expr = expr;
    } break;

    case T_SIZEOF: {
        // sizeof(type) or sizeof(expr)
        if (!expect(p, T_OPARENT)) return NULL;
        
        // Try to parse as type name first
        if (check(p, T_IDENT)) {
            Token *type_tok = advance(p);
            if (!expect(p, T_CPARENT)) return NULL;
            
            lhs = make_expr(AST_SIZEOF, p->arena);
            lhs->loc = tok->loc;
            lhs->as.size_of.type_name = type_tok->data.String;
            lhs->as.size_of.expr = NULL;
        } else {
            Expr *expr = parse_expression(p, 0);
            if (!expr) return NULL;
            if (!expect(p, T_CPARENT)) return NULL;
            
            lhs = make_expr(AST_SIZEOF, p->arena);
            lhs->loc = tok->loc;
            lhs->as.size_of.type_name = NULL;
            lhs->as.size_of.expr = expr;
        }
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
                    log_error(name->loc, "Expecting the parameter type after the name.");
                    return NULL;
                }
                Param p = {
                    .name = name->data.String,
                    .type = typename ? typename->data.String : "any"
                };
                da_append(&params, p);

            } while (match(p, T_COMMA));
        }

        if (is_param_list && match(p, T_CPARENT) && check(p, T_ARROW)) {
            advance(p); // consume ->

            Token *retval = NULL;

            // consume ident ret
            if (check(p, T_IDENT)) {
                retval = peek(p);
                advance(p);
            }

            if (!expect(p, T_OCPARENT)) return NULL;
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
            if (!expect(p, T_CPARENT)) return NULL;
        }
    } break;

    case T_MIN: { // unary minus
        Expr *rhs = parse_expression(p, BIGGEST_POWER);

        lhs = make_expr(AST_UNARY_OP, p->arena);
        lhs->loc = tok->loc;
        lhs->as.unary.op = T_MIN;
        lhs->as.unary.right = rhs;
    } break;

    case T_NOT: { // logical NOT: !expr
        Expr *rhs = parse_expression(p, BIGGEST_POWER);

        lhs = make_expr(AST_UNARY_OP, p->arena);
        lhs->loc = tok->loc;
        lhs->as.unary.op = T_NOT;
        lhs->as.unary.right = rhs;
    } break;

    case T_BIT_AND: { // address-of: &expr
        Expr *rhs = parse_expression(p, BIGGEST_POWER);

        lhs = make_expr(AST_ADDR_OF, p->arena);
        lhs->loc = tok->loc;
        lhs->as.addr_of.expr = rhs;
    } break;

    case T_STAR: { // dereference: *expr
        Expr *rhs = parse_expression(p, BIGGEST_POWER);

        lhs = make_expr(AST_DEREF, p->arena);
        lhs->loc = tok->loc;
        lhs->as.deref.expr = rhs;
    } break;

    case T_OCPARENT: { // block expression
        Stmt *block_stmt = parse_block(p, tok);
        if (!block_stmt) return NULL;

        lhs = make_expr(AST_BLOCK_EXPR, p->arena);
        lhs->loc = tok->loc;
        lhs->as.block_expr.statements = block_stmt->as.block.statements;
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

        if (next->tk == T_DOT) {
            // Member access: expr.member or expr.method()
            advance(p); // consume '.'
            
            Token *member_tok = peek(p);
            if (!expect(p, T_IDENT)) return NULL;
            
            Expr *member = make_expr(AST_MEMBER_ACCESS, p->arena);
            member->loc = next->loc;
            member->as.member_access.object = lhs;
            member->as.member_access.member_name = member_tok->data.String;
            
            lhs = member;
            continue;
        }

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

            if (!expect(p, T_CPARENT)) return NULL;

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

    if (!expect(p, T_CLOSING)) return NULL;
    return stmt;
}

static Stmt *parse_let(Parser *p, Token *kw) {
    Token *name = peek(p);
    if (!expect(p, T_IDENT)) return NULL;

    Token *lettype = NULL;
    
    // Check if type is specified or if we should infer
    if (check(p, T_IDENT)) {
        // Type is specified: let x Type = ...
        lettype = peek(p);
        advance(p);
    }
    // Otherwise type will be inferred from RHS

    if (!expect(p, T_EQUAL)) return NULL;

    Expr *value = parse_expression(p, 0);

    if (!expect(p, T_CLOSING)) return NULL;

    Stmt *stmt = make_stmt(STMT_LET, p->arena);
    stmt->loc = kw->loc;
    stmt->as.let.name = name->data.String;
    if (lettype) {
        stmt->as.let.type = lettype->data.String;
    } else {
        stmt->as.let.type = NULL;  // Will be inferred
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

    if (!expect(p, T_CCPARENT)) return NULL;

    return block;
}

static Stmt *parse_if(Parser *p, Token *kw) {
    Stmt *stmt = make_stmt(STMT_IF, p->arena);
    stmt->loc = kw->loc;

    if (!expect(p, T_OPARENT)) return NULL;
    stmt->as.if_stmt.condition = parse_expression(p, 0);
    if (!stmt->as.if_stmt.condition) return NULL;
    if (!expect(p, T_CPARENT)) return NULL;

    stmt->as.if_stmt.then_branch = parse_statement(p);
    if (!stmt->as.if_stmt.then_branch) return NULL;

    stmt->as.if_stmt.else_branch = NULL;
    if (match(p, T_ELSE)) {
        stmt->as.if_stmt.else_branch = parse_statement(p);
        if (!stmt->as.if_stmt.else_branch) return NULL;
    }

    return stmt;
}

static Stmt *parse_for(Parser *p, Token *kw) {
    Stmt *stmt = make_stmt(STMT_FOR, p->arena);
    stmt->loc = kw->loc;

    if (!expect(p, T_OPARENT)) return NULL;

    // init (optional)
    if (check(p, T_CLOSING)) {
        stmt->as.for_stmt.init = NULL;
        advance(p);
    } else {
        stmt->as.for_stmt.init = parse_statement(p);
        if (!stmt->as.for_stmt.init) return NULL;
    }

    // condition (optional)
    if (check(p, T_CLOSING)) {
        stmt->as.for_stmt.condition = NULL;
        advance(p);
    } else {
        stmt->as.for_stmt.condition = parse_expression(p, 0);
        if (!expect(p, T_CLOSING)) return NULL;
    }

    // increment (optional)
    if (check(p, T_CPARENT)) {
        stmt->as.for_stmt.increment = NULL;
    } else {
        stmt->as.for_stmt.increment = parse_expression(p, 0);
    }

    if (!expect(p, T_CPARENT)) return NULL;

    // body
    stmt->as.for_stmt.body = parse_statement(p);
    if (!stmt->as.for_stmt.body) return NULL;

    return stmt;
}

static Stmt *parse_break(Parser *p, Token *kw) {
    Stmt *stmt = make_stmt(STMT_BREAK, p->arena);
    stmt->loc = kw->loc;
    if (!expect(p, T_CLOSING)) return NULL;
    return stmt;
}

static Stmt *parse_continue(Parser *p, Token *kw) {
    Stmt *stmt = make_stmt(STMT_CONTINUE, p->arena);
    stmt->loc = kw->loc;
    if (!expect(p, T_CLOSING)) return NULL;
    return stmt;
}

static Stmt *parse_struct_def(Parser *p, Token *name_tok) {
    // Name :: type<T, U> { fields } or Name :: type { fields }
    Stmt *stmt = make_stmt(STMT_STRUCT_DEF, p->arena);
    stmt->loc = name_tok->loc;
    stmt->as.struct_def.name = name_tok->data.String;
    stmt->as.struct_def.fields = (StructFields){0};
    stmt->as.struct_def.type_params = (TypeParams){0};

    if (!expect(p, T_DCOLON)) return NULL;
    if (!expect(p, T_TYPE)) return NULL;
    
    // Check for generic type parameters: <T, U, ...>
    if (match(p, T_LT)) {
        do {
            Token *type_param = peek(p);
            if (!expect(p, T_IDENT)) return NULL;
            
            TypeParam param = { .name = type_param->data.String };
            da_append(&stmt->as.struct_def.type_params, param);
            
        } while (match(p, T_COMMA));
        
        if (!expect(p, T_GT)) return NULL;
    }
    
    if (!expect(p, T_OCPARENT)) return NULL;

    // Parse fields
    while (!check(p, T_CCPARENT) && !is_at_end(p)) {
        Token *field_name = peek(p);
        if (!expect(p, T_IDENT)) return NULL;

        // Check if this is a method: field_name fn(params) -> ret
        if (match(p, T_FN)) {
            // Parse function type
            if (!expect(p, T_OPARENT)) return NULL;
            
            Params method_params = {0};
            if (!check(p, T_CPARENT)) {
                do {
                    Token *param_name = peek(p);
                    if (!expect(p, T_IDENT)) return NULL;
                    
                    Token *param_type = peek(p);
                    if (!expect(p, T_IDENT)) return NULL;
                    
                    Param param = {
                        .name = param_name->data.String,
                        .type = param_type->data.String
                    };
                    da_append(&method_params, param);
                    
                } while (match(p, T_COMMA));
            }
            
            if (!expect(p, T_CPARENT)) return NULL;
            
            char *return_type = "void";
            if (match(p, T_ARROW)) {
                Token *ret_tok = peek(p);
                if (!expect(p, T_IDENT)) return NULL;
                return_type = ret_tok->data.String;
            }
            
            StructField field = {
                .name = field_name->data.String,
                .type = NULL,
                .is_method = true,
                .method_params = method_params,
                .method_return = return_type
            };
            da_append(&stmt->as.struct_def.fields, field);
        } else {
            // Regular field
            Token *field_type = peek(p);
            if (!expect(p, T_IDENT)) return NULL;

            StructField field = {
                .name = field_name->data.String,
                .type = field_type->data.String,
                .is_method = false
            };
            da_append(&stmt->as.struct_def.fields, field);
        }

        if (!match(p, T_CLOSING)) {
            // Allow optional semicolons between fields
        }
    }

    if (!expect(p, T_CCPARENT)) return NULL;

    return stmt;
}

static Stmt *parse_enum_def(Parser *p, Token *name_tok) {
    // Name :: enum { variants }
    Stmt *stmt = make_stmt(STMT_ENUM_DEF, p->arena);
    stmt->loc = name_tok->loc;
    stmt->as.enum_def.name = name_tok->data.String;
    stmt->as.enum_def.variants = (EnumVariants){0};

    if (!expect(p, T_DCOLON)) return NULL;
    if (!expect(p, T_ENUM)) return NULL;
    if (!expect(p, T_OCPARENT)) return NULL;

    // Parse variants
    int auto_value = 0;
    while (!check(p, T_CCPARENT) && !is_at_end(p)) {
        Token *variant_name = peek(p);
        if (!expect(p, T_IDENT)) return NULL;

        EnumVariant variant = {
            .name = variant_name->data.String,
            .has_value = false,
            .value = auto_value++,
            .data_type = NULL
        };

        // Check for union type: VARIANT(Type)
        if (match(p, T_OPARENT)) {
            Token *type_tok = peek(p);
            if (!expect(p, T_IDENT)) return NULL;
            variant.data_type = type_tok->data.String;
            if (!expect(p, T_CPARENT)) return NULL;
        }

        // Check for explicit value: VARIANT = 123 or VARIANT(Type) = 123
        if (match(p, T_EQUAL)) {
            Token *val_tok = peek(p);
            if (!expect(p, T_NUM)) return NULL;
            variant.has_value = true;
            variant.value = (int64_t)val_tok->data.Uint64;
            auto_value = variant.value + 1;
        }

        da_append(&stmt->as.enum_def.variants, variant);

        if (!match(p, T_COMMA)) {
            // Allow optional commas between variants
        }
    }

    if (!expect(p, T_CCPARENT)) return NULL;

    return stmt;
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

    if (match(p, T_IF)) {
        Token *kw = previous(p);
        return parse_if(p, kw);
    }

    if (match(p, T_FOR)) {
        Token *kw = previous(p);
        return parse_for(p, kw);
    }

    if (match(p, T_BREAK)) {
        Token *kw = previous(p);
        return parse_break(p, kw);
    }

    if (match(p, T_CONTINUE)) {
        Token *kw = previous(p);
        return parse_continue(p, kw);
    }

    if (match(p, T_OCPARENT)) {
        Token *kw = previous(p);
        return parse_block(p, kw);
    }

    // Check for struct/enum definition: Name :: type/enum { ... }
    if (check(p, T_IDENT)) {
        size_t saved = p->current;
        Token *name = advance(p);
        
        if (match(p, T_DCOLON)) {
            if (check(p, T_TYPE)) {
                // Struct definition
                return parse_struct_def(p, name);
            } else if (check(p, T_ENUM)) {
                // Enum definition
                return parse_enum_def(p, name);
            }
        }
        
        // Not a definition, rollback
        p->current = saved;
    }

    Expr *expr = parse_expression(p, 0);
    if (!expect(p, T_CLOSING)) return NULL;

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
    case AST_FUNCTION_TYPE:
        // @TODO: idk what they do but let me just put it here.
        break;
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

    case AST_LITERAL_NULL:
        printf("NULL\n");
        break;

    case AST_IDENTIFIER:
        printf("IDENT(%s)\n", e->as.identifier);
        break;

    case AST_UNARY_OP: {
        const char *op_str = "?";
        switch (e->as.unary.op) {
            case T_MIN: op_str = "-"; break;
            case T_NOT: op_str = "!"; break;
            default: break;
        }
        printf("UNARY(%s)\n", op_str);
        print_expr(e->as.unary.right, indent + 1);
        break;
    }

    case AST_BINARY_OP: {
        const char *op_str = "?";
        switch (e->as.binary.op) {
            case T_PLUS: op_str = "+"; break;
            case T_MIN: op_str = "-"; break;
            case T_STAR: op_str = "*"; break;
            case T_DIV: op_str = "/"; break;
            case T_MOD: op_str = "%"; break;
            case T_EQ: op_str = "=="; break;
            case T_NEQ: op_str = "!="; break;
            case T_LT: op_str = "<"; break;
            case T_GT: op_str = ">"; break;
            case T_LTE: op_str = "<="; break;
            case T_GTE: op_str = ">="; break;
            case T_AND: op_str = "&&"; break;
            case T_OR: op_str = "||"; break;
            case T_BIT_AND: op_str = "&"; break;
            case T_BIT_OR: op_str = "|"; break;
            case T_LSHIFT: op_str = "<<"; break;
            case T_RSHIFT: op_str = ">>"; break;
            default: break;
        }
        printf("BINARY(%s)\n", op_str);
        print_expr(e->as.binary.left, indent + 1);
        print_expr(e->as.binary.right, indent + 1);
        break;
    }
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

    case AST_BLOCK_EXPR:
        printf("BLOCK_EXPR\n");
        for (size_t i = 0; i < e->as.block_expr.statements.count; i++) {
            print_stmt(e->as.block_expr.statements.items[i], indent + 1);
        }
        break;

    case AST_CAST:
        printf("CAST(%s)\n", e->as.cast.target_type);
        print_expr(e->as.cast.expr, indent + 1);
        break;

    case AST_SIZEOF:
        if (e->as.size_of.type_name) {
            printf("SIZEOF(type: %s)\n", e->as.size_of.type_name);
        } else {
            printf("SIZEOF(expr)\n");
            print_expr(e->as.size_of.expr, indent + 1);
        }
        break;

    case AST_ADDR_OF:
        printf("ADDR_OF(&)\n");
        print_expr(e->as.addr_of.expr, indent + 1);
        break;

    case AST_DEREF:
        printf("DEREF(*)\n");
        print_expr(e->as.deref.expr, indent + 1);
        break;

    case AST_STRUCT_INIT:
        printf("STRUCT_INIT(%s", e->as.struct_init.type_name);
        if (e->as.struct_init.type_args.count > 0) {
            printf("<");
            for (size_t i = 0; i < e->as.struct_init.type_args.count; i++) {
                printf("%s", e->as.struct_init.type_args.items[i].name);
                if (i < e->as.struct_init.type_args.count - 1) printf(", ");
            }
            printf(">");
        }
        printf(")\n");
        for (size_t i = 0; i < e->as.struct_init.field_inits.count; i++) {
            print_indent(indent + 1);
            printf("%s:\n", e->as.struct_init.field_inits.items[i].name);
            print_expr(e->as.struct_init.field_inits.items[i].value, indent + 2);
        }
        break;

    case AST_MEMBER_ACCESS:
        printf("MEMBER_ACCESS(.%s)\n", e->as.member_access.member_name);
        print_expr(e->as.member_access.object, indent + 1);
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

    case STMT_IF:
        printf("IF\n");
        print_indent(indent + 1);
        printf("CONDITION:\n");
        print_expr(s->as.if_stmt.condition, indent + 2);
        print_indent(indent + 1);
        printf("THEN:\n");
        print_stmt(s->as.if_stmt.then_branch, indent + 2);
        if (s->as.if_stmt.else_branch) {
            print_indent(indent + 1);
            printf("ELSE:\n");
            print_stmt(s->as.if_stmt.else_branch, indent + 2);
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
            printf("CONDITION:\n");
            print_expr(s->as.for_stmt.condition, indent + 2);
        }
        if (s->as.for_stmt.increment) {
            print_indent(indent + 1);
            printf("INCREMENT:\n");
            print_expr(s->as.for_stmt.increment, indent + 2);
        }
        print_indent(indent + 1);
        printf("BODY:\n");
        print_stmt(s->as.for_stmt.body, indent + 2);
        break;

    case STMT_BREAK:
        printf("BREAK\n");
        break;

    case STMT_CONTINUE:
        printf("CONTINUE\n");
        break;

    case STMT_STRUCT_DEF:
        printf("STRUCT_DEF(%s", s->as.struct_def.name);
        if (s->as.struct_def.type_params.count > 0) {
            printf("<");
            for (size_t i = 0; i < s->as.struct_def.type_params.count; i++) {
                printf("%s", s->as.struct_def.type_params.items[i].name);
                if (i < s->as.struct_def.type_params.count - 1) printf(", ");
            }
            printf(">");
        }
        printf(")\n");
        for (size_t i = 0; i < s->as.struct_def.fields.count; i++) {
            print_indent(indent + 1);
            StructField *field = &s->as.struct_def.fields.items[i];
            if (field->is_method) {
                printf("METHOD(%s: fn(", field->name);
                for (size_t j = 0; j < field->method_params.count; j++) {
                    printf("%s %s", field->method_params.items[j].name, 
                           field->method_params.items[j].type);
                    if (j < field->method_params.count - 1) printf(", ");
                }
                printf(") -> %s)\n", field->method_return);
            } else {
                printf("FIELD(%s: %s)\n", field->name, field->type);
            }
        }
        break;

    case STMT_ENUM_DEF:
        printf("ENUM_DEF(%s)\n", s->as.enum_def.name);
        for (size_t i = 0; i < s->as.enum_def.variants.count; i++) {
            print_indent(indent + 1);
            EnumVariant *v = &s->as.enum_def.variants.items[i];
            if (v->data_type) {
                printf("VARIANT(%s(%s) = %ld)\n", v->name, v->data_type, v->value);
            } else {
                printf("VARIANT(%s = %ld)\n", v->name, v->value);
            }
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
