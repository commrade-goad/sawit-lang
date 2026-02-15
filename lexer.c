#include "lexer.h"
#include <ctype.h>
#include <math.h>
#include "utils.h"

static void write_to_sb(Rune *r, String_Builder *sb, char *rune_start) {
    sb_appendf(sb, "%.*s", (int)r->width, rune_start);
}

Rune utf8_next(const unsigned char *s) {
    Rune r = {0};

    if (s[0] < 0x80) {
        r.codepoint = s[0];
        r.width = 1;
    } else if ((s[0] & 0xE0) == 0xC0) {
        r.codepoint = ((s[0] & 0x1F) << 6) |
                      (s[1] & 0x3F);
        r.width = 2;
    } else if ((s[0] & 0xF0) == 0xE0) {
        r.codepoint = ((s[0] & 0x0F) << 12) |
                      ((s[1] & 0x3F) << 6) |
                      (s[2] & 0x3F);
        r.width = 3;
    } else {
        r.codepoint = ((s[0] & 0x07) << 18) |
                      ((s[1] & 0x3F) << 12) |
                      ((s[2] & 0x3F) << 6) |
                      (s[3] & 0x3F);
        r.width = 4;
    }

    return r;
}

int is_uint(const char *s, uint64_t *res) {
    char *end;
    errno = 0;
    int base = 10;

    // Check for hex (0x), binary (0b), or octal (0o) prefix
    if (s[0] == '0' && s[1] != '\0') {
        if (s[1] == 'x' || s[1] == 'X') {
            base = 16;
            s += 2;
        } else if (s[1] == 'b' || s[1] == 'B') {
            base = 2;
            s += 2;
        } else if (s[1] == 'o' || s[1] == 'O') {
            base = 8;
            s += 2;
        }
    }

    uint64_t val = strtoull(s, &end, base);
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

static Rune next_rune(InternalCursor *cur) {
    Rune r = {0};

    if (!cur || cur->offset >= cur->data->count) {
        return r;
    }

    unsigned char *s = (unsigned char *)cur->cursor;
    r = utf8_next(s);

    // newline check (ASCII only)
    if (r.codepoint == '\n') {
        cur->line++;
        cur->col = 1;
    } else {
        cur->col++;
    }

    cur->cursor += r.width;
    cur->offset += r.width;

    return r;
}

/* @NOTE: will be DEPRECATED LATER */
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
    } else if (strcmp(tmp, CONST_STR) == 0) {
        n.tk       = T_CONST;
        is_keyword = true;
    } else if (strcmp(tmp, FN_STR) == 0) {
        n.tk       = T_FN;
        is_keyword = true;
    } else if (strcmp(tmp, IF_STR) == 0) {
        n.tk       = T_IF;
        is_keyword = true;
    } else if (strcmp(tmp, ELSE_STR) == 0) {
        n.tk       = T_ELSE;
        is_keyword = true;
    } else if (strcmp(tmp, FOR_STR) == 0) {
        n.tk       = T_FOR;
        is_keyword = true;
    } else if (strcmp(tmp, BREAK_STR) == 0) {
        n.tk       = T_BREAK;
        is_keyword = true;
    } else if (strcmp(tmp, CONTINUE_STR) == 0) {
        n.tk       = T_CONTINUE;
        is_keyword = true;
    } else if (strcmp(tmp, TRUE_STR) == 0) {
        n.tk       = T_TRUE;
        is_keyword = true;
    } else if (strcmp(tmp, FALSE_STR) == 0) {
        n.tk       = T_FALSE;
        is_keyword = true;
    } else if (strcmp(tmp, TYPE_STR) == 0) {
        n.tk       = T_TYPE;
        is_keyword = true;
    } else if (strcmp(tmp, ENUM_STR) == 0) {
        n.tk       = T_ENUM;
        is_keyword = true;
    } else if (strcmp(tmp, TYPE_STR) == 0) {
        n.tk       = T_TYPE;
        is_keyword = true;
    } else if (strcmp(tmp, CAST_STR) == 0) {
        n.tk       = T_CAST;
        is_keyword = true;
    } else if (strcmp(tmp, SIZEOF_STR) == 0) {
        n.tk       = T_SIZEOF;
        is_keyword = true;
    } else if (strcmp(tmp, TYPEOF_STR) == 0) {
        n.tk       = T_TYPEOF;
        is_keyword = true;
    } else if (strcmp(tmp, MATCH_STR) == 0) {
        n.tk       = T_MATCH;
        is_keyword = true;
    } else if (strcmp(tmp, IMPORT_STR) == 0) {
        n.tk       = T_IMPORT;
        is_keyword = true;
    } else if (strcmp(tmp, DEFER_STR) == 0) {
        n.tk       = T_DEFER;
        is_keyword = true;
    } else if (strcmp(tmp, STATIC_STR) == 0) {
        n.tk       = T_STATIC;
        is_keyword = true;
    /* } else if (strcmp(tmp, MUT_STR) == 0) { */
    /*     n.tk       = T_MUT; */
    /*     is_keyword = true; */
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

        Rune r = next_rune(&cur);
        uint32_t ch = r.codepoint;

        /* // comment */
        if (ch == COMMENT_CHR2 && peek_expect(&cur, 0, COMMENT_CHR2)) {
            next_rune(&cur);
            while (cur.offset < cur.data->count) {
                Rune rr = next_rune(&cur);
                if (rr.codepoint == '\n') break;
            }
            continue;
        }
        /* # comment */
        if (ch == COMMENT_CHR) {
            while (cur.offset < cur.data->count) {
                Rune rr = next_rune(&cur);
                if (rr.codepoint == '\n') break;
            }
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
        case STAR_CHR:
        case DIV_CHR:
        case MOD_CHR:
        case COMMA_CHR:
        case STRING_CHR: // "
        case CLOSING_CHR:
        case EQUAL_CHR:
        case OPARENT_CHR:
        case CPARENT_CHR:
        case OCPARENT_CHR:
        case CCPARENT_CHR:
        case COLON_CHR:
        case LESS_CHR:
        case GREATER_CHR:
        case BANG_CHR:
        case AMPERSAND_CHR:
        case PIPE_CHR:
        case DOT_CHR:
        case CARET_CHR:
        case TILDE_CHR:
        case QUESTION_CHR:
        case AT_CHR:
        case DOLLAR_CHR:
            if (sb.count > 0) { make_ident_or_n(tokens, &sb, currentloc); }
            break;
        default:
            break;
        }

        bool match = true;
        switch (ch) {
        case STRING_CHR: {
            while (true) {
                if (cur.offset >= cur.data->count) {
                    log_error(currentloc, "Unexpected EOF in string");
                    return false;
                }

                char *rune_start = cur.cursor;
                Rune sr = next_rune(&cur);
                uint32_t sch = sr.codepoint;

                if (sch == STRING_CHR) break;

                if (sch == '\n') {
                    log_error(currentloc, "Unclosed string blocks");
                    return false;
                }

                if (sch == '\\') {
                    if (cur.offset >= cur.data->count) {
                        log_error(currentloc, "Trailing backslash at EOF");
                        return false;
                    }

                    Rune esc = next_rune(&cur);
                    switch (esc.codepoint) {
                    case 'n':  sb_appendf(&sb, "\n"); break;
                    case 't':  sb_appendf(&sb, "\t"); break;
                    case 'r':  sb_appendf(&sb, "\r"); break;
                    case '\\': sb_appendf(&sb, "\\"); break;
                    case '"':  sb_appendf(&sb, "\""); break;
                    case '0': {
                        char zero = '\0';
                        sb_appendf(&sb, "%c", zero);
                    }  break;
                    default:
                        write_to_sb(&esc, &sb, cur.cursor - esc.width);
                        break;
                    }
                } else {
                    write_to_sb(&sr, &sb, rune_start);
                }
            }

            t.tk = T_STR;
            t.data.String = flush_buffer(&sb);
            da_append(tokens, t);
            continue;
        }
        case PLUS_CHR:
        case MIN_CHR: {
            // Check for compound assignment (+=, -=)
            char *next_char = peek(&cur, 0);
            if (next_char && *next_char == EQUAL_CHR) {
                next_rune(&cur);
                t.tk = (ch == '+') ? T_PLUS_EQ : T_MIN_EQ;
                da_append(tokens, t);
                continue;
            }

            // Check for arrow (->)
            if (next_char && *next_char == '>') {
                next_rune(&cur);
                t.tk = T_ARROW;
                da_append(tokens, t);
                continue;
            }

            // SCIENTIFIC NOTATION CHECK:
            // @NOTE: only support ascii stuff here
            if (sb.count > 0 && (toupper(sb.items[sb.count-1]) == 'E')) {
                write_to_sb(&r, &sb, cur.cursor - r.width);
                continue;
            }

            if (sb.count > 0) make_ident_or_n(tokens, &sb, currentloc);
            t.tk = (ch == '+') ? T_PLUS : T_MIN;
            da_append(tokens, t);
            continue;
        } break;
        case STAR_CHR: {
            // Check for *=
            char *nc = peek(&cur, 0);
            if (nc && *nc == EQUAL_CHR) {
                t.tk = T_STAR_EQ;
                next_rune(&cur);
            } else {
                t.tk = T_STAR;
            }
            da_append(tokens, t);
            continue;
        } break;
        case DIV_CHR: {
            // Check for /=
            char *nc = peek(&cur, 0);
            if (nc && *nc == EQUAL_CHR) {
                t.tk = T_DIV_EQ;
                next_rune(&cur);
            } else {
                t.tk = T_DIV;
            }
            da_append(tokens, t);
            continue;
        } break;
        case MOD_CHR: {
            // Check for %=
            char *nc = peek(&cur, 0);
            if (nc && *nc == EQUAL_CHR) {
                t.tk = T_MOD_EQ;
                next_rune(&cur);
            } else {
                t.tk = T_MOD;
            }
            da_append(tokens, t);
        } break;
        case CLOSING_CHR: {
            t.tk = T_CLOSING;
            da_append(tokens, t);
        } break;
        case EQUAL_CHR: {
            char *nc = peek(&cur, 0);
            if (nc && *nc == EQUAL_CHR) {
                t.tk = T_EQ;
                next_rune(&cur);
            } else if (nc && *nc == '>') {
                t.tk = T_FATARROW;
                next_rune(&cur);
            } else {
                t.tk = T_EQUAL;
            }
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
                next_rune(&cur);
            }
            da_append(tokens, t);
        } break;
        case LESS_CHR: {
            char *nc = peek(&cur, 0);
            if (nc && *nc == LESS_CHR) {
                // Check for <<=
                char *nc2 = peek(&cur, 1);
                if (nc2 && *nc2 == EQUAL_CHR) {
                    t.tk = T_LSHIFT_EQ;
                    next_rune(&cur);
                    next_rune(&cur);
                } else {
                    t.tk = T_LSHIFT;
                    next_rune(&cur);
                }
            } else if (nc && *nc == EQUAL_CHR) {
                t.tk = T_LTE;
                next_rune(&cur);
            } else {
                t.tk = T_LT;
            }
            da_append(tokens, t);
        } break;
        case GREATER_CHR: {
            char *nc = peek(&cur, 0);
            if (nc && *nc == GREATER_CHR) {
                // Check for >>=
                char *nc2 = peek(&cur, 1);
                if (nc2 && *nc2 == EQUAL_CHR) {
                    t.tk = T_RSHIFT_EQ;
                    next_rune(&cur);
                    next_rune(&cur);
                } else {
                    t.tk = T_RSHIFT;
                    next_rune(&cur);
                }
            } else if (nc && *nc == EQUAL_CHR) {
                t.tk = T_GTE;
                next_rune(&cur);
            } else {
                t.tk = T_GT;
            }
            da_append(tokens, t);
        } break;
        case BANG_CHR: {
            char *nc = peek(&cur, 0);
            if (nc && *nc == EQUAL_CHR) {
                t.tk = T_NEQ;
                next_rune(&cur);
            } else {
                t.tk = T_NOT;
            }
            da_append(tokens, t);
        } break;
        case AMPERSAND_CHR: {
            char *nc = peek(&cur, 0);
            if (nc && *nc == AMPERSAND_CHR) {
                t.tk = T_AND;
                next_rune(&cur);
            } else if (nc && *nc == EQUAL_CHR) {
                t.tk = T_AND_EQ;
                next_rune(&cur);
            } else {
                t.tk = T_BIT_AND;
            }
            da_append(tokens, t);
        } break;
        case PIPE_CHR: {
            char *nc = peek(&cur, 0);
            if (nc && *nc == PIPE_CHR) {
                t.tk = T_OR;
                next_rune(&cur);
            } else if (nc && *nc == EQUAL_CHR) {
                t.tk = T_OR_EQ;
                next_rune(&cur);
            } else {
                t.tk = T_BIT_OR;
            }
            da_append(tokens, t);
        } break;
        case CARET_CHR: {
            char *nc = peek(&cur, 0);
            if (nc && *nc == EQUAL_CHR) {
                t.tk = T_XOR_EQ;
                next_rune(&cur);
            } else {
                t.tk = T_BIT_XOR;
            }
            da_append(tokens, t);
        } break;
        case TILDE_CHR: {
            t.tk = T_BIT_NOT;
            da_append(tokens, t);
        } break;
        case DOT_CHR: {
            char *nc = peek(&cur, 0);
            if (nc && *nc == DOT_CHR) {
                char *nc2 = peek(&cur, 1);
                if (nc2 && *nc2 == DOT_CHR) {
                    // ...
                    t.tk = T_DOTDOTDOT;
                    next_rune(&cur);
                    next_rune(&cur);
                } else {
                    // ..
                    t.tk = T_DOTDOT;
                    next_rune(&cur);
                }
            } else {
                t.tk = T_DOT;
            }
            da_append(tokens, t);
        } break;
        case QUESTION_CHR: {
            t.tk = T_QUESTION;
            da_append(tokens, t);
        } break;
        case AT_CHR: {
            t.tk = T_AT;
            da_append(tokens, t);
        } break;
        case DOLLAR_CHR: {
            t.tk = T_DOLLAR;
            da_append(tokens, t);
        } break;
        default: { match = false; } break;
        }

        if ((ch <= 127 && isspace(ch)) || match) {
            if (sb.count > 0) make_ident_or_n(tokens, &sb, currentloc);
            continue;
        }

        write_to_sb(&r, &sb, cur.cursor - r.width);
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
