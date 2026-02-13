#include "lexer.h"
#include <ctype.h>
#include <math.h>
#include "utils.h"

int is_uint(const char *s, uint64_t *res) {
    char *end;
    errno = 0;
    uint64_t val = strtoull(s, &end, 10);
    *res = val;
    if (errno != 0) return 0;    // overflow
    if (end == s) return 0;      // no digits
    if (*end != '\0') return 0;  // junk at end
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

inline static void make_ident_or_n(Tokens *t, String_Builder *sb, SrcLoc loc) {
    if (sb->count == 0) return;

    char *tmp = flush_buffer(sb);

    uint64_t ubuf = 0;
    double fbuf   = 0.0;

    // keyword stuff
    bool is_keyword = false;
    Token n = {0};
    n.loc = loc;
    n.loc.col -= strlen(tmp);

    if (strcmp(tmp, LET_STR) == 0) {
        n.tk       = T_LET;
        is_keyword = true;
    } else if (strcmp(tmp, RETURN_STR) == 0) {
        n.tk       = T_RETURN;
        is_keyword = true;
    }
    if (is_keyword) {
        free(tmp);
        da_append(t, n);
        return;
    }

    if (is_uint(tmp, &ubuf)) {
        Token n = {
            .tk = T_NUM,
            .loc = loc,
        };
        n.loc.col -= strlen(tmp);
        n.data.Uint64 = ubuf;
        da_append(t, n);
    } else if (is_float(tmp, &fbuf)) {
        Token n = {
            .tk = T_FLO,
            .loc = loc,
        };
        n.loc.col -= strlen(tmp);
        n.data.F64 = fbuf;
        da_append(t, n);
    } else {
        Token id = {
            .tk = T_IDENT,
            .loc = loc,
        };
        id.loc.col -= strlen(tmp);
        id.data.String = tmp;
        da_append(t, id);
        return;
    }

    free(tmp);
}

// TODO: boolean, error reporting, etc

bool parse_tokens_v2(Nob_String_Builder *data, Tokens *tokens, const char *name) {
    bool ret = true;
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
        SrcLoc currentloc = (SrcLoc){
            .line  = line,
            .col   = col,
            .name  = name,
        };

        Token t = {};
        t.loc = currentloc;

        switch (ch) {
        case '*':
        case '/':
        case COMMA_CHR:
        case STRING_CHR: // "
        case CLOSING_CHR:
        case EQUAL_CHR:
        case OPARENT_CHR:
        case CPARENT_CHR:
        case OCPARENT_CHR:
        case CCPARENT_CHR:
        case COLON_CHR:
            if (sb.count > 0) { make_ident_or_n(tokens, &sb, currentloc); }
            break;
        default:
            break;
        }

        bool match = true;
        switch (ch) {
        case STRING_CHR: {
            while (true) {
                p = next(&cur);

                if (!p) {
                    ret = false;
                    perr("%s:%lu:%lu: Unexpected EOF in string", currentloc.name, currentloc.line, currentloc.col);
                    break;
                }
                if (*p == STRING_CHR) break;
                if (*p == '\n') {
                    ret = false;
                    perr("%s:%lu:%lu: Unclosed string blocks", currentloc.name, currentloc.line, currentloc.col);
                    break;
                }

                if (*p == '\\') {
                    char *esc = next(&cur);
                    if (!esc) {
                        ret = false;
                        perr("%s:%lu:%lu: trailing backslash at end of file", currentloc.name, currentloc.line, currentloc.col);
                        break;
                    }

                    switch (*esc) {
                    case 'n':  sb_appendf(&sb, "\n"); break;
                    case 't':  sb_appendf(&sb, "\t"); break;
                    case 'r':  sb_appendf(&sb, "\r"); break;
                    case '\\': sb_appendf(&sb, "\\"); break;
                    case '"':  sb_appendf(&sb, "\""); break;
                    case '0':  sb_appendf(&sb, "\\0"); break;
                    default:
                        sb_appendf(&sb, "%c", *esc);
                        break;
                    }
                } else {
                    sb_appendf(&sb, "%c", *p);
                }
            }

            t.tk = T_STR;
            t.data.String = flush_buffer(&sb);
            da_append(tokens, t);
            continue;
        }
        case '+':
        case '-':
            // SCIENTIFIC NOTATION CHECK:
            if (sb.count > 0 && (toupper(sb.items[sb.count-1]) == 'E')) {
                sb_appendf(&sb, "%c", ch);
                continue;
            }

            if (sb.count > 0) make_ident_or_n(tokens, &sb, currentloc);
            t.tk = (ch == '+') ? T_PLUS : T_MIN;
            da_append(tokens, t);
            continue;

        case '*':
        case '/':
            t.tk = (ch == '*') ? T_STAR : T_DIV;
            da_append(tokens, t);
            continue;
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
        case COMMA_CHR: {
            t.tk = T_COMMA;
            da_append(tokens, t);
        } break;
        case COLON_CHR: {
            t.tk = T_COLON;
            char *nc = peek(&cur, 0);
            if (nc && *nc == COLON_CHR) {
                t.tk = T_DCOLON;
                next(&cur);
            }
            da_append(tokens, t);
        } break;
        default: { match = false; } break;
        }

        if (isspace(ch) || match) {
            if (sb.count > 0) make_ident_or_n(tokens, &sb, currentloc);
            continue;
        }

        sb_appendf(&sb, "%c", ch);
    }

    /* NOTE: exhaust the last token */
    if (sb.count > 0 && sb.items[0] != 0) {
        make_ident_or_n(tokens, &sb, (SrcLoc) { name, (size_t)cur.line, (size_t)cur.col });
    }

    Token teof = {
        .tk   = T_EOF,
        .loc = (SrcLoc) { name, cur.line, 1 },
    };
    da_append(tokens, teof);

    da_free(sb);
    return ret;
}

void tokens_deinit(Tokens *t) {
    for (size_t i = 0; i < t->count; i++) {
        Token current = t->items[i];
        if (current.tk == T_IDENT || current.tk == T_STR) {
            if (current.data.String) free(current.data.String);
        }
    }
}
