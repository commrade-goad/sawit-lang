#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX

#include "nob.h"
#include "utils.h"

int main(int argc, char **argv) {
    if (argc <= 1) perr_exit("Not enought args");

    shift(argv, argc);
    const char *file = argv[0];
    String_Builder sb = {0};
    if (!read_entire_file(file, &sb)) perr_exit("Failed to read the file `%s`", file);
    if (sb.count <= 1) perr_exit("Empty file!");

    sb_append_null(&sb);
    printf("INFO:\n%s", sb.items);
    return 0;
}
