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
        TokenKind tk = tokens.items[i].tk;
        switch (tk) {
            case T_STR:
            case T_NUM:
            case T_IDENT:
                {
                    printf("Token kind: %d -> %s\n", tk, tokens.items[i].data.String);
                } break;
            default:
                {
                    printf("Token kind: %d\n", tk);
                } break;
        }
    }
    return 0;
}
