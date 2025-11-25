#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>

#define perr_exit(fmt, ...)\
    do { fprintf(stderr, "ERROR: " fmt "\n", ##__VA_ARGS__); exit(1); } while(0)

#define perr(fmt, ...) \
    do { fprintf(stderr, "ERROR: " fmt "\n", ##__VA_ARGS__); } while(0)

#endif
