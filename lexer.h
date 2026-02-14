#ifndef TOKEN_H
#define TOKEN_H

#include <stdbool.h>
#include <stdlib.h>
#define NOB_STRIP_PREFIX
#include "nob.h"
#include "utils.h"

#define LET_STR "let"
#define CONST_STR "const"
#define TYPE_STR "type"
#define FN_STR "fn"
#define CONTINUE_STR "continue"
#define BREAK_STR "break"
#define IF_STR "if"
#define ELSE_STR "else"
#define FOR_STR "for"
#define RETURN_STR "return"
#define TRUE_STR "true"
#define FALSE_STR "false"
#define ENUM_STR "enum"
#define CAST_STR "cast"
#define SIZEOF_STR "sizeof"
#define TYPEOF_STR "typeof"
#define MATCH_STR "match"
#define IMPORT_STR "import"
#define DEFER_STR "defer"
#define STATIC_STR "static"
#define MUT_STR "mut"

#define STAR_CHR '*'
#define DIV_CHR '/'
#define PLUS_CHR '+'
#define MIN_CHR '-'
#define MOD_CHR '%'
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
#define LESS_CHR '<'
#define GREATER_CHR '>'
#define BANG_CHR '!'
#define AMPERSAND_CHR '&'
#define PIPE_CHR '|'
#define DOT_CHR '.'
#define CARET_CHR '^'
#define TILDE_CHR '~'
#define QUESTION_CHR '?'
#define AT_CHR '@'
#define DOLLAR_CHR '$'

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
    T_DOT,      // .
    T_DOTDOT,   // ..
    T_DOTDOTDOT,// ...
    T_QUESTION, // ?
    T_AT,       // @
    T_DOLLAR,   // $

    // BINOP
    T_MIN,      // -
    T_PLUS,     // +
    T_STAR,     // *
    T_DIV,      // /
    T_MOD,      // %

    // ASSIGNMENT OPERATORS
    T_PLUS_EQ,  // +=
    T_MIN_EQ,   // -=
    T_STAR_EQ,  // *=
    T_DIV_EQ,   // /=
    T_MOD_EQ,   // %=
    T_AND_EQ,   // &=
    T_OR_EQ,    // |=
    T_XOR_EQ,   // ^=
    T_LSHIFT_EQ,// <<=
    T_RSHIFT_EQ,// >>=

    // COMPARISON
    T_EQ,       // ==
    T_NEQ,      // !=
    T_LT,       // <
    T_GT,       // >
    T_LTE,      // <=
    T_GTE,      // >=

    // LOGICAL
    T_AND,      // &&
    T_OR,       // ||
    T_NOT,      // !

    // BITWISE
    T_BIT_AND,  // &
    T_BIT_OR,   // |
    T_BIT_XOR,  // ^
    T_BIT_NOT,  // ~
    T_LSHIFT,   // <<
    T_RSHIFT,   // >>

    T_LET,
    T_FN,
    T_CONST,
    T_RETURN,
    T_IF,
    T_ELSE,
    T_FOR,
    T_BREAK,
    T_CONTINUE,
    T_STRUCT,
    T_ENUM,
    T_TYPE,
    T_CAST,
    T_SIZEOF,
    T_TYPEOF,
    T_MATCH,
    T_IMPORT,
    T_DEFER,
    T_STATIC,
    T_MUT,

    T_TRUE,
    T_FALSE,
    T_NULL,

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
    case T_DOT:      { return "T_DOT";      } break;
    case T_DOTDOT:   { return "T_DOTDOT";   } break;
    case T_DOTDOTDOT:{ return "T_DOTDOTDOT";} break;
    case T_QUESTION: { return "T_QUESTION"; } break;
    case T_AT:       { return "T_AT";       } break;
    case T_DOLLAR:   { return "T_DOLLAR";   } break;
    case T_MIN:      { return "T_MIN";      } break;
    case T_PLUS:     { return "T_PLUS";     } break;
    case T_STAR:     { return "T_STAR";     } break;
    case T_DIV:      { return "T_DIV";      } break;
    case T_MOD:      { return "T_MOD";      } break;
    case T_PLUS_EQ:  { return "T_PLUS_EQ";  } break;
    case T_MIN_EQ:   { return "T_MIN_EQ";   } break;
    case T_STAR_EQ:  { return "T_STAR_EQ";  } break;
    case T_DIV_EQ:   { return "T_DIV_EQ";   } break;
    case T_MOD_EQ:   { return "T_MOD_EQ";   } break;
    case T_AND_EQ:   { return "T_AND_EQ";   } break;
    case T_OR_EQ:    { return "T_OR_EQ";    } break;
    case T_XOR_EQ:   { return "T_XOR_EQ";   } break;
    case T_LSHIFT_EQ:{ return "T_LSHIFT_EQ";} break;
    case T_RSHIFT_EQ:{ return "T_RSHIFT_EQ";} break;
    case T_EQ:       { return "T_EQ";       } break;
    case T_NEQ:      { return "T_NEQ";      } break;
    case T_LT:       { return "T_LT";       } break;
    case T_GT:       { return "T_GT";       } break;
    case T_LTE:      { return "T_LTE";      } break;
    case T_GTE:      { return "T_GTE";      } break;
    case T_AND:      { return "T_AND";      } break;
    case T_OR:       { return "T_OR";       } break;
    case T_NOT:      { return "T_NOT";      } break;
    case T_BIT_AND:  { return "T_BIT_AND";  } break;
    case T_BIT_OR:   { return "T_BIT_OR";   } break;
    case T_BIT_XOR:  { return "T_BIT_XOR";  } break;
    case T_BIT_NOT:  { return "T_BIT_NOT";  } break;
    case T_LSHIFT:   { return "T_LSHIFT";   } break;
    case T_RSHIFT:   { return "T_RSHIFT";   } break;
    case T_LET:      { return "T_LET";      } break;
    case T_FN:       { return "T_FN";       } break;
    case T_CONST:    { return "T_CONST";    } break;
    case T_RETURN:   { return "T_RETURN";   } break;
    case T_IF:       { return "T_IF";       } break;
    case T_ELSE:     { return "T_ELSE";     } break;
    case T_FOR:      { return "T_FOR";      } break;
    case T_BREAK:    { return "T_BREAK";    } break;
    case T_CONTINUE: { return "T_CONTINUE"; } break;
    case T_STRUCT:   { return "T_STRUCT";   } break;
    case T_ENUM:     { return "T_ENUM";     } break;
    case T_TYPE:     { return "T_TYPE";     } break;
    case T_CAST:     { return "T_CAST";     } break;
    case T_SIZEOF:   { return "T_SIZEOF";   } break;
    case T_TYPEOF:   { return "T_TYPEOF";   } break;
    case T_MATCH:    { return "T_MATCH";    } break;
    case T_IMPORT:   { return "T_IMPORT";   } break;
    case T_DEFER:    { return "T_DEFER";    } break;
    case T_STATIC:   { return "T_STATIC";   } break;
    case T_MUT:      { return "T_MUT";      } break;
    case T_TRUE:     { return "T_TRUE";     } break;
    case T_FALSE:    { return "T_FALSE";    } break;
    case T_NULL:     { return "T_NULL";     } break;
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
