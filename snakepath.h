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
#define SP_PRIV_STR(d, l)                                                                                              \
    SpStr { (d), (l) }
#define SP_PRIV_OPTS(f)                                                                                                \
    SpPathOpts { (f) }
#define SP_PRIV_ZERO                                                                                                   \
    {                                                                                                                  \
    }
#define SP_PRIV_NULL nullptr
#define SP_PRIV_CAST(type, val) static_cast<type>(val)
#else
#define SP_PRIV_STR(d, l) ((SpStr){.data = (d), .len = (l)})
#define SP_PRIV_OPTS(f) ((SpPathOpts){.flavor = (f)})
#define SP_PRIV_ZERO {0}
#define SP_PRIV_NULL NULL
#define SP_PRIV_CAST(type, val) ((type)(val))
#endif

/* Recommended SP_PATH_MAX values - define SP_PATH_MAX before including snakepath.h */
#define SP_PATH_MAX_WINDOWS 1024  /* Windows has 1MB default stack; larger values may cause stack overflow. Use /STACK linker flag to increase. */
#define SP_PATH_MAX_LINUX 4096    /* Linux PATH_MAX; typical 8MB stack handles this fine */

#ifndef SP_PATH_MAX
#error "SP_PATH_MAX must be defined before including snakepath.h. " \
       "Use: #define SP_PATH_MAX SP_PATH_MAX_WINDOWS (1024) or SP_PATH_MAX_LINUX (4096)"
#endif

#ifndef SP_MAX_SUFFIXES
#define SP_MAX_SUFFIXES 16
#endif

/* Error sentinels for path results (len=0 with special buf[0] values) */
#define SP_ERR_NONE '\x00'         /* No error (or empty path) */
#define SP_ERR_NOT_RELATIVE '\x01' /* Not relative to other path */
#define SP_ERR_NO_NAME '\x02'      /* Path has no usable name */
#define SP_ERR_INVALID_ARG '\x03'  /* Invalid argument (name/stem/suffix) */

/* Match result codes */
#define SP_MATCH_YES 1          /* Pattern matched */
#define SP_MATCH_NO 0           /* Pattern did not match */
#define SP_MATCH_ERR_EMPTY -1   /* Empty pattern */
#define SP_MATCH_ERR_INVALID -2 /* Invalid pattern (. or ..) */

/* Platform detection */
#if defined(_WIN32) || defined(_WIN64)
#define SP_WINDOWS 1
#else
#define SP_POSIX 1
#endif

/* Flavors for explicit platform behavior */
typedef enum { SP_FLAVOR_NATIVE = 0, SP_FLAVOR_POSIX, SP_FLAVOR_WINDOWS } SpFlavor;

/* Case sensitivity options for glob/match */
typedef enum { SP_CASE_PLATFORM_DEFAULT = 0, SP_CASE_SENSITIVE, SP_CASE_INSENSITIVE } SpCaseSensitivity;

/* Assertion macros for runtime invariant checking */
#define SP_ASSERT_PATH(p) assert((p) != NULL && "path pointer must not be NULL")
#define SP_ASSERT_FLAVOR(f)                                                                                            \
    assert(((f) == SP_FLAVOR_NATIVE || (f) == SP_FLAVOR_POSIX || (f) == SP_FLAVOR_WINDOWS) && "invalid flavor value")
#define SP_ASSERT_PATH_INVARIANT(p)                                                                                    \
    do {                                                                                                               \
        SP_ASSERT_PATH(p);                                                                                             \
        assert((p)->len < SP_PATH_MAX && "path length exceeds buffer size");                                           \
        assert((p)->buf[(p)->len] == '\0' && "path buffer not null-terminated");                                       \
        SP_ASSERT_FLAVOR((p)->flavor);                                                                                 \
    } while (0)

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

/* Glob iterator limits */
#ifndef SP_GLOB_MAX_DEPTH
#define SP_GLOB_MAX_DEPTH 32
#endif
#ifndef SP_GLOB_MAX_SEGMENTS
#define SP_GLOB_MAX_SEGMENTS 64  /* Support patterns with many segments (e.g., 50 ../..) */
#endif
#ifndef SP_GLOB_PATTERN_MAX
#define SP_GLOB_PATTERN_MAX 256
#endif

/* Glob iterator */
typedef struct {
    int depth;  /* Current depth (0 = base dir), -1 = done/error */
    struct {
        char pattern_buf[SP_GLOB_PATTERN_MAX];
        size_t seg_offsets[SP_GLOB_MAX_SEGMENTS];  /* Offsets into pattern_buf */
        int seg_types[SP_GLOB_MAX_SEGMENTS];
        size_t seg_count;
        bool dir_only;
        bool case_insensitive;
        bool yield_base_pending;  /* When pattern starts with **, yield base dir first */
        SpFlavor flavor;
        SpPath dirs[SP_GLOB_MAX_DEPTH];
        size_t seg_idxs[SP_GLOB_MAX_DEPTH];
        void *handles[SP_GLOB_MAX_DEPTH];
    } priv_;
} SpGlobIter;

/* ============ API Macros ============ */

/* Path creation with optional flavor: sp_path("foo/bar") or sp_path("foo", SP_FLAVOR_POSIX) */
typedef struct {
    SpFlavor flavor;
} SpPathOpts;
#define sp_path(s) sp_path_new((s), SP_PRIV_OPTS(SP_FLAVOR_NATIVE))
#define sp_path_f(s, f) sp_path_new((s), SP_PRIV_OPTS(f))

/* Join paths: sp_join(p, "a", "b", "c") - C only, use sp_join_one in C++ */
#ifndef __cplusplus
#define sp_join(base, ...) sp_join_impl((base), (const char *[]){__VA_ARGS__, NULL})
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
SpPath sp_join_sv(const SpPath *base, SpStr other);
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
int sp_match_ex(const SpPath *p, const char *pattern, int case_sensitive); /* Returns SP_MATCH_* codes */
bool sp_is_reserved(const SpPath *p);
bool sp_is_file(const SpPath *p);
bool sp_is_dir(const SpPath *p);
bool sp_exists(const SpPath *p);

/* Stat result structure - mirrors Python's os.stat_result */
/* Note: field names use sp_ prefix to avoid macro conflicts with system headers */
typedef struct {
    unsigned int sp_mode;      /* File type and permissions */
    unsigned long long sp_ino; /* Inode number */
    unsigned long long sp_dev; /* Device ID */
    unsigned long long sp_nlink; /* Number of hard links */
    unsigned int sp_uid;       /* User ID of owner */
    unsigned int sp_gid;       /* Group ID of owner */
    long long sp_size;         /* Total size in bytes */
    double sp_atime;           /* Time of last access */
    double sp_mtime;           /* Time of last modification */
    double sp_ctime;           /* Time of last status change (Unix) / creation (Windows) */
    bool valid;                /* True if stat succeeded */
} SpStatResult;

SpStatResult sp_stat(const SpPath *p);  /* follows symlinks; sp_lstat() coming later */
bool sp_stat_eq(const SpStatResult *a, const SpStatResult *b);
size_t sp_parents_count(const SpPath *p);

/* mkdir result codes */
#define SP_MKDIR_OK 0
#define SP_MKDIR_ERR_EXISTS 1          /* Directory already exists (and exist_ok=false) */
#define SP_MKDIR_ERR_NOT_FOUND 2       /* Parent directory doesn't exist */
#define SP_MKDIR_ERR_NOT_DIR 3         /* A parent component of the path is not a directory */
#define SP_MKDIR_ERR_PERMISSION 4      /* Permission denied */
#define SP_MKDIR_ERR_OTHER 5           /* Other error */
#define SP_MKDIR_ERR_EXISTS_NOT_DIR 6  /* Path exists but is not a directory (e.g., a file) */
#define SP_MKDIR_DEF_MODE 0777         /* Default mode (0 = use this); umask applied by OS */

int sp_mkdir(const SpPath *p, unsigned int mode, bool parents, bool exist_ok);

/* Glob iterator - iterate over paths matching a pattern
 * sp_glob_begin:  Initialize iterator, returns iterator with depth=0 (or -1 on error)
 * sp_glob_next:   Get next match, returns true if found (match written to out)
 * sp_glob_end:    Close iterator (must be called to release directory handles)
 * sp_rglob_begin: Like sp_glob_begin but prepends "**\/" to pattern
 */
SpGlobIter sp_glob_begin(const SpPath *base, const char *pattern, SpCaseSensitivity cs);
bool sp_glob_next(SpGlobIter *it, SpPath *out);
void sp_glob_end(SpGlobIter *it);
SpGlobIter sp_rglob_begin(const SpPath *base, const char *pattern, SpCaseSensitivity cs);

/* Glob foreach macro - iterates all matches, auto-closes on completion */
#define SP_GLOB_FOREACH(base, pattern, match_var) \
    for (struct { SpGlobIter it; int done; } sp_gctx_ = { sp_glob_begin(base, pattern, SP_CASE_PLATFORM_DEFAULT), 0 }; \
         !sp_gctx_.done; sp_glob_end(&sp_gctx_.it), sp_gctx_.done = 1) \
    for (SpPath match_var; sp_glob_next(&sp_gctx_.it, &match_var); )

#define SP_RGLOB_FOREACH(base, pattern, match_var) \
    for (struct { SpGlobIter it; int done; } sp_gctx_ = { sp_rglob_begin(base, pattern, SP_CASE_PLATFORM_DEFAULT), 0 }; \
         !sp_gctx_.done; sp_glob_end(&sp_gctx_.it), sp_gctx_.done = 1) \
    for (SpPath match_var; sp_glob_next(&sp_gctx_.it, &match_var); )

/* Error checking for path results */
static inline bool sp_path_is_error(const SpPath *p) { return p->len == 0 && p->buf[0] != SP_ERR_NONE; }
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

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sp_fluent_ SpPrivDontUseThisDirectly_;
struct sp_fluent_ {
    /* Terminators - end chain and return value */
    SpPath (*path)(void);
    SpStr (*name)(void);
    SpStr (*stem)(void);
    SpStr (*suffix)(void);
    SpSuffixes (*suffixes)(void);
    SpStr (*drive)(void);
    SpStr (*root)(void);
    SpStr (*anchor)(void);
    const char *(*str)(void);
    bool (*is_absolute)(void);
    bool (*is_relative_to)(const SpPath *);
    bool (*is_file)(void);
    bool (*is_dir)(void);
    bool (*exists)(void);
    /* Chainable - return pointer to avoid stack copies */
    SpPrivDontUseThisDirectly_ *(*parent)(void);
    SpPrivDontUseThisDirectly_ *(*join)(const char *);
    SpPrivDontUseThisDirectly_ *(*with_name)(const char *);
    SpPrivDontUseThisDirectly_ *(*with_stem)(const char *);
    SpPrivDontUseThisDirectly_ *(*with_suffix)(const char *);
    SpPrivDontUseThisDirectly_ *(*absolute)(void);
    SpPrivDontUseThisDirectly_ *(*relative_to)(const SpPath *);
    SpPrivDontUseThisDirectly_ *(*relative_to_walk_up)(const SpPath *);
};
SpPrivDontUseThisDirectly_ *sp_fluent_init_(SpPath);

/* SPF("/a")->join("b")->parent()->str() */
#define SPF(s) sp_fluent_init_(sp_path(s))
#define SPF_P(s) sp_fluent_init_(sp_path_f((s), SP_FLAVOR_POSIX))
#define SPF_W(s) sp_fluent_init_(sp_path_f((s), SP_FLAVOR_WINDOWS))

#ifdef __cplusplus
}
#endif

#endif /* SNAKEPATH_FLUENT */

#endif /* SNAKEPATH_H */

/* ============ Implementation ============ */
#ifdef SNAKEPATH_IMPLEMENTATION

/* Platform-specific includes for getcwd and stat */
#ifdef SP_WINDOWS
#include <direct.h>
#include <windows.h>
#define sp_priv_getcwd _getcwd
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
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

static inline char sp_priv_sep(SpFlavor flavor) { return sp_priv_is_windows_flavor(flavor) ? '\\' : '/'; }

static inline bool sp_priv_is_drive_letter(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }

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

/* Flavor-aware string view comparison (case-insensitive for Windows) */
static inline bool sp_priv_sv_eq_flavor(SpStr a, SpStr b, SpFlavor flavor) {
    return sp_priv_is_windows_flavor(flavor) ? sp_priv_str_eq_ci(a.data, a.len, b.data, b.len) : sp_sv_eq(a, b);
}

/* Path-building helpers */
static inline void sp_priv_append_sep(SpPath *r) {
    if (r->len > 0 && r->len + 1 < SP_PATH_MAX) r->buf[r->len++] = sp_priv_sep(r->flavor);
}
static inline void sp_priv_append_sv(SpPath *r, SpStr sv) {
    if (r->len + sv.len < SP_PATH_MAX) {
        memcpy(r->buf + r->len, sv.data, sv.len);
        r->len += sv.len;
    }
}
static inline void sp_priv_append_cstr(SpPath *r, const char *s, size_t len) {
    if (r->len + len < SP_PATH_MAX) {
        memcpy(r->buf + r->len, s, len);
        r->len += len;
    }
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
            size_t i = 7;                          /* Past "//?/UNC" */
            if (i >= len) return i;                /* Just "//?/UNC" */
            if (sp_priv_is_sep(s[i], flavor)) i++; /* Past the separator */
            if (i >= len) return i;                /* "//?/UNC/" - include trailing sep */

            /* Find server component */
            size_t server_start = i;
            while (i < len && !sp_priv_is_sep(s[i], flavor)) i++;
            if (i == server_start) return i; /* Empty server - return as is */
            if (i >= len) return i;          /* "//?/UNC/server" - no trailing sep */

            /* Include separator after server for incomplete paths */
            i++;                    /* Past separator */
            if (i >= len) return i; /* "//?/UNC/server/" - include trailing sep */

            /* Find share component */
            size_t share_start = i;
            while (i < len && !sp_priv_is_sep(s[i], flavor)) i++;
            if (i == share_start) return i; /* Empty share - include trailing sep */

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
            size_t i = 8;           /* Past "//?/UNC/" */
            if (i >= len) return 0; /* No server - incomplete */

            /* Find server component */
            while (i < len && !sp_priv_is_sep(s[i], flavor)) i++;
            if (i >= len) return 0; /* Server only, no separator */
            i++;                    /* Past separator */
            if (i >= len) return 0; /* Server and sep, no share */

            /* Find share component */
            size_t share_start = i;
            while (i < len && !sp_priv_is_sep(s[i], flavor)) i++;
            if (i == share_start) return 0; /* Empty share - incomplete */

            /* Complete //?/UNC/server/share - has root */
            if (start < len && sp_priv_is_sep(s[start], flavor)) {
                return 1;
            }
            return 1; /* Implicit root */
        }

        /* Device namespace paths (//. or //?) - check if it's one */
        bool is_device_ns = (len > 2 && (s[2] == '.' || s[2] == '?') && (len == 3 || sp_priv_is_sep(s[3], flavor)));
        if (is_device_ns) {
            /* Device namespace: only has root if there's explicit separator after drive */
            if (start < len && sp_priv_is_sep(s[start], flavor)) {
                return 1; /* Explicit root */
            }
            return 0; /* No root */
        }

        /* A complete UNC path has form //server/share where both server and share
           are non-empty. We detect this by checking if drive parsing found a
           separator after position 2 (between server and share). */
        size_t sep_after_server = 2;
        while (sep_after_server < len && !sp_priv_is_sep(s[sep_after_server], flavor)) sep_after_server++;
        if (sep_after_server >= len) return 0; /* No sep after server - incomplete */

        /* Check there's actual share content after the separator */
        size_t share_start = sep_after_server + 1;
        if (share_start >= len || sp_priv_is_sep(s[share_start], flavor)) return 0;

        /* Complete UNC has implicit root */
        if (start < len && sp_priv_is_sep(s[start], flavor)) {
            return 1; /* Explicit root separator present */
        }
        return 1; /* Implicit root */
    }

    /* POSIX: paths starting with exactly // have root // */
    if (!sp_priv_is_windows_flavor(flavor) && len >= 2 && s[0] == '/' && s[1] == '/' && (len == 2 || s[2] != '/')) {
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
        bool is_device_ns =
            (*len > 2 && (buf[2] == '.' || buf[2] == '?') && (*len <= 3 || sp_priv_is_sep(buf[3], flavor)));

        /* Check for //?/UNC paths which ARE complete UNC and need implicit root */
        if (is_device_ns && sp_priv_is_unc_device_path(buf, *len, flavor)) {
            /* //?/UNC paths: add implicit root if complete (has server+share) */
            size_t k = 8; /* Past "//?/UNC/" */
            if (k < *len) {
                /* Find server */
                while (k < *len && !sp_priv_is_sep(buf[k], flavor)) k++;
                if (k < *len) {
                    k++; /* Past separator */
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
            bool has_share = has_server_sep && (sep_pos + 1 < *len) && !sp_priv_is_sep(buf[sep_pos + 1], flavor);
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
    last_was_sep = (anchor == 0) || (j > 0 && sp_priv_is_sep(buf[j - 1], flavor)) ||
                   (drive > 0 && root == 0); /* Drive but no root (e.g., c:) */

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
                        if (next + 1 < *len && sp_priv_is_drive_letter(buf[next]) && buf[next + 1] == ':') {
                            /* Don't skip - keep the '.' to protect from drive parsing */
                            buf[j++] = buf[i];
                            last_was_sep = false;
                            continue;
                        }
                    }
                    /* It's just '.', skip it */
                    i = k - 1; /* will be incremented by loop */
                    continue;
                }
            }
            buf[j++] = buf[i];
            last_was_sep = false;
        }
    }
    /* Remove trailing sep unless it's part of the anchor */
    size_t new_anchor = sp_priv_anchor_len(buf, j, flavor);
    if (j > new_anchor && sp_priv_is_sep(buf[j - 1], flavor)) {
        j--;
    }
    buf[j] = '\0';
    *len = j;
}

static inline SpPath sp_priv_empty_path(void) {
    SpPath p = SP_PRIV_ZERO;
    return p;
}

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
    return p;
}

SpPath sp_path_from_sv(SpStr sv, SpFlavor flavor) {
    SP_ASSERT_FLAVOR(flavor);
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

SpPath sp_path_convert(const char *s, SpFlavor src_flavor, SpFlavor dest_flavor) {
    SP_ASSERT_FLAVOR(src_flavor);
    SP_ASSERT_FLAVOR(dest_flavor);
    if (!s || !*s) return sp_path_new(s, SP_PRIV_OPTS(dest_flavor));
    SpPath src = sp_path_new(s, SP_PRIV_OPTS(src_flavor));
    if (src_flavor == dest_flavor) return src;

    SpPath dest = SP_PRIV_ZERO;
    dest.flavor = dest_flavor;
    char dsep = sp_priv_sep(dest_flavor);
    SpPartsIter it = sp_parts_begin(&src);
    SpStr part;
    bool first = true;
    while (sp_parts_next(&it, &part)) {
        if (!first && dest.len > 0 && !sp_priv_is_sep(dest.buf[dest.len - 1], dest_flavor)) {
            if (dest.len + 1 < SP_PATH_MAX) dest.buf[dest.len++] = dsep;
        }
        for (size_t i = 0; i < part.len && dest.len < SP_PATH_MAX - 1; i++) {
            dest.buf[dest.len++] = sp_priv_is_sep(part.data[i], src_flavor) ? dsep : part.data[i];
        }
        first = false;
    }
    dest.buf[dest.len] = '\0';
    sp_priv_normalize(dest.buf, &dest.len, dest_flavor);
    return dest;
}

SpPath sp_path_copy(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    return *p;
}

const char *sp_str(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    return p->len == 0 ? "." : p->buf;
}

SpStr sp_as_sv(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    return p->len == 0 ? SP_PRIV_STR(".", 1) : SP_PRIV_STR(p->buf, p->len);
}

void sp_as_posix(const SpPath *p, char *out, size_t out_size) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (p->len == 0) {
        if (out_size >= 2) {
            out[0] = '.';
            out[1] = '\0';
        } else if (out_size > 0) {
            out[0] = '\0';
        }
        return;
    }
    size_t n = p->len < out_size - 1 ? p->len : out_size - 1;
    for (size_t i = 0; i < n; i++) {
        out[i] = (p->buf[i] == '\\') ? '/' : p->buf[i];
    }
    out[n] = '\0';
}

SpStr sp_drive(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (p->len == 0) return SP_PRIV_STR(SP_PRIV_NULL, 0);
    return SP_PRIV_STR(p->buf, sp_priv_drive_len(p->buf, p->len, p->flavor));
}

SpStr sp_root(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (p->len == 0) return SP_PRIV_STR(SP_PRIV_NULL, 0);
    size_t start = sp_priv_drive_len(p->buf, p->len, p->flavor);
    size_t rlen = sp_priv_root_len(p->buf, p->len, p->flavor);
    if (start + rlen > p->len) rlen = p->len > start ? p->len - start : 0;
    return SP_PRIV_STR(p->buf + start, rlen);
}

SpStr sp_anchor(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (p->len == 0) return SP_PRIV_STR(SP_PRIV_NULL, 0);
    size_t alen = sp_priv_anchor_len(p->buf, p->len, p->flavor);
    return SP_PRIV_STR(p->buf, alen > p->len ? p->len : alen);
}

SpStr sp_name(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (p->len == 0) return SP_PRIV_STR(SP_PRIV_NULL, 0);
    size_t anchor = sp_priv_anchor_len(p->buf, p->len, p->flavor);
    if (anchor == p->len) return SP_PRIV_STR(p->buf + p->len, 0);
    size_t i = p->len;
    while (i > anchor && !sp_priv_is_sep(p->buf[i - 1], p->flavor)) i--;
    if (i == 0 && p->len - i == 1 && p->buf[i] == '.') return SP_PRIV_STR(SP_PRIV_NULL, 0);
    return SP_PRIV_STR(p->buf + i, p->len - i);
}

SpStr sp_suffix(const SpPath *p) {
    SpStr name = sp_name(p);
    if (name.len == 0) return SP_PRIV_STR(SP_PRIV_NULL, 0);
    bool all_dots = true;
    for (size_t j = 0; j < name.len && all_dots; j++) {
        if (name.data[j] != '.') all_dots = false;
    }
    if (all_dots) return SP_PRIV_STR(SP_PRIV_NULL, 0);
    size_t i = name.len;
    while (i > 0 && name.data[i - 1] != '.') i--;
    if (i <= 1 || i == name.len) return SP_PRIV_STR(SP_PRIV_NULL, 0);
    return SP_PRIV_STR(name.data + i - 1, name.len - i + 1);
}

SpStr sp_stem(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpStr name = sp_name(p);
    if (name.len == 0) return SP_PRIV_STR(SP_PRIV_NULL, 0);
    return SP_PRIV_STR(name.data, name.len - sp_suffix(p).len);
}

SpSuffixes sp_suffixes(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpSuffixes r = SP_PRIV_ZERO;
    SpStr name = sp_name(p);
    if (name.len == 0 || name.data[name.len - 1] == '.') return r;
    size_t i = (name.data[0] == '.') ? 1 : 0;
    while (i < name.len && r.count < SP_MAX_SUFFIXES) {
        size_t dot = i;
        while (dot < name.len && name.data[dot] != '.') dot++;
        if (dot >= name.len) break;
        r.items[r.count].data = name.data + dot;
        size_t end = dot + 1;
        while (end < name.len && name.data[end] != '.') end++;
        r.items[r.count++].len = end - dot;
        i = end;
    }
    return r;
}

SpPath sp_parent(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (p->len == 0) return sp_path_new(".", SP_PRIV_OPTS(p->flavor));
    size_t anchor = sp_priv_anchor_len(p->buf, p->len, p->flavor);
    if (p->len <= anchor) return sp_path_copy(p);
    size_t i = p->len;
    while (i > anchor && !sp_priv_is_sep(p->buf[i - 1], p->flavor)) i--;
    if (i > anchor) i--;
    if (i == 0 && anchor == 0) return sp_path_new(".", SP_PRIV_OPTS(p->flavor));
    if (i <= anchor) i = anchor;
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
    if (p->len == 0) {
        it.anchor_done = true;
        return it;
    }
    it.include_anchor = sp_priv_anchor_len(p->buf, p->len, p->flavor) > 0;
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
retry:
    while (it->pos < p->len && sp_priv_is_sep(p->buf[it->pos], p->flavor)) {
        it->pos++;
    }
    if (it->pos >= p->len) return false;
    size_t start = it->pos;
    while (it->pos < p->len && !sp_priv_is_sep(p->buf[it->pos], p->flavor)) {
        it->pos++;
    }
    /* On Windows, skip leading '.' protecting a drive letter */
    if (sp_priv_is_windows_flavor(p->flavor) && start == 0 && it->pos - start == 1 && p->buf[start] == '.') {
        size_t ns = it->pos;
        while (ns < p->len && sp_priv_is_sep(p->buf[ns], p->flavor)) ns++;
        if (ns + 1 < p->len && sp_priv_is_drive_letter(p->buf[ns]) && p->buf[ns + 1] == ':') {
            goto retry;
        }
    }
    out->data = p->buf + start;
    out->len = it->pos - start;
    return true;
}

size_t sp_parts_count(const SpPath *p) {
    SpPartsIter it = sp_parts_begin(p);
    SpStr part;
    size_t c = 0;
    while (sp_parts_next(&it, &part)) c++;
    return c;
}

SpParentsIter sp_parents_begin(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpParentsIter it = SP_PRIV_ZERO;
    it.current = sp_parent(p);
    it.done = sp_path_eq(p, &it.current);
    return it;
}

bool sp_parents_next(SpParentsIter *it, SpPath *out) {
    if (it->done) return false;
    *out = it->current;
    SpPath next = sp_parent(&it->current);
    it->done = sp_path_eq(&next, &it->current);
    if (!it->done) it->current = next;
    return true;
}

/* Internal length-aware join - handles embedded nulls correctly */
static SpPath sp_priv_join_len(const SpPath *base, const char *other, size_t olen) {
    SpFlavor flavor = base->flavor;

    /* Check if other has root */
    if (olen > 0 && sp_priv_is_sep(other[0], flavor)) {
        if (sp_priv_has_drive(other, olen, flavor) || sp_priv_is_unc(other, olen, flavor)) {
            SpStr sv = {other, olen};
            return sp_path_from_sv(sv, flavor);
        }
        /* Root only - keep drive from base */
        size_t dlen = sp_priv_drive_len(base->buf, base->len, flavor);
        if (dlen > 0) {
            SpPath r = SP_PRIV_ZERO;
            r.flavor = flavor;
            memcpy(r.buf, base->buf, dlen);
            r.len = dlen;
            sp_priv_append_cstr(&r, other, olen);
            r.buf[r.len] = '\0';
            sp_priv_normalize(r.buf, &r.len, flavor);
            return r;
        }
        SpStr sv = {other, olen};
        return sp_path_from_sv(sv, flavor);
    }

    /* Check if other has drive */
    if (sp_priv_has_drive(other, olen, flavor)) {
        char od = other[0];
        char bd = base->len >= 2 ? base->buf[0] : '\0';
        bool same = sp_priv_is_drive_letter(od) && sp_priv_is_drive_letter(bd) && ((od | 32) == (bd | 32));
        if (same && olen == 2) {
            SpPath r = sp_path_copy(base);
            r.buf[0] = od;
            return r;
        }
        if (same) {
            if (olen > 2 && sp_priv_is_sep(other[2], flavor)) {
                SpStr sv = {other, olen};
                return sp_path_from_sv(sv, flavor);
            }
            SpPath r = SP_PRIV_ZERO;
            r.flavor = flavor;
            memcpy(r.buf, base->buf, base->len);
            r.len = base->len;
            r.buf[0] = od;
            if (r.len > 0 && !sp_priv_is_sep(r.buf[r.len - 1], flavor)) {
                sp_priv_append_sep(&r);
            }
            sp_priv_append_cstr(&r, other + 2, olen - 2);
            r.buf[r.len] = '\0';
            sp_priv_normalize(r.buf, &r.len, flavor);
            return r;
        }
        SpStr sv = {other, olen};
        return sp_path_from_sv(sv, flavor);
    }

    /* Relative path - simple join */
    SpPath r = sp_path_copy(base);
    size_t anchor = sp_priv_anchor_len(r.buf, r.len, flavor);
    bool drv_only = anchor == r.len && anchor == 2 && sp_priv_has_drive(r.buf, r.len, flavor) &&
                    sp_priv_root_len(r.buf, r.len, flavor) == 0;
    if (!drv_only && r.len > 0 && !sp_priv_is_sep(r.buf[r.len - 1], flavor)) {
        sp_priv_append_sep(&r);
    }
    sp_priv_append_cstr(&r, other, olen);
    r.buf[r.len] = '\0';
    sp_priv_normalize(r.buf, &r.len, flavor);
    return r;
}

SpPath sp_join_one(const SpPath *base, const char *other) {
    SP_ASSERT_PATH_INVARIANT(base);
    if (!other || !*other) return sp_path_copy(base);
    if (base->len == 0) return sp_path_new(other, SP_PRIV_OPTS(base->flavor));
    return sp_priv_join_len(base, other, strlen(other));
}

SpPath sp_join_sv(const SpPath *base, SpStr other) {
    SP_ASSERT_PATH_INVARIANT(base);
    if (other.len == 0 || !other.data) return sp_path_copy(base);
    if (base->len == 0) return sp_path_from_sv(other, base->flavor);
    return sp_priv_join_len(base, other.data, other.len);
}

SpPath sp_join_impl(const SpPath *base, const char **parts) {
    SP_ASSERT_PATH_INVARIANT(base);
    SpPath r = sp_path_copy(base);
    for (size_t i = 0; parts[i]; i++) {
        r = sp_join_one(&r, parts[i]);
    }
    return r;
}

SpPath sp_joinpath(const SpPath *base, const SpPath *other) {
    SP_ASSERT_PATH_INVARIANT(base);
    SP_ASSERT_PATH_INVARIANT(other);
    if (other->len == 0) return sp_path_copy(base);
    if (base->len == 0) return sp_path_copy(other);
    return sp_priv_join_len(base, other->buf, other->len);
}

static bool sp_priv_is_valid_name(const char *s, size_t len, SpFlavor flavor) {
    if (len == 0) return false;
    if (len == 1 && s[0] == '.') return false;
    if (len == 2 && s[0] == '.' && s[1] == '.') return false;
    for (size_t i = 0; i < len; i++) {
        if (sp_priv_is_sep(s[i], flavor)) return false;
    }
    return true;
}

static bool sp_priv_has_usable_name(const SpPath *p) {
    SpStr n = sp_name(p);
    return n.len > 0 && !(n.len == 1 && n.data[0] == '.');
}

SpPath sp_with_name(const SpPath *p, const char *name) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (!sp_priv_has_usable_name(p)) return sp_priv_error_path(SP_ERR_NO_NAME);
    size_t nlen = strlen(name);
    if (!sp_priv_is_valid_name(name, nlen, p->flavor)) return sp_priv_error_path(SP_ERR_INVALID_ARG);

    SpPath parent = sp_parent(p);
    SpPath r = SP_PRIV_ZERO;
    r.flavor = p->flavor;
    memcpy(r.buf, parent.buf, parent.len);
    r.len = parent.len;

    /* On Windows, protect drive-letter-like names with ".\" prefix */
    if (sp_priv_is_windows_flavor(p->flavor) && r.len == 0 && nlen >= 2 && sp_priv_is_drive_letter(name[0]) &&
        name[1] == ':') {
        r.buf[r.len++] = '.';
        r.buf[r.len++] = sp_priv_sep(p->flavor);
    }

    if (r.len > 0 && !sp_priv_is_sep(r.buf[r.len - 1], p->flavor)) {
        sp_priv_append_sep(&r);
    }
    sp_priv_append_cstr(&r, name, nlen);
    r.buf[r.len] = '\0';
    return r;
}

SpPath sp_with_stem(const SpPath *p, const char *stem) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (!sp_priv_has_usable_name(p)) return sp_priv_error_path(SP_ERR_NO_NAME);
    size_t slen = strlen(stem);
    if (!sp_priv_is_valid_name(stem, slen, p->flavor)) return sp_priv_error_path(SP_ERR_INVALID_ARG);

    SpStr suf = sp_suffix(p);
    char name[SP_PATH_MAX];
    if (slen + suf.len >= SP_PATH_MAX) slen = SP_PATH_MAX - suf.len - 1;
    memcpy(name, stem, slen);
    if (suf.len > 0) {
        memcpy(name + slen, suf.data, suf.len);
    }
    name[slen + suf.len] = '\0';
    return sp_with_name(p, name);
}

SpPath sp_with_suffix(const SpPath *p, const char *suffix) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (!sp_priv_has_usable_name(p)) return sp_priv_error_path(SP_ERR_NO_NAME);
    size_t suflen = strlen(suffix);
    if (suflen > 0) {
        if (suffix[0] != '.' || suflen == 1) return sp_priv_error_path(SP_ERR_INVALID_ARG);
        for (size_t i = 0; i < suflen; i++) {
            if (sp_priv_is_sep(suffix[i], p->flavor)) return sp_priv_error_path(SP_ERR_INVALID_ARG);
        }
        if (sp_priv_is_windows_flavor(p->flavor) && suflen >= 2 && suffix[1] == ':') {
            return sp_priv_error_path(SP_ERR_INVALID_ARG);
        }
    }

    SpStr stm = sp_stem(p);
    char name[SP_PATH_MAX];
    size_t stmlen = stm.len;
    if (stmlen + suflen >= SP_PATH_MAX) stmlen = SP_PATH_MAX - suflen - 1;
    if (stmlen > 0) {
        memcpy(name, stm.data, stmlen);
    }
    memcpy(name + stmlen, suffix, suflen);
    name[stmlen + suflen] = '\0';
    return sp_with_name(p, name);
}

bool sp_is_absolute(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpStr drv = sp_drive(p);
    SpStr rt = sp_root(p);
    if (sp_priv_is_windows_flavor(p->flavor)) {
        /* UNC paths are always absolute */
        if (drv.len >= 2 && sp_priv_is_sep(drv.data[0], p->flavor) && sp_priv_is_sep(drv.data[1], p->flavor)) {
            return true;
        }
        return drv.len > 0 && rt.len > 0;
    }
    return rt.len > 0;
}

SpPath sp_cwd(SpFlavor flavor) {
    char buf[SP_PATH_MAX];
    if (sp_priv_getcwd(buf, SP_PATH_MAX)) {
        return sp_path_new(buf, SP_PRIV_OPTS(flavor));
    }
    return sp_path_new("", SP_PRIV_OPTS(flavor));
}

SpPath sp_absolute(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (sp_is_absolute(p)) return sp_path_copy(p);
    SpPath cwd = sp_cwd(p->flavor);
    return sp_joinpath(&cwd, p);
}

bool sp_is_relative_to(const SpPath *p, const SpPath *other) {
    SP_ASSERT_PATH_INVARIANT(p);
    SP_ASSERT_PATH_INVARIANT(other);
    if (other->len == 0) {
        return sp_priv_anchor_len(p->buf, p->len, p->flavor) == 0;
    }
    SpPartsIter it_p = sp_parts_begin(p);
    SpPartsIter it_o = sp_parts_begin(other);
    SpStr part_p, part_o;
    while (sp_parts_next(&it_o, &part_o)) {
        if (!sp_parts_next(&it_p, &part_p)) return false;
        if (!sp_priv_sv_eq_flavor(part_p, part_o, p->flavor)) return false;
    }
    return true;
}

SpPath sp_relative_to(const SpPath *p, const SpPath *other) {
    SP_ASSERT_PATH_INVARIANT(p);
    SP_ASSERT_PATH_INVARIANT(other);
    if (!sp_is_relative_to(p, other)) return sp_priv_error_path(SP_ERR_NOT_RELATIVE);

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
        if (!first) sp_priv_append_sep(&r);
        sp_priv_append_sv(&r, part);
        first = false;
    }
    r.buf[r.len] = '\0';
    return r;
}

/* Helper to check if a part is ".." */
static inline bool sp_priv_is_dotdot(SpStr part) { return part.len == 2 && part.data[0] == '.' && part.data[1] == '.'; }

/* Collect path parts into array, returns count */
static size_t sp_priv_collect_parts(const SpPath *p, SpStr *out, size_t max) {
    SpPartsIter it = sp_parts_begin(p);
    SpStr part;
    size_t n = 0;
    while (sp_parts_next(&it, &part) && n < max) {
        out[n++] = part;
    }
    return n;
}

/* Error sentinel for sp_relative_to_walk_up */
static inline SpPath sp_priv_relative_to_error(SpFlavor flavor) {
    SpPath r = SP_PRIV_ZERO;
    r.flavor = flavor;
    r.buf[0] = SP_ERR_NOT_RELATIVE;
    return r;
}

static inline bool sp_relative_to_is_error(const SpPath *p) { return p->len == 0 && p->buf[0] == SP_ERR_NOT_RELATIVE; }

SpPath sp_relative_to_walk_up(const SpPath *p, const SpPath *other) {
    SP_ASSERT_PATH_INVARIANT(p);
    SP_ASSERT_PATH_INVARIANT(other);

    SpStr p_parts[SP_PATH_MAX / 2];
    SpStr o_parts[SP_PATH_MAX / 2];
    size_t p_count = sp_priv_collect_parts(p, p_parts, SP_PATH_MAX / 2);
    size_t o_count = sp_priv_collect_parts(other, o_parts, SP_PATH_MAX / 2);

    /* Check if other contains '..' - not allowed with walk_up */
    for (size_t i = 0; i < o_count; i++) {
        if (sp_priv_is_dotdot(o_parts[i])) {
            return sp_priv_relative_to_error(p->flavor);
        }
    }

    /* Check anchor compatibility */
    size_t p_anchor = sp_priv_anchor_len(p->buf, p->len, p->flavor);
    size_t o_anchor = sp_priv_anchor_len(other->buf, other->len, other->flavor);
    if (p_anchor > 0 || o_anchor > 0) {
        if (p_anchor > 0 && o_anchor > 0) {
            SpStr pa = SP_PRIV_STR(p->buf, p_anchor);
            SpStr oa = SP_PRIV_STR(other->buf, o_anchor);
            if (!sp_priv_sv_eq_flavor(pa, oa, p->flavor)) {
                return sp_priv_relative_to_error(p->flavor);
            }
        } else if ((o_anchor > 0) != (p_anchor > 0)) {
            return sp_priv_relative_to_error(p->flavor);
        }
    }

    /* Find common prefix length */
    size_t common = 0;
    while (common < p_count && common < o_count && sp_priv_sv_eq_flavor(p_parts[common], o_parts[common], p->flavor)) {
        common++;
    }

    /* Build result */
    SpPath r = SP_PRIV_ZERO;
    r.flavor = p->flavor;
    bool first = true;

    /* Add ".." for each remaining part in other */
    for (size_t i = common; i < o_count; i++) {
        if (i == 0 && o_anchor > 0) continue;
        if (!first) sp_priv_append_sep(&r);
        sp_priv_append_cstr(&r, "..", 2);
        first = false;
    }

    /* Add remaining parts from p */
    for (size_t i = common; i < p_count; i++) {
        if (i == 0 && p_anchor > 0) continue;
        if (!first) sp_priv_append_sep(&r);
        sp_priv_append_sv(&r, p_parts[i]);
        first = false;
    }

    r.buf[r.len] = '\0';
    return r;
}

bool sp_is_relative_to_parts(const SpPath *p, const char **parts) {
    SpPath empty = SP_PRIV_ZERO;
    empty.flavor = p->flavor;
    SpPath other = sp_join_impl(&empty, parts);
    return sp_is_relative_to(p, &other);
}

SpPath sp_relative_to_parts(const SpPath *p, const char **parts, bool walk_up) {
    SpPath empty = SP_PRIV_ZERO;
    empty.flavor = p->flavor;
    SpPath other = sp_join_impl(&empty, parts);
    return walk_up ? sp_relative_to_walk_up(p, &other) : sp_relative_to(p, &other);
}

size_t sp_as_uri(const SpPath *p, char *buf, size_t buf_size) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (buf_size == 0 || !sp_is_absolute(p)) {
        if (buf_size > 0) buf[0] = '\0';
        return 0;
    }

    char posix_buf[SP_PATH_MAX];
    sp_as_posix(p, posix_buf, SP_PATH_MAX);
    const char *path = posix_buf;
    size_t path_len = strlen(path);
    size_t pos = 0;

    /* Determine URI prefix based on path type */
    const char *prefix;
    size_t plen;
    if (path_len >= 2 && path[0] == '/' && path[1] == '/') {
        prefix = "file:";
        plen = 5;
    } else if (path_len > 0 && path[0] == '/') {
        prefix = "file://";
        plen = 7;
    } else {
        prefix = "file:///";
        plen = 8;
    }

    if (pos + plen >= buf_size) return 0;
    memcpy(buf, prefix, plen);
    pos = plen;

    /* URL-encode the path */
    static const char hex[] = "0123456789ABCDEF";
    for (size_t i = 0; i < path_len; i++) {
        unsigned char c = SP_PRIV_CAST(unsigned char, path[i]);
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.' ||
            c == '_' || c == '~' || c == '/') {
            if (pos + 1 >= buf_size) return 0;
            buf[pos++] = SP_PRIV_CAST(char, c);
        } else if (c == ':' && i > 0 && path[i - 1] != '/') {
            if (pos + 1 >= buf_size) return 0;
            buf[pos++] = ':';
        } else {
            if (pos + 3 >= buf_size) return 0;
            buf[pos++] = '%';
            buf[pos++] = hex[c >> 4];
            buf[pos++] = hex[c & 0x0F];
        }
    }
    buf[pos] = '\0';
    return pos;
}

bool sp_path_eq(const SpPath *a, const SpPath *b) {
    SP_ASSERT_PATH_INVARIANT(a);
    SP_ASSERT_PATH_INVARIANT(b);
    if (a->flavor != b->flavor || a->len != b->len) return false;
    if (sp_priv_is_windows_flavor(a->flavor)) {
        return sp_priv_str_eq_ci(a->buf, a->len, b->buf, b->len);
    }
    return memcmp(a->buf, b->buf, a->len) == 0;
}

int sp_path_cmp(const SpPath *a, const SpPath *b) {
    SP_ASSERT_PATH_INVARIANT(a);
    SP_ASSERT_PATH_INVARIANT(b);
    size_t min_len = a->len < b->len ? a->len : b->len;
    if (sp_priv_is_windows_flavor(a->flavor)) {
        for (size_t i = 0; i < min_len; i++) {
            char ca = sp_priv_tolower(a->buf[i]);
            char cb = sp_priv_tolower(b->buf[i]);
            if (ca != cb) return ca < cb ? -1 : 1;
        }
    } else {
        int cmp = memcmp(a->buf, b->buf, min_len);
        if (cmp != 0) return cmp;
    }
    return a->len < b->len ? -1 : (a->len > b->len ? 1 : 0);
}

unsigned long sp_path_hash(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    unsigned long hash = 5381;
    const char *str = sp_str(p);
    size_t len = p->len > 0 ? p->len : 1;
    bool win = sp_priv_is_windows_flavor(p->flavor);
    for (size_t i = 0; i < len; i++) {
        unsigned char c = SP_PRIV_CAST(unsigned char, win ? sp_priv_tolower(str[i]) : str[i]);
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

/* Glob pattern matching (supports * and ?) */
static bool sp_priv_fnmatch(const char *pat, size_t plen, const char *str, size_t slen, bool ci) {
    size_t pi = 0, si = 0, star_pi = SP_PRIV_CAST(size_t, -1), star_si = 0;
    while (si < slen) {
        if (pi < plen && pat[pi] == '*') {
            star_pi = pi++;
            star_si = si;
        } else if (pi < plen && (pat[pi] == '?' ||
                                 (ci ? sp_priv_tolower(pat[pi]) == sp_priv_tolower(str[si]) : pat[pi] == str[si]))) {
            pi++;
            si++;
        } else if (star_pi != SP_PRIV_CAST(size_t, -1)) {
            pi = star_pi + 1;
            si = ++star_si;
        } else
            return false;
    }
    while (pi < plen && pat[pi] == '*') pi++;
    return pi == plen;
}

#define SP_PRIV_IS_DOUBLESTAR(p, l) ((l) == 2 && (p)[0] == '*' && (p)[1] == '*')

int sp_match_ex(const SpPath *p, const char *pattern, int case_sensitive) {
    SP_ASSERT_PATH_INVARIANT(p);
    size_t plen = strlen(pattern);
    if (plen == 0) return SP_MATCH_ERR_EMPTY;
    if ((plen == 1 && pattern[0] == '.') || (plen == 2 && pattern[0] == '.' && pattern[1] == '.'))
        return SP_MATCH_ERR_INVALID;

    bool is_win = sp_priv_is_windows_flavor(p->flavor);
    bool ci = (case_sensitive == -1) ? is_win : (case_sensitive == 0);

    bool pat_has_sep = false;
    for (size_t i = 0; i < plen && !pat_has_sep; i++)
        if (pattern[i] == '/' || (is_win && pattern[i] == '\\')) pat_has_sep = true;

    if (!pat_has_sep) {
        if (SP_PRIV_IS_DOUBLESTAR(pattern, plen)) return SP_MATCH_YES;
        SpStr name = sp_name(p);
        return (name.len > 0 && sp_priv_fnmatch(pattern, plen, name.data, name.len, ci)) ? SP_MATCH_YES : SP_MATCH_NO;
    }

    bool pat_has_drive = false, pat_has_root = false;
    size_t pat_anchor = 0;
    if (is_win && plen >= 2 && (sp_priv_is_drive_letter(pattern[0]) || pattern[0] == '*') && pattern[1] == ':') {
        pat_has_drive = true;
        pat_anchor = 2;
        if (plen > 2 && sp_priv_is_sep(pattern[2], p->flavor)) {
            pat_has_root = true;
            pat_anchor = 3;
        }
    } else if (sp_priv_is_sep(pattern[0], p->flavor)) {
        pat_has_root = true;
        pat_anchor = 1;
    }
    bool pat_anchored = pat_has_drive || pat_has_root;

    if (pat_has_drive) {
        SpStr drv = sp_drive(p);
        if (drv.len < 2) return SP_MATCH_NO;
        char pd = sp_priv_tolower(pattern[0]), pthd = sp_priv_tolower(drv.data[0]);
        if (pattern[0] != '*' && pd != pthd) return SP_MATCH_NO;
    }
    if (pat_has_root) {
        if (sp_root(p).len == 0) return SP_MATCH_NO;
        if (is_win && !pat_has_drive && sp_drive(p).len > 0 && !sp_priv_is_unc(p->buf, p->len, p->flavor))
            return SP_MATCH_NO;
    }

    SpStr path_parts[SP_PATH_MAX / 2];
    size_t path_count = 0;
    bool is_unc = sp_priv_is_unc(p->buf, p->len, p->flavor);
    if (is_unc && pat_has_root && !pat_has_drive) {
        size_t i = 2, srv_start = i;
        while (i < p->len && !sp_priv_is_sep(p->buf[i], p->flavor)) i++;
        if (i > srv_start) {
            path_parts[path_count].data = p->buf + srv_start;
            path_parts[path_count++].len = i - srv_start;
        }
        if (i < p->len) {
            i++;
            size_t shr_start = i;
            while (i < p->len && !sp_priv_is_sep(p->buf[i], p->flavor)) i++;
            if (i > shr_start) {
                path_parts[path_count].data = p->buf + shr_start;
                path_parts[path_count++].len = i - shr_start;
            }
        }
        SpPartsIter it = sp_parts_begin(p);
        SpStr part;
        bool first = true;
        while (sp_parts_next(&it, &part) && path_count < SP_PATH_MAX / 2)
            if (first)
                first = false;
            else
                path_parts[path_count++] = part;
    } else
        path_count = sp_priv_collect_parts(p, path_parts, SP_PATH_MAX / 2);

    const char *pat_parts[SP_PATH_MAX / 2];
    size_t pat_lens[SP_PATH_MAX / 2], pat_count = 0, start = pat_anchor;
    for (size_t i = start; i <= plen && pat_count < SP_PATH_MAX / 2; i++) {
        if (i == plen || pattern[i] == '/' || (is_win && pattern[i] == '\\')) {
            if (i > start) {
                pat_parts[pat_count] = pattern + start;
                pat_lens[pat_count++] = i - start;
            }
            start = i + 1;
        }
    }

    size_t path_start = (pat_anchored && path_count > 0 && !(is_unc && pat_has_root && !pat_has_drive)) ? 1 : 0;
    size_t eff_count = path_count - path_start;
    if (pat_anchored && pat_count != eff_count) return SP_MATCH_NO;
    if (pat_count > eff_count) return SP_MATCH_NO;

    for (size_t i = 0; i < pat_count; i++) {
        size_t pi = pat_count - 1 - i, si = path_start + eff_count - 1 - i;
        if (!SP_PRIV_IS_DOUBLESTAR(pat_parts[pi], pat_lens[pi]) &&
            !sp_priv_fnmatch(pat_parts[pi], pat_lens[pi], path_parts[si].data, path_parts[si].len, ci))
            return SP_MATCH_NO;
    }
    return SP_MATCH_YES;
}

int sp_match(const SpPath *p, const char *pattern) { return sp_match_ex(p, pattern, -1); }

bool sp_is_reserved(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (!sp_priv_is_windows_flavor(p->flavor) || sp_priv_is_unc(p->buf, p->len, p->flavor)) return false;
    SpStr name = sp_name(p);
    if (name.len == 0 || name.len > 12) return false;
    char upper[13];
    size_t len = 0;
    for (size_t i = 0; i < name.len && name.data[i] != '.' && name.data[i] != ':' && len < 12; i++) {
        unsigned char c = SP_PRIV_CAST(unsigned char, name.data[i]);
        if (c == 0xC2 && i + 1 < name.len) {
            unsigned char c2 = SP_PRIV_CAST(unsigned char, name.data[i + 1]);
            if (c2 == 0xB9) {
                upper[len++] = '1';
                i++;
                continue;
            }
            if (c2 == 0xB2) {
                upper[len++] = '2';
                i++;
                continue;
            }
            if (c2 == 0xB3) {
                upper[len++] = '3';
                i++;
                continue;
            }
        }
        if (c != ' ') upper[len++] = SP_PRIV_CAST(char, (c >= 'a' && c <= 'z') ? c - 32 : c);
    }
    upper[len] = '\0';
    static const char *reserved[] = {"CON",  "PRN",  "AUX",  "NUL",  "COM1",   "COM2",    "COM3", "COM4", "COM5",
                                     "COM6", "COM7", "COM8", "COM9", "LPT1",   "LPT2",    "LPT3", "LPT4", "LPT5",
                                     "LPT6", "LPT7", "LPT8", "LPT9", "CONIN$", "CONOUT$", NULL};
    for (const char **r = reserved; *r; r++)
        if (strcmp(upper, *r) == 0) return true;
    return false;
}

bool sp_is_file(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    /* Check for embedded null bytes - such paths can't exist */
    for (size_t i = 0; i < p->len; i++) {
        if (p->buf[i] == '\0') return false;
    }
    const char *path_str = sp_str(p);
#ifdef SP_WINDOWS
    DWORD attrs = GetFileAttributesA(path_str);
    if (attrs == INVALID_FILE_ATTRIBUTES) return false;
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
    struct stat st;
    if (stat(path_str, &st) != 0) return false;
    return S_ISREG(st.st_mode);
#endif
}

bool sp_is_dir(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    /* Check for embedded null bytes - such paths can't exist */
    for (size_t i = 0; i < p->len; i++) {
        if (p->buf[i] == '\0') return false;
    }
    const char *path_str = sp_str(p);
#ifdef SP_WINDOWS
    DWORD attrs = GetFileAttributesA(path_str);
    if (attrs == INVALID_FILE_ATTRIBUTES) return false;
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat st;
    if (stat(path_str, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
#endif
}

bool sp_exists(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    /* Check for embedded null bytes - such paths can't exist */
    for (size_t i = 0; i < p->len; i++) {
        if (p->buf[i] == '\0') return false;
    }
    const char *path_str = sp_str(p);
#ifdef SP_WINDOWS
    return GetFileAttributesA(path_str) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st;
    return stat(path_str, &st) == 0;
#endif
}

SpStatResult sp_stat(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpStatResult result = SP_PRIV_ZERO;
    result.valid = false;

    /* Check for embedded null bytes - such paths can't exist */
    for (size_t i = 0; i < p->len; i++) {
        if (p->buf[i] == '\0') return result;
    }

    const char *path_str = sp_str(p);

#ifdef SP_WINDOWS
    /* Use pure Windows APIs to match Python's os.stat() behavior */
    HANDLE hFile = CreateFileA(path_str, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return result;
    }

    BY_HANDLE_FILE_INFORMATION info;
    if (!GetFileInformationByHandle(hFile, &info)) {
        CloseHandle(hFile);
        return result;
    }

    /* Try GetFileInformationByHandleEx for 64-bit volume serial (Windows 8+, like Python) */
    typedef struct { ULONGLONG VolumeSerialNumber; BYTE FileId[16]; } SP_FILE_ID_INFO;
    SP_FILE_ID_INFO fii;
    /* FileIdInfo = 18; cast for C++ compatibility */
    if (GetFileInformationByHandleEx(hFile, SP_PRIV_CAST(FILE_INFO_BY_HANDLE_CLASS, 18), &fii, sizeof(fii))) {
        result.sp_dev = fii.VolumeSerialNumber;
        /* Use 128-bit file ID, take lower 64 bits like Python */
        memcpy(&result.sp_ino, fii.FileId, sizeof(result.sp_ino));
    } else {
        /* Fallback to 32-bit values */
        result.sp_dev = SP_PRIV_CAST(unsigned long long, info.dwVolumeSerialNumber);
        result.sp_ino = (SP_PRIV_CAST(unsigned long long, info.nFileIndexHigh) << 32) |
                        SP_PRIV_CAST(unsigned long long, info.nFileIndexLow);
    }
    CloseHandle(hFile);

    result.sp_nlink = SP_PRIV_CAST(unsigned long long, info.nNumberOfLinks);

    /* Mode: derive from attributes */
    result.sp_mode = (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 040777 : 0100666;
    if (info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
        result.sp_mode &= ~0222;
    }

    /* Size */
    result.sp_size = (SP_PRIV_CAST(long long, info.nFileSizeHigh) << 32) |
                     SP_PRIV_CAST(long long, info.nFileSizeLow);

    /* Times: convert FILETIME to Unix timestamp */
    #define SP_FILETIME_TO_UNIX(ft) \
        (((SP_PRIV_CAST(double, (SP_PRIV_CAST(unsigned long long, (ft).dwHighDateTime) << 32) | (ft).dwLowDateTime)) - 116444736000000000.0) / 10000000.0)
    result.sp_atime = SP_FILETIME_TO_UNIX(info.ftLastAccessTime);
    result.sp_mtime = SP_FILETIME_TO_UNIX(info.ftLastWriteTime);
    result.sp_ctime = SP_FILETIME_TO_UNIX(info.ftCreationTime);
    #undef SP_FILETIME_TO_UNIX

    result.sp_uid = 0;
    result.sp_gid = 0;
#else
    struct stat st;
    if (stat(path_str, &st) != 0) {
        return result;
    }

    result.sp_mode = SP_PRIV_CAST(unsigned int, st.st_mode);
    result.sp_ino = SP_PRIV_CAST(unsigned long long, st.st_ino);
    result.sp_dev = SP_PRIV_CAST(unsigned long long, st.st_dev);
    result.sp_nlink = SP_PRIV_CAST(unsigned long long, st.st_nlink);
    result.sp_uid = SP_PRIV_CAST(unsigned int, st.st_uid);
    result.sp_gid = SP_PRIV_CAST(unsigned int, st.st_gid);
    result.sp_size = SP_PRIV_CAST(long long, st.st_size);

    /* Use standard time_t fields for C99 compatibility (second resolution) */
    result.sp_atime = SP_PRIV_CAST(double, st.st_atime);
    result.sp_mtime = SP_PRIV_CAST(double, st.st_mtime);
    result.sp_ctime = SP_PRIV_CAST(double, st.st_ctime);
#endif

    result.valid = true;
    return result;
}

bool sp_stat_eq(const SpStatResult *a, const SpStatResult *b) {
    if (!a->valid || !b->valid) return false;
    return a->sp_mode == b->sp_mode &&
           a->sp_ino == b->sp_ino &&
           a->sp_dev == b->sp_dev &&
           a->sp_nlink == b->sp_nlink &&
           a->sp_uid == b->sp_uid &&
           a->sp_gid == b->sp_gid &&
           a->sp_size == b->sp_size;
}

size_t sp_parents_count(const SpPath *p) {
    SpParentsIter it = sp_parents_begin(p);
    SpPath parent;
    size_t count = 0;
    while (sp_parents_next(&it, &parent)) count++;
    return count;
}

int sp_mkdir(const SpPath *p, unsigned int mode, bool parents, bool exist_ok) {
    SP_ASSERT_PATH_INVARIANT(p);

    /* mode=0 means use default; umask applied by OS */
    if (mode == 0) mode = SP_MKDIR_DEF_MODE;

    /* Check for embedded null bytes - such paths can't be created */
    for (size_t i = 0; i < p->len; i++) {
        if (p->buf[i] == '\0') return SP_MKDIR_ERR_OTHER;
    }

    const char *path_str = sp_str(p);

#ifdef SP_WINDOWS
    (void)mode; /* Windows ignores POSIX permissions; pass 0 for default */

    if (parents) {
        /* Create parent directories first */
        SpPath parent_path = sp_parent(p);
        if (!sp_path_eq(&parent_path, p) && parent_path.len > 0) {
            /* Check if parent exists */
            DWORD parent_attrs = GetFileAttributesA(sp_str(&parent_path));
            if (parent_attrs == INVALID_FILE_ATTRIBUTES) {
                /* Parent doesn't exist, try to create it recursively.
                   Mode doesn't matter on Windows, but use default for consistency. */
                int parent_result = sp_mkdir(&parent_path, SP_MKDIR_DEF_MODE, true, true);
                if (parent_result != SP_MKDIR_OK) {
                    return parent_result;
                }
            } else if (!(parent_attrs & FILE_ATTRIBUTE_DIRECTORY)) {
                /* Parent exists but is not a directory */
                return SP_MKDIR_ERR_NOT_DIR;
            }
        }
    }

    /* Check if the directory already exists */
    DWORD attrs = GetFileAttributesA(path_str);
    if (attrs != INVALID_FILE_ATTRIBUTES) {
        if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
            return exist_ok ? SP_MKDIR_OK : SP_MKDIR_ERR_EXISTS;
        } else {
            /* Path exists but is not a directory (e.g., a file) */
            return SP_MKDIR_ERR_EXISTS_NOT_DIR;
        }
    }

    /* Try to create the directory */
    if (CreateDirectoryA(path_str, NULL)) {
        return SP_MKDIR_OK;
    }

    /* Handle errors */
    DWORD err = GetLastError();
    if (err == ERROR_ALREADY_EXISTS) {
        return exist_ok ? SP_MKDIR_OK : SP_MKDIR_ERR_EXISTS;
    } else if (err == ERROR_PATH_NOT_FOUND) {
        return SP_MKDIR_ERR_NOT_FOUND;
    } else if (err == ERROR_ACCESS_DENIED) {
        return SP_MKDIR_ERR_PERMISSION;
    }
    return SP_MKDIR_ERR_OTHER;
#else
    if (parents) {
        /* Create parent directories first */
        SpPath parent_path = sp_parent(p);
        if (!sp_path_eq(&parent_path, p) && parent_path.len > 0) {
            struct stat st;
            if (stat(sp_str(&parent_path), &st) != 0) {
                /* Parent doesn't exist, try to create it recursively.
                   Use default mode for parents so we can create children. */
                int parent_result = sp_mkdir(&parent_path, SP_MKDIR_DEF_MODE, true, true);
                if (parent_result != SP_MKDIR_OK) {
                    return parent_result;
                }
            } else if (!S_ISDIR(st.st_mode)) {
                /* Parent exists but is not a directory */
                return SP_MKDIR_ERR_NOT_DIR;
            }
        }
    }

    /* Check if the directory already exists */
    struct stat st;
    if (stat(path_str, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return exist_ok ? SP_MKDIR_OK : SP_MKDIR_ERR_EXISTS;
        } else {
            /* Path exists but is not a directory (e.g., a file) */
            return SP_MKDIR_ERR_EXISTS_NOT_DIR;
        }
    }

    /* Try to create the directory */
    if (mkdir(path_str, SP_PRIV_CAST(mode_t, mode)) == 0) {
        return SP_MKDIR_OK;
    }

    /* Handle errors */
    if (errno == EEXIST) {
        /* Check if it's a directory that was just created (race condition) */
        if (stat(path_str, &st) == 0 && S_ISDIR(st.st_mode)) {
            return exist_ok ? SP_MKDIR_OK : SP_MKDIR_ERR_EXISTS;
        }
        return SP_MKDIR_ERR_NOT_DIR;
    } else if (errno == ENOENT) {
        return SP_MKDIR_ERR_NOT_FOUND;
    } else if (errno == EACCES || errno == EPERM) {
        return SP_MKDIR_ERR_PERMISSION;
    } else if (errno == ENOTDIR) {
        return SP_MKDIR_ERR_NOT_DIR;
    }
    return SP_MKDIR_ERR_OTHER;
#endif
}

/* ============ Glob Implementation ============ */

/* Include dirent for POSIX or use Windows APIs */
#ifdef SP_WINDOWS
/* Already included windows.h above */
#else
#include <dirent.h>
#endif

/* Pattern segment types */
#define SP_GLOB_SEG_LITERAL 0
#define SP_GLOB_SEG_PATTERN 1
#define SP_GLOB_SEG_DOUBLESTAR 2

static bool sp_priv_glob_has_wildcard(const char *s) {
    for (; *s; s++) if (*s == '*' || *s == '?') return true;
    return false;
}

static bool sp_priv_glob_is_doublestar(const char *s) {
    return s[0] == '*' && s[1] == '*' && s[2] == '\0';
}

static size_t sp_priv_glob_parse_pattern(const char *pattern, SpFlavor flavor,
                                          char *buf, size_t buf_size,
                                          size_t *seg_offsets, int *seg_types, size_t max_segs,
                                          bool *dir_only) {
    size_t count = 0;
    size_t len = strlen(pattern);
    if (len >= buf_size) len = buf_size - 1;
    memcpy(buf, pattern, len);
    buf[len] = '\0';
    *dir_only = (len > 0 && sp_priv_is_sep(pattern[len - 1], flavor));

    char *p = buf, *start = p;
    while (*p && count < max_segs) {
        if (sp_priv_is_sep(*p, flavor)) {
            *p = '\0';
            if (p > start) {
                seg_offsets[count] = SP_PRIV_CAST(size_t, start - buf);
                seg_types[count] = sp_priv_glob_is_doublestar(start) ? SP_GLOB_SEG_DOUBLESTAR
                                 : sp_priv_glob_has_wildcard(start) ? SP_GLOB_SEG_PATTERN
                                 : SP_GLOB_SEG_LITERAL;
                count++;
            }
            start = p + 1;
        }
        p++;
    }
    if (*start && count < max_segs) {
        seg_offsets[count] = SP_PRIV_CAST(size_t, start - buf);
        seg_types[count] = sp_priv_glob_is_doublestar(start) ? SP_GLOB_SEG_DOUBLESTAR
                         : sp_priv_glob_has_wildcard(start) ? SP_GLOB_SEG_PATTERN
                         : SP_GLOB_SEG_LITERAL;
        count++;
    }
    return count;
}

static bool sp_priv_glob_match_seg(const char *pat, const char *name, bool ci) {
    return sp_priv_fnmatch(pat, strlen(pat), name, strlen(name), ci);
}

static void sp_priv_glob_close_handle(void *h) {
    if (!h) return;
#ifdef SP_WINDOWS
    FindClose(SP_PRIV_CAST(HANDLE, h));
#else
    closedir(SP_PRIV_CAST(DIR *, h));
#endif
}

static void *sp_priv_glob_open_dir(const SpPath *dir) {
#ifdef SP_WINDOWS
    char search[SP_PATH_MAX + 3];
    size_t len = dir->len;
    if (len == 0) { search[0] = '.'; search[1] = '\\'; search[2] = '*'; search[3] = '\0'; }
    else {
        memcpy(search, dir->buf, len);
        if (!sp_priv_is_sep(search[len-1], dir->flavor)) search[len++] = '\\';
        search[len++] = '*'; search[len] = '\0';
    }
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(search, &fd);
    return (h == INVALID_HANDLE_VALUE) ? NULL : h;
#else
    const char *path = (dir->len == 0) ? "." : dir->buf;
    return opendir(path);
#endif
}

SpGlobIter sp_glob_begin(const SpPath *base, const char *pattern, SpCaseSensitivity cs) {
    SpGlobIter it;
    memset(&it, 0, sizeof(it));
    it.depth = -1;

    if (!base || !pattern || !*pattern) return it;
    SP_ASSERT_PATH_INVARIANT(base);

    it.priv_.flavor = base->flavor;
    switch (cs) {
        case SP_CASE_PLATFORM_DEFAULT: it.priv_.case_insensitive = sp_priv_is_windows_flavor(base->flavor); break;
        case SP_CASE_SENSITIVE:        it.priv_.case_insensitive = false; break;
        case SP_CASE_INSENSITIVE:      it.priv_.case_insensitive = true; break;
        default:                       it.priv_.case_insensitive = sp_priv_is_windows_flavor(base->flavor); break;
    }

    it.priv_.seg_count = sp_priv_glob_parse_pattern(pattern, base->flavor,
        it.priv_.pattern_buf, SP_GLOB_PATTERN_MAX,
        it.priv_.seg_offsets, it.priv_.seg_types, SP_GLOB_MAX_SEGMENTS, &it.priv_.dir_only);
    if (it.priv_.seg_count == 0) return it;

    void *h = sp_priv_glob_open_dir(base);
    if (!h) return it;

    it.depth = 0;
    it.priv_.dirs[0] = *base;
    it.priv_.seg_idxs[0] = 0;
    it.priv_.handles[0] = h;

    /* When pattern is just ** (single segment), yield base directory first */
    if (it.priv_.seg_types[0] == SP_GLOB_SEG_DOUBLESTAR && it.priv_.seg_count == 1) {
        it.priv_.yield_base_pending = true;
    }

    return it;
}

bool sp_glob_next(SpGlobIter *it, SpPath *out) {
    if (!it || it->depth < 0) return false;

    /* When pattern starts with **, yield base directory first */
    if (it->priv_.yield_base_pending) {
        it->priv_.yield_base_pending = false;
        /* Only yield base if it matches dir_only constraint */
        if (!it->priv_.dir_only || sp_is_dir(&it->priv_.dirs[0])) {
            *out = it->priv_.dirs[0];
            return true;
        }
    }

    while (it->depth >= 0) {
        int d = it->depth;
        void *h = it->priv_.handles[d];
        SpPath *dir = &it->priv_.dirs[d];
        size_t seg_idx = it->priv_.seg_idxs[d];

        if (seg_idx >= it->priv_.seg_count) { sp_priv_glob_close_handle(h); it->priv_.handles[d] = NULL; it->depth--; continue; }

        const char *pat = it->priv_.pattern_buf + it->priv_.seg_offsets[seg_idx];
        int stype = it->priv_.seg_types[seg_idx];
        bool is_last = (seg_idx == it->priv_.seg_count - 1);
        bool ci = it->priv_.case_insensitive;

        /* Special handling for literal . and .. segments - don't use readdir, just synthesize */
        /* This avoids opendir() issues on paths with .. (fails on some systems) */
        if (stype == SP_GLOB_SEG_LITERAL &&
            ((pat[0] == '.' && pat[1] == '\0') || (pat[0] == '.' && pat[1] == '.' && pat[2] == '\0'))) {
            SpPath full = sp_join_one(dir, pat);
            bool isdir = sp_is_dir(&full);
            if (isdir) {
                if (is_last) {
                    if (!it->priv_.dir_only || isdir) {
                        it->priv_.seg_idxs[d] = seg_idx + 1;  /* Advance so we don't yield again */
                        *out = full;
                        return true;
                    }
                } else {
                    /* Check if next segment is also a literal . or .. */
                    const char *npat = it->priv_.pattern_buf + it->priv_.seg_offsets[seg_idx + 1];
                    int ntype = it->priv_.seg_types[seg_idx + 1];
                    bool next_is_dot = (ntype == SP_GLOB_SEG_LITERAL) &&
                        ((npat[0] == '.' && npat[1] == '\0') || (npat[0] == '.' && npat[1] == '.' && npat[2] == '\0'));

                    if (next_is_dot) {
                        /* Both current and next are dots - just advance without opening */
                        it->priv_.seg_idxs[d] = seg_idx + 1;
                        it->priv_.dirs[d] = full;
                        continue;
                    }

                    /* Next segment needs directory iteration - reopen at new path */
                    sp_priv_glob_close_handle(h);
                    it->priv_.handles[d] = NULL;
                    void *nh = sp_priv_glob_open_dir(&full);
                    if (nh) {
                        it->priv_.handles[d] = nh;
                        it->priv_.seg_idxs[d] = seg_idx + 1;
                        it->priv_.dirs[d] = full;
                        continue;
                    }
                    /* Reopen failed - pop to previous depth */
                    it->depth--;
                    continue;
                }
            }
            /* Path doesn't exist or isn't a directory - pop */
            sp_priv_glob_close_handle(h); it->priv_.handles[d] = NULL; it->depth--;
            continue;
        }

        const char *name = NULL;
#ifdef SP_WINDOWS
        WIN32_FIND_DATAA fd;
        if (!FindNextFileA(SP_PRIV_CAST(HANDLE, h), &fd)) { sp_priv_glob_close_handle(h); it->priv_.handles[d] = NULL; it->depth--; continue; }
        name = fd.cFileName;
#else
        struct dirent *de = readdir(SP_PRIV_CAST(DIR *, h));
        if (!de) { sp_priv_glob_close_handle(h); it->priv_.handles[d] = NULL; it->depth--; continue; }
        name = de->d_name;
#endif

        /* Skip . and .. (unless pattern is literally ..) */
        if (name[0] == '.' && name[1] == '\0') continue;
        if (name[0] == '.' && name[1] == '.' && name[2] == '\0') {
            if (!(pat[0] == '.' && pat[1] == '.' && pat[2] == '\0')) continue;
        }
        /* Skip hidden unless pattern starts with . or is ** */
        if (name[0] == '.' && pat[0] != '.' && stype != SP_GLOB_SEG_DOUBLESTAR) continue;

        SpPath full = sp_join_one(dir, name);
        bool isdir = sp_is_dir(&full);

        if (stype == SP_GLOB_SEG_DOUBLESTAR) {
            /* ** - try to match next segment if exists */
            if (!is_last) {
                const char *npat = it->priv_.pattern_buf + it->priv_.seg_offsets[seg_idx + 1];
                int ntype = it->priv_.seg_types[seg_idx + 1];
                bool nlast = (seg_idx + 1 == it->priv_.seg_count - 1);
                bool nmatch = (ntype == SP_GLOB_SEG_LITERAL)
                    ? (ci ? sp_priv_str_eq_ci(name, strlen(name), npat, strlen(npat)) : strcmp(name, npat) == 0)
                    : (ntype == SP_GLOB_SEG_PATTERN) ? sp_priv_glob_match_seg(npat, name, ci) : false;

                if (nmatch) {
                    /* When matching segment after **, also recurse into matched dir to find more */
                    if (nlast && isdir && it->depth + 1 < SP_GLOB_MAX_DEPTH) {
                        void *nh = sp_priv_glob_open_dir(&full);
                        if (nh) { it->depth++; it->priv_.dirs[it->depth] = full; it->priv_.seg_idxs[it->depth] = seg_idx; it->priv_.handles[it->depth] = nh; }
                    }
                    if (nlast && (!it->priv_.dir_only || isdir)) { *out = full; return true; }
                    if (!nlast && isdir && it->depth + 1 < SP_GLOB_MAX_DEPTH) {
                        void *nh = sp_priv_glob_open_dir(&full);
                        if (nh) {
                            size_t next_seg = seg_idx + 2;
                            bool next_is_last_doublestar = (it->priv_.seg_types[next_seg] == SP_GLOB_SEG_DOUBLESTAR) &&
                                                          (next_seg == it->priv_.seg_count - 1);
                            it->depth++;
                            it->priv_.dirs[it->depth] = full;
                            it->priv_.seg_idxs[it->depth] = next_seg;
                            it->priv_.handles[it->depth] = nh;
                            /* If pushing to a trailing **, yield this dir (** matches zero subdirs) */
                            if (next_is_last_doublestar) { *out = full; return true; }
                        }
                    }
                }
            }
            /* ** at end yields only directories (Python behavior: ** matches directory paths) */
            /* Set up recursion BEFORE yielding so we continue exploring subdirs */
            if (isdir && it->depth + 1 < SP_GLOB_MAX_DEPTH) {
                void *nh = sp_priv_glob_open_dir(&full);
                if (nh) { it->depth++; it->priv_.dirs[it->depth] = full; it->priv_.seg_idxs[it->depth] = seg_idx; it->priv_.handles[it->depth] = nh; }
            }
            if (is_last && isdir) { *out = full; return true; }
        } else {
            bool match = (stype == SP_GLOB_SEG_LITERAL)
                ? (ci ? sp_priv_str_eq_ci(name, strlen(name), pat, strlen(pat)) : strcmp(name, pat) == 0)
                : sp_priv_glob_match_seg(pat, name, ci);
            if (match) {
                if (is_last && (!it->priv_.dir_only || isdir)) { *out = full; return true; }
                if (!is_last && isdir && it->depth + 1 < SP_GLOB_MAX_DEPTH) {
                    void *nh = sp_priv_glob_open_dir(&full);
                    if (nh) { it->depth++; it->priv_.dirs[it->depth] = full; it->priv_.seg_idxs[it->depth] = seg_idx + 1; it->priv_.handles[it->depth] = nh; }
                }
            }
        }
    }
    return false;
}

void sp_glob_end(SpGlobIter *it) {
    if (!it) return;
    for (int i = 0; i <= it->depth && i < SP_GLOB_MAX_DEPTH; i++) {
        sp_priv_glob_close_handle(it->priv_.handles[i]);
        it->priv_.handles[i] = NULL;
    }
    it->depth = -1;
}

SpGlobIter sp_rglob_begin(const SpPath *base, const char *pattern, SpCaseSensitivity cs) {
    if (!base || !pattern) { SpGlobIter it; memset(&it, 0, sizeof(it)); it.depth = -1; return it; }

    /* If pattern already starts with **, just use it */
    if (pattern[0] == '*' && pattern[1] == '*' &&
        (pattern[2] == '\0' || sp_priv_is_sep(pattern[2], base->flavor))) {
        return sp_glob_begin(base, pattern, cs);
    }

    /* Prepend "**\/" to pattern */
    char rglob[SP_GLOB_PATTERN_MAX];
    size_t plen = strlen(pattern);
    if (plen + 4 >= SP_GLOB_PATTERN_MAX) plen = SP_GLOB_PATTERN_MAX - 4;
    rglob[0] = '*'; rglob[1] = '*'; rglob[2] = sp_priv_sep(base->flavor);
    memcpy(rglob + 3, pattern, plen);
    rglob[3 + plen] = '\0';
    return sp_glob_begin(base, rglob, cs);
}

/* ============ Fluent API Implementation ============ */
#ifdef SNAKEPATH_FLUENT

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
#define SP_TLS _Thread_local
#elif defined(__GNUC__) || defined(__clang__)
#define SP_TLS __thread
#elif defined(_MSC_VER)
#define SP_TLS __declspec(thread)
#else
#define SP_TLS /* fallback: not thread-safe */
#endif

static SP_TLS SpPath sp_priv_f_ctx;
static SP_TLS bool sp_priv_f_ctx_active = false;

/* X-macro lists for fluent API generation */
#define SP_FLUENT_TERM_VOID(X)                                                                                         \
    X(path, SpPath, sp_priv_f_ctx)                                                                                     \
    X(suffixes, SpSuffixes, sp_suffixes(&sp_priv_f_ctx))                                                               \
    X(str, const char *, sp_str(&sp_priv_f_ctx))                                                                       \
    X(is_absolute, bool, sp_is_absolute(&sp_priv_f_ctx))                                                               \
    X(is_file, bool, sp_is_file(&sp_priv_f_ctx))                                                                       \
    X(is_dir, bool, sp_is_dir(&sp_priv_f_ctx))                                                                         \
    X(exists, bool, sp_exists(&sp_priv_f_ctx))
#define SP_FLUENT_TERM_STR(X)                                                                                          \
    X(name, SpStr, sp_name)                                                                                            \
    X(stem, SpStr, sp_stem) X(suffix, SpStr, sp_suffix) X(drive, SpStr, sp_drive) X(root, SpStr, sp_root)              \
        X(anchor, SpStr, sp_anchor)
#define SP_FLUENT_CHAIN_VOID(X) X(parent, sp_parent) X(absolute, sp_absolute)
#define SP_FLUENT_CHAIN_STR(X)                                                                                         \
    X(join, sp_join_one) X(with_name, sp_with_name) X(with_stem, sp_with_stem) X(with_suffix, sp_with_suffix)
#define SP_FLUENT_CHAIN_PATH(X)                                                                                        \
    X(relative_to, sp_relative_to)                                                                                     \
    X(relative_to_walk_up, sp_relative_to_walk_up)

/* Generate terminator functions */
#define SP_GEN_TERM_VOID(n, ret, expr)                                                                                 \
    static ret sp_priv_f_##n##_(void) {                                                                                \
        sp_priv_f_ctx_active = false;                                                                                  \
        return expr;                                                                                                   \
    }
#define SP_GEN_TERM_STR(n, ret, fn)                                                                                    \
    static ret sp_priv_f_##n##_(void) {                                                                                \
        sp_priv_f_ctx_active = false;                                                                                  \
        return fn(&sp_priv_f_ctx);                                                                                     \
    }
SP_FLUENT_TERM_VOID(SP_GEN_TERM_VOID)
SP_FLUENT_TERM_STR(SP_GEN_TERM_STR)
static bool sp_priv_f_is_relative_to_(const SpPath *o) {
    sp_priv_f_ctx_active = false;
    return sp_is_relative_to(&sp_priv_f_ctx, o);
}

/* Chainable: declare, then instance, then define (instance must exist for return) */
#define SP_DECL_CHAIN_VOID(n, fn) static SpPrivDontUseThisDirectly_ *sp_priv_f_##n##_(void);
#define SP_DECL_CHAIN_STR(n, fn) static SpPrivDontUseThisDirectly_ *sp_priv_f_##n##_(const char *);
#define SP_DECL_CHAIN_PATH(n, fn) static SpPrivDontUseThisDirectly_ *sp_priv_f_##n##_(const SpPath *);
SP_FLUENT_CHAIN_VOID(SP_DECL_CHAIN_VOID)
SP_FLUENT_CHAIN_STR(SP_DECL_CHAIN_STR)
SP_FLUENT_CHAIN_PATH(SP_DECL_CHAIN_PATH)

static SpPrivDontUseThisDirectly_ sp_priv_f_instance = {sp_priv_f_path_,
                                                        sp_priv_f_name_,
                                                        sp_priv_f_stem_,
                                                        sp_priv_f_suffix_,
                                                        sp_priv_f_suffixes_,
                                                        sp_priv_f_drive_,
                                                        sp_priv_f_root_,
                                                        sp_priv_f_anchor_,
                                                        sp_priv_f_str_,
                                                        sp_priv_f_is_absolute_,
                                                        sp_priv_f_is_relative_to_,
                                                        sp_priv_f_is_file_,
                                                        sp_priv_f_is_dir_,
                                                        sp_priv_f_exists_,
                                                        sp_priv_f_parent_,
                                                        sp_priv_f_join_,
                                                        sp_priv_f_with_name_,
                                                        sp_priv_f_with_stem_,
                                                        sp_priv_f_with_suffix_,
                                                        sp_priv_f_absolute_,
                                                        sp_priv_f_relative_to_,
                                                        sp_priv_f_relative_to_walk_up_};

#define SP_DEF_CHAIN_VOID(n, fn)                                                                                       \
    static SpPrivDontUseThisDirectly_ *sp_priv_f_##n##_(void) {                                                        \
        sp_priv_f_ctx = fn(&sp_priv_f_ctx);                                                                            \
        return &sp_priv_f_instance;                                                                                    \
    }
#define SP_DEF_CHAIN_STR(n, fn)                                                                                        \
    static SpPrivDontUseThisDirectly_ *sp_priv_f_##n##_(const char *s) {                                               \
        sp_priv_f_ctx = fn(&sp_priv_f_ctx, s);                                                                         \
        return &sp_priv_f_instance;                                                                                    \
    }
#define SP_DEF_CHAIN_PATH(n, fn)                                                                                       \
    static SpPrivDontUseThisDirectly_ *sp_priv_f_##n##_(const SpPath *o) {                                             \
        sp_priv_f_ctx = fn(&sp_priv_f_ctx, o);                                                                         \
        return &sp_priv_f_instance;                                                                                    \
    }
SP_FLUENT_CHAIN_VOID(SP_DEF_CHAIN_VOID)
SP_FLUENT_CHAIN_STR(SP_DEF_CHAIN_STR)
SP_FLUENT_CHAIN_PATH(SP_DEF_CHAIN_PATH)

SpPrivDontUseThisDirectly_ *sp_fluent_init_(SpPath p) {
    sp_priv_f_ctx_active = true;
    sp_priv_f_ctx = p;
    return &sp_priv_f_instance;
}

#endif /* SNAKEPATH_FLUENT */

#ifdef __cplusplus
}
#endif

#endif /* SNAKEPATH_IMPLEMENTATION */
