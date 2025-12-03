#include "token.h"
#include <ctype.h>

char *peek(InternalCursor *cur, size_t n) {
    if (cur->offset + n >= cur->data->count &&
        *(cur->cursor + n) == '\0') return NULL;
    return (cur->cursor + n);
}

char *next(InternalCursor *cur) {
    if (!cur || !cur->data) return NULL;

    if (cur->offset >= cur->data->count)
        return NULL;

    char *old = cur->cursor;
    if (*old == '\n') {
        cur->line++;
        cur->col = 1;
    } else {
        cur->col++;
    }

    cur->offset++;
    cur->cursor++;
    if (cur->offset >= cur->data->count) return NULL;
    return cur->cursor;
}


bool peek_expect(InternalCursor *cur, size_t n, char expect) {
    char *p = peek(cur, n);
    if (p) return *p == expect;
    return false;
}

char *next_until(InternalCursor *cur, char ch) {
    char n = '\0';
    do {
        char *n2 = next(cur);
        if (cur) n = *n2;
        else return NULL;
    } while (n != ch);
    return cur->cursor;
}

char *next_until_nl(InternalCursor *cur) {
    if (!cur) return NULL;
    char n = '\0';
    do {
        char *n2 = next(cur);
        if (!n2) return NULL;
        n = *n2;
    } while (n != '\n' && n != '\r');
    if (n == '\r') next(cur);
    return cur->cursor;
}

bool parse_tokens(Nob_String_Builder *data, Tokens *toks) {
    InternalCursor cur = {
        .cursor = data->items,
        .offset = 0,
        .line = 1,
        .col = 1,
        .data = data,
    };

    static String_Builder sb = {0};

    // TODO: Handle the error mine friende
    while (cur.cursor && *(cur.cursor) != '\0') {
        size_t line = cur.line;
        size_t col = cur.col;
        char *cursor = next(&cur);

        Token t ={0};
        t.col = col;
        t.line = line;

        // Check comment
        // The supported comment are : `//` and `#`
        if ((*cursor == '/' && peek_expect(&cur, 0, '/')) ||
            *cursor == '#')
            {
                next_until_nl(&cur);
            }

        if (isspace(*cursor)) continue;

        /*
        switch (*cursor) {
        // Check comment
        case  '/': {
            if (peek_expect(&cur, 0, '/')) next_until(&cur, '\n');
        } break;
        case '#': {
            next_until(&cur, '\n');
        } break;

        default: {
            if (isspace(*cursor)) {
                t.tk = T_LIT;
                t.data.String = strdup(sb.items);
                if (sb.count > 0) da_append(toks, t);
                printf("saved the token lit with the content:"
                       "%s\n", sb.items);
                sb.count = 0;
                continue;
            } else {
                sb_appendf(&sb, "%c", *cursor);
            }
        } break;
        } */
    }
    da_free(sb);
    return false;
}
