#ifndef TOKEN_H
#define TOKEN_H

#include <stdbool.h>
#include <stdlib.h>
#define NOB_STRIP_PREFIX
#include "nob.h"
#include "utils.h"

#define LET_STR "let"
// @NOTE: this is just for normal struct it just easier to type class
#define CLASS_STR "class"
#define RETURN_STR "return"
#define STAR_CHR '*'
#define DIV_CHR '/'
#define PLUS_CHR '+'
#define MIN_CHR '-'
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
#define COMMA_CHR ','

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
    T_DCOLON,   // ::
    T_COMMA,    // ,

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

static inline const char *get_token_str(TokenKind tk) {
    switch (tk) {
    case T_EOF:      { return "T_EOF";      } break;
    case T_OPARENT:  { return "T_OPARENT";  } break;
    case T_CPARENT:  { return "T_CPARENT";  } break;
    case T_OCPARENT: { return "T_OCPARENT"; } break;
    case T_CCPARENT: { return "T_CCPARENT"; } break;
    case T_OSPARENT: { return "T_OSPARENT"; } break;
    case T_CSPARENT: { return "T_CSPARENT"; } break;
    case T_CLOSING:  { return "T_CLOSING";  } break;
    case T_EQUAL:    { return "T_EQUAL";    } break;
    case T_FATARROW: { return "T_FATARROW"; } break;
    case T_ARROW:    { return "T_ARROW";    } break;
    case T_COLON:    { return "T_COLON";    } break;
    case T_DCOLON:   { return "T_DCOLON";   } break;
    case T_COMMA:    { return "T_COMMA";    } break;
    case T_MIN:      { return "T_MIN";      } break;
    case T_PLUS:     { return "T_PLUS";     } break;
    case T_STAR:     { return "T_STAR";     } break;
    case T_DIV:      { return "T_DIV";      } break;
    case T_LET:      { return "T_LET";      } break;
    case T_CONST:    { return "T_CONST";    } break;
    case T_RETURN:   { return "T_RETURN";   } break;
    case T_IF:       { return "T_IF";       } break;
    case T_ELSE:     { return "T_ELSE";     } break;
    case T_STRUCT:   { return "T_STRUCT";   } break;
    case T_ENUM:     { return "T_ENUM";     } break;
    case T_IDENT:    { return "T_IDENT";    } break;
    case T_CHR:      { return "T_CHR";      } break;
    case T_STR:      { return "T_STR";      } break;
    case T_NUM:      { return "T_NUM";      } break;
    case T_FLO:      { return "T_FLO";      } break;
    default:         { return "UNKNOWN";    } break;
    }
}


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
void tokens_deinit(Tokens *t);

#endif
