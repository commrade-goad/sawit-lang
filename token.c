#include "token.h"
#include <ctype.h>
#include <math.h>

int is_uint(const char *s, uint64_t *res) {
    char *end;
    errno = 0;
    unsigned long long val = strtoull(s, &end, 10);
    *res = val;
    if (errno != 0) return 0;    // overflow
    if (end == s) return 0;      // no digits
    if (*end != '\0') return 0;  // junk at end
    return 1;
}

int is_int(const char *s, int64_t *res)
{
    char *end;
    errno = 0;
    long val = strtol(s, &end, 10);
    *res = val;
    if (errno != 0) return 0;          // overflow / underflow
    if (end == s) return 0;            // no digits
    if (*end != '\0') return 0;        // extra junk
    return 1;
}

int is_float(const char *s, double *res)
{
    char *end;
    errno = 0;
    double val = strtod(s, &end);

    if (end == s) return 0;     // no conversion
    if (*end != '\0') return 0; // leftover junk
    if (!isfinite(val)) return 0; // check for overflow to inf

    *res = val;
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
    if (sb->count == 0) return NULL;

    char saved = sb->items[sb->count];
    sb->items[sb->count] = '\0';

    char *out = strdup(sb->items);

    sb->items[sb->count] = saved;
    sb->count = 0;

    return out;
}

inline static void make_ident(Tokens *t, String_Builder *sb, int line, int col) {
    Token flush_t = {
        .tk   = T_IDENT,
        .line = line,
        .col  = col - sb->count,
    };
    flush_t.data.String = flush_buffer(sb);
    da_append(t, flush_t);
}

inline static void make_ident_or_n(Tokens *t, String_Builder *sb, int line, int col) {
    if (sb->count == 0) return;

    char *tmp = flush_buffer(sb);

    int64_t ibuf  = 0;
    uint64_t ubuf = 0;
    double fbuf   = 0.0;

    if (is_int(tmp, &ibuf)) {
        Token n = {
            .tk = T_NUM,
            .line = line,
            .col = col - strlen(tmp)
        };
        n.data.Int64 = ibuf;
        da_append(t, n);
    } else if (is_uint(tmp, &ubuf)) {
        Token n = {
            .tk = T_UNUM,
            .line = line,
            .col = col - strlen(tmp)
        };
        n.data.Uint64 = ubuf;
        da_append(t, n);
    } else if (is_float(tmp, &fbuf)) {
        Token n = {
            .tk = T_FLO,
            .line = line,
            .col = col - strlen(tmp)
        };
        n.data.F64 = fbuf;
        da_append(t, n);
    } else {
        Token id = {
            .tk = T_IDENT,
            .line = line,
            .col = col - strlen(tmp)
        };
        id.data.String = tmp;
        da_append(t, id);
        return;
    }

    free(tmp);
}

// TODO: string, binop, etc (LATER)

bool parse_tokens_v2(Nob_String_Builder *data, Tokens *tokens) {
    InternalCursor cur = {
        .cursor = data->items,
        .offset = 0,
        .line = 1,
        .col = 1,
        .data = data,
    };

    String_Builder sb = {0};

    while (cur.offset < data->count) {
        size_t line = cur.line;
        size_t col  = cur.col;

        char *p = next(&cur);
        if (!p) break;
        char ch = *p;

        /* // comment */
        if (ch == COMMENT_CHR2 && peek_expect(&cur, 0, COMMENT_CHR2)) {
            next(&cur);
            next_until_nl(&cur);
            continue;
        }
        /* # comment */
        if (ch == COMMENT_CHR) {
            next_until_nl(&cur);
            continue;
        }

        Token t = {};
        t.line  = line;
        t.col   = col;

        switch (ch) {
        case CLOSING_CHR:
        case EQUAL_CHR:
        case OPARENT_CHR:
        case CPARENT_CHR:
        case OCPARENT_CHR:
        case CCPARENT_CHR:
        case COLON_CHR:
            if (sb.count > 0) { make_ident_or_n(tokens, &sb, line, col); }
            break;
        default:
            break;
        }

        bool match = true;
        switch (ch) {
        case CLOSING_CHR: {
            t.tk = T_CLOSING;
            da_append(tokens, t);
        } break;
        case EQUAL_CHR: {
            t.tk = T_EQUAL;
            da_append(tokens, t);
        } break;
        case OPARENT_CHR: {
            t.tk = T_OPARENT;
            da_append(tokens, t);
        } break;
        case CPARENT_CHR: {
            t.tk = T_CPARENT;
            da_append(tokens, t);
        } break;
        case OCPARENT_CHR: {
            t.tk = T_OCPARENT;
            da_append(tokens, t);
        } break;
        case CCPARENT_CHR: {
            t.tk = T_CCPARENT;
            da_append(tokens, t);
        } break;
        case COLON_CHR: {
            t.tk = T_COLON;
            da_append(tokens, t);
        } break;
        default: { match = false; } break;
        }

        if (isspace(ch) || match) {
            if (sb.count > 0) { make_ident_or_n(tokens, &sb, line, col); }
            continue;
        }

        sb_appendf(&sb, "%c", ch);
    }

    /* NOTE: exhaust the last token */
    if (sb.count > 0 && !isspace(sb.items[sb.count]) && sb.items[0] != 0) {
        make_ident_or_n(tokens, &sb, cur.line, cur.col);
    }

    da_free(sb);
    return true;
}
