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
#define SP_PRIV_CAST(type, val) static_cast<type>(val)
#else
#define SP_PRIV_STR(d, l) ((SpStr){.data = (d), .len = (l)})
#define SP_PRIV_OPTS(f) ((SpPathOpts){.flavor = (f)})
#define SP_PRIV_ZERO {0}
#define SP_PRIV_NULL NULL
#define SP_PRIV_CAST(type, val) ((type)(val))
#endif

#ifndef SP_PATH_MAX
#define SP_PATH_MAX 4096
#endif

#ifndef SP_MAX_SUFFIXES
#define SP_MAX_SUFFIXES 16
#endif

/* Error sentinels for path results (len=0 with special buf[0] values) */
#define SP_ERR_NONE       '\x00'  /* No error (or empty path) */
#define SP_ERR_NOT_RELATIVE '\x01'  /* Not relative to other path */
#define SP_ERR_NO_NAME    '\x02'  /* Path has no usable name */
#define SP_ERR_INVALID_ARG '\x03'  /* Invalid argument (name/stem/suffix) */

/* Match result codes */
#define SP_MATCH_YES      1   /* Pattern matched */
#define SP_MATCH_NO       0   /* Pattern did not match */
#define SP_MATCH_ERR_EMPTY   -1  /* Empty pattern */
#define SP_MATCH_ERR_INVALID -2  /* Invalid pattern (. or ..) */

/* Platform detection */
#if defined(_WIN32) || defined(_WIN64)
#define SP_WINDOWS 1
#else
#define SP_POSIX 1
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
SpPath sp_path_convert(const char *s, SpFlavor src_flavor, SpFlavor dest_flavor);
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
SpPath sp_relative_to_walk_up(const SpPath *p, const SpPath *other);
bool sp_is_relative_to(const SpPath *p, const SpPath *other);
/* Multi-segment variants (parts is NULL-terminated array of strings) */
SpPath sp_relative_to_parts(const SpPath *p, const char **parts, bool walk_up);
bool sp_is_relative_to_parts(const SpPath *p, const char **parts);

bool sp_is_absolute(const SpPath *p);
SpPath sp_cwd(SpFlavor flavor);
SpPath sp_absolute(const SpPath *p);
size_t sp_as_uri(const SpPath *p, char *buf, size_t buf_size);
bool sp_path_eq(const SpPath *a, const SpPath *b);
int sp_path_cmp(const SpPath *a, const SpPath *b);
unsigned long sp_path_hash(const SpPath *p);
int sp_match(const SpPath *p, const char *pattern);
int sp_match_ex(const SpPath *p, const char *pattern, int case_sensitive);  /* Returns SP_MATCH_* codes */
bool sp_is_reserved(const SpPath *p);

/* Error checking for path results */
static inline bool sp_path_is_error(const SpPath *p) {
    return p->len == 0 && p->buf[0] != SP_ERR_NONE;
}
static inline int sp_path_error_code(const SpPath *p) {
    return p->len == 0 ? SP_PRIV_CAST(int, SP_PRIV_CAST(unsigned char, p->buf[0])) : 0;
}

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

/* Forward declare the fluent struct */
typedef struct SpFluentPath SpFluentPath;

/* Function pointer types for chainable methods */
typedef SpFluentPath (*SpFluentVoidFn)(void);
typedef SpFluentPath (*SpFluentStrFn)(const char *);
typedef SpFluentPath (*SpFluentPathFn)(const SpPath *);

/* The fluent API struct - each instance carries its own path.
 * Only path→path transformations are methods; use sp_*(&f.path) for accessors. */
struct SpFluentPath {
    SpPath path;
    SpFluentVoidFn parent;
    SpFluentStrFn join;
    SpFluentStrFn with_name;
    SpFluentStrFn with_stem;
    SpFluentStrFn with_suffix;
    SpFluentVoidFn absolute;
    SpFluentPathFn relative_to;
    SpFluentPathFn relative_to_walk_up;
};

/* Initialize fluent context and create SpFluentPath (called by macros) */
SpFluentPath sp_fluent_init(SpPath p);

/* Path creation macros - return SpFluentPath with its own copy of the path */
#define SPF(s)   sp_fluent_init(sp_path(s))
#define SPF_P(s) sp_fluent_init(sp_path_f((s), SP_FLAVOR_POSIX))
#define SPF_W(s) sp_fluent_init(sp_path_f((s), SP_FLAVOR_WINDOWS))

/* Resume chaining from a stored SpFluentPath */
#define SP(f) sp_fluent_init((f).path)

#endif /* SNAKEPATH_FLUENT */

#endif /* SNAKEPATH_H */

/* ============ Implementation ============ */
#ifdef SNAKEPATH_IMPLEMENTATION

/* Platform-specific includes for getcwd */
#ifdef SP_WINDOWS
#include <direct.h>
#define sp_priv_getcwd _getcwd
#else
#include <unistd.h>
#define sp_priv_getcwd getcwd
#endif

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

static inline char sp_priv_tolower(char c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

/* Case-insensitive string comparison for Windows paths */
static inline bool sp_priv_str_eq_ci(const char *a, size_t alen, const char *b, size_t blen) {
    if (alen != blen) return false;
    for (size_t i = 0; i < alen; i++) {
        if (sp_priv_tolower(a[i]) != sp_priv_tolower(b[i])) return false;
    }
    return true;
}

static inline bool sp_priv_has_drive(const char *s, size_t len, SpFlavor flavor) {
    if (!sp_priv_is_windows_flavor(flavor)) return false;
    return len >= 2 && sp_priv_is_drive_letter(s[0]) && s[1] == ':';
}

static inline bool sp_priv_is_unc(const char *s, size_t len, SpFlavor flavor) {
    if (!sp_priv_is_windows_flavor(flavor)) return false;
    return len >= 2 && sp_priv_is_sep(s[0], flavor) && sp_priv_is_sep(s[1], flavor);
}

/* Helper to check if s starts with "//?/UNC" (case-insensitive) */
static inline bool sp_priv_is_unc_device_path(const char *s, size_t len, SpFlavor flavor) {
    if (!sp_priv_is_windows_flavor(flavor) || len < 7) return false;
    if (!sp_priv_is_sep(s[0], flavor) || !sp_priv_is_sep(s[1], flavor)) return false;
    if (s[2] != '?' && s[2] != '.') return false;
    if (!sp_priv_is_sep(s[3], flavor)) return false;
    /* Check for "UNC" (case-insensitive) */
    char c4 = s[4], c5 = s[5], c6 = s[6];
    if (c4 >= 'a' && c4 <= 'z') c4 -= 32;
    if (c5 >= 'a' && c5 <= 'z') c5 -= 32;
    if (c6 >= 'a' && c6 <= 'z') c6 -= 32;
    return c4 == 'U' && c5 == 'N' && c6 == 'C' && (len == 7 || sp_priv_is_sep(s[7], flavor));
}

static size_t sp_priv_drive_len(const char *s, size_t len, SpFlavor flavor) {
    if (sp_priv_has_drive(s, len, flavor)) {
        return 2;
    }
    if (sp_priv_is_unc(s, len, flavor)) {
        /* Check for special //?/UNC namespace */
        if (sp_priv_is_unc_device_path(s, len, flavor)) {
            /* //?/UNC namespace: drive extends through server\share if complete,
               or includes trailing separator if incomplete */
            size_t i = 7;  /* Past "//?/UNC" */
            if (i >= len) return i;  /* Just "//?/UNC" */
            if (sp_priv_is_sep(s[i], flavor)) i++;  /* Past the separator */
            if (i >= len) return i;  /* "//?/UNC/" - include trailing sep */

            /* Find server component */
            size_t server_start = i;
            while (i < len && !sp_priv_is_sep(s[i], flavor)) i++;
            if (i == server_start) return i;  /* Empty server - return as is */
            if (i >= len) return i;  /* "//?/UNC/server" - no trailing sep */

            /* Include separator after server for incomplete paths */
            i++;  /* Past separator */
            if (i >= len) return i;  /* "//?/UNC/server/" - include trailing sep */

            /* Find share component */
            size_t share_start = i;
            while (i < len && !sp_priv_is_sep(s[i], flavor)) i++;
            if (i == share_start) return i;  /* Empty share - include trailing sep */

            /* Complete UNC - drive is up to end of share (no trailing sep) */
            return i;
        }

        /* Regular UNC: //server/share */
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

    /* Windows UNC paths: only COMPLETE UNC (//server/share) has root,
       and device namespace paths (//. or //?) only have root if explicit */
    if (sp_priv_is_windows_flavor(flavor) && sp_priv_is_unc(s, len, flavor)) {
        /* Special handling for //?/UNC namespace */
        if (sp_priv_is_unc_device_path(s, len, flavor)) {
            /* //?/UNC paths only have root if complete (with server AND share) */
            size_t i = 8;  /* Past "//?/UNC/" */
            if (i >= len) return 0;  /* No server - incomplete */

            /* Find server component */
            while (i < len && !sp_priv_is_sep(s[i], flavor)) i++;
            if (i >= len) return 0;  /* Server only, no separator */
            i++;  /* Past separator */
            if (i >= len) return 0;  /* Server and sep, no share */

            /* Find share component */
            size_t share_start = i;
            while (i < len && !sp_priv_is_sep(s[i], flavor)) i++;
            if (i == share_start) return 0;  /* Empty share - incomplete */

            /* Complete //?/UNC/server/share - has root */
            if (start < len && sp_priv_is_sep(s[start], flavor)) {
                return 1;
            }
            return 1;  /* Implicit root */
        }

        /* Device namespace paths (//. or //?) - check if it's one */
        bool is_device_ns = (len > 2 && (s[2] == '.' || s[2] == '?') &&
                            (len == 3 || sp_priv_is_sep(s[3], flavor)));
        if (is_device_ns) {
            /* Device namespace: only has root if there's explicit separator after drive */
            if (start < len && sp_priv_is_sep(s[start], flavor)) {
                return 1;  /* Explicit root */
            }
            return 0;  /* No root */
        }

        /* A complete UNC path has form //server/share where both server and share
           are non-empty. We detect this by checking if drive parsing found a
           separator after position 2 (between server and share). */
        size_t sep_after_server = 2;
        while (sep_after_server < len && !sp_priv_is_sep(s[sep_after_server], flavor))
            sep_after_server++;
        if (sep_after_server >= len) return 0;  /* No sep after server - incomplete */

        /* Check there's actual share content after the separator */
        size_t share_start = sep_after_server + 1;
        if (share_start >= len || sp_priv_is_sep(s[share_start], flavor)) return 0;

        /* Complete UNC has implicit root */
        if (start < len && sp_priv_is_sep(s[start], flavor)) {
            return 1;  /* Explicit root separator present */
        }
        return 1;  /* Implicit root */
    }

    /* POSIX: paths starting with exactly // have root // */
    if (!sp_priv_is_windows_flavor(flavor) && len >= 2 &&
        s[0] == '/' && s[1] == '/' && (len == 2 || s[2] != '/')) {
        return 2;
    }

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
    size_t drive = sp_priv_drive_len(buf, *len, flavor);
    size_t root = sp_priv_root_len(buf, *len, flavor);
    size_t anchor = drive + root;

    /* Preserve anchor as-is but normalize its separators on Windows */
    for (i = 0; i < anchor && i < *len; i++) {
        if (sp_priv_is_sep(buf[i], flavor)) {
            buf[j++] = sep;
        } else {
            buf[j++] = buf[i];
        }
    }

    /* For COMPLETE UNC paths without trailing separator, add the implicit root.
       A complete UNC has form //server/share - check that we have both parts.
       Device namespace paths (//. or //?) need special handling. */
    if (sp_priv_is_windows_flavor(flavor) && sp_priv_is_unc(buf, *len, flavor)) {
        bool is_device_ns = (*len > 2 && (buf[2] == '.' || buf[2] == '?') &&
                            (*len <= 3 || sp_priv_is_sep(buf[3], flavor)));

        /* Check for //?/UNC paths which ARE complete UNC and need implicit root */
        if (is_device_ns && sp_priv_is_unc_device_path(buf, *len, flavor)) {
            /* //?/UNC paths: add implicit root if complete (has server+share) */
            size_t k = 8;  /* Past "//?/UNC/" */
            if (k < *len) {
                /* Find server */
                while (k < *len && !sp_priv_is_sep(buf[k], flavor)) k++;
                if (k < *len) {
                    k++;  /* Past separator */
                    size_t share_start = k;
                    while (k < *len && !sp_priv_is_sep(buf[k], flavor)) k++;
                    if (k > share_start && drive == *len && j + 1 < SP_PATH_MAX) {
                        /* Complete //?/UNC/server/share - add root separator */
                        buf[j++] = sep;
                    }
                }
            }
        } else if (!is_device_ns) {
            /* Regular UNC: Check for complete UNC (has separator between server and share) */
            size_t sep_pos = 2;
            while (sep_pos < *len && !sp_priv_is_sep(buf[sep_pos], flavor)) sep_pos++;
            bool has_server_sep = (sep_pos < *len);
            bool has_share = has_server_sep && (sep_pos + 1 < *len) &&
                             !sp_priv_is_sep(buf[sep_pos + 1], flavor);
            if (has_share && drive == *len && j + 1 < SP_PATH_MAX) {
                /* Complete UNC path with no content after share - add root separator */
                buf[j++] = sep;
            }
        }
        /* Other device namespace paths (//./device, //?/device) don't get implicit root */
    }

    /* For relative paths (no anchor) or paths with just a drive (no root),
       treat start as "after separator" for '.' handling.
       This ensures c:. normalizes to c: just like ./a normalizes to a */
    last_was_sep = (anchor == 0) ||
                   (j > 0 && sp_priv_is_sep(buf[j-1], flavor)) ||
                   (drive > 0 && root == 0);  /* Drive but no root (e.g., c:) */

    for (; i < *len; i++) {
        if (sp_priv_is_sep(buf[i], flavor)) {
            if (!last_was_sep) {
                buf[j++] = sep;
                last_was_sep = true;
            }
        } else {
            /* Skip single '.' components (but not '..' or longer) */
            if (buf[i] == '.' && last_was_sep) {
                size_t k = i + 1;
                if (k >= *len || sp_priv_is_sep(buf[k], flavor)) {
                    /* On Windows, don't skip '.' if next component looks like a drive
                       AND we don't already have a drive (e.g., './c:a' should stay as
                       '.\c:a', not become 'c:a', but 'D:/./c:a' can become 'D:/c:a') */
                    if (sp_priv_is_windows_flavor(flavor) && k < *len && drive == 0) {
                        /* Check if component after separator looks like drive letter */
                        size_t next = k + 1;
                        if (next + 1 < *len && sp_priv_is_drive_letter(buf[next]) &&
                            buf[next + 1] == ':') {
                            /* Don't skip - keep the '.' to protect from drive parsing */
                            buf[j++] = buf[i];
                            last_was_sep = false;
                            continue;
                        }
                    }
                    /* It's just '.', skip it */
                    i = k - 1;  /* will be incremented by loop */
                    continue;
                }
            }
            buf[j++] = buf[i];
            last_was_sep = false;
        }
    }
    /* Remove trailing sep unless it's part of the anchor */
    size_t new_anchor = sp_priv_anchor_len(buf, j, flavor);
    if (j > new_anchor && sp_priv_is_sep(buf[j-1], flavor)) {
        j--;
    }
    buf[j] = '\0';
    *len = j;
}

/* Helper to return an empty path (works in both C and C++) */
static inline SpPath sp_priv_empty_path(void) {
    SpPath p = SP_PRIV_ZERO;
    return p;
}

/* Helper to return an error path with specific error code */
static inline SpPath sp_priv_error_path(char err_code) {
    SpPath p = SP_PRIV_ZERO;
    p.buf[0] = err_code;
    return p;
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
    /* Empty paths stay empty - sp_str will return "." for display */
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
    /* Empty paths stay empty - sp_str will return "." for display */
    return p;
}

SpPath sp_path_convert(const char *s, SpFlavor src_flavor, SpFlavor dest_flavor) {
    SP_ASSERT_FLAVOR(src_flavor);
    SP_ASSERT_FLAVOR(dest_flavor);
    if (!s || !*s) return sp_path_new(s, SP_PRIV_OPTS(dest_flavor));

    /* Parse with source flavor, then rebuild with dest flavor */
    SpPath src = sp_path_new(s, SP_PRIV_OPTS(src_flavor));
    if (src_flavor == dest_flavor) return src;

    /* Extract parts from source and join with dest separator */
    SpPath dest = SP_PRIV_ZERO;
    dest.flavor = dest_flavor;
    char dest_sep = sp_priv_sep(dest_flavor);

    SpPartsIter it = sp_parts_begin(&src);
    SpStr part;
    bool first = true;
    while (sp_parts_next(&it, &part)) {
        if (!first && dest.len > 0 && !sp_priv_is_sep(dest.buf[dest.len-1], dest_flavor)) {
            if (dest.len + 1 < SP_PATH_MAX) dest.buf[dest.len++] = dest_sep;
        }
        /* Copy part, converting separators */
        for (size_t i = 0; i < part.len && dest.len < SP_PATH_MAX - 1; i++) {
            if (sp_priv_is_sep(part.data[i], src_flavor)) {
                dest.buf[dest.len++] = dest_sep;
            } else {
                dest.buf[dest.len++] = part.data[i];
            }
        }
        first = false;
    }
    dest.buf[dest.len] = '\0';
    sp_priv_normalize(dest.buf, &dest.len, dest_flavor);
    return dest;
}

SpPath sp_path_copy(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpPath r = *p;
    return r;
}

const char *sp_str(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    /* Empty path displays as "." */
    if (p->len == 0) return ".";
    return p->buf;
}

SpStr sp_as_sv(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    /* Empty path returns "." as string view */
    if (p->len == 0) return SP_PRIV_STR(".", 1);
    return SP_PRIV_STR(p->buf, p->len);
}

void sp_as_posix(const SpPath *p, char *out, size_t out_size) {
    SP_ASSERT_PATH_INVARIANT(p);
    assert(out != NULL && "output buffer must not be NULL");
    assert(out_size > 0 && "output buffer size must be positive");
    /* Empty path displays as "." */
    if (p->len == 0) {
        if (out_size >= 2) {
            out[0] = '.';
            out[1] = '\0';
        } else {
            out[0] = '\0';
        }
        return;
    }
    size_t i;
    size_t n = p->len < out_size - 1 ? p->len : out_size - 1;
    for (i = 0; i < n; i++) {
        out[i] = (p->buf[i] == '\\') ? '/' : p->buf[i];
    }
    out[n] = '\0';
}

SpStr sp_drive(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (p->len == 0) return SP_PRIV_STR(SP_PRIV_NULL, 0);
    size_t dlen = sp_priv_drive_len(p->buf, p->len, p->flavor);
    assert(dlen <= p->len && "drive length must not exceed path length");
    return SP_PRIV_STR(p->buf, dlen);
}

SpStr sp_root(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (p->len == 0) return SP_PRIV_STR(SP_PRIV_NULL, 0);
    size_t start = sp_priv_drive_len(p->buf, p->len, p->flavor);
    size_t rlen = sp_priv_root_len(p->buf, p->len, p->flavor);
    /* UNC paths are normalized with trailing separator, so root is always in buffer */
    if (start + rlen > p->len) rlen = p->len > start ? p->len - start : 0;
    return SP_PRIV_STR(p->buf + start, rlen);
}

SpStr sp_anchor(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (p->len == 0) return SP_PRIV_STR(SP_PRIV_NULL, 0);
    size_t alen = sp_priv_anchor_len(p->buf, p->len, p->flavor);
    /* UNC paths are normalized with trailing separator, so anchor is always in buffer */
    if (alen > p->len) alen = p->len;
    return SP_PRIV_STR(p->buf, alen);
}

SpStr sp_name(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    /* Empty path has empty name */
    if (p->len == 0) return SP_PRIV_STR(SP_PRIV_NULL, 0);
    size_t anchor = sp_priv_anchor_len(p->buf, p->len, p->flavor);
    if (anchor == p->len) return SP_PRIV_STR(p->buf + p->len, 0);
    size_t i = p->len;
    while (i > anchor && !sp_priv_is_sep(p->buf[i-1], p->flavor)) i--;
    size_t name_len = p->len - i;
    const char *name_start = p->buf + i;
    /* '.' has empty name when it's the entire relative path (no parent directory).
       Note: '..' does NOT have empty name - CPython treats it as a real name component */
    if (i == 0 && name_len == 1 && name_start[0] == '.') {
        return SP_PRIV_STR(SP_PRIV_NULL, 0);
    }
    return SP_PRIV_STR(name_start, name_len);
}

SpStr sp_suffix(const SpPath *p) {
    SpStr name = sp_name(p);
    if (name.len == 0) return SP_PRIV_STR(SP_PRIV_NULL, 0);
    /* Special cases: names consisting only of dots have no suffix (., .., ..., etc.) */
    bool all_dots = true;
    for (size_t j = 0; j < name.len; j++) {
        if (name.data[j] != '.') { all_dots = false; break; }
    }
    if (all_dots) return SP_PRIV_STR(SP_PRIV_NULL, 0);
    size_t i = name.len;
    while (i > 0 && name.data[i-1] != '.') i--;
    /* i <= 1 means no dot found, or dot is at start (hidden file like .bashrc) */
    if (i <= 1) return SP_PRIV_STR(SP_PRIV_NULL, 0);
    /* i == name.len means trailing dot only (file.) - no suffix (CPython behavior) */
    if (i == name.len) return SP_PRIV_STR(SP_PRIV_NULL, 0);
    /* Suffix includes the dot */
    return SP_PRIV_STR(name.data + i - 1, name.len - i + 1);
}

SpStr sp_stem(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpStr name = sp_name(p);
    if (name.len == 0) return SP_PRIV_STR(SP_PRIV_NULL, 0);
    SpStr suffix = sp_suffix(p);
    return SP_PRIV_STR(name.data, name.len - suffix.len);
}

SpSuffixes sp_suffixes(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpSuffixes r = SP_PRIV_ZERO;
    SpStr name = sp_name(p);
    if (name.len == 0) return r;
    /* Trailing dot means no suffixes (CPython behavior) */
    if (name.data[name.len - 1] == '.') return r;
    size_t i = 0;
    /* Skip leading dot (hidden file) */
    if (name.data[0] == '.') i = 1;
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
    /* Empty path's parent is '.' */
    if (p->len == 0) {
        return sp_path_new(".", SP_PRIV_OPTS(p->flavor));
    }
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
    /* Empty path has no parts */
    if (p->len == 0) {
        it.include_anchor = false;
        it.anchor_done = true;
        return it;
    }
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
retry:
    while (it->pos < p->len && sp_priv_is_sep(p->buf[it->pos], p->flavor)) {
        it->pos++;
    }
    if (it->pos >= p->len) return false;
    size_t start = it->pos;
    while (it->pos < p->len && !sp_priv_is_sep(p->buf[it->pos], p->flavor)) {
        it->pos++;
    }
    assert(start < it->pos && "part must have non-zero length");

    /* On Windows, skip leading '.' if it's just protecting a drive letter.
       This matches CPython behavior where '.\c:a' has parts=('c:a',) */
    if (sp_priv_is_windows_flavor(p->flavor) && start == 0 &&
        it->pos - start == 1 && p->buf[start] == '.') {
        /* Check if next component looks like a drive letter */
        size_t next_start = it->pos;
        while (next_start < p->len && sp_priv_is_sep(p->buf[next_start], p->flavor))
            next_start++;
        if (next_start + 1 < p->len && sp_priv_is_drive_letter(p->buf[next_start]) &&
            p->buf[next_start + 1] == ':') {
            /* Skip this '.' part, get the next one */
            goto retry;
        }
    }

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
    /* If parent equals self, there are no parents (e.g., '.', '/') */
    it.done = sp_path_eq(p, &it.current);
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
    /* If base is empty, joining gives us other */
    if (base->len == 0) {
        return sp_path_new(other, SP_PRIV_OPTS(base->flavor));
    }
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
            /* Same drive letter only (e.g., "c:") - keep base but use other's drive casing */
            SpPath r = sp_path_copy(base);
            r.buf[0] = other_drive;  /* Use casing from other */
            return r;
        }
        if (same_drive) {
            /* Check if other has a root (e.g., c:/x/y vs c:x/y) */
            bool other_has_root = (olen > 2 && sp_priv_is_sep(other[2], flavor));
            if (other_has_root) {
                /* Same drive with root - other replaces base entirely */
                return sp_path_new(other, SP_PRIV_OPTS(flavor));
            }
            /* Same drive, no root - use other's drive casing, keep base path, append other's relative part */
            SpPath r = SP_PRIV_ZERO;
            r.flavor = flavor;
            /* Copy base path but with other's drive casing */
            memcpy(r.buf, base->buf, base->len);
            r.len = base->len;
            r.buf[0] = other_drive;  /* Use casing from other */
            /* Add separator if needed */
            if (r.len > 0 && !sp_priv_is_sep(r.buf[r.len-1], flavor)) {
                if (r.len + 1 < SP_PATH_MAX) r.buf[r.len++] = sp_priv_sep(flavor);
            }
            /* Append content after other's drive */
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
    size_t anchor = sp_priv_anchor_len(r.buf, r.len, flavor);
    /* Add separator unless:
       - base ends with separator already
       - base is drive-only (e.g., "D:") - drive-relative paths don't get separator */
    bool is_drive_only = (anchor == r.len && anchor == 2 &&
                          sp_priv_has_drive(r.buf, r.len, flavor) &&
                          sp_priv_root_len(r.buf, r.len, flavor) == 0);
    if (!is_drive_only && r.len > 0 && !sp_priv_is_sep(r.buf[r.len-1], flavor)) {
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

/* Validate name/stem doesn't contain separators.
   Returns true if valid, false if invalid. */
static bool sp_priv_is_valid_name(const char *s, size_t len, SpFlavor flavor) {
    if (len == 0) return false;
    /* Reject '.' and '..' */
    if (len == 1 && s[0] == '.') return false;
    if (len == 2 && s[0] == '.' && s[1] == '.') return false;
    for (size_t i = 0; i < len; i++) {
        if (sp_priv_is_sep(s[i], flavor)) return false;
    }
    /* Note: Python pathlib allows drive-letter-like names (e.g., 'd:') */
    return true;
}

/* Check if path has a usable name (not empty, not '.') */
static bool sp_priv_has_usable_name(const SpPath *p) {
    SpStr name = sp_name(p);
    if (name.len == 0) return false;
    if (name.len == 1 && name.data[0] == '.') return false;
    return true;
}

SpPath sp_with_name(const SpPath *p, const char *name) {
    SP_ASSERT_PATH_INVARIANT(p);
    assert(name != NULL && "name must not be NULL");
    /* Validate: path must have a name, new name must be valid */
    if (!sp_priv_has_usable_name(p)) return sp_priv_error_path(SP_ERR_NO_NAME);
    size_t nlen = strlen(name);
    if (!sp_priv_is_valid_name(name, nlen, p->flavor)) return sp_priv_error_path(SP_ERR_INVALID_ARG);

    /* Build path: parent + separator + name (treat name as literal, not as path) */
    SpPath parent = sp_parent(p);
    SpPath r = SP_PRIV_ZERO;
    r.flavor = p->flavor;
    char sep = sp_priv_sep(p->flavor);

    /* Copy parent */
    memcpy(r.buf, parent.buf, parent.len);
    r.len = parent.len;

    /* On Windows, if parent is empty and name looks like a drive letter,
       we need to prefix with ".\" to keep it relative (e.g., "d:" → ".\d:") */
    if (sp_priv_is_windows_flavor(p->flavor) && r.len == 0 &&
        nlen >= 2 && sp_priv_is_drive_letter(name[0]) && name[1] == ':') {
        r.buf[r.len++] = '.';
        r.buf[r.len++] = sep;
    }

    /* Add separator if parent is not empty and doesn't end with separator */
    if (r.len > 0 && !sp_priv_is_sep(r.buf[r.len-1], p->flavor)) {
        if (r.len + 1 < SP_PATH_MAX) r.buf[r.len++] = sep;
    }

    /* Append name (copy literally, don't interpret as path) */
    if (r.len + nlen < SP_PATH_MAX) {
        memcpy(r.buf + r.len, name, nlen);
        r.len += nlen;
    }
    r.buf[r.len] = '\0';
    return r;
}

SpPath sp_with_stem(const SpPath *p, const char *stem) {
    SP_ASSERT_PATH_INVARIANT(p);
    assert(stem != NULL && "stem must not be NULL");
    /* Validate: path must have a name, new stem must be valid */
    if (!sp_priv_has_usable_name(p)) return sp_priv_error_path(SP_ERR_NO_NAME);
    size_t slen = strlen(stem);
    if (!sp_priv_is_valid_name(stem, slen, p->flavor)) return sp_priv_error_path(SP_ERR_INVALID_ARG);

    /* Build new name: stem + suffix */
    SpStr suffix = sp_suffix(p);
    char name[SP_PATH_MAX];
    if (slen + suffix.len >= SP_PATH_MAX) slen = SP_PATH_MAX - suffix.len - 1;
    memcpy(name, stem, slen);
    if (suffix.len > 0) {
        memcpy(name + slen, suffix.data, suffix.len);
    }
    name[slen + suffix.len] = '\0';

    /* Use with_name to properly set the name (treats it as literal) */
    return sp_with_name(p, name);
}

SpPath sp_with_suffix(const SpPath *p, const char *suffix) {
    SP_ASSERT_PATH_INVARIANT(p);
    assert(suffix != NULL && "suffix must not be NULL");
    /* Validate: path must have a name */
    if (!sp_priv_has_usable_name(p)) return sp_priv_error_path(SP_ERR_NO_NAME);
    size_t suflen = strlen(suffix);
    /* Validate: suffix must be empty or start with '.' followed by content, no separators */
    if (suflen > 0) {
        if (suffix[0] != '.') return sp_priv_error_path(SP_ERR_INVALID_ARG);
        /* Suffix of just '.' is invalid - must have content after dot */
        if (suflen == 1) return sp_priv_error_path(SP_ERR_INVALID_ARG);
        for (size_t i = 0; i < suflen; i++) {
            if (sp_priv_is_sep(suffix[i], p->flavor)) return sp_priv_error_path(SP_ERR_INVALID_ARG);
        }
        /* Check for drive letter in suffix (Windows) */
        if (sp_priv_is_windows_flavor(p->flavor) && suflen >= 2 && suffix[1] == ':') {
            return sp_priv_error_path(SP_ERR_INVALID_ARG);
        }
    }
    SpStr stem = sp_stem(p);
    char name[SP_PATH_MAX];
    size_t stemlen = stem.len;
    if (stemlen + suflen >= SP_PATH_MAX) stemlen = SP_PATH_MAX - suflen - 1;
    if (stemlen > 0) {
        memcpy(name, stem.data, stemlen);
    }
    memcpy(name + stemlen, suffix, suflen);
    name[stemlen + suflen] = '\0';
    return sp_with_name(p, name);
}

bool sp_is_absolute(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpStr drive = sp_drive(p);
    SpStr root = sp_root(p);

    if (sp_priv_is_windows_flavor(p->flavor)) {
        /* Windows: absolute requires drive + root, OR UNC path */
        if (drive.len >= 2 && sp_priv_is_sep(drive.data[0], p->flavor) &&
            sp_priv_is_sep(drive.data[1], p->flavor)) {
            /* UNC path is always absolute */
            return true;
        }
        /* Regular drive: need both drive and root */
        return drive.len > 0 && root.len > 0;
    }
    /* POSIX: absolute if has root */
    return root.len > 0;
}

SpPath sp_cwd(SpFlavor flavor) {
    char buf[SP_PATH_MAX];
    if (sp_priv_getcwd(buf, SP_PATH_MAX) == NULL) {
        /* On error, return empty path */
        return sp_path_new("", SP_PRIV_OPTS(flavor));
    }
    return sp_path_new(buf, SP_PRIV_OPTS(flavor));
}

SpPath sp_absolute(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (sp_is_absolute(p)) {
        return sp_path_copy(p);
    }
    /* Join cwd with the relative path */
    SpPath cwd = sp_cwd(p->flavor);
    return sp_joinpath(&cwd, p);
}

bool sp_is_relative_to(const SpPath *p, const SpPath *other) {
    SP_ASSERT_PATH_INVARIANT(p);
    SP_ASSERT_PATH_INVARIANT(other);
    /* Empty path matches only relative paths (no anchor) */
    if (other->len == 0) {
        size_t p_anchor = sp_priv_anchor_len(p->buf, p->len, p->flavor);
        return p_anchor == 0;
    }
    SpPartsIter it_p = sp_parts_begin(p);
    SpPartsIter it_o = sp_parts_begin(other);
    SpStr part_p, part_o;
    bool is_windows = sp_priv_is_windows_flavor(p->flavor);
    while (sp_parts_next(&it_o, &part_o)) {
        if (!sp_parts_next(&it_p, &part_p)) return false;
        /* Windows uses case-insensitive comparison */
        if (is_windows) {
            if (!sp_priv_str_eq_ci(part_p.data, part_p.len, part_o.data, part_o.len)) return false;
        } else {
            if (!sp_sv_eq(part_p, part_o)) return false;
        }
    }
    return true;
}

SpPath sp_relative_to(const SpPath *p, const SpPath *other) {
    SP_ASSERT_PATH_INVARIANT(p);
    SP_ASSERT_PATH_INVARIANT(other);
    if (!sp_is_relative_to(p, other)) {
        return sp_priv_error_path(SP_ERR_NOT_RELATIVE);
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
    /* When paths are equal, relative_to returns empty path (Python pathlib behavior) */
    /* r.len == 0 means no remaining parts, which is the empty path */
    assert(r.len < SP_PATH_MAX && "result length must be within bounds");
    return r;
}

/* Helper to check if a part is ".." */
static inline bool sp_priv_is_dotdot(SpStr part) {
    return part.len == 2 && part.data[0] == '.' && part.data[1] == '.';
}

/* Error sentinel for sp_relative_to_walk_up: len=0 and buf[0]=0x01 means error */
static inline SpPath sp_priv_relative_to_error(SpFlavor flavor) {
    SpPath r = SP_PRIV_ZERO;
    r.flavor = flavor;
    r.buf[0] = SP_ERR_NOT_RELATIVE;
    return r;
}

/* Check if result from sp_relative_to or sp_relative_to_walk_up is an error */
static inline bool sp_relative_to_is_error(const SpPath *p) {
    return p->len == 0 && p->buf[0] == SP_ERR_NOT_RELATIVE;
}

SpPath sp_relative_to_walk_up(const SpPath *p, const SpPath *other) {
    SP_ASSERT_PATH_INVARIANT(p);
    SP_ASSERT_PATH_INVARIANT(other);

    /* Collect parts into arrays for comparison */
    SpStr p_parts[SP_PATH_MAX / 2];
    SpStr o_parts[SP_PATH_MAX / 2];
    size_t p_count = 0, o_count = 0;

    SpPartsIter it_p = sp_parts_begin(p);
    SpPartsIter it_o = sp_parts_begin(other);
    SpStr part;

    while (sp_parts_next(&it_p, &part) && p_count < SP_PATH_MAX / 2) {
        p_parts[p_count++] = part;
    }
    while (sp_parts_next(&it_o, &part) && o_count < SP_PATH_MAX / 2) {
        o_parts[o_count++] = part;
    }

    /* Check if other contains '..' - not allowed with walk_up */
    for (size_t i = 0; i < o_count; i++) {
        if (sp_priv_is_dotdot(o_parts[i])) {
            return sp_priv_relative_to_error(p->flavor);
        }
    }

    /* Check if anchors are compatible (must match if present) */
    size_t p_anchor = sp_priv_anchor_len(p->buf, p->len, p->flavor);
    size_t o_anchor = sp_priv_anchor_len(other->buf, other->len, other->flavor);
    bool is_windows = sp_priv_is_windows_flavor(p->flavor);

    if (p_anchor > 0 || o_anchor > 0) {
        /* Both must have same anchor for walk_up to work */
        if (p_anchor > 0 && o_anchor > 0) {
            bool anchor_eq = is_windows
                ? sp_priv_str_eq_ci(p->buf, p_anchor, other->buf, o_anchor)
                : (p_anchor == o_anchor && memcmp(p->buf, other->buf, p_anchor) == 0);
            if (!anchor_eq) {
                return sp_priv_relative_to_error(p->flavor);
            }
        } else if (o_anchor > 0 && p_anchor == 0) {
            /* other is absolute, p is relative - incompatible */
            return sp_priv_relative_to_error(p->flavor);
        } else if (p_anchor > 0 && o_anchor == 0) {
            /* p is absolute, other is relative - incompatible */
            return sp_priv_relative_to_error(p->flavor);
        }
    }

    /* Find common prefix length */
    size_t common = 0;
    while (common < p_count && common < o_count) {
        bool eq = is_windows
            ? sp_priv_str_eq_ci(p_parts[common].data, p_parts[common].len,
                               o_parts[common].data, o_parts[common].len)
            : sp_sv_eq(p_parts[common], o_parts[common]);
        if (!eq) break;
        common++;
    }

    /* Build result: ".." for each part in other after common, then parts from p after common */
    SpPath r = SP_PRIV_ZERO;
    r.flavor = p->flavor;
    char sep = sp_priv_sep(p->flavor);
    bool first = true;

    /* Add ".." for each remaining part in other */
    for (size_t i = common; i < o_count; i++) {
        /* Skip if it's the anchor */
        if (i == 0 && o_anchor > 0) continue;
        if (!first) {
            if (r.len + 1 < SP_PATH_MAX) r.buf[r.len++] = sep;
        }
        if (r.len + 2 < SP_PATH_MAX) {
            r.buf[r.len++] = '.';
            r.buf[r.len++] = '.';
        }
        first = false;
    }

    /* Add remaining parts from p */
    for (size_t i = common; i < p_count; i++) {
        /* Skip if it's the anchor */
        if (i == 0 && p_anchor > 0) continue;
        if (!first) {
            if (r.len + 1 < SP_PATH_MAX) r.buf[r.len++] = sep;
        }
        if (r.len + p_parts[i].len < SP_PATH_MAX) {
            memcpy(r.buf + r.len, p_parts[i].data, p_parts[i].len);
            r.len += p_parts[i].len;
        }
        first = false;
    }

    r.buf[r.len] = '\0';
    return r;
}

bool sp_is_relative_to_parts(const SpPath *p, const char **parts) {
    SP_ASSERT_PATH_INVARIANT(p);
    /* Join parts into a temporary path */
    SpPath empty = SP_PRIV_ZERO;
    empty.flavor = p->flavor;
    SpPath other = sp_join_impl(&empty, parts);
    return sp_is_relative_to(p, &other);
}

SpPath sp_relative_to_parts(const SpPath *p, const char **parts, bool walk_up) {
    SP_ASSERT_PATH_INVARIANT(p);
    /* Join parts into a temporary path */
    SpPath empty = SP_PRIV_ZERO;
    empty.flavor = p->flavor;
    SpPath other = sp_join_impl(&empty, parts);
    return walk_up ? sp_relative_to_walk_up(p, &other) : sp_relative_to(p, &other);
}

size_t sp_as_uri(const SpPath *p, char *buf, size_t buf_size) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (buf_size == 0) return 0;
    buf[0] = '\0';

    /* Only absolute paths can be expressed as URIs */
    if (!sp_is_absolute(p)) return 0;

    /* Get POSIX representation */
    char posix_buf[SP_PATH_MAX];
    sp_as_posix(p, posix_buf, SP_PATH_MAX);
    const char *path = posix_buf;
    size_t path_len = strlen(path);

    /* Calculate required buffer size and build URI */
    size_t pos = 0;

    /* UNC paths: //server/share -> file://server/share */
    if (path_len >= 2 && path[0] == '/' && path[1] == '/') {
        if (pos + 5 < buf_size) {
            memcpy(buf + pos, "file:", 5);
            pos += 5;
        } else {
            return 0;
        }
    } else if (path_len > 0 && path[0] == '/') {
        /* POSIX absolute paths: /path -> file:///path */
        if (pos + 7 < buf_size) {
            memcpy(buf + pos, "file://", 7);
            pos += 7;
        } else {
            return 0;
        }
    } else {
        /* Windows drive paths: C:/path -> file:///C:/path (need extra slash) */
        if (pos + 8 < buf_size) {
            memcpy(buf + pos, "file:///", 8);
            pos += 8;
        } else {
            return 0;
        }
    }

    /* URL-encode the path (RFC 3986) */
    for (size_t i = 0; i < path_len; i++) {
        unsigned char c = SP_PRIV_CAST(unsigned char, path[i]);
        /* Unreserved characters: don't encode */
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '.' ||
            c == '_' || c == '~' || c == '/') {
            if (pos + 1 < buf_size) {
                buf[pos++] = SP_PRIV_CAST(char, c);
            } else {
                return 0;
            }
        } else if (c == ':' && i > 0 && path[i-1] != '/') {
            /* Allow ':' after drive letter (e.g., C:) but encode otherwise */
            if (pos + 1 < buf_size) {
                buf[pos++] = ':';
            } else {
                return 0;
            }
        } else {
            /* Percent-encode */
            if (pos + 3 < buf_size) {
                static const char hex[] = "0123456789ABCDEF";
                buf[pos++] = '%';
                buf[pos++] = hex[c >> 4];
                buf[pos++] = hex[c & 0x0F];
            } else {
                return 0;
            }
        }
    }

    buf[pos] = '\0';
    return pos;
}

bool sp_path_eq(const SpPath *a, const SpPath *b) {
    SP_ASSERT_PATH_INVARIANT(a);
    SP_ASSERT_PATH_INVARIANT(b);
    /* Must have same flavor to compare */
    if (a->flavor != b->flavor) return false;
    if (a->len != b->len) return false;
    /* Windows uses case-insensitive comparison */
    if (sp_priv_is_windows_flavor(a->flavor)) {
        return sp_priv_str_eq_ci(a->buf, a->len, b->buf, b->len);
    }
    return memcmp(a->buf, b->buf, a->len) == 0;
}

int sp_path_cmp(const SpPath *a, const SpPath *b) {
    SP_ASSERT_PATH_INVARIANT(a);
    SP_ASSERT_PATH_INVARIANT(b);
    /* Windows uses case-insensitive comparison */
    if (sp_priv_is_windows_flavor(a->flavor)) {
        size_t min_len = a->len < b->len ? a->len : b->len;
        for (size_t i = 0; i < min_len; i++) {
            char ca = sp_priv_tolower(a->buf[i]);
            char cb = sp_priv_tolower(b->buf[i]);
            if (ca < cb) return -1;
            if (ca > cb) return 1;
        }
        if (a->len < b->len) return -1;
        if (a->len > b->len) return 1;
        return 0;
    }
    int cmp = memcmp(a->buf, b->buf, a->len < b->len ? a->len : b->len);
    if (cmp != 0) return cmp;
    if (a->len < b->len) return -1;
    if (a->len > b->len) return 1;
    return 0;
}

unsigned long sp_path_hash(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    /* DJB2 hash algorithm */
    unsigned long hash = 5381;
    const char *str = sp_str(p);  /* Use sp_str for empty path handling */
    size_t len = p->len > 0 ? p->len : 1;  /* "." for empty paths */
    if (sp_priv_is_windows_flavor(p->flavor)) {
        /* Case-insensitive hash for Windows */
        for (size_t i = 0; i < len; i++) {
            hash = ((hash << 5) + hash) + SP_PRIV_CAST(unsigned char, sp_priv_tolower(str[i]));
        }
    } else {
        for (size_t i = 0; i < len; i++) {
            hash = ((hash << 5) + hash) + SP_PRIV_CAST(unsigned char, str[i]);
        }
    }
    return hash;
}

/* Simple glob pattern matching (supports * and ?) */
static bool sp_priv_fnmatch(const char *pattern, size_t plen, const char *str, size_t slen, bool case_insensitive) {
    size_t pi = 0, si = 0;
    size_t star_pi = SP_PRIV_CAST(size_t, -1), star_si = 0;

    while (si < slen) {
        if (pi < plen && pattern[pi] == '*') {
            star_pi = pi++;
            star_si = si;
        } else if (pi < plen && (pattern[pi] == '?' ||
                   (case_insensitive ? sp_priv_tolower(pattern[pi]) == sp_priv_tolower(str[si])
                                     : pattern[pi] == str[si]))) {
            pi++;
            si++;
        } else if (star_pi != SP_PRIV_CAST(size_t, -1)) {
            pi = star_pi + 1;
            si = ++star_si;
        } else {
            return false;
        }
    }
    while (pi < plen && pattern[pi] == '*') pi++;
    return pi == plen;
}

/* Helper: check if pattern part is "**" (matches any single component) */
static bool sp_priv_is_doublestar(const char *pat, size_t len) {
    return len == 2 && pat[0] == '*' && pat[1] == '*';
}

/* Match with explicit case sensitivity control.
   case_sensitive: 0 = case-insensitive, 1 = case-sensitive, -1 = use flavor default */
int sp_match_ex(const SpPath *p, const char *pattern, int case_sensitive) {
    SP_ASSERT_PATH_INVARIANT(p);
    assert(pattern != NULL && "pattern must not be NULL");

    size_t plen = strlen(pattern);
    if (plen == 0) return SP_MATCH_ERR_EMPTY;

    /* '.' and '..' are not valid patterns */
    if ((plen == 1 && pattern[0] == '.') ||
        (plen == 2 && pattern[0] == '.' && pattern[1] == '.')) {
        return SP_MATCH_ERR_INVALID;
    }

    bool is_windows = sp_priv_is_windows_flavor(p->flavor);
    /* Determine case sensitivity: -1 means use flavor default */
    bool case_insensitive = (case_sensitive == -1) ? is_windows : (case_sensitive == 0);

    /* Check if pattern has path separator */
    bool pattern_has_sep = false;
    for (size_t i = 0; i < plen; i++) {
        if (pattern[i] == '/' || (is_windows && pattern[i] == '\\')) {
            pattern_has_sep = true;
            break;
        }
    }

    if (!pattern_has_sep) {
        /* No separator - check for special case of just "**" */
        if (plen == 2 && pattern[0] == '*' && pattern[1] == '*') {
            /* "**" matches everything including empty path */
            return SP_MATCH_YES;
        }
        /* Match against name only */
        SpStr name = sp_name(p);
        if (name.len == 0) return SP_MATCH_NO;
        return sp_priv_fnmatch(pattern, plen, name.data, name.len, case_insensitive) ? SP_MATCH_YES : SP_MATCH_NO;
    }

    /* Check if pattern has anchor (drive and/or root) */
    bool pattern_has_drive = false;
    bool pattern_has_root = false;
    size_t pattern_anchor_len = 0;

    /* Check for drive pattern: letter: or *: */
    if (is_windows && plen >= 2 && (sp_priv_is_drive_letter(pattern[0]) || pattern[0] == '*') && pattern[1] == ':') {
        pattern_has_drive = true;
        pattern_anchor_len = 2;
        if (plen > 2 && sp_priv_is_sep(pattern[2], p->flavor)) {
            pattern_has_root = true;
            pattern_anchor_len = 3;
        }
    } else if (sp_priv_is_sep(pattern[0], p->flavor)) {
        pattern_has_root = true;
        pattern_anchor_len = 1;
    }

    bool pattern_is_anchored = pattern_has_drive || pattern_has_root;

    /* If pattern has drive, path must have compatible drive */
    if (pattern_has_drive) {
        SpStr path_drive = sp_drive(p);
        if (path_drive.len < 2) return SP_MATCH_NO;  /* No drive */
        /* Check drive letter matches (pattern has * or letter) */
        char pdrive = (pattern[0] >= 'a' && pattern[0] <= 'z') ? pattern[0] - 32 : pattern[0];
        char pathd = (path_drive.data[0] >= 'a' && path_drive.data[0] <= 'z') ? path_drive.data[0] - 32 : path_drive.data[0];
        if (pattern[0] != '*' && pdrive != pathd) return SP_MATCH_NO;
    }

    /* If pattern has root, path must have root */
    if (pattern_has_root) {
        SpStr path_root = sp_root(p);
        if (path_root.len == 0) return SP_MATCH_NO;
        /* If pattern has root but no drive, path must also have no drive (on Windows) */
        if (is_windows && !pattern_has_drive) {
            SpStr path_drive = sp_drive(p);
            if (path_drive.len > 0 && !sp_priv_is_unc(p->buf, p->len, p->flavor)) {
                return SP_MATCH_NO;  /* Path has drive but pattern doesn't */
            }
        }
    }

    /* Collect path parts - for UNC paths with root-only patterns, decompose anchor */
    SpStr path_parts[SP_PATH_MAX / 2];
    size_t path_count = 0;
    bool is_unc = sp_priv_is_unc(p->buf, p->len, p->flavor);

    /* For UNC paths with a root-only pattern (no drive), decompose the UNC anchor
       into server and share parts for matching. Pattern like /x/y/z.py matches
       //server/share/file.py where x=server, y=share, z.py=file.py */
    if (is_unc && pattern_has_root && !pattern_has_drive) {
        /* Extract server and share from UNC path */
        size_t i = 2;  /* Skip leading // */
        size_t server_start = i;
        while (i < p->len && !sp_priv_is_sep(p->buf[i], p->flavor)) i++;
        size_t server_len = i - server_start;
        if (server_len > 0) {
            path_parts[path_count].data = p->buf + server_start;
            path_parts[path_count].len = server_len;
            path_count++;
        }
        if (i < p->len) {
            i++;  /* Skip separator */
            size_t share_start = i;
            while (i < p->len && !sp_priv_is_sep(p->buf[i], p->flavor)) i++;
            size_t share_len = i - share_start;
            if (share_len > 0) {
                path_parts[path_count].data = p->buf + share_start;
                path_parts[path_count].len = share_len;
                path_count++;
            }
        }
        /* Now add remaining parts after the anchor */
        SpPartsIter it = sp_parts_begin(p);
        SpStr part;
        bool first = true;
        while (sp_parts_next(&it, &part) && path_count < SP_PATH_MAX / 2) {
            if (first) { first = false; continue; }  /* Skip anchor */
            path_parts[path_count++] = part;
        }
    } else {
        SpPartsIter it = sp_parts_begin(p);
        SpStr part;
        while (sp_parts_next(&it, &part) && path_count < SP_PATH_MAX / 2) {
            path_parts[path_count++] = part;
        }
    }

    /* Split pattern into parts, skipping the anchor if present */
    const char *pattern_parts[SP_PATH_MAX / 2];
    size_t pattern_lens[SP_PATH_MAX / 2];
    size_t pattern_count = 0;
    size_t start = pattern_anchor_len;  /* Skip anchor */
    for (size_t i = start; i <= plen && pattern_count < SP_PATH_MAX / 2; i++) {
        if (i == plen || pattern[i] == '/' || (is_windows && pattern[i] == '\\')) {
            if (i > start) {
                pattern_parts[pattern_count] = pattern + start;
                pattern_lens[pattern_count] = i - start;
                pattern_count++;
            }
            start = i + 1;
        }
    }

    /* For anchored patterns, skip the anchor part (first part is the root/drive).
       But for UNC with root-only pattern, we already decomposed above. */
    size_t path_start = 0;
    if (pattern_is_anchored && path_count > 0 && !(is_unc && pattern_has_root && !pattern_has_drive)) {
        path_start = 1;  /* Skip anchor */
    }
    size_t effective_path_count = path_count - path_start;

    /* For anchored patterns, part counts must match exactly.
       For relative patterns, match from right (pattern can be shorter than path). */
    if (pattern_is_anchored && pattern_count != effective_path_count) return SP_MATCH_NO;
    if (pattern_count > effective_path_count) return SP_MATCH_NO;

    for (size_t i = 0; i < pattern_count; i++) {
        size_t pi = pattern_count - 1 - i;
        size_t si = path_start + effective_path_count - 1 - i;
        const char *pat = pattern_parts[pi];
        size_t pat_len = pattern_lens[pi];

        /* ** matches any single component */
        if (sp_priv_is_doublestar(pat, pat_len)) {
            continue;  /* Always matches */
        }

        if (!sp_priv_fnmatch(pat, pat_len, path_parts[si].data, path_parts[si].len, case_insensitive)) {
            return SP_MATCH_NO;
        }
    }
    return SP_MATCH_YES;
}

/* Match using flavor default for case sensitivity */
int sp_match(const SpPath *p, const char *pattern) {
    return sp_match_ex(p, pattern, -1);
}

bool sp_is_reserved(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    /* Only Windows has reserved names */
    if (!sp_priv_is_windows_flavor(p->flavor)) return false;

    /* UNC paths are never reserved (including device namespace paths) */
    if (sp_priv_is_unc(p->buf, p->len, p->flavor)) return false;

    SpStr name = sp_name(p);
    if (name.len == 0 || name.len > 12) return false;  /* Reserved names are short */

    /* Get name without extension or stream name, normalizing superscript digits to ASCII,
       and ignoring spaces (Windows treats "PRN  " as "PRN").
       Stop at '.' (extension) or ':' (NTFS alternate data stream) */
    char upper[13];
    size_t len = 0;
    for (size_t i = 0; i < name.len && name.data[i] != '.' && name.data[i] != ':' && len < 12; i++) {
        unsigned char c = SP_PRIV_CAST(unsigned char, name.data[i]);
        /* Check for UTF-8 encoded superscript digits (U+00B9, U+00B2, U+00B3) */
        if (c == 0xC2 && i + 1 < name.len) {
            unsigned char c2 = SP_PRIV_CAST(unsigned char, name.data[i + 1]);
            if (c2 == 0xB9) { upper[len++] = '1'; i++; continue; }  /* ¹ → 1 */
            if (c2 == 0xB2) { upper[len++] = '2'; i++; continue; }  /* ² → 2 */
            if (c2 == 0xB3) { upper[len++] = '3'; i++; continue; }  /* ³ → 3 */
        }
        /* Skip trailing spaces - don't add them */
        if (c == ' ') continue;
        /* Normal ASCII uppercasing */
        upper[len++] = SP_PRIV_CAST(char, (c >= 'a' && c <= 'z') ? c - 32 : c);
    }
    upper[len] = '\0';

    /* Check against reserved names */
    static const char *reserved[] = {
        "CON", "PRN", "AUX", "NUL",
        "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
        "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9",
        "CONIN$", "CONOUT$",
        NULL
    };
    for (const char **r = reserved; *r; r++) {
        if (strcmp(upper, *r) == 0) return true;
    }
    return false;
}


/* ============ Fluent API Implementation ============ */
#ifdef SNAKEPATH_FLUENT

/* Thread-local storage - portable across compilers */
/* Note: MSVC check must come first because MSVC with /std:c11 sets __STDC_VERSION__
   but doesn't support _Thread_local - it only supports __declspec(thread) */
#if defined(_MSC_VER)
  #define SP_TLS __declspec(thread)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
  #define SP_TLS _Thread_local
#elif defined(__GNUC__) || defined(__clang__)
  #define SP_TLS __thread
#else
  #define SP_TLS /* fallback: not thread-safe */
#endif

/* Thread-local context for fluent chaining */
static SP_TLS SpPath sp_priv_f_ctx;

/* Forward declarations of chainable methods */
static SpFluentPath sp_priv_f_parent(void);
static SpFluentPath sp_priv_f_join(const char *s);
static SpFluentPath sp_priv_f_with_name(const char *s);
static SpFluentPath sp_priv_f_with_stem(const char *s);
static SpFluentPath sp_priv_f_with_suffix(const char *s);
static SpFluentPath sp_priv_f_absolute(void);
static SpFluentPath sp_priv_f_relative_to(const SpPath *other);
static SpFluentPath sp_priv_f_relative_to_walk_up(const SpPath *other);

/* Helper to create SpFluentPath with current context */
static SpFluentPath sp_priv_f_make(void) {
    return (SpFluentPath){
        .path = sp_priv_f_ctx,
        .parent = sp_priv_f_parent,
        .join = sp_priv_f_join,
        .with_name = sp_priv_f_with_name,
        .with_stem = sp_priv_f_with_stem,
        .with_suffix = sp_priv_f_with_suffix,
        .absolute = sp_priv_f_absolute,
        .relative_to = sp_priv_f_relative_to,
        .relative_to_walk_up = sp_priv_f_relative_to_walk_up,
    };
}

/* Initialize the fluent context and return SpFluentPath */
SpFluentPath sp_fluent_init(SpPath p) {
    assert(p.len < SP_PATH_MAX && "path length must be within bounds");
    assert(p.buf[p.len] == '\0' && "path must be null-terminated");
    SP_ASSERT_FLAVOR(p.flavor);
    sp_priv_f_ctx = p;
    return sp_priv_f_make();
}

/* Chainable method implementations - update context, return SpFluentPath with copy */
static SpFluentPath sp_priv_f_parent(void) {
    sp_priv_f_ctx = sp_parent(&sp_priv_f_ctx);
    return sp_priv_f_make();
}

static SpFluentPath sp_priv_f_join(const char *s) {
    assert(s != NULL && "join argument must not be NULL");
    sp_priv_f_ctx = sp_join_one(&sp_priv_f_ctx, s);
    return sp_priv_f_make();
}

static SpFluentPath sp_priv_f_with_name(const char *s) {
    assert(s != NULL && "name argument must not be NULL");
    sp_priv_f_ctx = sp_with_name(&sp_priv_f_ctx, s);
    return sp_priv_f_make();
}

static SpFluentPath sp_priv_f_with_stem(const char *s) {
    assert(s != NULL && "stem argument must not be NULL");
    sp_priv_f_ctx = sp_with_stem(&sp_priv_f_ctx, s);
    return sp_priv_f_make();
}

static SpFluentPath sp_priv_f_with_suffix(const char *s) {
    assert(s != NULL && "suffix argument must not be NULL");
    sp_priv_f_ctx = sp_with_suffix(&sp_priv_f_ctx, s);
    return sp_priv_f_make();
}

static SpFluentPath sp_priv_f_absolute(void) {
    sp_priv_f_ctx = sp_absolute(&sp_priv_f_ctx);
    return sp_priv_f_make();
}

static SpFluentPath sp_priv_f_relative_to(const SpPath *other) {
    SP_ASSERT_PATH_INVARIANT(other);
    sp_priv_f_ctx = sp_relative_to(&sp_priv_f_ctx, other);
    return sp_priv_f_make();
}

static SpFluentPath sp_priv_f_relative_to_walk_up(const SpPath *other) {
    SP_ASSERT_PATH_INVARIANT(other);
    sp_priv_f_ctx = sp_relative_to_walk_up(&sp_priv_f_ctx, other);
    return sp_priv_f_make();
}

#endif /* SNAKEPATH_FLUENT */

#ifdef __cplusplus
}
#endif

#endif /* SNAKEPATH_IMPLEMENTATION */
