#include "token.h"
#include <ctype.h>

int is_int(const char *s)
{
    char *end;
    errno = 0;
    long val = strtol(s, &end, 10);
    (void)val;
    if (errno != 0) return 0;          // overflow / underflow
    if (end == s) return 0;            // no digits
    if (*end != '\0') return 0;        // extra junk
    return 1;
}

int is_float(const char *s)
{
    char *end;
    errno = 0;
    double val = strtod(s, &end);
    (void)val;
    if (errno != 0) return 0;   // overflow / underflow
    if (end == s) return 0;     // no conversion
    if (*end != '\0') return 0; // leftover junk
    return 1;
}

char *peek(InternalCursor *cur, size_t n) {
    if (!cur || !cur->data) return NULL;
    if (cur->offset + n >= cur->data->count) return NULL;
    return cur->cursor + n;
}

char *next(InternalCursor *cur) {
    if (!cur || !cur->data) return NULL;
    if (cur->offset >= cur->data->count) return NULL;

    char *current = cur->cursor;

    if (*current == '\n') {
        cur->line++;
        cur->col = 1;
    } else {
        cur->col++;
    }

    cur->cursor++;
    cur->offset++;

    return current;
}


bool peek_expect(InternalCursor *cur, size_t n, char expect) {
    char *p = peek(cur, n);
    return p && *p == expect;
}

char *next_until(InternalCursor *cur, char ch) {
    if (!cur) return NULL;

    char *p;
    do {
        p = next(cur);
        if (!p) return NULL;
    } while (*p != ch);

    return cur->cursor;
}

char *next_until_nl(InternalCursor *cur) {
    if (!cur) return NULL;

    char *p;
    do {
        p = next(cur);
        if (!p) return NULL;
    } while (*p != '\n' && *p != '\r');

    if (*p == '\r') {
        char *n = peek(cur, 0);
        if (n && *n == '\n') next(cur);
    }

    return cur->cursor;
}

char *flush_buffer(String_Builder *sb) {
    if (sb->count > 0) {
        sb->count = 0;
        return strdup(sb->items);
    }
    return NULL;
}

bool parse_tokens(Nob_String_Builder *data, Tokens *toks) {
    InternalCursor cur = {
        .cursor = data->items,
        .offset = 0,
        .line = 1,
        .col = 1,
        .data = data,
    };

    String_Builder sb = {0};

    // TODO: I feels like i want to rewrite this for the N times.
    // TODO: Handle the error mine friende
    while (cur.offset < data->count) {
        size_t line = cur.line;
        size_t col = cur.col;
        char *cursor = next(&cur);

        Token t = {0};
        t.col = col;
        t.line = line;

        if (cur.offset == data->count) {
            // Mean didnt flush correctly the last token.
            if (sb.count > 0) {
                t.tk = T_ERR;
            } else {
                t.tk = T_EOF;
            }
            da_append(toks, t);
            break;
        }

        if ((isspace(*cursor) || *cursor == CLOSING_CHR) && sb.count > 0)
        {
            char *extracted = flush_buffer(&sb);
            if (strncmp(extracted, LET_STR, strlen(LET_STR)) == 0) {
                t.tk = T_LET;
                free(extracted);
                goto end;
            } else {
                if (is_int(extracted) || is_float(extracted)) {
                    t.tk = T_NUM;
                    t.data.String = extracted;
                } else {
                    t.tk = T_IDENT;
                    t.data.String = extracted;
                }
                goto end;
            }
        }

        if (isspace(*cursor)) continue;

        if ((*cursor == COMMENT_CHR2 && peek_expect(&cur, 0, COMMENT_CHR2)) ||
            *cursor == COMMENT_CHR)
       {
            next_until_nl(&cur);
            continue;
       } else {
            switch (*cursor) {
            case OCPARENT_CHR: { t.tk = T_OCPARENT; } break;
            case CCPARENT_CHR: { t.tk = T_CCPARENT; } break;
            case EQUAL_CHR:    { t.tk = T_EQUAL;    } break;
            case CLOSING_CHR:  { t.tk = T_CLOSING;  } break;
            case COLON_CHR:    { t.tk = T_COLON;    } break;
            default: {
                sb_appendf(&sb, "%c", *cursor);
            } break;
            }
        }
    end:
        if (t.tk != T_EOF) { da_append(toks, t); }
    }
    da_free(sb);
    return false;
}
