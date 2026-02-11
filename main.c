#define ARENA_IMPLEMENTATION
#include "arena.h"
#undef ARENA_IMPLEMENTATION

#include "utils.h"
#include "token.h"

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
    parse_tokens_v2(&sb, &tokens);
    for (size_t i = 0; i < tokens.count; i++) {
        Token *tok = &tokens.items[i];
        printf("Token kind: %d -> ", tok->tk);

        switch (tok->tk) {
        case T_IDENT:
        case T_STR:
            printf("%s", tok->data.String);
            break;
        case T_UNUM:
            printf("%lu", tok->data.Uint64);
            break;
        case T_NUM:
            printf("%ld", tok->data.Int64);
            break;
        case T_FLO:
            printf("%f", tok->data.F64);
            break;
        default:
            printf("(symbol)");
            break;
        }

        printf("\n");
    }
    return 0;
}
