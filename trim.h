/* trim.h - a C header cut down to what a program uses.

   trim() keeps the header's file-scope definitions that the program names, directly or through other definitions it
   keeps, on every #if branch, and leaves out the rest with the comment above each. A definition is a function, a
   prototype, a variable, a type or a #define. Whatever is not a definition stays, and so does everything it names:
   #include and #if lines, extern "C", anonymous enums. The caller gives all the context: the header, the program, and
   the #ifdef blocks to leave out whole. It allocates nothing: the result goes in the caller's buffer, and a header
   has at most TRIM_MAX_PIECES pieces (definitions and the text between them).

     #define TRIM_IMPLEMENTATION
     #include "trim.h"

   It reads C as clang-format lays it out: a function's body ends on its first line or at a '}' that starts a line. */

#ifndef TRIM_H
#define TRIM_H

#include <stddef.h>

#ifndef TRIM_MAX_PIECES
#define TRIM_MAX_PIECES 4096
#endif

typedef struct {
    const char *header;
    size_t header_len;
    const char *program; /* what the program names is what is kept */
    size_t program_len;
    const char *const *drop_ifdefs; /* the NAMEs of #ifdef NAME blocks to leave out, say an optional API */
    size_t drop_count;
} TrimContext;

typedef enum { TRIM_OK, TRIM_TOO_LONG, TRIM_LIMIT } TrimError;

/* Writes the trimmed header to out and its length to *out_len. TRIM_TOO_LONG: it doesn't fit out_size. TRIM_LIMIT:
 * the header has more than TRIM_MAX_PIECES pieces. */
TrimError trim(const TrimContext *ctx, char *out, size_t out_size, size_t *out_len);

#endif /* TRIM_H */

#ifdef TRIM_IMPLEMENTATION

#include <string.h>

/* A piece of the header: a definition (named) or the text between definitions (not), from the end of the piece
 * before it, so a definition takes its comment along */
typedef struct {
    size_t from;
    size_t to;
    size_t name;
    size_t name_len;
    int used; /* 0: not kept (yet); 1: kept, and what it names is still to be marked; 2: kept and marked */
} TrimPiece;

typedef struct {
    const char *s;
    size_t len;
    TrimPiece pieces[TRIM_MAX_PIECES];
    size_t count;
    size_t start; /* where the piece being read began */
    size_t word;  /* the last identifier at file scope */
    size_t word_len;
    size_t name; /* the statement's name: the identifier before its first '(', '[' or '=', else its last */
    size_t name_len;
} TrimState;

static int trim_ident(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

static size_t trim_blanks(const char *s, size_t len, size_t i) {
    while (i < len && (s[i] == ' ' || s[i] == '\t'))
        i++;
    return i;
}

static size_t trim_word_end(const char *s, size_t len, size_t i) {
    while (i < len && trim_ident(s[i]))
        i++;
    return i;
}

/* Past the comment or the string or character literal at i, or i when there is none */
static size_t trim_skip(const char *s, size_t len, size_t i) {
    char quote = s[i];

    if (i + 1 < len && s[i] == '/' && s[i + 1] == '*') {
        for (i += 2; i + 1 < len && !(s[i] == '*' && s[i + 1] == '/'); i++)
            continue;
        return i + 1 < len ? i + 2 : len;
    }
    if (i + 1 < len && s[i] == '/' && s[i + 1] == '/') {
        while (i < len && s[i] != '\n')
            i++;
        return i;
    }
    if (quote != '"' && quote != '\'')
        return i;
    for (i++; i < len && s[i] != quote; i++)
        if (s[i] == '\\')
            i++;
    return i < len ? i + 1 : len;
}

/* Past the line at i, with its backslash continuations */
static size_t trim_line_end(const char *s, size_t len, size_t i) {
    while (i < len && s[i] != '\n') {
        size_t next = trim_skip(s, len, i);

        if (next != i)
            i = next;
        else
            i += s[i] == '\\' && i + 1 < len ? 2 : 1;
    }
    return i < len ? i + 1 : len;
}

/* Where the argument of the directive at i begins when the directive is "#name", else 0 */
static size_t trim_directive(const char *s, size_t len, size_t i, const char *name) {
    size_t n = strlen(name);

    i = trim_blanks(s, len, i + 1);
    if (i + n > len || memcmp(s + i, name, n) != 0 || trim_word_end(s, len, i) != i + n)
        return 0;
    return trim_blanks(s, len, i + n);
}

/* Whether the directive at i opens a block the caller leaves out */
static int trim_dropped(const TrimContext *ctx, const char *s, size_t len, size_t i) {
    size_t at = trim_directive(s, len, i, "ifdef");
    size_t n = trim_word_end(s, len, at) - at;
    size_t d;

    for (d = 0; at && d < ctx->drop_count; d++)
        if (strlen(ctx->drop_ifdefs[d]) == n && memcmp(s + at, ctx->drop_ifdefs[d], n) == 0)
            return 1;
    return 0;
}

/* Past the #endif that closes the block opening at i */
static size_t trim_block_end(const char *s, size_t len, size_t i) {
    size_t nesting = 0;

    do {
        i = trim_blanks(s, len, i);
        if (i < len && s[i] == '#') {
            nesting += trim_directive(s, len, i, "if") || trim_directive(s, len, i, "ifdef") ||
                       trim_directive(s, len, i, "ifndef");
            nesting -= trim_directive(s, len, i, "endif") != 0;
        }
        i = trim_line_end(s, len, i);
    } while (nesting > 0 && i < len);
    return i;
}

/* Past the body of the function whose '{' is at i: on its first line when it closes there, else at the '}' clang-format
 * puts at the start of a line (counting braces can't tell, since #if branches may each open or close their own) */
static size_t trim_body_end(const char *s, size_t len, size_t i) {
    size_t braces = 0;

    for (; i < len && s[i] != '\n'; i++) {
        braces += s[i] == '{';
        braces -= s[i] == '}';
        if (braces == 0)
            return i + 1;
    }
    while (i < len && !(s[i] == '}' && s[i - 1] == '\n')) {
        size_t next = trim_skip(s, len, i);

        i = next != i ? next : i + 1;
    }
    return i < len ? i + 1 : len;
}

/* Ends the piece being read at end: a definition with the statement's name when named, kept from the start if not */
static int trim_piece(TrimState *t, size_t end, int named) {
    TrimPiece *p;

    if (t->count == TRIM_MAX_PIECES)
        return 0;
    p = &t->pieces[t->count];
    end = trim_blanks(t->s, t->len, end); /* the piece takes the rest of its line */
    if (end < t->len && t->s[end] == '\n')
        end++;
    p->from = t->start;
    p->to = end;
    p->name = t->name;
    p->name_len = named ? t->name_len : 0;
    p->used = !named;
    t->count++;
    t->start = end;
    t->word_len = 0;
    t->name_len = 0;
    return 1;
}

/* A directive at file scope: the piece it ends, with what came before it (its comment). A #define is a definition, and
 * a block left out a piece nothing names. */
static int trim_directive_piece(TrimState *t, const TrimContext *ctx, size_t i, size_t *end) {
    size_t define = trim_directive(t->s, t->len, i, "define");

    t->name = define;
    t->name_len = define ? trim_word_end(t->s, t->len, define) - define : 0;
    if (trim_dropped(ctx, t->s, t->len, i)) {
        *end = trim_block_end(t->s, t->len, i);
        return trim_piece(t, *end, 1);
    }
    *end = trim_line_end(t->s, t->len, i);
    return trim_piece(t, *end, define != 0);
}

static int trim_scan(TrimState *t, const TrimContext *ctx) {
    const char *s = t->s;
    size_t i = 0;
    size_t depth = 0;
    size_t parens = 0;
    char last = ';';
    int line_start = 1;

    while (i < t->len) {
        size_t next = trim_skip(s, t->len, i);
        char c = s[i];
        size_t end = i + 1;

        if (next != i) {
            if (c == '"')
                last = '"';
            i = next;
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            line_start = line_start || c == '\n';
            i++;
            continue;
        }
        if (c == '#' && line_start) {
            if (depth > 0)
                end = trim_line_end(s, t->len, i);
            else if (!trim_directive_piece(t, ctx, i, &end))
                return 0;
            i = end;
            continue;
        }
        line_start = 0;

        if (trim_ident(c)) {
            end = trim_word_end(s, t->len, i);
            if (depth == 0 && parens == 0) {
                t->word = i;
                t->word_len = end - i;
            }
            last = 'a';
            i = end;
            continue;
        }

        if (depth == 0 && parens == 0 && t->name_len == 0 && (c == '(' || c == '[' || c == '=')) {
            t->name = t->word;
            t->name_len = t->word_len;
        }
        if (c == '(') {
            parens++;
        } else if (c == ')' && parens > 0) {
            parens--;
        } else if (c == '{' && depth == 0 && parens == 0 && last == ')') {
            end = trim_body_end(s, t->len, i);
            if (!trim_piece(t, end, t->name_len > 0))
                return 0;
        } else if (c == '{' && last != '"') { /* the braces of extern "C" are no block */
            depth++;
        } else if (c == '}' && depth > 0) {
            depth--;
        } else if (c == '}' || (c == ';' && depth == 0 && parens == 0)) {
            if (c == ';' && t->name_len == 0) { /* a variable or a type, named last */
                t->name = t->word;
                t->name_len = t->word_len;
            }
            if (!trim_piece(t, end, c == ';' && t->name_len > 0))
                return 0;
        }
        last = c;
        i = end;
    }
    return trim_piece(t, t->len, 0);
}

/* Marks what the identifiers in s[from..to) name as kept */
static void trim_use(TrimState *t, const char *s, size_t from, size_t to) {
    size_t i = from;

    while (i < to) {
        size_t next = trim_skip(s, to, i);
        size_t end = trim_word_end(s, to, i);
        size_t p;

        if (next != i || end == i) {
            i = next != i ? next : i + 1;
            continue;
        }
        for (p = 0; p < t->count; p++) {
            TrimPiece *piece = &t->pieces[p];

            if (!piece->used && piece->name_len == end - i && memcmp(t->s + piece->name, s + i, end - i) == 0)
                piece->used = 1;
        }
        i = end;
    }
}

TrimError trim(const TrimContext *ctx, char *out, size_t out_size, size_t *out_len) {
    static TrimState t;
    size_t n = 0;
    size_t p;
    size_t i;
    int more = 1;

    t.s = ctx->header;
    t.len = ctx->header_len;
    t.count = 0;
    t.start = 0;
    t.word_len = 0;
    t.name_len = 0;
    *out_len = 0;
    if (!trim_scan(&t, ctx))
        return TRIM_LIMIT;

    /* what the program names, then what that names, until nothing more is */
    trim_use(&t, ctx->program, 0, ctx->program_len);
    while (more) {
        more = 0;
        for (p = 0; p < t.count; p++) {
            if (t.pieces[p].used == 1) {
                t.pieces[p].used = 2;
                trim_use(&t, t.s, t.pieces[p].from, t.pieces[p].to);
                more = 1;
            }
        }
    }

    /* what is kept, with no more than one blank line in a row */
    for (p = 0; p < t.count; p++) {
        for (i = t.pieces[p].from; t.pieces[p].used && i < t.pieces[p].to; i++) {
            if (ctx->header[i] == '\n' && n >= 2 && out[n - 1] == '\n' && out[n - 2] == '\n')
                continue;
            if (n == out_size)
                return TRIM_TOO_LONG;
            out[n++] = ctx->header[i];
        }
    }
    *out_len = n;
    return TRIM_OK;
}

#endif /* TRIM_IMPLEMENTATION */
