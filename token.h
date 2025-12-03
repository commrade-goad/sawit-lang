#ifndef TOKEN_H
#define TOKEN_H

#include <stdbool.h>
#include <stdlib.h>
#define NOB_STRIP_PREFIX
#include "nob.h"

typedef enum {
    T_OPARENT = 0,
    T_CPARENT,
    T_OCPARENT,
    T_CCPARENT,
    T_EQUAL,
    T_SEMICOLON,
    T_EOF,
    T_LIT,
} TokenKind;

typedef struct {
    TokenKind tk;
    union {
        int32_t  Int32;
        int64_t  Int64;
        char    *String;
    } data;
    size_t line, col;
} Token;

typedef struct {
    Token *items;
    size_t count;
    size_t capacity;
} Tokens;

typedef struct {
    size_t line, col;
    char *cursor;
    size_t offset;
    Nob_String_Builder *data;
} InternalCursor;

bool parse_tokens(Nob_String_Builder *data, Tokens *toks);

#endif
