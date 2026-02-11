#ifndef TOKEN_H
#define TOKEN_H

#include <stdbool.h>
#include <stdlib.h>
#define NOB_STRIP_PREFIX
#include "nob.h"
#include "utils.h"

#define LET_STR "let"
#define COMMENT_CHR2 '/'
#define COMMENT_CHR '#'
#define OCPARENT_CHR '{'
#define CCPARENT_CHR '}'
#define OPARENT_CHR '('
#define CPARENT_CHR ')'
#define CLOSING_CHR ';'
#define EQUAL_CHR '='
#define COLON_CHR ':'
#define STRING_CHR '"'

typedef enum {
    T_EOF = 0,

    T_OPARENT,
    T_CPARENT,
    T_OCPARENT, // {
    T_CCPARENT, // }
    T_OSPARENT, // [
    T_CSPARENT, // ]

    T_CLOSING,  // ;
    T_EQUAL,    // =
    T_FATARROW, // =>
    T_ARROW,    // ->
    T_COLON,    // :

    // BINOP
    T_MIN,      // -
    T_PLUS,     // +
    T_STAR,     // *
    T_DIV,      // /

    T_LET,
    T_CONST,
    T_RETURN,
    T_IF,
    T_ELSE,
    T_STRUCT,
    T_ENUM,

    T_IDENT,
    T_CHR,
    T_STR,
    T_NUM,
    T_FLO,
} TokenKind;

typedef struct {
    TokenKind tk;
    union {
        char     Char;
        int64_t  Int64;  // i will just save it as the biggest for now will be handled later.
        uint64_t Uint64; // i will just save it as the biggest for now will be handled later.
        double   F64;    // i will just save it as the biggest for now will be handled later.
        char    *String;
    } data;
    SrcLoc loc;
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

bool parse_tokens_v2(Nob_String_Builder *data, Tokens *t, const char *name);

#endif
