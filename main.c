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

    Tokens tokens = {0};
    parse_tokens(&sb, &tokens);
    for (size_t i = 0; i < tokens.count; i++) {
        if (tokens.items[i].tk == T_NUM || tokens.items[i].tk == T_IDENT) {
            printf("Token kind: %d -> %s\n", tokens.items[i].tk, tokens.items[i].data.String);
        }
        else {
            printf("Token kind: %d\n", tokens.items[i].tk);
        }
    }
    return 0;
}
