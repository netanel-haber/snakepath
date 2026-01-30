/* snakepath.h - C99 pathlib port, STB-style header-only library
 * No mallocs. POSIX and Windows compatible.
 *
 * Usage:
 *   #define SNAKEPATH_IMPLEMENTATION
 *   #include "snakepath.h"
 */

#ifndef SNAKEPATH_H
#define SNAKEPATH_H

#include <assert.h>
#include <stddef.h>
#include <string.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* C/C++ compatibility helpers */
#ifdef __cplusplus
#define SP_PRIV_STR(d, l) SpStr{(d), (l)}
#define SP_PRIV_OPTS(f) SpPathOpts{(f)}
#define SP_PRIV_ZERO {}
#define SP_PRIV_NULL nullptr
#else
#define SP_PRIV_STR(d, l) ((SpStr){.data = (d), .len = (l)})
#define SP_PRIV_OPTS(f) ((SpPathOpts){.flavor = (f)})
#define SP_PRIV_ZERO {0}
#define SP_PRIV_NULL NULL
#endif

#ifndef SP_PATH_MAX
#define SP_PATH_MAX 4096
#endif

#ifndef SP_MAX_PARTS
#define SP_MAX_PARTS 256
#endif

#ifndef SP_MAX_SUFFIXES
#define SP_MAX_SUFFIXES 16
#endif

/* Platform detection */
#if defined(_WIN32) || defined(_WIN64)
#define SP_WINDOWS 1
#define SP_SEP '\\'
#define SP_SEP_STR "\\"
#define SP_ALTSEP '/'
#else
#define SP_POSIX 1
#define SP_SEP '/'
#define SP_SEP_STR "/"
#define SP_ALTSEP '\0'
#endif

/* Flavors for explicit platform behavior */
typedef enum {
    SP_FLAVOR_NATIVE = 0,
    SP_FLAVOR_POSIX,
    SP_FLAVOR_WINDOWS
} SpFlavor;

/* Assertion macros for runtime invariant checking */
#define SP_ASSERT_PATH(p) assert((p) != NULL && "path pointer must not be NULL")
#define SP_ASSERT_FLAVOR(f) assert(((f) == SP_FLAVOR_NATIVE || (f) == SP_FLAVOR_POSIX || (f) == SP_FLAVOR_WINDOWS) && "invalid flavor value")
#define SP_ASSERT_PATH_INVARIANT(p) do { \
    SP_ASSERT_PATH(p); \
    assert((p)->len > 0 && "path must not be empty"); \
    assert((p)->len < SP_PATH_MAX && "path length exceeds buffer size"); \
    assert((p)->buf[(p)->len] == '\0' && "path buffer not null-terminated"); \
    SP_ASSERT_FLAVOR((p)->flavor); \
} while(0)

/* String view - non-owning slice */
typedef struct {
    const char *data;
    size_t len;
} SpStr;

/* Path structure - fixed buffer, no heap */
typedef struct {
    char buf[SP_PATH_MAX];
    size_t len;
    SpFlavor flavor;
} SpPath;

/* Parts iterator */
typedef struct {
    const SpPath *path;
    size_t pos;
    bool include_anchor;
    bool anchor_done;
} SpPartsIter;

/* Suffixes result */
typedef struct {
    SpStr items[SP_MAX_SUFFIXES];
    size_t count;
} SpSuffixes;

/* Parents iterator */
typedef struct {
    SpPath current;
    bool done;
} SpParentsIter;

/* ============ API Macros ============ */

/* Path creation with optional flavor: sp_path("foo/bar") or sp_path("foo", SP_FLAVOR_POSIX) */
typedef struct { SpFlavor flavor; } SpPathOpts;
#define sp_path(s) sp_path_new((s), SP_PRIV_OPTS(SP_FLAVOR_NATIVE))
#define sp_path_f(s, f) sp_path_new((s), SP_PRIV_OPTS(f))

/* Join paths: sp_join(p, "a", "b", "c") - C only, use sp_join_one in C++ */
#ifndef __cplusplus
#define sp_join(base, ...) sp_join_impl((base), (const char*[]){__VA_ARGS__, NULL})
#endif

/* Comparison: sp_eq(a, b) */
#define sp_eq(a, b) sp_path_eq(&(a), &(b))

/* String view literal */
#define sp_sv(s) SP_PRIV_STR((s), sizeof(s) - 1)
#define sp_sv_from(s, n) SP_PRIV_STR((s), (n))

/* ============ Core Functions ============ */

SpPath sp_path_new(const char *s, SpPathOpts opts);
SpPath sp_path_from_sv(SpStr sv, SpFlavor flavor);
SpPath sp_path_copy(const SpPath *p);

const char *sp_str(const SpPath *p);
SpStr sp_as_sv(const SpPath *p);
void sp_as_posix(const SpPath *p, char *out, size_t out_size);

SpStr sp_drive(const SpPath *p);
SpStr sp_root(const SpPath *p);
SpStr sp_anchor(const SpPath *p);
SpStr sp_name(const SpPath *p);
SpStr sp_stem(const SpPath *p);
SpStr sp_suffix(const SpPath *p);
SpSuffixes sp_suffixes(const SpPath *p);
SpPath sp_parent(const SpPath *p);

SpPartsIter sp_parts_begin(const SpPath *p);
bool sp_parts_next(SpPartsIter *it, SpStr *out);
size_t sp_parts_count(const SpPath *p);

SpParentsIter sp_parents_begin(const SpPath *p);
bool sp_parents_next(SpParentsIter *it, SpPath *out);

SpPath sp_join_one(const SpPath *base, const char *other);
SpPath sp_join_impl(const SpPath *base, const char **parts);
SpPath sp_joinpath(const SpPath *base, const SpPath *other);

SpPath sp_with_name(const SpPath *p, const char *name);
SpPath sp_with_stem(const SpPath *p, const char *stem);
SpPath sp_with_suffix(const SpPath *p, const char *suffix);

SpPath sp_relative_to(const SpPath *p, const SpPath *other);
bool sp_is_relative_to(const SpPath *p, const SpPath *other);

bool sp_is_absolute(const SpPath *p);
bool sp_path_eq(const SpPath *a, const SpPath *b);

/* ============ Helper Functions ============ */
static inline bool sp_sv_eq(SpStr a, SpStr b) {
    assert((a.len == 0 || a.data != NULL) && "SpStr with non-zero len must have valid data");
    assert((b.len == 0 || b.data != NULL) && "SpStr with non-zero len must have valid data");
    return a.len == b.len && memcmp(a.data, b.data, a.len) == 0;
}
static inline bool sp_sv_eq_cstr(SpStr a, const char *b) {
    assert((a.len == 0 || a.data != NULL) && "SpStr with non-zero len must have valid data");
    assert(b != NULL && "string pointer must not be NULL");
    size_t blen = strlen(b);
    return a.len == blen && memcmp(a.data, b, a.len) == 0;
}

#ifdef __cplusplus
}
#endif

/* ============ Fluent API ============ */
#ifdef SNAKEPATH_FLUENT

/* Thread-local storage - portable across compilers */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
  #define SP_TLS _Thread_local
#elif defined(__GNUC__) || defined(__clang__)
  #define SP_TLS __thread
#elif defined(_MSC_VER)
  #define SP_TLS __declspec(thread)
#else
  #define SP_TLS /* fallback: not thread-safe */
#endif

/* Forward declare the fluent struct */
typedef struct SpFluent SpFluent;

/* Function pointer types for fluent methods */
typedef SpFluent (*SpFluentVoidFn)(void);
typedef SpFluent (*SpFluentStrFn)(const char *);
typedef SpStr (*SpFluentSvFn)(void);
typedef SpSuffixes (*SpFluentSuffixesFn)(void);
typedef const char *(*SpFluentCstrFn)(void);
typedef bool (*SpFluentBoolFn)(void);
typedef const SpPath *(*SpFluentGetFn)(void);
typedef bool (*SpFluentRelToFn)(const SpPath *);
typedef SpPath (*SpFluentRelativeToFn)(const SpPath *);

/* The fluent API struct with function pointers for chaining */
struct SpFluent {
    /* Chainable methods (modify context, return SpFluent) */
    SpFluentVoidFn parent;
    SpFluentStrFn join;
    SpFluentStrFn with_name;
    SpFluentStrFn with_stem;
    SpFluentStrFn with_suffix;
    
    /* Accessor methods (return values) */
    SpFluentSvFn name;
    SpFluentSvFn stem;
    SpFluentSvFn suffix;
    SpFluentSuffixesFn suffixes;
    SpFluentSvFn drive;
    SpFluentSvFn root;
    SpFluentSvFn anchor;
    SpFluentSvFn as_sv;
    SpFluentCstrFn str;
    SpFluentCstrFn as_posix;
    SpFluentBoolFn is_absolute;
    SpFluentRelToFn is_relative_to;
    SpFluentRelativeToFn relative_to;
    SpFluentGetFn get;
};

/* Global fluent instance (declared, defined in implementation) */
#ifdef SNAKEPATH_IMPLEMENTATION
extern SpFluent spf;
#else
extern SpFluent spf;
#endif

/* Path creation macros */
#define SPF(s)   (sp_fluent_init(sp_path(s)), spf)
#define SPF_P(s) (sp_fluent_init(sp_path_f((s), SP_FLAVOR_POSIX)), spf)
#define SPF_W(s) (sp_fluent_init(sp_path_f((s), SP_FLAVOR_WINDOWS)), spf)

/* Initialize fluent context (called by macros) */
void sp_fluent_init(SpPath p);

#endif /* SNAKEPATH_FLUENT */

#endif /* SNAKEPATH_H */

/* ============ Implementation ============ */
#ifdef SNAKEPATH_IMPLEMENTATION

#ifdef __cplusplus
extern "C" {
#endif

static inline bool sp_priv_is_windows_flavor(SpFlavor flavor) {
#ifdef SP_WINDOWS
    return flavor == SP_FLAVOR_WINDOWS || flavor == SP_FLAVOR_NATIVE;
#else
    return flavor == SP_FLAVOR_WINDOWS;
#endif
}

static inline bool sp_priv_is_sep(char c, SpFlavor flavor) {
    return sp_priv_is_windows_flavor(flavor) ? (c == '/' || c == '\\') : (c == '/');
}

static inline char sp_priv_sep(SpFlavor flavor) {
    return sp_priv_is_windows_flavor(flavor) ? '\\' : '/';
}

static inline bool sp_priv_is_drive_letter(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static inline bool sp_priv_has_drive(const char *s, size_t len, SpFlavor flavor) {
    if (!sp_priv_is_windows_flavor(flavor)) return false;
    return len >= 2 && sp_priv_is_drive_letter(s[0]) && s[1] == ':';
}

static inline bool sp_priv_is_unc(const char *s, size_t len, SpFlavor flavor) {
    if (!sp_priv_is_windows_flavor(flavor)) return false;
    return len >= 2 && sp_priv_is_sep(s[0], flavor) && sp_priv_is_sep(s[1], flavor);
}

static size_t sp_priv_drive_len(const char *s, size_t len, SpFlavor flavor) {
    if (sp_priv_has_drive(s, len, flavor)) {
        return 2;
    }
    if (sp_priv_is_unc(s, len, flavor)) {
        /* UNC: //server/share */
        size_t i = 2;
        while (i < len && !sp_priv_is_sep(s[i], flavor)) i++;
        if (i < len) {
            i++;
            while (i < len && !sp_priv_is_sep(s[i], flavor)) i++;
        }
        return i;
    }
    return 0;
}

static size_t sp_priv_root_len(const char *s, size_t len, SpFlavor flavor) {
    size_t start = sp_priv_drive_len(s, len, flavor);
    if (start < len && sp_priv_is_sep(s[start], flavor)) {
        return 1;
    }
    return 0;
}

static size_t sp_priv_anchor_len(const char *s, size_t len, SpFlavor flavor) {
    return sp_priv_drive_len(s, len, flavor) + sp_priv_root_len(s, len, flavor);
}

static void sp_priv_normalize(char *buf, size_t *len, SpFlavor flavor) {
    size_t i, j = 0;
    char sep = sp_priv_sep(flavor);
    bool last_was_sep = false;
    size_t anchor = sp_priv_anchor_len(buf, *len, flavor);

    /* Preserve anchor as-is but normalize its separators on Windows */
    for (i = 0; i < anchor && i < *len; i++) {
        if (sp_priv_is_sep(buf[i], flavor)) {
            buf[j++] = sep;
        } else {
            buf[j++] = buf[i];
        }
    }
    last_was_sep = (j > 0 && sp_priv_is_sep(buf[j-1], flavor));

    for (; i < *len; i++) {
        if (sp_priv_is_sep(buf[i], flavor)) {
            if (!last_was_sep) {
                buf[j++] = sep;
                last_was_sep = true;
            }
        } else {
            buf[j++] = buf[i];
            last_was_sep = false;
        }
    }
    /* Remove trailing sep unless it's the root */
    if (j > anchor && sp_priv_is_sep(buf[j-1], flavor)) {
        j--;
    }
    buf[j] = '\0';
    *len = j;
}

SpPath sp_path_new(const char *s, SpPathOpts opts) {
    SP_ASSERT_FLAVOR(opts.flavor);
    SpPath p = SP_PRIV_ZERO;
    p.flavor = opts.flavor;
    if (s) {
        p.len = strlen(s);
        if (p.len >= SP_PATH_MAX) p.len = SP_PATH_MAX - 1;
        memcpy(p.buf, s, p.len);
        p.buf[p.len] = '\0';
        sp_priv_normalize(p.buf, &p.len, p.flavor);
    }
    /* Normalize empty path to "." (matches Python pathlib behavior) */
    if (p.len == 0) {
        p.buf[0] = '.';
        p.buf[1] = '\0';
        p.len = 1;
    }
    return p;
}

SpPath sp_path_from_sv(SpStr sv, SpFlavor flavor) {
    SP_ASSERT_FLAVOR(flavor);
    assert((sv.len == 0 || sv.data != NULL) && "SpStr with non-zero len must have valid data");
    SpPath p = SP_PRIV_ZERO;
    p.flavor = flavor;
    p.len = sv.len;
    if (p.len >= SP_PATH_MAX) p.len = SP_PATH_MAX - 1;
    if (sv.data && p.len > 0) {
        memcpy(p.buf, sv.data, p.len);
    }
    p.buf[p.len] = '\0';
    sp_priv_normalize(p.buf, &p.len, p.flavor);
    /* Normalize empty path to "." (matches Python pathlib behavior) */
    if (p.len == 0) {
        p.buf[0] = '.';
        p.buf[1] = '\0';
        p.len = 1;
    }
    return p;
}

SpPath sp_path_copy(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpPath r = *p;
    return r;
}

const char *sp_str(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    return p->buf;
}

SpStr sp_as_sv(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    return SP_PRIV_STR(p->buf, p->len);
}

void sp_as_posix(const SpPath *p, char *out, size_t out_size) {
    SP_ASSERT_PATH_INVARIANT(p);
    assert(out != NULL && "output buffer must not be NULL");
    assert(out_size > 0 && "output buffer size must be positive");
    size_t i;
    size_t n = p->len < out_size - 1 ? p->len : out_size - 1;
    for (i = 0; i < n; i++) {
        out[i] = (p->buf[i] == '\\') ? '/' : p->buf[i];
    }
    out[n] = '\0';
}

SpStr sp_drive(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    size_t dlen = sp_priv_drive_len(p->buf, p->len, p->flavor);
    assert(dlen <= p->len && "drive length must not exceed path length");
    return SP_PRIV_STR(p->buf, dlen);
}

SpStr sp_root(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    size_t start = sp_priv_drive_len(p->buf, p->len, p->flavor);
    size_t rlen = sp_priv_root_len(p->buf, p->len, p->flavor);
    assert(start + rlen <= p->len && "root must not exceed path bounds");
    return SP_PRIV_STR(p->buf + start, rlen);
}

SpStr sp_anchor(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    size_t alen = sp_priv_anchor_len(p->buf, p->len, p->flavor);
    assert(alen <= p->len && "anchor length must not exceed path length");
    return SP_PRIV_STR(p->buf, alen);
}

SpStr sp_name(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (p->len == 0) return SP_PRIV_STR(p->buf, 0);
    size_t anchor = sp_priv_anchor_len(p->buf, p->len, p->flavor);
    if (anchor == p->len) return SP_PRIV_STR(p->buf + p->len, 0);
    size_t i = p->len;
    while (i > anchor && !sp_priv_is_sep(p->buf[i-1], p->flavor)) i--;
    assert(i <= p->len && "name start position must be within bounds");
    return SP_PRIV_STR(p->buf + i, p->len - i);
}

SpStr sp_suffix(const SpPath *p) {
    SpStr name = sp_name(p);
    if (name.len == 0) return SP_PRIV_STR(SP_PRIV_NULL, 0);
    size_t i = name.len;
    while (i > 0 && name.data[i-1] != '.') i--;
    if (i <= 1) return SP_PRIV_STR(SP_PRIV_NULL, 0);
    if (i >= name.len) return SP_PRIV_STR(SP_PRIV_NULL, 0);
    return SP_PRIV_STR(name.data + i - 1, name.len - i + 1);
}

SpStr sp_stem(const SpPath *p) {
    SpStr name = sp_name(p);
    SpStr suffix = sp_suffix(p);
    return SP_PRIV_STR(name.data, name.len - suffix.len);
}

SpSuffixes sp_suffixes(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpSuffixes r = SP_PRIV_ZERO;
    SpStr name = sp_name(p);
    if (name.len == 0) return r;
    size_t i = 0;
    /* Skip leading dot (hidden file) */
    if (name.len > 0 && name.data[0] == '.') i = 1;
    while (i < name.len && r.count < SP_MAX_SUFFIXES) {
        size_t dot = i;
        while (dot < name.len && name.data[dot] != '.') dot++;
        if (dot < name.len) {
            r.items[r.count].data = name.data + dot;
            size_t end = dot + 1;
            while (end < name.len && name.data[end] != '.') end++;
            r.items[r.count].len = end - dot;
            r.count++;
            i = end;
        } else {
            break;
        }
    }
    assert(r.count <= SP_MAX_SUFFIXES && "suffixes count must not exceed maximum");
    return r;
}

SpPath sp_parent(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    size_t anchor = sp_priv_anchor_len(p->buf, p->len, p->flavor);
    if (p->len <= anchor) return sp_path_copy(p);
    size_t i = p->len;
    while (i > anchor && !sp_priv_is_sep(p->buf[i-1], p->flavor)) i--;
    if (i > anchor) i--;
    if (i == 0 && anchor == 0) {
        return sp_path_new(".", SP_PRIV_OPTS(p->flavor));
    }
    if (i <= anchor) i = anchor;
    assert(i < SP_PATH_MAX && "parent length must be within bounds");
    SpPath r = SP_PRIV_ZERO;
    r.flavor = p->flavor;
    r.len = i;
    memcpy(r.buf, p->buf, r.len);
    r.buf[r.len] = '\0';
    return r;
}

SpPartsIter sp_parts_begin(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpPartsIter it = SP_PRIV_ZERO;
    it.path = p;
    it.pos = 0;
    it.include_anchor = sp_priv_anchor_len(p->buf, p->len, p->flavor) > 0;
    it.anchor_done = false;
    return it;
}

bool sp_parts_next(SpPartsIter *it, SpStr *out) {
    assert(it != NULL && "iterator must not be NULL");
    assert(it->path != NULL && "iterator path must not be NULL");
    assert(out != NULL && "output SpStr must not be NULL");
    assert(it->pos <= it->path->len && "iterator position must be within bounds");
    const SpPath *p = it->path;
    size_t anchor = sp_priv_anchor_len(p->buf, p->len, p->flavor);
    if (it->include_anchor && !it->anchor_done) {
        out->data = p->buf;
        out->len = anchor;
        it->anchor_done = true;
        it->pos = anchor;
        return true;
    }
    while (it->pos < p->len && sp_priv_is_sep(p->buf[it->pos], p->flavor)) {
        it->pos++;
    }
    if (it->pos >= p->len) return false;
    size_t start = it->pos;
    while (it->pos < p->len && !sp_priv_is_sep(p->buf[it->pos], p->flavor)) {
        it->pos++;
    }
    assert(start < it->pos && "part must have non-zero length");
    out->data = p->buf + start;
    out->len = it->pos - start;
    return true;
}

size_t sp_parts_count(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpPartsIter it = sp_parts_begin(p);
    SpStr part;
    size_t count = 0;
    while (sp_parts_next(&it, &part)) count++;
    return count;
}

SpParentsIter sp_parents_begin(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpParentsIter it = SP_PRIV_ZERO;
    it.current = sp_parent(p);
    size_t anchor = sp_priv_anchor_len(p->buf, p->len, p->flavor);
    it.done = (p->len <= anchor);
    return it;
}

bool sp_parents_next(SpParentsIter *it, SpPath *out) {
    assert(it != NULL && "iterator must not be NULL");
    assert(out != NULL && "output path must not be NULL");
    if (it->done) return false;
    *out = it->current;
    SpPath next = sp_parent(&it->current);
    if (sp_path_eq(&next, &it->current)) {
        it->done = true;
    } else {
        it->current = next;
    }
    return true;
}

SpPath sp_join_one(const SpPath *base, const char *other) {
    SP_ASSERT_PATH_INVARIANT(base);
    if (!other || !*other) return sp_path_copy(base);
    size_t olen = strlen(other);
    SpFlavor flavor = base->flavor;
    /* Check if other is absolute or has a drive */
    if (sp_priv_is_sep(other[0], flavor)) {
        /* Has root - check for drive replacement on Windows */
        if (sp_priv_has_drive(other, olen, flavor) || sp_priv_is_unc(other, olen, flavor)) {
            return sp_path_new(other, SP_PRIV_OPTS(flavor));
        }
        /* Root only - keep drive from base */
        size_t dlen = sp_priv_drive_len(base->buf, base->len, flavor);
        if (dlen > 0) {
            SpPath r = SP_PRIV_ZERO;
            r.flavor = flavor;
            memcpy(r.buf, base->buf, dlen);
            r.len = dlen;
            if (r.len + olen < SP_PATH_MAX) {
                memcpy(r.buf + r.len, other, olen);
                r.len += olen;
            }
            r.buf[r.len] = '\0';
            sp_priv_normalize(r.buf, &r.len, flavor);
            return r;
        }
        return sp_path_new(other, SP_PRIV_OPTS(flavor));
    }
    if (sp_priv_has_drive(other, olen, flavor)) {
        /* Other has drive */
        char other_drive = other[0];
        char base_drive = base->len >= 2 ? base->buf[0] : '\0';
        bool same_drive = (sp_priv_is_drive_letter(other_drive) && sp_priv_is_drive_letter(base_drive) &&
                         ((other_drive | 32) == (base_drive | 32)));
        if (same_drive && olen == 2) {
            /* Same drive, relative */
            return sp_path_copy(base);
        }
        if (same_drive) {
            /* Same drive - append after drive */
            SpPath r = sp_path_copy(base);
            if (r.len > 0 && !sp_priv_is_sep(r.buf[r.len-1], flavor)) {
                if (r.len + 1 < SP_PATH_MAX) r.buf[r.len++] = sp_priv_sep(flavor);
            }
            if (r.len + olen - 2 < SP_PATH_MAX) {
                memcpy(r.buf + r.len, other + 2, olen - 2);
                r.len += olen - 2;
            }
            r.buf[r.len] = '\0';
            sp_priv_normalize(r.buf, &r.len, flavor);
            return r;
        }
        /* Different drive */
        return sp_path_new(other, SP_PRIV_OPTS(flavor));
    }
    /* Relative path - simple join */
    SpPath r = sp_path_copy(base);
    if (r.len > 0 && !sp_priv_is_sep(r.buf[r.len-1], flavor)) {
        if (r.len + 1 < SP_PATH_MAX) r.buf[r.len++] = sp_priv_sep(flavor);
    }
    if (r.len + olen < SP_PATH_MAX) {
        memcpy(r.buf + r.len, other, olen);
        r.len += olen;
    }
    r.buf[r.len] = '\0';
    sp_priv_normalize(r.buf, &r.len, flavor);
    return r;
}

SpPath sp_join_impl(const SpPath *base, const char **parts) {
    SP_ASSERT_PATH_INVARIANT(base);
    assert(parts != NULL && "parts array must not be NULL");
    SpPath r = sp_path_copy(base);
    for (size_t i = 0; parts[i] != NULL; i++) {
        r = sp_join_one(&r, parts[i]);
    }
    return r;
}

SpPath sp_joinpath(const SpPath *base, const SpPath *other) {
    SP_ASSERT_PATH_INVARIANT(base);
    SP_ASSERT_PATH_INVARIANT(other);
    return sp_join_one(base, other->buf);
}

SpPath sp_with_name(const SpPath *p, const char *name) {
    SP_ASSERT_PATH_INVARIANT(p);
    assert(name != NULL && "name must not be NULL");
    SpPath parent = sp_parent(p);
    return sp_join_one(&parent, name);
}

SpPath sp_with_stem(const SpPath *p, const char *stem) {
    SP_ASSERT_PATH_INVARIANT(p);
    assert(stem != NULL && "stem must not be NULL");
    SpStr suffix = sp_suffix(p);
    char name[SP_PATH_MAX];
    size_t slen = strlen(stem);
    if (slen + suffix.len >= SP_PATH_MAX) slen = SP_PATH_MAX - suffix.len - 1;
    memcpy(name, stem, slen);
    if (suffix.len > 0) {
        assert(suffix.data != NULL && "suffix data must be valid when len > 0");
        memcpy(name + slen, suffix.data, suffix.len);
    }
    name[slen + suffix.len] = '\0';
    return sp_with_name(p, name);
}

SpPath sp_with_suffix(const SpPath *p, const char *suffix) {
    SP_ASSERT_PATH_INVARIANT(p);
    assert(suffix != NULL && "suffix must not be NULL");
    SpStr stem = sp_stem(p);
    char name[SP_PATH_MAX];
    size_t suflen = strlen(suffix);
    size_t stemlen = stem.len;
    if (stemlen + suflen >= SP_PATH_MAX) stemlen = SP_PATH_MAX - suflen - 1;
    if (stemlen > 0) {
        assert(stem.data != NULL && "stem data must be valid when len > 0");
        memcpy(name, stem.data, stemlen);
    }
    memcpy(name + stemlen, suffix, suflen);
    name[stemlen + suflen] = '\0';
    return sp_with_name(p, name);
}

bool sp_is_absolute(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpStr root = sp_root(p);
    if (root.len > 0) {
        if (sp_priv_is_windows_flavor(p->flavor)) {
            return sp_drive(p).len > 0;
        }
        return true;
    }
    return false;
}

bool sp_is_relative_to(const SpPath *p, const SpPath *other) {
    SP_ASSERT_PATH_INVARIANT(p);
    SP_ASSERT_PATH_INVARIANT(other);
    SpPartsIter it_p = sp_parts_begin(p);
    SpPartsIter it_o = sp_parts_begin(other);
    SpStr part_p, part_o;
    while (sp_parts_next(&it_o, &part_o)) {
        if (!sp_parts_next(&it_p, &part_p)) return false;
        if (!sp_sv_eq(part_p, part_o)) return false;
    }
    return true;
}

SpPath sp_relative_to(const SpPath *p, const SpPath *other) {
    SP_ASSERT_PATH_INVARIANT(p);
    SP_ASSERT_PATH_INVARIANT(other);
    if (!sp_is_relative_to(p, other)) {
        SpPath empty = SP_PRIV_ZERO;
        return empty;
    }
    SpPartsIter it_p = sp_parts_begin(p);
    SpPartsIter it_o = sp_parts_begin(other);
    SpStr part;
    while (sp_parts_next(&it_o, &part)) {
        sp_parts_next(&it_p, &part);
    }
    SpPath r = SP_PRIV_ZERO;
    r.flavor = p->flavor;
    bool first = true;
    while (sp_parts_next(&it_p, &part)) {
        assert(part.data != NULL && "part data must be valid");
        if (!first) {
            if (r.len + 1 < SP_PATH_MAX) r.buf[r.len++] = sp_priv_sep(p->flavor);
        }
        if (r.len + part.len < SP_PATH_MAX) {
            memcpy(r.buf + r.len, part.data, part.len);
            r.len += part.len;
        }
        first = false;
    }
    r.buf[r.len] = '\0';
    if (r.len == 0) {
        r.buf[0] = '.';
        r.buf[1] = '\0';
        r.len = 1;
    }
    assert(r.len < SP_PATH_MAX && "result length must be within bounds");
    return r;
}

bool sp_path_eq(const SpPath *a, const SpPath *b) {
    SP_ASSERT_PATH_INVARIANT(a);
    SP_ASSERT_PATH_INVARIANT(b);
    if (a->len != b->len) return false;
    return memcmp(a->buf, b->buf, a->len) == 0;
}


/* ============ Fluent API Implementation ============ */
#ifdef SNAKEPATH_FLUENT

/* Thread-local context for fluent operations */
static SP_TLS SpPath sp_priv_f_ctx;
static SP_TLS char sp_priv_f_posix_buf[SP_PATH_MAX];

/* Initialize the fluent context */
void sp_fluent_init(SpPath p) {
    assert(p.len < SP_PATH_MAX && "path length must be within bounds");
    assert(p.buf[p.len] == '\0' && "path must be null-terminated");
    SP_ASSERT_FLAVOR(p.flavor);
    sp_priv_f_ctx = p;
}

/* Chainable method implementations */
static SpFluent sp_priv_f_parent(void) {
    sp_priv_f_ctx = sp_parent(&sp_priv_f_ctx);
    extern SpFluent spf;
    return spf;
}

static SpFluent sp_priv_f_join(const char *s) {
    assert(s != NULL && "join argument must not be NULL");
    sp_priv_f_ctx = sp_join_one(&sp_priv_f_ctx, s);
    extern SpFluent spf;
    return spf;
}

static SpFluent sp_priv_f_with_name(const char *s) {
    assert(s != NULL && "name argument must not be NULL");
    sp_priv_f_ctx = sp_with_name(&sp_priv_f_ctx, s);
    extern SpFluent spf;
    return spf;
}

static SpFluent sp_priv_f_with_stem(const char *s) {
    assert(s != NULL && "stem argument must not be NULL");
    sp_priv_f_ctx = sp_with_stem(&sp_priv_f_ctx, s);
    extern SpFluent spf;
    return spf;
}

static SpFluent sp_priv_f_with_suffix(const char *s) {
    assert(s != NULL && "suffix argument must not be NULL");
    sp_priv_f_ctx = sp_with_suffix(&sp_priv_f_ctx, s);
    extern SpFluent spf;
    return spf;
}

/* Accessor method implementations */
static SpStr sp_priv_f_name(void) {
    return sp_name(&sp_priv_f_ctx);
}

static SpStr sp_priv_f_stem(void) {
    return sp_stem(&sp_priv_f_ctx);
}

static SpStr sp_priv_f_suffix(void) {
    return sp_suffix(&sp_priv_f_ctx);
}

static SpSuffixes sp_priv_f_suffixes(void) {
    return sp_suffixes(&sp_priv_f_ctx);
}

static SpStr sp_priv_f_drive(void) {
    return sp_drive(&sp_priv_f_ctx);
}

static SpStr sp_priv_f_root(void) {
    return sp_root(&sp_priv_f_ctx);
}

static SpStr sp_priv_f_anchor(void) {
    return sp_anchor(&sp_priv_f_ctx);
}

static SpStr sp_priv_f_as_sv(void) {
    return sp_as_sv(&sp_priv_f_ctx);
}

static const char *sp_priv_f_str(void) {
    return sp_str(&sp_priv_f_ctx);
}

static const char *sp_priv_f_as_posix(void) {
    sp_as_posix(&sp_priv_f_ctx, sp_priv_f_posix_buf, sizeof(sp_priv_f_posix_buf));
    return sp_priv_f_posix_buf;
}

static bool sp_priv_f_is_absolute(void) {
    return sp_is_absolute(&sp_priv_f_ctx);
}

static bool sp_priv_f_is_relative_to(const SpPath *other) {
    SP_ASSERT_PATH_INVARIANT(other);
    return sp_is_relative_to(&sp_priv_f_ctx, other);
}

static SpPath sp_priv_f_relative_to(const SpPath *other) {
    SP_ASSERT_PATH_INVARIANT(other);
    return sp_relative_to(&sp_priv_f_ctx, other);
}

static const SpPath *sp_priv_f_get(void) {
    return &sp_priv_f_ctx;
}

/* Global fluent instance with all methods */
SpFluent spf = {
    /* Chainable */
    .parent = sp_priv_f_parent,
    .join = sp_priv_f_join,
    .with_name = sp_priv_f_with_name,
    .with_stem = sp_priv_f_with_stem,
    .with_suffix = sp_priv_f_with_suffix,
    /* Accessors */
    .name = sp_priv_f_name,
    .stem = sp_priv_f_stem,
    .suffix = sp_priv_f_suffix,
    .suffixes = sp_priv_f_suffixes,
    .drive = sp_priv_f_drive,
    .root = sp_priv_f_root,
    .anchor = sp_priv_f_anchor,
    .as_sv = sp_priv_f_as_sv,
    .str = sp_priv_f_str,
    .as_posix = sp_priv_f_as_posix,
    .is_absolute = sp_priv_f_is_absolute,
    .is_relative_to = sp_priv_f_is_relative_to,
    .relative_to = sp_priv_f_relative_to,
    .get = sp_priv_f_get
};

#endif /* SNAKEPATH_FLUENT */

#ifdef __cplusplus
}
#endif

#endif /* SNAKEPATH_IMPLEMENTATION */
