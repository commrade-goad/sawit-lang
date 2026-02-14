#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

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

#define log_error(loc, fmt, ...) \
    do { fprintf(stderr, "%s:%lu:%lu: error: " fmt "\n", loc.name, loc.line, loc.col, ##__VA_ARGS__); } while(0)

#define perr_exit(fmt, ...)\
    do { fprintf(stderr, "ERROR: " fmt "\n", ##__VA_ARGS__); exit(1); } while(0)

#define perr(fmt, ...) \
    do { fprintf(stderr, "ERROR: " fmt "\n", ##__VA_ARGS__); } while(0)

#define WIP(msg) assert(true && "Not Yet Implemented: " msg)

static inline long long current_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (long long)ts.tv_sec * 1000000000LL +
           (long long)ts.tv_nsec;
}

#endif
