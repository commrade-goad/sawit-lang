#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>

typedef struct {
    const char *name;
    size_t line;
    size_t col;
} SrcLoc;

typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} Errors;

#define perr_exit(fmt, ...)\
    do { fprintf(stderr, "ERROR: " fmt "\n", ##__VA_ARGS__); exit(1); } while(0)

#define perr(fmt, ...) \
    do { fprintf(stderr, "ERROR: " fmt "\n", ##__VA_ARGS__); } while(0)

#endif
