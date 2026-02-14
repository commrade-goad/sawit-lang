#include "semantic.h"
#include "ast.h"
#include <string.h>

// @TODO: not done yet:
// - Function return name check right now it just a free style but when struct done impl it should not be!
// - Return type checking (function return matches declaration) -- Return type checking - Check that return expressions match function's declared return type
// - Undefined function detection -- Function vs variable distinction - Ensure identifiers used as functions are actually functions
// - rest of the stuff: struct, enum, array bound(index), control flow (break, etc)
// - function defined below will not be usable at the top i want to fix that
// - module system will benefit by this ^
//   so we need to do the typechecking twice for the funciton stuff so if its still on the same scope or on the parent scope then allow it to be Undefined for a while until last part of semantic chekcing if that thing still not available then error out!
// - semantic check twice!
// - const !
// - pointer type okay?
// - dereference, address

// LATER
// - type aliasing so i can alias u32 to FLAG or something!
// - if as n expr (later to make it not needed to use tenary op
// - enum can be a union will be nice (not supported for now idk what is a good syntax for this)
// - namespace on enum will be nice later on (not right now!)

// ============================================================================
// Type Creation Functions
// ============================================================================

Type *make_type(TypeKind kind, const char *name, Arena *a) {
    Type *t = (Type *)arena_alloc(a, sizeof(Type));
    t->kind = kind;

    if (name) {
        size_t len = strlen(name);
        t->name = (char *)arena_alloc(a, len + 1);
        strcpy(t->name, name);
    } else {
        t->name = NULL;
    }

    return t;
}

Type *make_pointer_type(Type *pointee, Arena *a) {
    Type *ptr = make_type(TYPE_POINTER, NULL, a);
    ptr->as.pointee = pointee;

    // Build name like "*u32"
    if (pointee && pointee->name) {
        size_t len = strlen(pointee->name) + 2;
        char *name = (char *)arena_alloc(a, len);
        snprintf(name, len, "*%s", pointee->name);
        ptr->name = name;
    } else {
        ptr->name = "*unknown";
    }

    return ptr;
}

Type *make_function_type(Type *return_type, Type **param_types, size_t param_count, Arena *a) {
    Type *fn_type = make_type(TYPE_FUNCTION, NULL, a);

    FunctionType *fn = (FunctionType *)arena_alloc(a, sizeof(FunctionType));
    fn->return_type = return_type;
    fn->param_count = param_count;

    if (param_count > 0) {
        fn->param_types = (Type **)arena_alloc(a, sizeof(Type *) * param_count);
        for (size_t i = 0; i < param_count; i++) {
            fn->param_types[i] = param_types[i];
        }
    } else {
        fn->param_types = NULL;
    }

    fn_type->as.function = fn;

    String_Builder sb = {0};
    sb_appendf(&sb, "fn(");
    for (size_t i = 0; i < fn->param_count; i++) {
        sb_appendf(&sb, "%s", fn->param_types[i]->name);
        if (i + 1 < fn->param_count) sb_appendf(&sb, ", ");
    }
    sb_appendf(&sb, ") -> %s", fn->return_type->name);

    size_t size = sizeof(char) * (sb.count + 1);
    char *copy = arena_alloc(a, size);
    strncpy(copy, sb.items, sb.count);
    copy[sb.count] = '\0';
    fn_type->name = copy;

    da_free(sb);

    return fn_type;
}

Type *make_array_type(Type *element_type, size_t size, Arena *a) {
    Type *arr = make_type(TYPE_ARRAY, NULL, a);
    arr->as.array.element_type = element_type;
    arr->as.array.size = size;

    // Build name like "[10]u32" or "[]u32"
    if (element_type && element_type->name) {
        char buf[256];
        if (size > 0) {
            snprintf(buf, sizeof(buf), "[%zu]%s", size, element_type->name);
        } else {
            snprintf(buf, sizeof(buf), "[]%s", element_type->name);
        }
        size_t len = strlen(buf);
        arr->name = (char *)arena_alloc(a, len + 1);
        strcpy(arr->name, buf);
    }

    return arr;
}

// ============================================================================
// Type Parsing and Conversion
// ============================================================================

Type *type_from_string(Semantic *s, const char *name) {
    if (!name || strcmp(name, "comptimecheck") == 0) {
        return NULL;  // Will be inferred
    }

    // Check for pointer type: *TypeName
    if (name[0] == '*') {
        Type *pointee = type_from_string(s, name + 1);
        if (!pointee) return NULL;
        return make_pointer_type(pointee, s->arena);
    }

    // Check for array type: [N]TypeName or []TypeName
    if (name[0] == '[') {
        const char *close = strchr(name, ']');
        if (!close) return NULL;

        size_t size = 0;
        if (close - name > 1) {
            // Parse the number between [ and ]
            char *size_str = (char *)arena_alloc(s->arena, (close - name));
            strncpy(size_str, name + 1, close - name - 1);
            size_str[close - name - 1] = '\0';
            size = (size_t)atoi(size_str);
        }

        // Get element type
        Type *element = type_from_string(s, close + 1);
        if (!element) return NULL;

        return make_array_type(element, size, s->arena);
    }

    // Primitive types
    if (strcmp(name, "u8") == 0)   return make_type(TYPE_U8, name, s->arena);
    if (strcmp(name, "u16") == 0)  return make_type(TYPE_U16, name, s->arena);
    if (strcmp(name, "u32") == 0)  return make_type(TYPE_U32, name, s->arena);
    if (strcmp(name, "u64") == 0)  return make_type(TYPE_U64, name, s->arena);
    if (strcmp(name, "s8") == 0)   return make_type(TYPE_S8, name, s->arena);
    if (strcmp(name, "s16") == 0)  return make_type(TYPE_S16, name, s->arena);
    if (strcmp(name, "s32") == 0)  return make_type(TYPE_S32, name, s->arena);
    if (strcmp(name, "s64") == 0)  return make_type(TYPE_S64, name, s->arena);
    if (strcmp(name, "f32") == 0)  return make_type(TYPE_F32, name, s->arena);
    if (strcmp(name, "f64") == 0)  return make_type(TYPE_F64, name, s->arena);
    if (strcmp(name, "char") == 0) return make_type(TYPE_CHAR, name, s->arena);
    if (strcmp(name, "bool") == 0) return make_type(TYPE_BOOL, name, s->arena);
    if (strcmp(name, "void") == 0) return make_type(TYPE_VOID, name, s->arena);

    // Look up user-defined type (enum, struct, type alias)
    Symbol *sym = lookup_symbol(s, name);
    if (sym && sym->kind == SYM_TYPE) {
        return sym->type;
    }

    // Unknown type - return NULL to trigger error
    return NULL;
}

const char *type_to_string(Type *t) {
    if (!t) return "unknown";
    if (t->name) return t->name;

    switch (t->kind) {
    case TYPE_U8:       return "u8";
    case TYPE_U16:      return "u16";
    case TYPE_U32:      return "u32";
    case TYPE_U64:      return "u64";
    case TYPE_S8:       return "s8";
    case TYPE_S16:      return "s16";
    case TYPE_S32:      return "s32";
    case TYPE_S64:      return "s64";
    case TYPE_F32:      return "f32";
    case TYPE_F64:      return "f64";
    case TYPE_CHAR:     return "char";
    case TYPE_BOOL:     return "bool";
    case TYPE_VOID:     return "void";
    case TYPE_POINTER:  return t->name ? t->name : "*unknown";
    case TYPE_FUNCTION: return t->name ? t->name : "function";
    case TYPE_ARRAY:    return t->name ? t->name : "array";
    case TYPE_STRUCT:   return t->name ? t->name : "struct";
    case TYPE_ENUM:     return t->name ? t->name : "enum";
    case TYPE_UNKNOWN:  return "unknown";
    default:            return "unknown";
    }
}

// ============================================================================
// Type Size Calculation
// ============================================================================

static size_t get_type_size(Type *t) {
    if (!t) return 0;

    switch (t->kind) {
    case TYPE_U8:
    case TYPE_S8:
    case TYPE_CHAR:
    case TYPE_BOOL:
        return 1;

    case TYPE_U16:
    case TYPE_S16:
        return 2;

    case TYPE_U32:
    case TYPE_S32:
    case TYPE_F32:
        return 4;

    case TYPE_U64:
    case TYPE_S64:
    case TYPE_F64:
        return 8;

    case TYPE_POINTER:
        return 8; // Assuming 64-bit pointers

    case TYPE_ARRAY:
        if (t->as.array.size > 0) {
            return t->as.array.size * get_type_size(t->as.array.element_type);
        }
        return 8; // array is NOT Dynamic array

    case TYPE_FUNCTION:
        return 8; // Function pointer

    case TYPE_VOID:
    case TYPE_UNKNOWN:
    default:
        return 0;
    }
}

// ============================================================================
// Type Comparison
// ============================================================================

bool types_equal(Type *a, Type *b) {
    if (!a || !b) return false;
    if (a == b) return true;

    // Handle aliases
    if ((a->kind == TYPE_CHAR && b->kind == TYPE_U8) ||
        (a->kind == TYPE_U8 && b->kind == TYPE_CHAR)) {
        return true;
    }

    if ((a->kind == TYPE_BOOL && b->kind == TYPE_S8) ||
        (a->kind == TYPE_S8 && b->kind == TYPE_BOOL)) {
        return true;
    }

    if (a->kind != b->kind) return false;

    // For pointers, check pointee types
    if (a->kind == TYPE_POINTER) {
        return types_equal(a->as.pointee, b->as.pointee);
    }

    // For functions, check return type and parameters
    if (a->kind == TYPE_FUNCTION) {
        FunctionType *fn_a = a->as.function;
        FunctionType *fn_b = b->as.function;

        if (!types_equal(fn_a->return_type, fn_b->return_type)) return false;
        if (fn_a->param_count != fn_b->param_count) return false;

        for (size_t i = 0; i < fn_a->param_count; i++) {
            if (!types_equal(fn_a->param_types[i], fn_b->param_types[i])) {
                return false;
            }
        }
        return true;
    }

    // For arrays, check element type and size
    if (a->kind == TYPE_ARRAY) {
        if (a->as.array.size != b->as.array.size) return false;
        return types_equal(a->as.array.element_type, b->as.array.element_type);
    }

    return true;
}

// Check if types are compatible for implicit conversion
static bool is_signed_integer(Type *t) {
    return t->kind == TYPE_S8 || t->kind == TYPE_S16 ||
           t->kind == TYPE_S32 || t->kind == TYPE_S64;
}

static bool is_unsigned_integer(Type *t) {
    return t->kind == TYPE_U8 || t->kind == TYPE_U16 ||
           t->kind == TYPE_U32 || t->kind == TYPE_U64 ||
           t->kind == TYPE_CHAR;
}

static bool is_integer(Type *t) {
    return is_signed_integer(t) || is_unsigned_integer(t) || t->kind == TYPE_BOOL;
}

static bool is_float(Type *t) {
    return t->kind == TYPE_F32 || t->kind == TYPE_F64;
}

static bool is_numeric(Type *t) {
    return is_integer(t) || is_float(t);
}

// Check if value_type can be safely converted to target_type
bool type_can_assign_to(Type *value_type, Type *target_type, bool is_init) {
    if (!value_type || !target_type) return false;

    // Exact match is always OK
    if (types_equal(value_type, target_type)) return true;

    if (is_init &&
        value_type->kind == TYPE_ARRAY &&
        target_type->kind == TYPE_ARRAY &&
        value_type->as.array.element_type->kind == target_type->as.array.element_type->kind)
    {
        // target is the left side and value is the right side
        target_type->as.array.size = value_type->as.array.size;
        target_type->name = value_type->name;
        return true;
    }

    // Integer to integer conversions
    if (is_integer(value_type) && is_integer(target_type)) {
        size_t value_size = get_type_size(value_type);
        size_t target_size = get_type_size(target_type);

        // Allow widening conversions (smaller to larger)
        if (value_size <= target_size) {
            // @TODO: Warn if mixing signed/unsigned but allow it
            return true;
        }

        // Narrowing conversions are allowed but should warn
        // @TODO: Add warning system
        return true;
    }

    // Float to float conversions
    if (is_float(value_type) && is_float(target_type)) {
        // @TODO: Allow f32 -> f64 (widening), warn on f64 -> f32 (narrowing)
        return true;
    }

    // Integer to float is allowed (implicit conversion)
    if (is_integer(value_type) && is_float(target_type)) {
        return true;
    }

    // Pointer type compatibility
    if (value_type->kind == TYPE_POINTER && target_type->kind == TYPE_POINTER) {
        // void* can be assigned to any pointer and vice versa
        if (value_type->as.pointee->kind == TYPE_VOID ||
            target_type->as.pointee->kind == TYPE_VOID) {
            return true;
        }

        // Otherwise, pointee types must match
        return types_equal(value_type->as.pointee, target_type->as.pointee);
    }

    return false;
}

// ============================================================================
// Type Inference
// ============================================================================

Type *infer_expr_type(Semantic *s, Expr *expr) {
    if (!expr) return make_type(TYPE_UNKNOWN, NULL, s->arena);

    switch (expr->type) {
    case AST_LITERAL_INT:
        // Default integer literals to s32
        return make_type(TYPE_S32, "s32", s->arena);

    case AST_LITERAL_FLOAT:
        // Default float literals to f64
        return make_type(TYPE_F64, "f64", s->arena);

    case AST_LITERAL_STRING: {
        // Strings are [N]u8 (array of chars)
        size_t len = strlen(expr->as.identifier) + 1; // +1 for null terminator
        return make_array_type(make_type(TYPE_U8, "u8", s->arena), len, s->arena);
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

        TokenKind op = expr->as.binary.op;

        // Comparison operators return bool
        if (op == T_EQ || op == T_NEQ || op == T_LT ||
            op == T_GT || op == T_LTE || op == T_GTE) {
            return make_type(TYPE_BOOL, "bool", s->arena);
        }

        // Logical operators return bool
        if (op == T_AND || op == T_OR) {
            return make_type(TYPE_BOOL, "bool", s->arena);
        }

        // Bitwise operators preserve integer type
        if (op == T_BIT_AND || op == T_BIT_OR || op == T_BIT_XOR ||
            op == T_LSHIFT || op == T_RSHIFT || op == T_BIT_NOT) {
            // Return the larger of the two types
            size_t left_size = get_type_size(left_type);
            size_t right_size = get_type_size(right_type);
            return (left_size >= right_size) ? left_type : right_type;
        }

        // Arithmetic operators: perform type promotion
        if (op == T_PLUS || op == T_MIN || op == T_STAR ||
            op == T_DIV || op == T_MOD) {

            // If either is float, result is float
            if (is_float(left_type) || is_float(right_type)) {
                // Promote to larger float type
                if (left_type->kind == TYPE_F64 || right_type->kind == TYPE_F64) {
                    return make_type(TYPE_F64, "f64", s->arena);
                }
                return make_type(TYPE_F32, "f32", s->arena);
            }

            // Both are integers, return the larger type
            size_t left_size = get_type_size(left_type);
            size_t right_size = get_type_size(right_type);

            // If sizes are equal, prefer unsigned
            if (left_size == right_size) {
                if (is_unsigned_integer(left_type)) return left_type;
                if (is_unsigned_integer(right_type)) return right_type;
            }

            return (left_size >= right_size) ? left_type : right_type;
        }

        // Default: return left type
        return left_type;
    }

    case AST_UNARY_OP: {
        Type *operand_type = infer_expr_type(s, expr->as.unary.right);

        // NOT operator returns bool
        if (expr->as.unary.op == T_NOT) {
            return make_type(TYPE_BOOL, "bool", s->arena);
        }

        // Bitwise NOT preserves type
        if (expr->as.unary.op == T_BIT_NOT) {
            return operand_type;
        }

        // Unary minus/plus preserve type
        return operand_type;
    }

    case AST_ASSIGN: {
        Symbol *sym = lookup_symbol(s, expr->as.assign.name);
        if (sym && sym->type) {
            return sym->type;
        }
        return infer_expr_type(s, expr->as.assign.value);
    }

    case AST_FUNCTION: {
        // Build function type from return type and parameters
        Type *return_type = type_from_string(s, expr->as.function.ret);
        if (!return_type) {
            // If return type is unknown, it's an error (not void)
            return make_type(TYPE_UNKNOWN, NULL, s->arena);
        }

        size_t param_count = expr->as.function.params.count;
        Type **param_types = NULL;

        if (param_count > 0) {
            param_types = (Type **)arena_alloc(s->arena, sizeof(Type *) * param_count);
            for (size_t i = 0; i < param_count; i++) {
                param_types[i] = type_from_string(s, expr->as.function.params.items[i].type);
                if (!param_types[i]) {
                    param_types[i] = make_type(TYPE_UNKNOWN, NULL, s->arena);
                }
            }
        }

        return make_function_type(return_type, param_types, param_count, s->arena);
    }

    case AST_CALL: {
        Type *callee_type = infer_expr_type(s, expr->as.call.callee);
        if (callee_type && callee_type->kind == TYPE_FUNCTION) {
            return callee_type->as.function->return_type;
        }
        return make_type(TYPE_UNKNOWN, NULL, s->arena);
    }

    default:
        return make_type(TYPE_UNKNOWN, NULL, s->arena);
    }
}

// ============================================================================
// Scope Management
// ============================================================================

void enter_scope(Semantic *s) {
    Scope *scope = (Scope *)arena_alloc(s->arena, sizeof(Scope));
    scope->symbols = (Symbols){0};
    scope->parent = s->current_scope;
    s->current_scope = scope;
}

void leave_scope(Semantic *s) {
    if (s->current_scope) {
        s->current_scope = s->current_scope->parent;
    }
}

// ============================================================================
// Symbol Table Operations
// ============================================================================

bool define_symbol(Semantic *s, Symbol symbol) {
    Scope *scope = s->current_scope;

    // Check for duplicates in current scope only
    for (size_t i = 0; i < scope->symbols.count; i++) {
        if (strcmp(scope->symbols.items[i].name, symbol.name) == 0) {
            return false;  // Duplicate
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

// ============================================================================
// Statement and Expression Analysis
// ============================================================================


static void analyze_expr(Semantic *s, Expr *expr);
static void analyze_stmt(Semantic *s, Stmt *stmt);

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

    case AST_BINARY_OP: {
        analyze_expr(s, expr->as.binary.left);
        analyze_expr(s, expr->as.binary.right);

        Type *left_type = infer_expr_type(s, expr->as.binary.left);
        Type *right_type = infer_expr_type(s, expr->as.binary.right);

        TokenKind op = expr->as.binary.op;

        // Type checking for arithmetic operators
        if (op == T_PLUS || op == T_MIN || op == T_STAR || op == T_DIV || op == T_MOD) {
            if (!is_numeric(left_type)) {
                log_error(expr->loc, "Left operand of arithmetic operation must be numeric, got %s",
                         type_to_string(left_type));
                s->had_error = true;
            }
            if (!is_numeric(right_type)) {
                log_error(expr->loc, "Right operand of arithmetic operation must be numeric, got %s",
                         type_to_string(right_type));
                s->had_error = true;
            }

            // Modulo requires integers
            if (op == T_MOD && (is_float(left_type) || is_float(right_type))) {
                log_error(expr->loc, "Modulo operator requires integer operands");
                s->had_error = true;
            }
        }

        // Type checking for bitwise operators
        if (op == T_BIT_AND || op == T_BIT_OR || op == T_BIT_XOR ||
            op == T_LSHIFT || op == T_RSHIFT) {
            if (!is_integer(left_type)) {
                log_error(expr->loc, "Left operand of bitwise operation must be integer, got %s",
                         type_to_string(left_type));
                s->had_error = true;
            }
            if (!is_integer(right_type)) {
                log_error(expr->loc, "Right operand of bitwise operation must be integer, got %s",
                         type_to_string(right_type));
                s->had_error = true;
            }
        }

        // Type checking for comparison operators
        if (op == T_EQ || op == T_NEQ || op == T_LT || op == T_GT || op == T_LTE || op == T_GTE) {
            // Operands should be comparable
            if (!types_equal(left_type, right_type) &&
                !type_can_assign_to(right_type, left_type, false) &&
                !type_can_assign_to(left_type, right_type, false)) {
                log_error(expr->loc, "Cannot compare incompatible types: %s and %s",
                         type_to_string(left_type), type_to_string(right_type));
                s->had_error = true;
            }
        }

        // Type checking for logical operators
        if (op == T_AND || op == T_OR) {
            // Operands should be boolean-ish (integers, bools, or pointers)
            bool left_ok = is_integer(left_type) || left_type->kind == TYPE_POINTER;
            bool right_ok = is_integer(right_type) || right_type->kind == TYPE_POINTER;

            if (!left_ok) {
                log_error(expr->loc, "Left operand of logical operation must be boolean/integer/pointer, got %s",
                         type_to_string(left_type));
                s->had_error = true;
            }
            if (!right_ok) {
                log_error(expr->loc, "Right operand of logical operation must be boolean/integer/pointer, got %s",
                         type_to_string(right_type));
                s->had_error = true;
            }
        }
    } break;

    case AST_UNARY_OP: {
        analyze_expr(s, expr->as.unary.right);

        Type *operand_type = infer_expr_type(s, expr->as.unary.right);
        TokenKind op = expr->as.unary.op;

        // Unary minus/plus requires numeric type
        if (op == T_MIN || op == T_PLUS) {
            if (!is_numeric(operand_type)) {
                log_error(expr->loc, "Unary %s requires numeric operand, got %s",
                         (op == T_MIN) ? "minus" : "plus",
                         type_to_string(operand_type));
                s->had_error = true;
            }
        }

        // Logical NOT requires boolean/integer/pointer
        if (op == T_NOT) {
            bool ok = is_integer(operand_type) || operand_type->kind == TYPE_POINTER;
            if (!ok) {
                log_error(expr->loc, "Logical NOT requires boolean/integer/pointer operand, got %s",
                         type_to_string(operand_type));
                s->had_error = true;
            }
        }

        // Bitwise NOT requires integer
        if (op == T_BIT_NOT) {
            if (!is_integer(operand_type)) {
                log_error(expr->loc, "Bitwise NOT requires integer operand, got %s",
                         type_to_string(operand_type));
                s->had_error = true;
            }
        }
    } break;

    case AST_ASSIGN: {
        Symbol *sym = lookup_symbol(s, expr->as.assign.name);
        if (!sym) {
            log_error(expr->loc, "Cannot assign to undefined variable '%s'",
                     expr->as.assign.name);
            s->had_error = true;
            break;
        }

        analyze_expr(s, expr->as.assign.value);

        Type *value_type = infer_expr_type(s, expr->as.assign.value);
        if (!type_can_assign_to(value_type, sym->type, false)) {
            log_error(expr->loc, "Type mismatch: cannot assign %s to variable of type %s",
                     type_to_string(value_type), type_to_string(sym->type));
            s->had_error = true;
        }
    } break;

    case AST_CALL: {
        analyze_expr(s, expr->as.call.callee);

        Type *callee_type = infer_expr_type(s, expr->as.call.callee);

        // Check if callee is actually a function
        if (callee_type->kind != TYPE_FUNCTION) {
            log_error(expr->loc, "Cannot call non-function type %s",
                     type_to_string(callee_type));
            s->had_error = true;
            break;
        }

        FunctionType *fn_type = callee_type->as.function;

        // Check argument count
        if (expr->as.call.args.count != fn_type->param_count) {
            log_error(expr->loc, "Function expects %zu arguments, got %zu",
                     fn_type->param_count, expr->as.call.args.count);
            s->had_error = true;
        }

        // Analyze and type-check each argument
        size_t arg_count = expr->as.call.args.count < fn_type->param_count
                          ? expr->as.call.args.count
                          : fn_type->param_count;

        for (size_t i = 0; i < arg_count; i++) {
            analyze_expr(s, expr->as.call.args.items[i]);

            Type *arg_type = infer_expr_type(s, expr->as.call.args.items[i]);
            Type *param_type = fn_type->param_types[i];

            if (!type_can_assign_to(arg_type, param_type, false)) {
                log_error(expr->as.call.args.items[i]->loc,
                         "Argument %zu: cannot pass %s to parameter of type %s",
                         i + 1, type_to_string(arg_type), type_to_string(param_type));
                s->had_error = true;
            }
        }
    } break;

    case AST_FUNCTION: {
        // Check return type exists
        Type *return_type = type_from_string(s, expr->as.function.ret);
        if (!return_type) {
            log_error(expr->loc, "Unknown return type '%s'", expr->as.function.ret);
            s->had_error = true;
        }

        enter_scope(s);

        // Define parameters in new scope
        for (size_t i = 0; i < expr->as.function.params.count; i++) {
            Param *p = &expr->as.function.params.items[i];

            Symbol sym = {0};
            sym.name = p->name;
            sym.kind = SYM_VAR;
            sym.type = type_from_string(s, p->type);
            sym.is_const = false;

            if (!sym.type) {
                log_error(expr->loc, "Unknown type '%s' for parameter '%s'",
                         p->type, p->name);
                s->had_error = true;
                sym.type = make_type(TYPE_UNKNOWN, NULL, s->arena);
            }

            if (!define_symbol(s, sym)) {
                log_error(expr->loc, "Duplicate parameter '%s'", p->name);
                s->had_error = true;
            }
        }

        // Analyze function body
        analyze_stmt(s, expr->as.function.body);

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

static void analyze_stmt(Semantic *s, Stmt *stmt) {
    if (!stmt) return;

    switch (stmt->type) {
    case STMT_LET: {
        // Analyze the value expression first
        if (stmt->as.let.value) {
            analyze_expr(s, stmt->as.let.value);
        }

        Symbol sym = {0};
        sym.name = stmt->as.let.name;
        sym.kind = SYM_VAR;
        sym.is_const = false;

        // Type inference or explicit type
        if (stmt->as.let.type && strcmp(stmt->as.let.type, "comptimecheck") != 0) {
            // Explicit type
            sym.type = type_from_string(s, stmt->as.let.type);
            if (!sym.type) {
                log_error(stmt->loc, "Unknown type '%s'", stmt->as.let.type);
                s->had_error = true;
                sym.type = make_type(TYPE_UNKNOWN, NULL, s->arena);
            }

            // Check if value matches declared type
            if (stmt->as.let.value) {
                // target is the left side and value is the right side
                Type *value_type = infer_expr_type(s, stmt->as.let.value);
                if (!type_can_assign_to(value_type, sym.type, true)) {
                    log_error(stmt->loc, "Type mismatch: cannot assign %s to variable of type %s",
                             type_to_string(value_type), type_to_string(sym.type));
                    s->had_error = true;
                }
            }
        } else {
            // Type inference
            if (stmt->as.let.value) {
                sym.type = infer_expr_type(s, stmt->as.let.value);
            } else {
                log_error(stmt->loc, "Cannot infer type for variable '%s' without initializer",
                         sym.name);
                s->had_error = true;
                sym.type = make_type(TYPE_UNKNOWN, NULL, s->arena);
            }
        }

        if (!define_symbol(s, sym)) {
            log_error(stmt->loc, "Duplicate variable '%s'", sym.name);
            s->had_error = true;
        }
    } break;

    case STMT_IF: {
        // Analyze condition - allows integers, bools, and pointers
        analyze_expr(s, stmt->as.if_stmt.condition);

        Type *cond_type = infer_expr_type(s, stmt->as.if_stmt.condition);
        bool cond_ok = is_integer(cond_type) || cond_type->kind == TYPE_POINTER;

        if (!cond_ok && cond_type->kind != TYPE_UNKNOWN) {
            log_error(stmt->loc, "If condition must be boolean/integer/pointer type, got %s",
                     type_to_string(cond_type));
            s->had_error = true;
        }

        // Analyze branches
        analyze_stmt(s, stmt->as.if_stmt.then_b);
        if (stmt->as.if_stmt.else_b) {
            analyze_stmt(s, stmt->as.if_stmt.else_b);
        }
    } break;

    case STMT_FOR: {
        enter_scope(s); // for loop creates its own scope

        // Analyze init
        if (stmt->as.for_stmt.init) {
            analyze_stmt(s, stmt->as.for_stmt.init);
        }

        // Analyze condition (if present)
        if (stmt->as.for_stmt.condition) {
            analyze_expr(s, stmt->as.for_stmt.condition);

            Type *cond_type = infer_expr_type(s, stmt->as.for_stmt.condition);
            bool cond_ok = is_integer(cond_type) || cond_type->kind == TYPE_POINTER;

            if (!cond_ok && cond_type->kind != TYPE_UNKNOWN) {
                log_error(stmt->loc, "For loop condition must be boolean/integer/pointer type, got %s",
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

        leave_scope(s);
    } break;

    case STMT_ENUM_DEF: {
        // Create enum type
        Type *enum_type = make_type(TYPE_ENUM, stmt->as.enum_def.name, s->arena);
        enum_type->as.user_data = stmt; // Store enum definition

        // Define enum type in symbol table
        Symbol enum_sym = {0};
        enum_sym.name = stmt->as.enum_def.name;
        enum_sym.kind = SYM_TYPE;
        enum_sym.type = enum_type;

        if (!define_symbol(s, enum_sym)) {
            log_error(stmt->loc, "Duplicate enum definition '%s'", enum_sym.name);
            s->had_error = true;
        }

        // Define each enum variant as a constant
        for (size_t i = 0; i < stmt->as.enum_def.variants.count; i++) {
            Symbol variant_sym = {0};
            variant_sym.name = stmt->as.enum_def.variants.items[i].name;
            variant_sym.kind = SYM_VAR;
            variant_sym.type = enum_type; // Variants have the enum type
            variant_sym.is_const = true;

            if (!define_symbol(s, variant_sym)) {
                log_error(stmt->loc, "Duplicate enum variant '%s'", variant_sym.name);
                s->had_error = true;
            }
        }
    } break;

    case STMT_BLOCK: {
        enter_scope(s);

        for (size_t i = 0; i < stmt->as.block.statements.count; i++) {
            analyze_stmt(s, stmt->as.block.statements.items[i]);
        }

        leave_scope(s);
    } break;

    case STMT_RET:
        if (stmt->as.expr.expr) {
            analyze_expr(s, stmt->as.expr.expr);
            // @TODO: Check return type matches function declaration
        }
        break;

    case STMT_EXPR:
        analyze_expr(s, stmt->as.expr.expr);
        break;

    default:
        break;
    }
}

// ============================================================================
// Main Entry Point
// ============================================================================

void semantic_check(Semantic *s, Statements *p) {
    enter_scope(s);  // Global scope

    for (size_t i = 0; i < p->count; i++) {
        analyze_stmt(s, p->items[i]);
    }

    leave_scope(s);
}
