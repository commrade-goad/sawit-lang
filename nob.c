#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#define PROG_NAME "sawit"
#include "nob.h"

static Cmd cmd = {0};

static void cflags(Cmd *cmd) {
    cmd_append(cmd, "-Wall");
    cmd_append(cmd, "-Wextra");
    cmd_append(cmd, "-ggdb");
}

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    cmd_append(&cmd, "cc");
    cflags(&cmd);
    cmd_append(&cmd, "-o", PROG_NAME);
    cmd_append(&cmd, "nob_inc.c");
    cmd_append(&cmd, "lexer.c");
    cmd_append(&cmd, "semantic.c");
    cmd_append(&cmd, "ast.c");
    cmd_append(&cmd, "main.c");

    if (!cmd_run(&cmd)) return 1;

    bool run = false;
    if (argc >= 2) {
        shift(argv, argc);
        if (strcmp(argv[0], "-run") == 0) {
            cmd_append(&cmd, "./"PROG_NAME);
            shift(argv, argc);
            for (int i = 0; i < argc; i++) {
                cmd_append(&cmd, argv[i]);
            }
            cmd_run(&cmd);
        }
    }

    return 0;
}
