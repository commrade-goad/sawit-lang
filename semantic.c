#include "semantic.h"
#include "ast.h"

Type *make_type(TypeKind kind, const char *name, Arena *a) {
    Type *t = (Type *)arena_alloc(a, sizeof(Type));
    t->kind = kind;
    t->pointee = NULL;
    t->user_data = NULL;
    if (name) {
        size_t len = strlen(name);
        t->name = (char *)arena_alloc(a, len + 1);
        strcpy(t->name, name);
    } else {
        t->name = NULL;
    }
    return t;
}

Type *type_from_string(const char *name, Arena *a) {
    if (!name) return make_type(TYPE_UNKNOWN, name, a);

    // Check for pointer type: *TypeName
    if (name[0] == '*') {
        Type *pointee = type_from_string(name + 1, a);
        Type *ptr = make_type(TYPE_POINTER, name, a);
        ptr->pointee = pointee;
        return ptr;
    }

    if (strcmp(name, "s8") == 0) return make_type(TYPE_S8, name, a);
    if (strcmp(name, "s16") == 0) return make_type(TYPE_S16, name, a);
    if (strcmp(name, "s32") == 0) return make_type(TYPE_S32, name, a);
    if (strcmp(name, "s64") == 0) return make_type(TYPE_S64, name, a);
    if (strcmp(name, "u8") == 0) return make_type(TYPE_U8, name, a);
    if (strcmp(name, "u16") == 0) return make_type(TYPE_U16, name, a);
    if (strcmp(name, "u32") == 0) return make_type(TYPE_U32, name, a);
    if (strcmp(name, "u64") == 0) return make_type(TYPE_U64, name, a);
    if (strcmp(name, "f32") == 0) return make_type(TYPE_F32, name, a);
    if (strcmp(name, "f64") == 0) return make_type(TYPE_F64, name, a);
    if (strcmp(name, "bool") == 0) return make_type(TYPE_BOOL, name, a);
    if (strcmp(name, "void") == 0) return make_type(TYPE_VOID, name, a);

    // Otherwise assume it's a user-defined type (struct/enum)
    // We'll need to look it up in the symbol table
    return make_type(TYPE_UNKNOWN, name, a);
}

const char *type_to_string(Type *t) {
    if (!t) return "unknown";
    if (t->name) return t->name;

    switch (t->kind) {
    case TYPE_S8: return "s8";
    case TYPE_S16: return "s16";
    case TYPE_S32: return "s32";
    case TYPE_S64: return "s64";
    case TYPE_U8: return "u8";
    case TYPE_U16: return "u16";
    case TYPE_U32: return "u32";
    case TYPE_U64: return "u64";
    case TYPE_F32: return "f32";
    case TYPE_F64: return "f64";
    case TYPE_BOOL: return "bool";
    case TYPE_VOID: return "void";
    case TYPE_POINTER: {
        if (t->pointee) {
            // Would need to build string, for now just return name if available
            return t->name ? t->name : "*?";
        }
        return "*void";
    }
    case TYPE_STRUCT: return t->name ? t->name : "struct";
    case TYPE_ENUM: return t->name ? t->name : "enum";
    case TYPE_UNKNOWN: return "unknown";
    default: return "unknown";
    }
}

bool types_equal(Type *a, Type *b) {
    if (!a || !b) return false;

    // bool and s8 are equivalent (bool is just an alias for s8)
    if ((a->kind == TYPE_BOOL && b->kind == TYPE_S8) ||
        (a->kind == TYPE_S8 && b->kind == TYPE_BOOL)) {
        return true;
    }
    if (a->kind == TYPE_BOOL && b->kind == TYPE_BOOL) return true;

    // Pointer comparison
    if (a->kind == TYPE_POINTER && b->kind == TYPE_POINTER) {
        return types_equal(a->pointee, b->pointee);
    }

    // Named types (struct/enum) compare by name
    if ((a->kind == TYPE_STRUCT || a->kind == TYPE_ENUM) &&
        (b->kind == TYPE_STRUCT || b->kind == TYPE_ENUM)) {
        if (a->name && b->name) {
            return strcmp(a->name, b->name) == 0;
        }
    }

    return a->kind == b->kind;
}

// Check if value_type can be assigned to target_type
// This is more lenient than types_equal - allows implicit conversions
bool type_can_assign_to(Type *value_type, Type *target_type) {
    if (!value_type || !target_type) return false;

    // If types are equal, always OK
    if (types_equal(value_type, target_type)) return true;

    // Integer literals (TYPE_S32 by default) can be assigned to any integer type
    // This allows: let x u8 = 10;
    bool value_is_int = (value_type->kind >= TYPE_S8 && value_type->kind <= TYPE_U64) ||
                        value_type->kind == TYPE_BOOL;
    bool target_is_int = (target_type->kind >= TYPE_S8 && target_type->kind <= TYPE_U64) ||
                         target_type->kind == TYPE_BOOL;

    if (value_is_int && target_is_int) {
        // Allow assignment between integer types
        // TODO: add range checking for literals
        return true;
    }

    return false;
}

// Build instantiated type name: Vec<s32> -> Vec_s32
char *build_instantiated_name(const char *base_name, TypeParams *type_args, Arena *a) {
    if (!type_args || type_args->count == 0) {
        return (char *)base_name;
    }

    size_t total_len = strlen(base_name);
    for (size_t i = 0; i < type_args->count; i++) {
        total_len += strlen(type_args->items[i].name) + 1; // +1 for '_'
    }

    char *new_name = (char *)arena_alloc(a, total_len + 1);
    strcpy(new_name, base_name);
    for (size_t i = 0; i < type_args->count; i++) {
        strcat(new_name, "_");
        strcat(new_name, type_args->items[i].name);
    }

    return new_name;
}

// Instantiate a generic template with concrete types
Stmt *instantiate_template(Semantic *s, Stmt *template_def, TypeParams *type_args) {
    if (!template_def || template_def->type != STMT_STRUCT_DEF) {
        return NULL;
    }

    // Create new struct definition with substituted types
    Stmt *instance = (Stmt *)arena_alloc(s->arena, sizeof(Stmt));
    instance->type = STMT_STRUCT_DEF;
    instance->loc = template_def->loc;

    // Build instantiated name
    instance->as.struct_def.name = build_instantiated_name(
        template_def->as.struct_def.name, type_args, s->arena);

    // No type params on instance (it's concrete)
    instance->as.struct_def.type_params = (TypeParams){0};
    instance->as.struct_def.fields = (StructFields){0};

    // Substitute type parameters in fields
    for (size_t i = 0; i < template_def->as.struct_def.fields.count; i++) {
        StructField *template_field = &template_def->as.struct_def.fields.items[i];
        StructField instance_field = *template_field;

        if (instance_field.type) {
            // Check if field type is a type parameter
            for (size_t j = 0; j < template_def->as.struct_def.type_params.count; j++) {
                if (strcmp(instance_field.type, template_def->as.struct_def.type_params.items[j].name) == 0) {
                    // Substitute with concrete type
                    instance_field.type = type_args->items[j].name;
                    break;
                }
            }
        }

        // Handle method parameters substitution
        if (instance_field.is_method) {
            for (size_t k = 0; k < instance_field.method_params.count; k++) {
                Param *param = &instance_field.method_params.items[k];
                for (size_t j = 0; j < template_def->as.struct_def.type_params.count; j++) {
                    if (strcmp(param->type, template_def->as.struct_def.type_params.items[j].name) == 0) {
                        param->type = type_args->items[j].name;
                        break;
                    }
                }
            }

            // Substitute return type
            for (size_t j = 0; j < template_def->as.struct_def.type_params.count; j++) {
                if (strcmp(instance_field.method_return, template_def->as.struct_def.type_params.items[j].name) == 0) {
                    instance_field.method_return = type_args->items[j].name;
                    break;
                }
            }
        }

        da_append(&instance->as.struct_def.fields, instance_field);
    }

    return instance;
}

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

Type *infer_expr_type(Semantic *s, Expr *expr) {
    if (!expr) return make_type(TYPE_UNKNOWN, NULL, s->arena);

    switch (expr->type) {
    case AST_LITERAL_INT:
        // Default integer literals to s32
        return make_type(TYPE_S32, "s32", s->arena);

    case AST_LITERAL_FLOAT:
        // Default float literals to f64
        return make_type(TYPE_F64, "f64", s->arena);

    case AST_LITERAL_STRING:
        // Strings are a special type we'll handle later
        return make_type(TYPE_POINTER, "str", s->arena);

    case AST_LITERAL_NULL:
        // NULL is a void pointer
        {
            Type *ptr_type = make_type(TYPE_POINTER, "*void", s->arena);
            ptr_type->pointee = make_type(TYPE_VOID, "void", s->arena);
            return ptr_type;
        }

    case AST_IDENTIFIER: {
        Symbol *sym = lookup_symbol(s, expr->as.identifier);
        if (sym && sym->type) {
            return sym->type;
        }
        return make_type(TYPE_UNKNOWN, NULL, s->arena);
    }

    case AST_BINARY_OP: {
        Type *left_type = infer_expr_type(s, expr->as.binary.left);
        Type *right_type = infer_expr_type(s, expr->as.binary.right);

        // Comparison and logical operators return bool (s8)
        TokenKind op = expr->as.binary.op;
        if (op == T_EQ || op == T_NEQ || op == T_LT ||
            op == T_GT || op == T_LTE || op == T_GTE ||
            op == T_AND || op == T_OR) {

            // Still check that operands are compatible
            if (!types_equal(left_type, right_type)) {
                log_error(expr->loc, "Type mismatch in operation: %s vs %s",
                         type_to_string(left_type), type_to_string(right_type));
                s->had_error = true;
            }

            return make_type(TYPE_BOOL, "bool", s->arena);
        }

        // Type checking for arithmetic, bitwise, and shift operations
        if (!types_equal(left_type, right_type)) {
            log_error(expr->loc, "Type mismatch in binary operation: %s vs %s",
                     type_to_string(left_type), type_to_string(right_type));
            s->had_error = true;
            return make_type(TYPE_UNKNOWN, NULL, s->arena);
        }

        return left_type;
    }

    case AST_UNARY_OP: {
        Type *inner_type = infer_expr_type(s, expr->as.unary.right);

        // NOT operator returns bool
        if (expr->as.unary.op == T_NOT) {
            return make_type(TYPE_BOOL, "bool", s->arena);
        }

        // Other unary operators return same type as operand
        return inner_type;
    }

    case AST_ASSIGN: {
        Symbol *sym = lookup_symbol(s, expr->as.assign.name);
        Type *value_type = infer_expr_type(s, expr->as.assign.value);

        if (sym && sym->type) {
            if (!types_equal(sym->type, value_type)) {
                log_error(expr->loc, "Cannot assign %s to variable of type %s",
                         type_to_string(value_type), type_to_string(sym->type));
                s->had_error = true;
            }
            return sym->type;
        }

        return value_type;
    }

    case AST_CALL:
        // For now, assume calls return 'any'
        return make_type(TYPE_POINTER, "any", s->arena);

    case AST_FUNCTION:
        // Return the function's return type
        return type_from_string(expr->as.function.ret, s->arena);

    case AST_CAST:
        // Cast expression returns the target type
        return type_from_string(expr->as.cast.target_type, s->arena);

    case AST_SIZEOF:
        // sizeof always returns u64
        return make_type(TYPE_U64, "u64", s->arena);

    case AST_ADDR_OF: {
        // &expr returns pointer to expr's type
        Type *inner_type = infer_expr_type(s, expr->as.addr_of.expr);
        Type *ptr_type = make_type(TYPE_POINTER, NULL, s->arena);
        ptr_type->pointee = inner_type;
        // Build name like "*s32"
        if (inner_type && inner_type->name) {
            size_t len = strlen(inner_type->name) + 2;
            char *name = (char *)arena_alloc(s->arena, len);
            snprintf(name, len, "*%s", inner_type->name);
            ptr_type->name = name;
        }
        return ptr_type;
    }

    case AST_DEREF: {
        // *expr - dereference pointer
        Type *ptr_type = infer_expr_type(s, expr->as.deref.expr);
        if (ptr_type->kind != TYPE_POINTER) {
            log_error(expr->loc, "Cannot dereference non-pointer type %s",
                     type_to_string(ptr_type));
            s->had_error = true;
            return make_type(TYPE_UNKNOWN, NULL, s->arena);
        }
        return ptr_type->pointee ? ptr_type->pointee : make_type(TYPE_UNKNOWN, NULL, s->arena);
    }

    case AST_STRUCT_INIT: {
        // Look up struct type - handle generics
        char *type_name = expr->as.struct_init.type_name;

        // If there are type arguments, construct instantiated type name
        if (expr->as.struct_init.type_args.count > 0) {
            // Build name like "Vec_s32"
            size_t total_len = strlen(type_name);
            for (size_t i = 0; i < expr->as.struct_init.type_args.count; i++) {
                total_len += strlen(expr->as.struct_init.type_args.items[i].name) + 1; // +1 for '_'
            }

            char *new_name = (char *)arena_alloc(s->arena, total_len + 1);
            strcpy(new_name, type_name);
            for (size_t i = 0; i < expr->as.struct_init.type_args.count; i++) {
                strcat(new_name, "_");
                strcat(new_name, expr->as.struct_init.type_args.items[i].name);
            }
            type_name = new_name;
        }

        Symbol *sym = lookup_symbol(s, type_name);
        if (!sym || sym->kind != SYM_STRUCT) {
            log_error(expr->loc, "Unknown struct type '%s'", type_name);
            s->had_error = true;
            return make_type(TYPE_UNKNOWN, NULL, s->arena);
        }
        return sym->type;
    }

    case AST_MEMBER_ACCESS: {
        // Get the type of the object
        Type *obj_type = infer_expr_type(s, expr->as.member_access.object);

        // Look up the struct definition to find the field type
        if (obj_type->kind == TYPE_STRUCT) {
            Symbol *struct_sym = lookup_symbol(s, obj_type->name);
            if (struct_sym && struct_sym->kind == SYM_STRUCT) {
                // Find the field in the struct definition
                Stmt *struct_def = (Stmt *)obj_type->user_data;
                if (struct_def && struct_def->type == STMT_STRUCT_DEF) {
                    for (size_t i = 0; i < struct_def->as.struct_def.fields.count; i++) {
                        StructField *field = &struct_def->as.struct_def.fields.items[i];
                        if (strcmp(field->name, expr->as.member_access.member_name) == 0) {
                            return type_from_string(field->type, s->arena);
                        }
                    }
                }
            }
            log_error(expr->loc, "Struct '%s' has no field '%s'",
                     obj_type->name, expr->as.member_access.member_name);
            s->had_error = true;
        } else {
            log_error(expr->loc, "Member access on non-struct type %s",
                     type_to_string(obj_type));
            s->had_error = true;
        }
        return make_type(TYPE_UNKNOWN, NULL, s->arena);
    }

    case AST_BLOCK_EXPR: {
        // Block expressions need to have a return statement
        // The type is the type of the return value
        if (expr->as.block_expr.statements.count == 0) {
            log_error(expr->loc, "Block expression must have at least one statement");
            s->had_error = true;
            return make_type(TYPE_UNKNOWN, NULL, s->arena);
        }

        // Check last statement for return
        Stmt *last = expr->as.block_expr.statements.items[expr->as.block_expr.statements.count - 1];
        if (last->type != STMT_RET) {
            log_error(expr->loc, "Block expression must end with a return statement");
            s->had_error = true;
            return make_type(TYPE_UNKNOWN, NULL, s->arena);
        }

        if (last->as.expr.expr) {
            return infer_expr_type(s, last->as.expr.expr);
        }
        return make_type(TYPE_VOID, "void", s->arena);
    }

    default:
        return make_type(TYPE_UNKNOWN, NULL, s->arena);
    }
}

bool check_block_has_return(Stmt *block) {
    if (!block || block->type != STMT_BLOCK) return false;
    if (block->as.block.statements.count == 0) return false;

    Stmt *last = block->as.block.statements.items[block->as.block.statements.count - 1];
    return last->type == STMT_RET;
}

static void analyze_stmt(Semantic *s, Stmt *stmt) {
    switch (stmt->type) {

    case STMT_LET: {
        Type *value_type = NULL;
        if (stmt->as.let.value) {
            analyze_expr(s, stmt->as.let.value);
            value_type = infer_expr_type(s, stmt->as.let.value);
        }

        Symbol sym = {0};
        sym.name = stmt->as.let.name;
        sym.kind = SYM_VAR;

        Type *declared_type = NULL;

        // Type inference: if no type specified, use inferred type
        if (stmt->as.let.type == NULL) {
            if (value_type) {
                declared_type = value_type;
            } else {
                log_error(stmt->loc, "Cannot infer type for variable '%s' without initializer",
                         sym.name);
                s->had_error = true;
                declared_type = make_type(TYPE_UNKNOWN, NULL, s->arena);
            }
        } else {
            // Type was explicitly specified
            declared_type = type_from_string(stmt->as.let.type, s->arena);

            // Type check with assignment compatibility
            if (value_type && declared_type->kind != TYPE_UNKNOWN) {
                if (!type_can_assign_to(value_type, declared_type)) {
                    log_error(stmt->loc, "Type mismatch: cannot assign %s to variable of type %s",
                             type_to_string(value_type), type_to_string(declared_type));
                    s->had_error = true;
                }
            }
        }

        sym.type = declared_type;

        if (!define_symbol(s, sym)) {
            log_error(stmt->loc, "Duplicate variable '%s'", sym.name);
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

    case STMT_IF: {
        // Analyze condition
        analyze_expr(s, stmt->as.if_stmt.condition);

        // Type check condition - should be bool/s8 OR pointer (NULL check)
        Type *cond_type = infer_expr_type(s, stmt->as.if_stmt.condition);
        if (cond_type->kind != TYPE_BOOL && cond_type->kind != TYPE_S8 &&
            cond_type->kind != TYPE_POINTER && cond_type->kind != TYPE_UNKNOWN) {
            log_error(stmt->loc, "If condition must be s8/bool or pointer type, got %s",
                     type_to_string(cond_type));
            s->had_error = true;
        }

        // Analyze branches
        analyze_stmt(s, stmt->as.if_stmt.then_branch);
        if (stmt->as.if_stmt.else_branch) {
            analyze_stmt(s, stmt->as.if_stmt.else_branch);
        }
    } break;

    case STMT_FOR: {
        enter_scope(s); // for loop creates its own scope
        s->in_loop++;  // Track that we're in a loop

        // Analyze init
        if (stmt->as.for_stmt.init) {
            analyze_stmt(s, stmt->as.for_stmt.init);
        }

        // Analyze condition
        if (stmt->as.for_stmt.condition) {
            analyze_expr(s, stmt->as.for_stmt.condition);

            // Type check condition - should be bool/s8
            Type *cond_type = infer_expr_type(s, stmt->as.for_stmt.condition);
            if (cond_type->kind != TYPE_BOOL && cond_type->kind != TYPE_S8 &&
                cond_type->kind != TYPE_UNKNOWN) {
                log_error(stmt->loc, "For loop condition must be s8/bool type, got %s",
                         type_to_string(cond_type));
                s->had_error = true;
            }
        }

        // Analyze increment
        if (stmt->as.for_stmt.increment) {
            analyze_expr(s, stmt->as.for_stmt.increment);
        }

        // Analyze body
        analyze_stmt(s, stmt->as.for_stmt.body);

        s->in_loop--;  // Exit loop
        leave_scope(s);
    } break;

    case STMT_BREAK:
        if (s->in_loop == 0) {
            log_error(stmt->loc, "Break statement outside of loop");
            s->had_error = true;
        }
        break;

    case STMT_CONTINUE:
        if (s->in_loop == 0) {
            log_error(stmt->loc, "Continue statement outside of loop");
            s->had_error = true;
        }
        break;

    case STMT_STRUCT_DEF: {
        // Check if this is a generic template
        if (stmt->as.struct_def.type_params.count > 0) {
            // This is a template - store it but don't register yet
            // It will be instantiated when used with concrete types

            // For now, just register the template name so we know it exists
            Symbol sym = {0};
            sym.name = stmt->as.struct_def.name;
            sym.kind = SYM_TYPE;  // Mark as template type
            sym.type = make_type(TYPE_UNKNOWN, stmt->as.struct_def.name, s->arena);
            sym.type->user_data = stmt;  // Store template definition

            if (!define_symbol(s, sym)) {
                log_error(stmt->loc, "Duplicate template definition '%s'", sym.name);
                s->had_error = true;
            }
        } else {
            // Regular struct - define immediately
            Symbol sym = {0};
            sym.name = stmt->as.struct_def.name;
            sym.kind = SYM_STRUCT;

            Type *struct_type = make_type(TYPE_STRUCT, stmt->as.struct_def.name, s->arena);
            struct_type->user_data = stmt;  // Store reference to definition
            sym.type = struct_type;

            if (!define_symbol(s, sym)) {
                log_error(stmt->loc, "Duplicate struct definition '%s'", sym.name);
                s->had_error = true;
            }
        }
    } break;

    case STMT_ENUM_DEF: {
        // Define enum type in symbol table
        Symbol sym = {0};
        sym.name = stmt->as.enum_def.name;
        sym.kind = SYM_ENUM;

        Type *enum_type = make_type(TYPE_ENUM, stmt->as.enum_def.name, s->arena);
        enum_type->user_data = &stmt->as.enum_def;  // Store reference to definition
        sym.type = enum_type;

        if (!define_symbol(s, sym)) {
            log_error(stmt->loc, "Duplicate enum definition '%s'", sym.name);
            s->had_error = true;
        }

        // Also define each enum variant as a constant
        for (size_t i = 0; i < stmt->as.enum_def.variants.count; i++) {
            Symbol variant_sym = {0};
            variant_sym.name = stmt->as.enum_def.variants.items[i].name;
            variant_sym.kind = SYM_VAR;  // Treat as constant for now
            variant_sym.type = enum_type;

            if (!define_symbol(s, variant_sym)) {
                log_error(stmt->loc, "Duplicate enum variant '%s'", variant_sym.name);
                s->had_error = true;
            }
        }
    } break;

    case STMT_RET:
        if (stmt->as.expr.expr) {
            analyze_expr(s, stmt->as.expr.expr);
        }
        break;

    case STMT_EXPR:
        analyze_expr(s, stmt->as.expr.expr);
        break;

    default:
        break;
    }
}

static void analyze_expr(Semantic *s, Expr *expr) {
    if (!expr) return;

    switch (expr->type) {

    case AST_IDENTIFIER: {
        Symbol *sym = lookup_symbol(s, expr->as.identifier);

        if (!sym) {
            log_error(expr->loc, "Undefined identifier '%s'", expr->as.identifier);
            s->had_error = true;
        }
    } break;

    case AST_BINARY_OP:
        analyze_expr(s, expr->as.binary.left);
        analyze_expr(s, expr->as.binary.right);

        // Type checking is done in infer_expr_type
        (void)infer_expr_type(s, expr);
        break;

    case AST_UNARY_OP:
        analyze_expr(s, expr->as.unary.right);
        break;

    case AST_ASSIGN: {
        Symbol *sym = lookup_symbol(s, expr->as.assign.name);
        if (!sym) {
            log_error(expr->loc, "Cannot assign to undefined variable '%s'", expr->as.assign.name);
            s->had_error = true;
        }

        analyze_expr(s, expr->as.assign.value);

        // Type check assignment with compatibility
        if (sym && sym->type) {
            Type *value_type = infer_expr_type(s, expr->as.assign.value);
            if (!type_can_assign_to(value_type, sym->type)) {
                log_error(expr->loc, "Type mismatch: cannot assign %s to variable of type %s",
                         type_to_string(value_type), type_to_string(sym->type));
                s->had_error = true;
            }
        }
    } break;

    case AST_CALL:
        analyze_expr(s, expr->as.call.callee);

        for (size_t i = 0; i < expr->as.call.args.count; i++) {
            analyze_expr(s, expr->as.call.args.items[i]);
        }
        break;

    case AST_CAST:
        // Analyze the expression being cast
        analyze_expr(s, expr->as.cast.expr);
        // TODO: validate that the cast is valid
        break;

    case AST_SIZEOF:
        // If it's sizeof(expr), analyze the expression
        if (expr->as.size_of.expr) {
            analyze_expr(s, expr->as.size_of.expr);
        }
        // sizeof(type) doesn't need analysis
        break;

    case AST_ADDR_OF:
        analyze_expr(s, expr->as.addr_of.expr);
        // TODO: check that expr is an lvalue
        break;

    case AST_DEREF:
        analyze_expr(s, expr->as.deref.expr);
        break;

    case AST_STRUCT_INIT:
        // Verify struct exists and validate field initializers
        {
            char *type_name = expr->as.struct_init.type_name;

            // Handle generics - build instantiated name if needed
            if (expr->as.struct_init.type_args.count > 0) {
                char *inst_name = build_instantiated_name(
                    type_name, &expr->as.struct_init.type_args, s->arena);

                // Check if this instantiation already exists
                Symbol *inst_sym = lookup_symbol(s, inst_name);
                if (!inst_sym || inst_sym->kind != SYM_STRUCT) {
                    // Need to instantiate the template
                    Symbol *template_sym = lookup_symbol(s, type_name);
                    if (!template_sym || template_sym->kind != SYM_TYPE) {
                        log_error(expr->loc, "Unknown template type '%s'", type_name);
                        s->had_error = true;
                        break;
                    }

                    // Get template definition
                    Stmt *template_def = (Stmt *)template_sym->type->user_data;
                    if (!template_def) {
                        log_error(expr->loc, "Invalid template definition for '%s'", type_name);
                        s->had_error = true;
                        break;
                    }

                    // Instantiate the template
                    Stmt *instance = instantiate_template(s, template_def, &expr->as.struct_init.type_args);
                    if (!instance) {
                        log_error(expr->loc, "Failed to instantiate template '%s'", type_name);
                        s->had_error = true;
                        break;
                    }

                    // Register the instantiated type
                    Symbol inst_symbol = {0};
                    inst_symbol.name = inst_name;
                    inst_symbol.kind = SYM_STRUCT;

                    Type *struct_type = make_type(TYPE_STRUCT, inst_name, s->arena);
                    struct_type->user_data = instance;
                    inst_symbol.type = struct_type;

                    if (!define_symbol(s, inst_symbol)) {
                        // Already defined (shouldn't happen but handle gracefully)
                    }

                    // Store in generated instances for codegen
                    if (s->generated_instances) {
                        da_append(s->generated_instances, instance);
                    }
                }

                type_name = inst_name;
            }

            Symbol *sym = lookup_symbol(s, type_name);
            if (!sym || sym->kind != SYM_STRUCT) {
                log_error(expr->loc, "Unknown struct type '%s'", type_name);
                s->had_error = true;
                break;
            }

            // Analyze field values
            for (size_t i = 0; i < expr->as.struct_init.field_inits.count; i++) {
                analyze_expr(s, expr->as.struct_init.field_inits.items[i].value);
            }
        }
        break;

    case AST_MEMBER_ACCESS:
        // Analyze the object expression
        analyze_expr(s, expr->as.member_access.object);
        // Member name will be validated in infer_expr_type
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
            sym.type = type_from_string(p->type, s->arena);

            if (!define_symbol(s, sym)) {
                log_error(expr->loc, "Duplicate parameter '%s'", p->name);
                s->had_error = true;
            }
        }

        // analyze body
        analyze_stmt(s, expr->as.function.body);

        leave_scope(s);

    } break;

    case AST_BLOCK_EXPR: {
        // Block expressions create a new scope
        enter_scope(s);

        for (size_t i = 0; i < expr->as.block_expr.statements.count; i++) {
            analyze_stmt(s, expr->as.block_expr.statements.items[i]);
        }

        // Check that block ends with return
        if (!check_block_has_return((Stmt *)expr)) {
            // Actually need to check the statements list
            if (expr->as.block_expr.statements.count == 0) {
                log_error(expr->loc, "Block expression cannot be empty");
                s->had_error = true;
            } else {
                Stmt *last = expr->as.block_expr.statements.items[expr->as.block_expr.statements.count - 1];
                if (last->type != STMT_RET) {
                    log_error(expr->loc, "Block expression must end with a return statement");
                    s->had_error = true;
                }
            }
        }

        leave_scope(s);
    } break;

    case AST_LITERAL_INT:
    case AST_LITERAL_FLOAT:
    case AST_LITERAL_STRING:
        // Literals don't need additional analysis
        break;

    default:
        break;
    }
}

void semantic_check(Semantic *s, Statements *p) {
    s->in_loop = 0;  // Initialize loop counter

    // Initialize generated instances storage
    Statements generated = {0};
    s->generated_instances = &generated;

    enter_scope(s); // global scope

    for (size_t i = 0; i < p->count; i++) {
        analyze_stmt(s, p->items[i]);
    }

    leave_scope(s);
}
