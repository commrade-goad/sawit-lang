#define ARENA_IMPLEMENTATION
#include "arena.h"
#undef ARENA_IMPLEMENTATION

#include "utils.h"
#include "token.h"
#include "ast.h"

int main(int argc, char **argv) {
    if (argc <= 1) perr_exit("Not enought args");

    shift(argv, argc);
    const char *file = argv[0];
    String_Builder sb = {0};

    if (!read_entire_file(file, &sb))
        perr_exit("Failed to read the file `%s`", file);
    if (sb.count <= 1) perr_exit("Empty file");
    sb_append_null(&sb);

    Arena rarena = {0};
    if (arena_init(&rarena, ARENA_DEFAULT_SIZE) != 0) {
        perr_exit("Failed to allocate the runtime stack arena `%s`", strerror(errno));
    }
    Tokens tokens = {0};
    bool res = parse_tokens_v2(&sb, &tokens, file);
    if (!res) return -1;
    /*
    for (size_t i = 0; i < tokens.count; i++) {
        Token *tok = &tokens.items[i];
        printf("Pos: %s:%lu:%lu - Token kind: %d -> ", tok->loc.name, tok->loc.line, tok->loc.col, tok->tk);

        switch (tok->tk) {
        case T_IDENT:
        case T_STR:
            printf("%s", tok->data.String);
            break;
        case T_NUM:
            printf("%lu", tok->data.Uint64);
            break;
        case T_FLO:
            printf("%f", tok->data.F64);
            break;
        case T_EQUAL:
            printf("=");
            break;
        case T_OPARENT:
            printf("(");
            break;
        case T_CPARENT:
            printf(")");
            break;
        case T_OCPARENT:
            printf("{");
            break;
        case T_CCPARENT:
            printf("}");
            break;
        case T_CLOSING:
            printf("; (CLOSING)");
            break;
        default:
            printf("(symbol)");
            break;
        }

        printf("\n");
    }
    */
    Statements program = {0};
    make_ast(&rarena, &program, &tokens);
    for (size_t i = 0; i < program.count; i++) {
        print_stmt(program.items[i], 0);
    }
    return 0;
}
