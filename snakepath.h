/* snakepath.h - C99 pathlib port, STB-style header-only library
 * No mallocs. POSIX and Windows compatible.
 *
 * Usage:
 *   #define SNAKEPATH_IMPLEMENTATION
 *   #include "snakepath.h"
 */

#ifndef SNAKEPATH_H
#define SNAKEPATH_H

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
bool sp_is_empty(const SpPath *p);

/* ============ Helper Functions ============ */
static inline bool sp_sv_eq(SpStr a, SpStr b) {
    return a.len == b.len && (a.len == 0 || memcmp(a.data, b.data, a.len) == 0);
}
static inline bool sp_sv_eq_cstr(SpStr a, const char *b) {
    size_t blen = strlen(b);
    return a.len == blen && (a.len == 0 || memcmp(a.data, b, a.len) == 0);
}

#ifdef __cplusplus
}
#endif

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

static size_t sp_priv_root_start(const char *s, size_t len, SpFlavor flavor) {
    return sp_priv_drive_len(s, len, flavor);
}

static size_t sp_priv_root_len(const char *s, size_t len, SpFlavor flavor) {
    size_t start = sp_priv_root_start(s, len, flavor);
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
    SpPath p = SP_PRIV_ZERO;
    p.flavor = opts.flavor;
    if (s) {
        p.len = strlen(s);
        if (p.len >= SP_PATH_MAX) p.len = SP_PATH_MAX - 1;
        memcpy(p.buf, s, p.len);
        p.buf[p.len] = '\0';
        sp_priv_normalize(p.buf, &p.len, p.flavor);
    }
    return p;
}

SpPath sp_path_from_sv(SpStr sv, SpFlavor flavor) {
    SpPath p = SP_PRIV_ZERO;
    p.flavor = flavor;
    p.len = sv.len;
    if (p.len >= SP_PATH_MAX) p.len = SP_PATH_MAX - 1;
    if (sv.data && p.len > 0) {
        memcpy(p.buf, sv.data, p.len);
    }
    p.buf[p.len] = '\0';
    sp_priv_normalize(p.buf, &p.len, p.flavor);
    return p;
}

SpPath sp_path_copy(const SpPath *p) {
    SpPath r = *p;
    return r;
}

const char *sp_str(const SpPath *p) {
    return p->buf;
}

SpStr sp_as_sv(const SpPath *p) {
    return SP_PRIV_STR(p->buf, p->len);
}

void sp_as_posix(const SpPath *p, char *out, size_t out_size) {
    size_t i;
    size_t n = p->len < out_size - 1 ? p->len : out_size - 1;
    for (i = 0; i < n; i++) {
        out[i] = (p->buf[i] == '\\') ? '/' : p->buf[i];
    }
    out[n] = '\0';
}

SpStr sp_drive(const SpPath *p) {
    size_t dlen = sp_priv_drive_len(p->buf, p->len, p->flavor);
    return SP_PRIV_STR(p->buf, dlen);
}

SpStr sp_root(const SpPath *p) {
    size_t start = sp_priv_root_start(p->buf, p->len, p->flavor);
    size_t rlen = sp_priv_root_len(p->buf, p->len, p->flavor);
    return SP_PRIV_STR(p->buf + start, rlen);
}

SpStr sp_anchor(const SpPath *p) {
    size_t alen = sp_priv_anchor_len(p->buf, p->len, p->flavor);
    return SP_PRIV_STR(p->buf, alen);
}

SpStr sp_name(const SpPath *p) {
    if (p->len == 0) return SP_PRIV_STR(p->buf, 0);
    size_t anchor = sp_priv_anchor_len(p->buf, p->len, p->flavor);
    if (anchor == p->len) return SP_PRIV_STR(p->buf + p->len, 0);
    size_t i = p->len;
    while (i > anchor && !sp_priv_is_sep(p->buf[i-1], p->flavor)) i--;
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
    return r;
}

SpPath sp_parent(const SpPath *p) {
    size_t anchor = sp_priv_anchor_len(p->buf, p->len, p->flavor);
    if (p->len <= anchor) return sp_path_copy(p);
    size_t i = p->len;
    while (i > anchor && !sp_priv_is_sep(p->buf[i-1], p->flavor)) i--;
    if (i > anchor) i--;
    if (i == 0 && anchor == 0) {
        return sp_path_new(".", SP_PRIV_OPTS(p->flavor));
    }
    if (i <= anchor) i = anchor;
    SpPath r = SP_PRIV_ZERO;
    r.flavor = p->flavor;
    r.len = i;
    memcpy(r.buf, p->buf, r.len);
    r.buf[r.len] = '\0';
    return r;
}

SpPartsIter sp_parts_begin(const SpPath *p) {
    SpPartsIter it = SP_PRIV_ZERO;
    it.path = p;
    it.pos = 0;
    it.include_anchor = sp_priv_anchor_len(p->buf, p->len, p->flavor) > 0;
    it.anchor_done = false;
    return it;
}

bool sp_parts_next(SpPartsIter *it, SpStr *out) {
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
    out->data = p->buf + start;
    out->len = it->pos - start;
    return true;
}

size_t sp_parts_count(const SpPath *p) {
    SpPartsIter it = sp_parts_begin(p);
    SpStr part;
    size_t count = 0;
    while (sp_parts_next(&it, &part)) count++;
    return count;
}

SpParentsIter sp_parents_begin(const SpPath *p) {
    SpParentsIter it = SP_PRIV_ZERO;
    it.current = sp_parent(p);
    size_t anchor = sp_priv_anchor_len(p->buf, p->len, p->flavor);
    it.done = (p->len <= anchor);
    return it;
}

bool sp_parents_next(SpParentsIter *it, SpPath *out) {
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
    SpPath r = sp_path_copy(base);
    for (size_t i = 0; parts[i] != NULL; i++) {
        r = sp_join_one(&r, parts[i]);
    }
    return r;
}

SpPath sp_joinpath(const SpPath *base, const SpPath *other) {
    return sp_join_one(base, other->buf);
}

SpPath sp_with_name(const SpPath *p, const char *name) {
    SpPath parent = sp_parent(p);
    return sp_join_one(&parent, name);
}

SpPath sp_with_stem(const SpPath *p, const char *stem) {
    SpStr suffix = sp_suffix(p);
    char name[SP_PATH_MAX];
    size_t slen = strlen(stem);
    if (slen + suffix.len >= SP_PATH_MAX) slen = SP_PATH_MAX - suffix.len - 1;
    memcpy(name, stem, slen);
    if (suffix.len > 0) {
        memcpy(name + slen, suffix.data, suffix.len);
    }
    name[slen + suffix.len] = '\0';
    return sp_with_name(p, name);
}

SpPath sp_with_suffix(const SpPath *p, const char *suffix) {
    SpStr stem = sp_stem(p);
    char name[SP_PATH_MAX];
    size_t suflen = strlen(suffix);
    size_t stemlen = stem.len;
    if (stemlen + suflen >= SP_PATH_MAX) stemlen = SP_PATH_MAX - suflen - 1;
    memcpy(name, stem.data, stemlen);
    memcpy(name + stemlen, suffix, suflen);
    name[stemlen + suflen] = '\0';
    return sp_with_name(p, name);
}

bool sp_is_absolute(const SpPath *p) {
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
    return r;
}

bool sp_path_eq(const SpPath *a, const SpPath *b) {
    if (a->len != b->len) return false;
    return memcmp(a->buf, b->buf, a->len) == 0;
}

bool sp_is_empty(const SpPath *p) {
    return p->len == 0;
}

#ifdef __cplusplus
}
#endif

#endif /* SNAKEPATH_IMPLEMENTATION */
