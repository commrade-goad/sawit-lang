#define ARENA_IMPLEMENTATION
#include "arena.h"
#undef ARENA_IMPLEMENTATION

#include "utils.h"
#include "lexer.h"
#include "ast.h"
#include "semantic.h"

[[maybe_unused]] static inline void print_token(Tokens *tokens) {
    for (size_t i = 0; i < tokens->count; i++) {
        Token *tok = &tokens->items[i];
        printf("%s:%lu:%lu: %s :: ", tok->loc.name, tok->loc.line, tok->loc.col, get_token_str(tok->tk));

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
        default:
            printf("(NONE)");
            break;
        }

        printf("\n");
    }
}

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

    // == TOKENIZING
    Tokens tokens = {0};

    long long start = current_time_ns();
    bool res = parse_tokens_v2(&sb, &tokens, file);
    long long end = current_time_ns();

    if (!res) {
        goto cleanup;
    }

    double elapsed_ms = (double)(end - start) / 1e6;
    printf("Token parsing     : %.3f ms\n", elapsed_ms);

    /* print_token(&tokens); */


    // == AST-ING
    Statements program = {0};
    start = current_time_ns();
    if (!make_ast(&rarena, &program, &tokens)) { goto cleanup; }
    end = current_time_ns();
    elapsed_ms = (double)(end - start) / 1e6;
    printf("AST parsing       : %.3f ms\n", elapsed_ms);

    for (size_t i = 0; i < program.count; i++) {
        print_stmt(program.items[i], 0);
    }

    // == SEMANTIC CHECKING
    Semantic semantic = {0};
    semantic.arena = &rarena;

    start = current_time_ns();
    semantic_check(&semantic, &program);
    end = current_time_ns();
    elapsed_ms = (double)(end - start) / 1e6;
    printf("Semantic Checking : %.3f ms\n", elapsed_ms);

    goto cleanup;

 cleanup:
    arena_deinit(&rarena);
    tokens_deinit(&tokens);
    da_free(tokens);
    return 0;
}
