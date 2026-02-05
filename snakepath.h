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
#define SP_ERR_OTHER '\x04'        /* Other error (I/O, permission, etc.) */

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

/* Terminated string - owning, null-terminated copy for component strings */
#ifndef SP_TERM_MAX
#define SP_TERM_MAX 256
#endif
typedef struct {
    char buf[SP_TERM_MAX];
    size_t len;
} SpTerm;

/* Helper to create SpTerm from data pointer and length */
static inline SpTerm sp_priv_term(const char *data, size_t len) {
    SpTerm t = SP_PRIV_ZERO;
    if (data && len > 0) {
        size_t n = len < SP_TERM_MAX - 1 ? len : SP_TERM_MAX - 1;
        memcpy(t.buf, data, n);
        t.buf[n] = '\0';
        t.len = n;
    }
    return t;
}

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
SpPath sp_path_from_n(const char *s, size_t len, SpFlavor flavor);
SpPath sp_path_convert(const char *s, SpFlavor src_flavor, SpFlavor dest_flavor);
SpPath sp_path_copy(const SpPath *p);

const char *sp_str(const SpPath *p);
void sp_as_posix(const SpPath *p, char *out, size_t out_size);

SpTerm sp_drive(const SpPath *p);
SpTerm sp_root(const SpPath *p);
SpTerm sp_anchor(const SpPath *p);
SpTerm sp_name(const SpPath *p);
SpTerm sp_stem(const SpPath *p);
SpTerm sp_suffix(const SpPath *p);
SpSuffixes sp_suffixes(const SpPath *p);
SpPath sp_parent(const SpPath *p);

SpPartsIter sp_parts_begin(const SpPath *p);
bool sp_parts_next(SpPartsIter *it, SpStr *out);
size_t sp_parts_count(const SpPath *p);

SpParentsIter sp_parents_begin(const SpPath *p);
bool sp_parents_next(SpParentsIter *it, SpPath *out);

SpPath sp_join_one(const SpPath *base, const char *other);
SpPath sp_join_n(const SpPath *base, const char *s, size_t len);
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
bool sp_is_symlink(const SpPath *p);
bool sp_is_block_device(const SpPath *p);
bool sp_is_char_device(const SpPath *p);
bool sp_is_fifo(const SpPath *p);
bool sp_is_socket(const SpPath *p);
bool sp_is_mount(const SpPath *p);
bool sp_is_junction(const SpPath *p);

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
    long long sp_atime_ns;     /* Time of last access (nanoseconds since epoch) */
    long long sp_mtime_ns;     /* Time of last modification (nanoseconds since epoch) */
    long long sp_ctime_ns;     /* Time of last status change (nanoseconds since epoch) */
    bool valid;                /* True if stat succeeded */
} SpStatResult;

SpStatResult sp_stat(const SpPath *p);   /* follows symlinks */
SpStatResult sp_lstat(const SpPath *p);  /* does not follow symlinks */
bool sp_stat_eq(const SpStatResult *a, const SpStatResult *b);
size_t sp_parents_count(const SpPath *p);

/* Symlink & link operations */
SpPath sp_readlink(const SpPath *p);  /* read symlink target; returns error path if not a symlink */
SpPath sp_resolve(const SpPath *p, bool strict);  /* resolve to canonical absolute path */
bool sp_symlink_to(const SpPath *p, const SpPath *target, bool target_is_directory);  /* create symlink */
bool sp_hardlink_to(const SpPath *p, const SpPath *target);  /* create hard link */
bool sp_samefile(const SpPath *a, const SpPath *b);  /* check if paths refer to same file */

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

/* File/directory modification operations */
bool sp_touch(const SpPath *p, unsigned int mode, bool exist_ok);  /* create file or update timestamps */
bool sp_unlink(const SpPath *p, bool missing_ok);  /* delete file */
bool sp_rmdir(const SpPath *p);  /* delete empty directory */
SpPath sp_rename(const SpPath *p, const SpPath *target);  /* rename file/directory, returns new path */
SpPath sp_replace(const SpPath *p, const SpPath *target);  /* replace target with this file, returns new path */
bool sp_chmod(const SpPath *p, unsigned int mode);  /* change file permissions */

/* Home directory and user expansion */
SpPath sp_home(SpFlavor flavor);  /* get user's home directory */
SpPath sp_expanduser(const SpPath *p);  /* expand ~ to home directory */

/* User/group info */
SpTerm sp_owner(const SpPath *p);  /* get file owner name */
SpTerm sp_group(const SpPath *p);  /* get file group name */

/* Directory iteration */
typedef struct {
    SpPath dir;           /* Directory being iterated */
    int done;             /* -1 = error, 0 = in progress, 1 = done */
    struct {
#ifdef SP_WINDOWS
        void *handle;     /* HANDLE from FindFirstFileA */
        char find_data[320]; /* WIN32_FIND_DATAA */
        bool first;
#else
        void *handle;     /* DIR* from opendir */
#endif
    } priv_;
} SpIterdirIter;

SpIterdirIter sp_iterdir_begin(const SpPath *p);
bool sp_iterdir_next(SpIterdirIter *it, SpPath *out);  /* returns child path */
void sp_iterdir_end(SpIterdirIter *it);

/* Iterdir foreach macro */
#define SP_ITERDIR_FOREACH(dir, entry_var) \
    for (struct { SpIterdirIter it; int done; } sp_ictx_ = { sp_iterdir_begin(dir), 0 }; \
         !sp_ictx_.done; sp_iterdir_end(&sp_ictx_.it), sp_ictx_.done = 1) \
    for (SpPath entry_var; sp_iterdir_next(&sp_ictx_.it, &entry_var); )

/* Walk limits */
#ifndef SP_WALK_MAX_ENTRIES
#define SP_WALK_MAX_ENTRIES 64
#endif
#ifndef SP_WALK_NAME_MAX
#define SP_WALK_NAME_MAX 128   /* Max filename length */
#endif

/* Walk buffer sizes - when full, directories and their subtrees are skipped */
#define SP_WALK_ENTRIES_CAP(n) ((n) * SP_PATH_MAX)  /* Buffer size for n pending directories */
#define SP_WALK_ENTRIES_CAP_64   SP_WALK_ENTRIES_CAP(64)    /* ~64KB with 1024 path max */
#define SP_WALK_ENTRIES_CAP_256  SP_WALK_ENTRIES_CAP(256)   /* ~256KB */
#define SP_WALK_ENTRIES_CAP_1024 SP_WALK_ENTRIES_CAP(1024)  /* ~1MB */

/* Walk entry for callback-based API */
typedef struct SpWalkEntry {
    SpPath dirpath;
    char (*dirnames)[SP_WALK_NAME_MAX];  /* Pointer to array of names */
    char (*filenames)[SP_WALK_NAME_MAX];
    size_t dirname_count;
    size_t filename_count;
    void *user_data;
} SpWalkEntry;

/* Walk error callback - called when a directory can't be read */
typedef void (*SpWalkErrorFn)(const SpPath *path, int error_code, void *user_data);

/* Walk callback - return true to continue, false to stop. Modify dirname_count for pruning. */
typedef bool (*SpWalkFn)(struct SpWalkEntry *entry);

/* Walk iterator - caller allocates (stack or heap, their choice)
 * Use SP_WALK_ENTRIES_CAP(n) for buffer size. Overflow skips subtrees.
 *
 * After sp_walk_next() returns true, read results from:
 *   it.dirpath      - current directory path
 *   it.dirnames     - array of subdirectory names (not full paths)
 *   it.dirname_count
 *   it.filenames    - array of file names (not full paths)
 *   it.filename_count
 *
 * For top-down pruning: modify dirname_count before next sp_walk_next() call.
 */
typedef struct SpWalkIter {
    /* === Public: read these after sp_walk_next() returns true === */
    SpPath dirpath;
    char dirnames[SP_WALK_MAX_ENTRIES][SP_WALK_NAME_MAX];
    char filenames[SP_WALK_MAX_ENTRIES][SP_WALK_NAME_MAX];
    size_t dirname_count;
    size_t filename_count;

    /* === Private: do not access directly === */
    struct {
        bool top_down;
        bool follow_symlinks;
        SpWalkErrorFn on_error;
        void *user_data;
        int state;              /* 0=need_scan, 1=yielded_topdown, 2=descending, -1=done */
        size_t subdir_index;    /* Next subdir to descend into (for top-down) */
        char *pending_buf;      /* Caller-provided buffer for pending directories */
        size_t pending_capacity;/* Max entries that fit in buffer */
        size_t pending_count;   /* Current pending count */
        SpFlavor flavor;        /* Preserve original path's flavor */
    } priv_;
} SpWalkIter;

/* Initialize walk iterator with caller-provided pending storage.
 *
 * Parameters:
 *   it           - Iterator struct (caller allocates)
 *   p            - Directory to walk (must be a directory)
 *   top_down     - true=yield parent before children, false=yield children first
 *   follow_symlinks - true=follow symlinks to directories
 *   pending_buf  - Buffer for pending directories (caller allocates)
 *   pending_buf_size - Size of pending buffer in bytes
 *   on_error     - Optional callback for unreadable directories (may be NULL)
 *   user_data    - Passed to on_error callback
 *
 * Returns false immediately if:
 *   - it is NULL
 *   - p is NULL or has embedded nulls
 *   - p is not a directory
 *   - pending_buf is NULL
 *   - pending_buf_size < SP_PATH_MAX (can't hold even 1 entry)
 */
bool sp_walk_begin(SpWalkIter *it, const SpPath *p, bool top_down, bool follow_symlinks,
                   void *pending_buf, size_t pending_buf_size,
                   SpWalkErrorFn on_error, void *user_data);

/* Get next walk entry. Returns true if entry available, false when done.
 * After returning true, read it->dirpath, it->dirnames, it->filenames, etc.
 * For top-down pruning, modify it->dirname_count before calling again.
 */
bool sp_walk_next(SpWalkIter *it);

/* End walk iteration (releases any resources). */
void sp_walk_end(SpWalkIter *it);

/* Callback-based walk - processes entire tree, using real stack for unlimited depth.
 * Calls callback for each directory. Return false from callback to stop early.
 * For top-down pruning, modify entry->dirname_count before returning.
 */
bool sp_walk(const SpPath *p, bool top_down, bool follow_symlinks,
             SpWalkFn callback, SpWalkErrorFn on_error, void *user_data);

/* Walk foreach macro - iterates directory tree with caller-provided buffer.
 * cap: char array for pending dirs (use SP_WALK_ENTRIES_CAP_64/256/1024). Overflow skips subtrees.
 * iter_var: SpWalkIter* to access dirpath, dirnames, filenames, dirname_count, filename_count
 *
 * Example:
 *   SpPath dir = sp_path("src");
 *   char cap[SP_WALK_ENTRIES_CAP_256];
 *   SP_WALK_FOREACH(&dir, true, cap, it) {
 *       printf("%s: %zu dirs, %zu files\n", sp_str(&it->dirpath), it->dirname_count, it->filename_count);
 *   }
 */
#define SP_WALK_FOREACH(dir, top_down, cap, iter_var) \
    for (struct { SpWalkIter it; int done; } sp_wctx_ = {{{0}}, 0}; \
         !sp_wctx_.done && sp_walk_begin(&sp_wctx_.it, dir, top_down, false, cap, sizeof(cap), NULL, NULL); \
         sp_walk_end(&sp_wctx_.it), sp_wctx_.done = 1) \
    for (SpWalkIter *iter_var = &sp_wctx_.it; sp_walk_next(iter_var); )

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
    SpTerm (*name)(void);
    SpTerm (*stem)(void);
    SpTerm (*suffix)(void);
    SpSuffixes (*suffixes)(void);
    SpTerm (*drive)(void);
    SpTerm (*root)(void);
    SpTerm (*anchor)(void);
    SpTerm (*owner)(void);
    SpTerm (*group)(void);
    const char *(*str)(void);
    bool (*is_absolute)(void);
    bool (*is_relative_to)(const SpPath *);
    bool (*is_file)(void);
    bool (*is_dir)(void);
    bool (*exists)(void);
    bool (*is_symlink)(void);
    bool (*is_block_device)(void);
    bool (*is_char_device)(void);
    bool (*is_fifo)(void);
    bool (*is_socket)(void);
    bool (*is_mount)(void);
    bool (*is_junction)(void);
    /* Chainable - return pointer to avoid stack copies */
    SpPrivDontUseThisDirectly_ *(*parent)(void);
    SpPrivDontUseThisDirectly_ *(*join)(const char *);
    SpPrivDontUseThisDirectly_ *(*with_name)(const char *);
    SpPrivDontUseThisDirectly_ *(*with_stem)(const char *);
    SpPrivDontUseThisDirectly_ *(*with_suffix)(const char *);
    SpPrivDontUseThisDirectly_ *(*absolute)(void);
    SpPrivDontUseThisDirectly_ *(*expanduser)(void);
    SpPrivDontUseThisDirectly_ *(*relative_to)(const SpPath *);
    SpPrivDontUseThisDirectly_ *(*relative_to_walk_up)(const SpPath *);
};
SpPrivDontUseThisDirectly_ *sp_fluent_init_(SpPath);

/* SPF("/a")->join("b")->parent()->str() */
#define SPF(s) sp_fluent_init_(sp_path(s))
#define SPF_P(s) sp_fluent_init_(sp_path_f((s), SP_FLAVOR_POSIX))
#define SPF_W(s) sp_fluent_init_(sp_path_f((s), SP_FLAVOR_WINDOWS))
#define SPF_PATH(p) sp_fluent_init_(p)

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
#include <stdlib.h>  /* For realpath */
#include <stdio.h>   /* For rename */
#include <fcntl.h>   /* For O_CREAT, etc. */
#include <utime.h>   /* For utime() */
#include <pwd.h>     /* For getpwuid, getpwnam */
#include <grp.h>     /* For getgrgid */
#define sp_priv_getcwd getcwd
/* C99 workaround - these functions exist but aren't declared without feature test macros.
   C++ headers already expose them via stdlib.h/cstdlib, so only declare in C mode. */
#ifndef __cplusplus
extern int lstat(const char *path, struct stat *buf);
extern ssize_t readlink(const char *path, char *buf, size_t bufsiz);
extern char *realpath(const char *path, char *resolved_path);
extern int symlink(const char *target, const char *linkpath);
extern int link(const char *oldpath, const char *newpath);
extern int chmod(const char *path, mode_t mode);
#endif
#endif

/* stat mode constants - define if not provided by system headers */
#ifndef S_IFMT
#define S_IFMT 0170000
#endif
#ifndef S_IFSOCK
#define S_IFSOCK 0140000
#endif
#ifndef S_IFLNK
#define S_IFLNK 0120000
#endif
#ifndef S_IFREG
#define S_IFREG 0100000
#endif
#ifndef S_IFBLK
#define S_IFBLK 0060000
#endif
#ifndef S_IFDIR
#define S_IFDIR 0040000
#endif
#ifndef S_IFCHR
#define S_IFCHR 0020000
#endif
#ifndef S_IFIFO
#define S_IFIFO 0010000
#endif
#ifndef S_ISLNK
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#endif
#ifndef S_ISSOCK
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)
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

/* Check if path contains embedded null bytes (which would truncate C strings) */
static inline bool sp_priv_has_embedded_null(const SpPath *p) {
    for (size_t i = 0; i < p->len; i++) {
        if (p->buf[i] == '\0') return true;
    }
    return false;
}

static inline bool sp_priv_has_drive(const char *s, size_t len, SpFlavor flavor) {
    if (!sp_priv_is_windows_flavor(flavor)) return false;
    return len >= 2 && sp_priv_is_drive_letter(s[0]) && s[1] == ':';
}

static inline bool sp_priv_is_unc(const char *s, size_t len, SpFlavor flavor) {
    if (!sp_priv_is_windows_flavor(flavor)) return false;
    return len >= 2 && sp_priv_is_sep(s[0], flavor) && sp_priv_is_sep(s[1], flavor);
}

/* Unified UNC path parsing result */
typedef struct {
    size_t server_end;    /* Position after server (0 if none) */
    size_t share_end;     /* Position after share (0 if incomplete) */
    bool is_unc_device;   /* //?/UNC/... pattern */
    bool is_device_ns;    /* //. or //? pattern (without UNC) */
    bool is_complete;     /* Has both non-empty server AND share */
} SpUncInfo;

/* Parse UNC path structure. Assumes sp_priv_is_unc() already returned true. */
static SpUncInfo sp_priv_parse_unc(const char *s, size_t len, SpFlavor flavor) {
    SpUncInfo info = {0, 0, false, false, false};

    /* Check for device namespace (//. or //?) */
    bool has_device_prefix = (len > 2 && (s[2] == '.' || s[2] == '?') && (len == 3 || sp_priv_is_sep(s[3], flavor)));
    info.is_device_ns = has_device_prefix;

    /* Check for //?/UNC pattern (case-insensitive) */
    if (has_device_prefix && len >= 8 && sp_priv_is_sep(s[3], flavor)) {
        char c4 = s[4], c5 = s[5], c6 = s[6];
        if (c4 >= 'a' && c4 <= 'z') c4 -= 32;
        if (c5 >= 'a' && c5 <= 'z') c5 -= 32;
        if (c6 >= 'a' && c6 <= 'z') c6 -= 32;
        if (c4 == 'U' && c5 == 'N' && c6 == 'C' && (len == 7 || sp_priv_is_sep(s[7], flavor))) {
            info.is_unc_device = true;
            info.is_device_ns = false; /* UNC device is separate from plain device ns */
            size_t i = 8; /* Past "//?/UNC/" */
            if (i < len) {
                while (i < len && !sp_priv_is_sep(s[i], flavor)) i++;
                if (i > 8) info.server_end = i; /* Non-empty server */
                if (i < len) {
                    i++; /* Past separator */
                    size_t share_start = i;
                    while (i < len && !sp_priv_is_sep(s[i], flavor)) i++;
                    if (i > share_start) { info.share_end = i; info.is_complete = true; }
                }
            }
            return info;
        }
    }

    /* Regular UNC: //server/share */
    size_t i = 2;
    while (i < len && !sp_priv_is_sep(s[i], flavor)) i++;
    if (i > 2) info.server_end = i; /* Non-empty server */
    if (i < len && info.server_end > 0) {
        i++; /* Past separator */
        size_t share_start = i;
        while (i < len && !sp_priv_is_sep(s[i], flavor)) i++;
        if (i > share_start) { info.share_end = i; info.is_complete = true; }
    }
    return info;
}

static size_t sp_priv_drive_len(const char *s, size_t len, SpFlavor flavor) {
    if (sp_priv_has_drive(s, len, flavor)) return 2;
    if (!sp_priv_is_unc(s, len, flavor)) return 0;

    SpUncInfo info = sp_priv_parse_unc(s, len, flavor);
    if (info.is_unc_device) {
        /* //?/UNC paths: include trailing sep for incomplete, exclude for complete */
        if (info.share_end > 0) return info.share_end;
        if (info.server_end > 0) return info.server_end < len ? info.server_end + 1 : info.server_end;
        return len < 8 ? len : 8; /* Include //?/UNC/ */
    }
    /* Regular UNC: drive extends through //server/share */
    if (info.share_end > 0) return info.share_end;
    /* Incomplete UNC: include trailing separator if present */
    if (info.server_end > 0) {
        return (info.server_end < len && sp_priv_is_sep(s[info.server_end], flavor))
               ? info.server_end + 1 : info.server_end;
    }
    return 2;
}

static size_t sp_priv_root_len(const char *s, size_t len, SpFlavor flavor) {
    size_t drive = sp_priv_drive_len(s, len, flavor);

    if (sp_priv_is_windows_flavor(flavor) && sp_priv_is_unc(s, len, flavor)) {
        SpUncInfo info = sp_priv_parse_unc(s, len, flavor);
        if (info.is_device_ns) return (drive < len && sp_priv_is_sep(s[drive], flavor)) ? 1 : 0;
        return info.is_complete ? 1 : 0; /* Complete UNC has implicit root */
    }

    /* POSIX: paths starting with exactly // have root // */
    if (!sp_priv_is_windows_flavor(flavor) && len >= 2 && s[0] == '/' && s[1] == '/' && (len == 2 || s[2] != '/'))
        return 2;

    return (drive < len && sp_priv_is_sep(s[drive], flavor)) ? 1 : 0;
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

    /* For COMPLETE UNC paths without trailing separator, add the implicit root. */
    if (sp_priv_is_windows_flavor(flavor) && sp_priv_is_unc(buf, *len, flavor)) {
        SpUncInfo info = sp_priv_parse_unc(buf, *len, flavor);
        if (info.is_complete && !info.is_device_ns && drive == *len && j + 1 < SP_PATH_MAX)
            buf[j++] = sep;
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
    return sp_path_from_n(s, s ? strlen(s) : 0, opts.flavor);
}

SpPath sp_path_from_n(const char *s, size_t len, SpFlavor flavor) {
    SP_ASSERT_FLAVOR(flavor);
    SpPath p = SP_PRIV_ZERO;
    p.flavor = flavor;
    p.len = len;
    if (p.len >= SP_PATH_MAX) p.len = SP_PATH_MAX - 1;
    if (s && p.len > 0) {
        memcpy(p.buf, s, p.len);
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

SpTerm sp_drive(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (p->len == 0) return sp_priv_term(NULL, 0);
    return sp_priv_term(p->buf, sp_priv_drive_len(p->buf, p->len, p->flavor));
}

SpTerm sp_root(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (p->len == 0) return sp_priv_term(NULL, 0);
    size_t start = sp_priv_drive_len(p->buf, p->len, p->flavor);
    size_t rlen = sp_priv_root_len(p->buf, p->len, p->flavor);
    if (start + rlen > p->len) rlen = p->len > start ? p->len - start : 0;
    return sp_priv_term(p->buf + start, rlen);
}

SpTerm sp_anchor(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (p->len == 0) return sp_priv_term(NULL, 0);
    size_t alen = sp_priv_anchor_len(p->buf, p->len, p->flavor);
    return sp_priv_term(p->buf, alen > p->len ? p->len : alen);
}

/* Private: get name as SpStr pointing into path buffer (for internal use) */
static inline SpStr sp_priv_name_sv(const SpPath *p) {
    if (p->len == 0) return SP_PRIV_STR(SP_PRIV_NULL, 0);
    size_t anchor = sp_priv_anchor_len(p->buf, p->len, p->flavor);
    if (anchor == p->len) return SP_PRIV_STR(p->buf + p->len, 0);
    size_t i = p->len;
    while (i > anchor && !sp_priv_is_sep(p->buf[i - 1], p->flavor)) i--;
    if (i == 0 && p->len - i == 1 && p->buf[i] == '.') return SP_PRIV_STR(SP_PRIV_NULL, 0);
    return SP_PRIV_STR(p->buf + i, p->len - i);
}

SpTerm sp_name(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpStr sv = sp_priv_name_sv(p);
    return sp_priv_term(sv.data, sv.len);
}

SpTerm sp_suffix(const SpPath *p) {
    SpTerm name = sp_name(p);
    if (name.len == 0) return sp_priv_term(NULL, 0);
    bool all_dots = true;
    for (size_t j = 0; j < name.len && all_dots; j++) {
        if (name.buf[j] != '.') all_dots = false;
    }
    if (all_dots) return sp_priv_term(NULL, 0);
    size_t i = name.len;
    while (i > 0 && name.buf[i - 1] != '.') i--;
    if (i <= 1 || i == name.len) return sp_priv_term(NULL, 0);
    return sp_priv_term(name.buf + i - 1, name.len - i + 1);
}

SpTerm sp_stem(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpTerm name = sp_name(p);
    if (name.len == 0) return sp_priv_term(NULL, 0);
    return sp_priv_term(name.buf, name.len - sp_suffix(p).len);
}

SpSuffixes sp_suffixes(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpSuffixes r = SP_PRIV_ZERO;
    SpStr name = sp_priv_name_sv(p);
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
            return sp_path_from_n(other, olen, flavor);
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
        return sp_path_from_n(other, olen, flavor);
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
                return sp_path_from_n(other, olen, flavor);
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
        return sp_path_from_n(other, olen, flavor);
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

SpPath sp_join_n(const SpPath *base, const char *s, size_t len) {
    SP_ASSERT_PATH_INVARIANT(base);
    if (len == 0 || !s) return sp_path_copy(base);
    if (base->len == 0) return sp_path_from_n(s, len, base->flavor);
    return sp_priv_join_len(base, s, len);
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
    SpTerm n = sp_name(p);
    return n.len > 0 && !(n.len == 1 && n.buf[0] == '.');
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

    SpTerm suf = sp_suffix(p);
    char name[SP_PATH_MAX];
    if (slen + suf.len >= SP_PATH_MAX) slen = SP_PATH_MAX - suf.len - 1;
    memcpy(name, stem, slen);
    if (suf.len > 0) {
        memcpy(name + slen, suf.buf, suf.len);
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

    SpTerm stm = sp_stem(p);
    char name[SP_PATH_MAX];
    size_t stmlen = stm.len;
    if (stmlen + suflen >= SP_PATH_MAX) stmlen = SP_PATH_MAX - suflen - 1;
    if (stmlen > 0) {
        memcpy(name, stm.buf, stmlen);
    }
    memcpy(name + stmlen, suffix, suflen);
    name[stmlen + suflen] = '\0';
    return sp_with_name(p, name);
}

bool sp_is_absolute(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpTerm drv = sp_drive(p);
    SpTerm rt = sp_root(p);
    if (sp_priv_is_windows_flavor(p->flavor)) {
        /* UNC paths are always absolute */
        if (drv.len >= 2 && sp_priv_is_sep(drv.buf[0], p->flavor) && sp_priv_is_sep(drv.buf[1], p->flavor)) {
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
        SpTerm name = sp_name(p);
        return (name.len > 0 && sp_priv_fnmatch(pattern, plen, name.buf, name.len, ci)) ? SP_MATCH_YES : SP_MATCH_NO;
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
        SpTerm drv = sp_drive(p);
        if (drv.len < 2) return SP_MATCH_NO;
        char pd = sp_priv_tolower(pattern[0]), pthd = sp_priv_tolower(drv.buf[0]);
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

/* Helper for POSIX-only file type checks (returns false on Windows) */
#ifndef SP_WINDOWS
static bool sp_priv_check_posix_type(const SpPath *p, unsigned int type_mask, bool use_lstat) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (sp_priv_has_embedded_null(p)) return false;
    const char *path_str = sp_str(p);
    struct stat st;
    if (use_lstat ? lstat(path_str, &st) : stat(path_str, &st)) return false;
    return (st.st_mode & S_IFMT) == type_mask;
}
#endif

bool sp_is_reserved(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (!sp_priv_is_windows_flavor(p->flavor) || sp_priv_is_unc(p->buf, p->len, p->flavor)) return false;
    SpTerm name = sp_name(p);
    if (name.len == 0 || name.len > 12) return false;
    char upper[13];
    size_t len = 0;
    for (size_t i = 0; i < name.len && name.buf[i] != '.' && name.buf[i] != ':' && len < 12; i++) {
        unsigned char c = SP_PRIV_CAST(unsigned char, name.buf[i]);
        if (c == 0xC2 && i + 1 < name.len) {
            unsigned char c2 = SP_PRIV_CAST(unsigned char, name.buf[i + 1]);
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
#ifdef SP_WINDOWS
    SP_ASSERT_PATH_INVARIANT(p);
    if (sp_priv_has_embedded_null(p)) return false;
    DWORD attrs = GetFileAttributesA(sp_str(p));
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY)) return false;
    HANDLE h = CreateFileA(sp_str(p), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD type = GetFileType(h);
    CloseHandle(h);
    return type == FILE_TYPE_DISK;
#else
    return sp_priv_check_posix_type(p, S_IFREG, false);
#endif
}

bool sp_is_dir(const SpPath *p) {
#ifdef SP_WINDOWS
    SP_ASSERT_PATH_INVARIANT(p);
    if (sp_priv_has_embedded_null(p)) return false;
    DWORD attrs = GetFileAttributesA(sp_str(p));
    return (attrs != INVALID_FILE_ATTRIBUTES) && (attrs & FILE_ATTRIBUTE_DIRECTORY);
#else
    return sp_priv_check_posix_type(p, S_IFDIR, false);
#endif
}

bool sp_exists(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (sp_priv_has_embedded_null(p)) return false;
    const char *path_str = sp_str(p);
#ifdef SP_WINDOWS
    return GetFileAttributesA(path_str) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st;
    return stat(path_str, &st) == 0;
#endif
}

bool sp_is_symlink(const SpPath *p) {
#ifdef SP_WINDOWS
    SP_ASSERT_PATH_INVARIANT(p);
    if (sp_priv_has_embedded_null(p)) return false;
    DWORD attrs = GetFileAttributesA(sp_str(p));
    return (attrs != INVALID_FILE_ATTRIBUTES) && (attrs & FILE_ATTRIBUTE_REPARSE_POINT);
#else
    return sp_priv_check_posix_type(p, S_IFLNK, true);
#endif
}

bool sp_is_block_device(const SpPath *p) {
#ifdef SP_WINDOWS
    (void)p; return false;
#else
    return sp_priv_check_posix_type(p, S_IFBLK, false);
#endif
}

bool sp_is_char_device(const SpPath *p) {
#ifdef SP_WINDOWS
    SP_ASSERT_PATH_INVARIANT(p);
    if (sp_priv_has_embedded_null(p)) return false;
    DWORD attrs = GetFileAttributesA(sp_str(p));
    if (attrs == INVALID_FILE_ATTRIBUTES) return false;
    HANDLE h = CreateFileA(sp_str(p), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD type = GetFileType(h);
    CloseHandle(h);
    return type == FILE_TYPE_CHAR;
#else
    return sp_priv_check_posix_type(p, S_IFCHR, false);
#endif
}

bool sp_is_fifo(const SpPath *p) {
#ifdef SP_WINDOWS
    (void)p; return false;
#else
    return sp_priv_check_posix_type(p, S_IFIFO, false);
#endif
}

bool sp_is_socket(const SpPath *p) {
#ifdef SP_WINDOWS
    (void)p; return false;
#else
    return sp_priv_check_posix_type(p, S_IFSOCK, false);
#endif
}

bool sp_is_mount(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (sp_priv_has_embedded_null(p)) return false;
    const char *path_str = sp_str(p);
#ifdef SP_WINDOWS
    /* On Windows, check if path is a drive root or mount point */
    char vol_path[SP_PATH_MAX];
    if (!GetVolumePathNameA(path_str, vol_path, SP_PATH_MAX)) return false;
    /* Compare the volume path with the actual path (normalized) */
    size_t vlen = strlen(vol_path);
    size_t plen = p->len > 0 ? p->len : 1;
    /* Remove trailing backslash from both for comparison */
    if (vlen > 0 && vol_path[vlen - 1] == '\\') vlen--;
    if (plen > 0 && (path_str[plen - 1] == '\\' || path_str[plen - 1] == '/')) plen--;
    /* Path is a mount point if it equals the volume path */
    return sp_priv_str_eq_ci(path_str, plen, vol_path, vlen);
#else
    struct stat st_path, st_parent;
    if (lstat(path_str, &st_path) != 0) return false;
    /* Must be a directory to be a mount point */
    if (!S_ISDIR(st_path.st_mode)) return false;
    /* Get parent path */
    SpPath parent = sp_parent(p);
    const char *parent_str = sp_str(&parent);
    if (lstat(parent_str, &st_parent) != 0) return false;
    /* Path is a mount point if it's on a different device than its parent */
    if (st_path.st_dev != st_parent.st_dev) return true;
    /* Or if path == parent (root case: / has parent /) */
    return st_path.st_ino == st_parent.st_ino;
#endif
}

bool sp_is_junction(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (sp_priv_has_embedded_null(p)) return false;
#ifdef SP_WINDOWS
    const char *path_str = sp_str(p);
    DWORD attrs = GetFileAttributesA(path_str);
    if (attrs == INVALID_FILE_ATTRIBUTES) return false;
    /* Must be a directory with reparse point attribute */
    if (!(attrs & FILE_ATTRIBUTE_DIRECTORY)) return false;
    if (!(attrs & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
    /* Check if it's specifically a junction (IO_REPARSE_TAG_MOUNT_POINT) */
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(path_str, &fd);
    if (h == INVALID_HANDLE_VALUE) return false;
    FindClose(h);
    /* Junction has IO_REPARSE_TAG_MOUNT_POINT (0xA0000003) */
    return fd.dwReserved0 == 0xA0000003;
#else
    /* Junctions don't exist on POSIX systems */
    (void)p;
    return false;
#endif
}

/* Helper to fill SpStatResult from POSIX stat structure */
#ifndef SP_WINDOWS
static void sp_priv_fill_stat_result(SpStatResult *r, const struct stat *st) {
    r->sp_mode = SP_PRIV_CAST(unsigned int, st->st_mode);
    r->sp_ino = SP_PRIV_CAST(unsigned long long, st->st_ino);
    r->sp_dev = SP_PRIV_CAST(unsigned long long, st->st_dev);
    r->sp_nlink = SP_PRIV_CAST(unsigned long long, st->st_nlink);
    r->sp_uid = SP_PRIV_CAST(unsigned int, st->st_uid);
    r->sp_gid = SP_PRIV_CAST(unsigned int, st->st_gid);
    r->sp_size = SP_PRIV_CAST(long long, st->st_size);
    r->sp_atime = SP_PRIV_CAST(double, st->st_atime);
    r->sp_mtime = SP_PRIV_CAST(double, st->st_mtime);
    r->sp_ctime = SP_PRIV_CAST(double, st->st_ctime);
    /* Nanosecond timestamps - use st_atimespec on BSD, seconds elsewhere.
     * Note: Linux glibc has st_atim but requires _POSIX_C_SOURCE >= 200809L,
     * which we can't guarantee. Using seconds-only for portability. */
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    /* BSD-style: st_atimespec, st_mtimespec, st_ctimespec */
    r->sp_atime_ns = SP_PRIV_CAST(long long, st->st_atimespec.tv_sec) * 1000000000LL + st->st_atimespec.tv_nsec;
    r->sp_mtime_ns = SP_PRIV_CAST(long long, st->st_mtimespec.tv_sec) * 1000000000LL + st->st_mtimespec.tv_nsec;
    r->sp_ctime_ns = SP_PRIV_CAST(long long, st->st_ctimespec.tv_sec) * 1000000000LL + st->st_ctimespec.tv_nsec;
#else
    /* Fallback: seconds-only precision */
    r->sp_atime_ns = SP_PRIV_CAST(long long, st->st_atime) * 1000000000LL;
    r->sp_mtime_ns = SP_PRIV_CAST(long long, st->st_mtime) * 1000000000LL;
    r->sp_ctime_ns = SP_PRIV_CAST(long long, st->st_ctime) * 1000000000LL;
#endif
    r->valid = true;
}
#endif

/* Windows FILETIME to Unix timestamp conversion macros */
#ifdef SP_WINDOWS
#define SP_FILETIME_TO_UNIX(ft) \
    (((SP_PRIV_CAST(double, (SP_PRIV_CAST(unsigned long long, (ft).dwHighDateTime) << 32) | (ft).dwLowDateTime)) - 116444736000000000.0) / 10000000.0)
#define SP_FILETIME_TO_NS(ft) \
    ((SP_PRIV_CAST(long long, (SP_PRIV_CAST(unsigned long long, (ft).dwHighDateTime) << 32) | (ft).dwLowDateTime) - 116444736000000000LL) * 100LL)
#endif

SpStatResult sp_stat(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpStatResult result = SP_PRIV_ZERO;
    result.valid = false;
    if (sp_priv_has_embedded_null(p)) return result;
    const char *path_str = sp_str(p);

#ifdef SP_WINDOWS
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

    /* Try GetFileInformationByHandleEx for 64-bit volume serial (Windows 8+) */
    typedef struct { ULONGLONG VolumeSerialNumber; BYTE FileId[16]; } SP_FILE_ID_INFO;
    SP_FILE_ID_INFO fii;
    if (GetFileInformationByHandleEx(hFile, SP_PRIV_CAST(FILE_INFO_BY_HANDLE_CLASS, 18), &fii, sizeof(fii))) {
        result.sp_dev = fii.VolumeSerialNumber;
        memcpy(&result.sp_ino, fii.FileId, sizeof(result.sp_ino));
    } else {
        result.sp_dev = SP_PRIV_CAST(unsigned long long, info.dwVolumeSerialNumber);
        result.sp_ino = (SP_PRIV_CAST(unsigned long long, info.nFileIndexHigh) << 32) |
                        SP_PRIV_CAST(unsigned long long, info.nFileIndexLow);
    }
    CloseHandle(hFile);

    result.sp_nlink = SP_PRIV_CAST(unsigned long long, info.nNumberOfLinks);
    result.sp_mode = (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 040777 : 0100666;
    if (info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
        result.sp_mode &= ~0222;
    }
    result.sp_size = (SP_PRIV_CAST(long long, info.nFileSizeHigh) << 32) |
                     SP_PRIV_CAST(long long, info.nFileSizeLow);

    result.sp_atime = SP_FILETIME_TO_UNIX(info.ftLastAccessTime);
    result.sp_mtime = SP_FILETIME_TO_UNIX(info.ftLastWriteTime);
    result.sp_ctime = SP_FILETIME_TO_UNIX(info.ftCreationTime);
    result.sp_atime_ns = SP_FILETIME_TO_NS(info.ftLastAccessTime);
    result.sp_mtime_ns = SP_FILETIME_TO_NS(info.ftLastWriteTime);
    result.sp_ctime_ns = SP_FILETIME_TO_NS(info.ftCreationTime);

    result.sp_uid = 0;
    result.sp_gid = 0;
    result.valid = true;
#else
    struct stat st;
    if (stat(path_str, &st) == 0) sp_priv_fill_stat_result(&result, &st);
#endif
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

SpStatResult sp_lstat(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpStatResult result = SP_PRIV_ZERO;
    result.valid = false;
    if (sp_priv_has_embedded_null(p)) return result;
    const char *path_str = sp_str(p);

#ifdef SP_WINDOWS
    /* On Windows, lstat uses FindFirstFileA to get attributes without following symlinks */
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(path_str, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return result;
    }
    FindClose(h);

    result.sp_mode = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 040777 : 0100666;
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
        result.sp_mode &= ~0222;
    }
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
        result.sp_mode = (result.sp_mode & ~S_IFMT) | S_IFLNK;
    }

    result.sp_size = (SP_PRIV_CAST(long long, fd.nFileSizeHigh) << 32) |
                     SP_PRIV_CAST(long long, fd.nFileSizeLow);

    result.sp_atime = SP_FILETIME_TO_UNIX(fd.ftLastAccessTime);
    result.sp_mtime = SP_FILETIME_TO_UNIX(fd.ftLastWriteTime);
    result.sp_ctime = SP_FILETIME_TO_UNIX(fd.ftCreationTime);
    result.sp_atime_ns = SP_FILETIME_TO_NS(fd.ftLastAccessTime);
    result.sp_mtime_ns = SP_FILETIME_TO_NS(fd.ftLastWriteTime);
    result.sp_ctime_ns = SP_FILETIME_TO_NS(fd.ftCreationTime);

    /* Get inode/dev/nlink via file handle with FILE_FLAG_OPEN_REPARSE_POINT */
    HANDLE fh = CreateFileA(path_str, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            NULL, OPEN_EXISTING,
                            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (fh == INVALID_HANDLE_VALUE) {
        return result;
    }

    BY_HANDLE_FILE_INFORMATION info;
    if (!GetFileInformationByHandle(fh, &info)) {
        CloseHandle(fh);
        return result;
    }
    result.sp_nlink = info.nNumberOfLinks;

    typedef struct { unsigned long long VolumeSerialNumber; unsigned char FileId[16]; } SpFileIdInfo;
    SpFileIdInfo id_info;
    if (!GetFileInformationByHandleEx(fh, (FILE_INFO_BY_HANDLE_CLASS)18, &id_info, sizeof(id_info))) {
        CloseHandle(fh);
        return result;
    }
    result.sp_dev = id_info.VolumeSerialNumber;
    memcpy(&result.sp_ino, id_info.FileId, sizeof(result.sp_ino));
    CloseHandle(fh);

    result.sp_uid = 0;
    result.sp_gid = 0;
    result.valid = true;
#else
    struct stat st;
    if (lstat(path_str, &st) == 0) sp_priv_fill_stat_result(&result, &st);
#endif
    return result;
}

SpPath sp_readlink(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpPath result = SP_PRIV_ZERO;
    result.flavor = p->flavor;
    if (sp_priv_has_embedded_null(p)) {
        result.buf[0] = SP_ERR_OTHER;
        return result;
    }
    const char *path_str = sp_str(p);

#ifdef SP_WINDOWS
    /* On Windows, use CreateFile with FILE_FLAG_OPEN_REPARSE_POINT to open
       the symlink itself, then use DeviceIoControl to read the target. */
    HANDLE h = CreateFileA(path_str, GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           NULL, OPEN_EXISTING,
                           FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        result.buf[0] = SP_ERR_OTHER;
        return result;
    }

    /* Buffer for reparse data (REPARSE_DATA_BUFFER) */
    char reparse_buf[16384];
    DWORD bytes_returned;
    if (!DeviceIoControl(h, 0x000900A8 /* FSCTL_GET_REPARSE_POINT */,
                         NULL, 0, reparse_buf, sizeof(reparse_buf),
                         &bytes_returned, NULL)) {
        CloseHandle(h);
        result.buf[0] = SP_ERR_OTHER;
        return result;
    }
    CloseHandle(h);

    /* Parse REPARSE_DATA_BUFFER structure */
    DWORD tag = *(DWORD *)reparse_buf;
    if (tag == 0xA000000C /* IO_REPARSE_TAG_SYMLINK */) {
        /* SymbolicLinkReparseBuffer starts at offset 8 */
        WORD subst_offset = *(WORD *)(reparse_buf + 8);
        WORD subst_len = *(WORD *)(reparse_buf + 10);
        WORD print_offset = *(WORD *)(reparse_buf + 12);
        WORD print_len = *(WORD *)(reparse_buf + 14);
        DWORD flags = *(DWORD *)(reparse_buf + 16);
        (void)subst_offset; (void)subst_len; (void)flags;

        /* Use PrintName (more readable than SubstituteName) */
        WCHAR *print_name = (WCHAR *)(reparse_buf + 20 + print_offset);
        int print_chars = print_len / 2;

        /* Convert UTF-16 to UTF-8 */
        int utf8_len = WideCharToMultiByte(CP_UTF8, 0, print_name, print_chars,
                                           result.buf, SP_PATH_MAX - 1, NULL, NULL);
        if (utf8_len > 0) {
            result.len = SP_PRIV_CAST(size_t, utf8_len);
            result.buf[result.len] = '\0';
            sp_priv_normalize(result.buf, &result.len, result.flavor);
        } else {
            result.buf[0] = SP_ERR_OTHER;
        }
    } else if (tag == 0xA0000003 /* IO_REPARSE_TAG_MOUNT_POINT (junction) */) {
        /* MountPointReparseBuffer - similar structure */
        WORD subst_offset = *(WORD *)(reparse_buf + 8);
        WORD subst_len = *(WORD *)(reparse_buf + 10);
        WORD print_offset = *(WORD *)(reparse_buf + 12);
        WORD print_len = *(WORD *)(reparse_buf + 14);
        (void)subst_offset; (void)subst_len;

        WCHAR *print_name = (WCHAR *)(reparse_buf + 16 + print_offset);
        int print_chars = print_len / 2;

        int utf8_len = WideCharToMultiByte(CP_UTF8, 0, print_name, print_chars,
                                           result.buf, SP_PATH_MAX - 1, NULL, NULL);
        if (utf8_len > 0) {
            result.len = SP_PRIV_CAST(size_t, utf8_len);
            result.buf[result.len] = '\0';
            sp_priv_normalize(result.buf, &result.len, result.flavor);
        } else {
            result.buf[0] = SP_ERR_OTHER;
        }
    } else {
        /* Not a symlink or junction */
        result.buf[0] = SP_ERR_OTHER;
    }
#else
    char buf[SP_PATH_MAX];
    ssize_t len = readlink(path_str, buf, SP_PATH_MAX - 1);
    if (len < 0) {
        result.buf[0] = SP_ERR_OTHER;
        return result;
    }
    buf[len] = '\0';
    memcpy(result.buf, buf, SP_PRIV_CAST(size_t, len) + 1);
    result.len = SP_PRIV_CAST(size_t, len);
#endif

    return result;
}

SpPath sp_resolve(const SpPath *p, bool strict) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpPath result = SP_PRIV_ZERO;
    result.flavor = p->flavor;
    if (sp_priv_has_embedded_null(p)) {
        if (strict) {
            result.buf[0] = SP_ERR_OTHER;
            return result;
        }
        return sp_absolute(p);
    }
    const char *path_str = sp_str(p);

#ifdef SP_WINDOWS
    /* On Windows, use GetFullPathNameA for resolution */
    char full_path[SP_PATH_MAX];
    DWORD len = GetFullPathNameA(path_str, SP_PATH_MAX, full_path, NULL);
    if (len == 0 || len >= SP_PATH_MAX) {
        if (strict) {
            result.buf[0] = SP_ERR_OTHER;
            return result;
        }
        return sp_absolute(p);
    }

    /* Check if path exists when strict=true */
    if (strict) {
        DWORD attrs = GetFileAttributesA(full_path);
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            result.buf[0] = SP_ERR_OTHER;
            return result;
        }
    }

    memcpy(result.buf, full_path, len);
    result.len = len;
    result.buf[result.len] = '\0';
    sp_priv_normalize(result.buf, &result.len, result.flavor);
#else
    char resolved[SP_PATH_MAX];
    if (realpath(path_str, resolved) != NULL) {
        size_t len = strlen(resolved);
        memcpy(result.buf, resolved, len);
        result.len = len;
        result.buf[result.len] = '\0';
    } else {
        if (strict) { result.buf[0] = SP_ERR_OTHER; return result; }
        /* Non-strict: resolve longest existing prefix, append rest */
        SpPath abs_path = sp_absolute(p);
        SpStr parts[SP_PATH_MAX / 2];
        size_t part_count = sp_priv_collect_parts(&abs_path, parts, SP_PATH_MAX / 2);
        char test_path[SP_PATH_MAX];
        for (size_t existing_parts = part_count; existing_parts > 0; existing_parts--) {
            size_t pos = 0;
            for (size_t i = 0; i < existing_parts && pos < SP_PATH_MAX - 1; i++) {
                if (i > 0 || (parts[0].len > 0 && parts[0].data[0] != '/'))
                    if (pos > 0 && test_path[pos-1] != '/') test_path[pos++] = '/';
                size_t n = parts[i].len < SP_PATH_MAX - 1 - pos ? parts[i].len : SP_PATH_MAX - 1 - pos;
                memcpy(test_path + pos, parts[i].data, n);
                pos += n;
            }
            test_path[pos] = '\0';
            if (realpath(test_path, resolved) != NULL) {
                result.len = strlen(resolved);
                memcpy(result.buf, resolved, result.len);
                for (size_t i = existing_parts; i < part_count; i++) {
                    if (result.len > 0 && result.buf[result.len - 1] != '/') result.buf[result.len++] = '/';
                    size_t n = parts[i].len < SP_PATH_MAX - 1 - result.len ? parts[i].len : SP_PATH_MAX - 1 - result.len;
                    memcpy(result.buf + result.len, parts[i].data, n);
                    result.len += n;
                }
                result.buf[result.len] = '\0';
                return result;
            }
        }
        return abs_path;
    }
#endif

    return result;
}

bool sp_symlink_to(const SpPath *p, const SpPath *target, bool target_is_directory) {
    SP_ASSERT_PATH_INVARIANT(p);
    SP_ASSERT_PATH_INVARIANT(target);
    if (sp_priv_has_embedded_null(p) || sp_priv_has_embedded_null(target)) return false;
    const char *link_path = sp_str(p);
    const char *target_path = sp_str(target);

#ifdef SP_WINDOWS
    /* On Windows, use CreateSymbolicLinkA */
    DWORD flags = target_is_directory ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0;
    /* SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE = 0x2 (Windows 10+) */
    flags |= 0x2;
    return CreateSymbolicLinkA(link_path, target_path, flags) != 0;
#else
    (void)target_is_directory; /* POSIX symlink doesn't need this flag */
    return symlink(target_path, link_path) == 0;
#endif
}

bool sp_hardlink_to(const SpPath *p, const SpPath *target) {
    SP_ASSERT_PATH_INVARIANT(p);
    SP_ASSERT_PATH_INVARIANT(target);
    if (sp_priv_has_embedded_null(p) || sp_priv_has_embedded_null(target)) return false;
    const char *link_path = sp_str(p);
    const char *target_path = sp_str(target);

#ifdef SP_WINDOWS
    return CreateHardLinkA(link_path, target_path, NULL) != 0;
#else
    return link(target_path, link_path) == 0;
#endif
}

bool sp_samefile(const SpPath *a, const SpPath *b) {
    SP_ASSERT_PATH_INVARIANT(a);
    SP_ASSERT_PATH_INVARIANT(b);
    if (sp_priv_has_embedded_null(a) || sp_priv_has_embedded_null(b)) return false;
    SpStatResult stat_a = sp_stat(a);
    SpStatResult stat_b = sp_stat(b);

    if (!stat_a.valid || !stat_b.valid) return false;

    /* Same file if device and inode match */
    return stat_a.sp_dev == stat_b.sp_dev && stat_a.sp_ino == stat_b.sp_ino;
}

/* Helper to create parent directories recursively */
static int sp_priv_mkdir_parents(const SpPath *p) {
    SpPath parent_path = sp_parent(p);
    if (sp_path_eq(&parent_path, p) || parent_path.len == 0) return SP_MKDIR_OK;
#ifdef SP_WINDOWS
    DWORD parent_attrs = GetFileAttributesA(sp_str(&parent_path));
    if (parent_attrs == INVALID_FILE_ATTRIBUTES) {
        int r = sp_mkdir(&parent_path, SP_MKDIR_DEF_MODE, true, true);
        if (r != SP_MKDIR_OK) return r;
    } else if (!(parent_attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        return SP_MKDIR_ERR_NOT_DIR;
    }
#else
    struct stat st;
    if (stat(sp_str(&parent_path), &st) != 0) {
        int r = sp_mkdir(&parent_path, SP_MKDIR_DEF_MODE, true, true);
        if (r != SP_MKDIR_OK) return r;
    } else if (!S_ISDIR(st.st_mode)) {
        return SP_MKDIR_ERR_NOT_DIR;
    }
#endif
    return SP_MKDIR_OK;
}

int sp_mkdir(const SpPath *p, unsigned int mode, bool parents, bool exist_ok) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (mode == 0) mode = SP_MKDIR_DEF_MODE;
    if (sp_priv_has_embedded_null(p)) return SP_MKDIR_ERR_OTHER;
    const char *path_str = sp_str(p);
    if (parents) { int r = sp_priv_mkdir_parents(p); if (r != SP_MKDIR_OK) return r; }
#ifdef SP_WINDOWS
    (void)mode;
    DWORD attrs = GetFileAttributesA(path_str);
    if (attrs != INVALID_FILE_ATTRIBUTES)
        return (attrs & FILE_ATTRIBUTE_DIRECTORY) ? (exist_ok ? SP_MKDIR_OK : SP_MKDIR_ERR_EXISTS) : SP_MKDIR_ERR_EXISTS_NOT_DIR;
    if (CreateDirectoryA(path_str, NULL)) return SP_MKDIR_OK;
    DWORD err = GetLastError();
    if (err == ERROR_ALREADY_EXISTS) return exist_ok ? SP_MKDIR_OK : SP_MKDIR_ERR_EXISTS;
    if (err == ERROR_PATH_NOT_FOUND) return SP_MKDIR_ERR_NOT_FOUND;
    if (err == ERROR_ACCESS_DENIED) return SP_MKDIR_ERR_PERMISSION;
    return SP_MKDIR_ERR_OTHER;
#else
    struct stat st;
    if (stat(path_str, &st) == 0)
        return S_ISDIR(st.st_mode) ? (exist_ok ? SP_MKDIR_OK : SP_MKDIR_ERR_EXISTS) : SP_MKDIR_ERR_EXISTS_NOT_DIR;
    if (mkdir(path_str, SP_PRIV_CAST(mode_t, mode)) == 0) return SP_MKDIR_OK;
    if (errno == EEXIST) return (stat(path_str, &st) == 0 && S_ISDIR(st.st_mode)) ? (exist_ok ? SP_MKDIR_OK : SP_MKDIR_ERR_EXISTS) : SP_MKDIR_ERR_NOT_DIR;
    if (errno == ENOENT) return SP_MKDIR_ERR_NOT_FOUND;
    if (errno == EACCES || errno == EPERM) return SP_MKDIR_ERR_PERMISSION;
    if (errno == ENOTDIR) return SP_MKDIR_ERR_NOT_DIR;
    return SP_MKDIR_ERR_OTHER;
#endif
}

/* ============ File/Directory Modification Implementation ============ */

bool sp_touch(const SpPath *p, unsigned int mode, bool exist_ok) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (sp_priv_has_embedded_null(p)) return false;
    const char *path_str = sp_str(p);
    if (mode == 0) mode = 0666;

#ifdef SP_WINDOWS
    /* Check if file exists first */
    DWORD attrs = GetFileAttributesA(path_str);
    bool file_exists = (attrs != INVALID_FILE_ATTRIBUTES);

    if (file_exists) {
        if (!exist_ok) return false;  /* File exists and exist_ok=false */
        /* Update timestamps */
        HANDLE h = CreateFileA(path_str, FILE_WRITE_ATTRIBUTES,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE) return false;
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);  /* Full 100-nanosecond precision */
        SetFileTime(h, NULL, &ft, &ft);
        CloseHandle(h);
        return true;
    }
    /* File doesn't exist - create it */
    HANDLE h = CreateFileA(path_str, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    CloseHandle(h);
    return true;
#else
    /* Check if file exists first */
    struct stat st;
    bool file_exists = (stat(path_str, &st) == 0);

    if (file_exists) {
        if (!exist_ok) return false;  /* File exists and exist_ok=false */
        /* Update timestamps to current time (NULL = now) */
        return utime(path_str, SP_PRIV_NULL) == 0;
    }
    /* File doesn't exist - create it */
    int fd = open(path_str, O_CREAT | O_WRONLY, SP_PRIV_CAST(mode_t, mode));
    if (fd < 0) return false;
    close(fd);
    return true;
#endif
}

bool sp_unlink(const SpPath *p, bool missing_ok) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (sp_priv_has_embedded_null(p)) return false;
    const char *path_str = sp_str(p);

#ifdef SP_WINDOWS
    if (DeleteFileA(path_str)) return true;
    DWORD err = GetLastError();
    if (missing_ok && (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND)) return true;
    return false;
#else
    if (unlink(path_str) == 0) return true;
    if (missing_ok && errno == ENOENT) return true;
    return false;
#endif
}

bool sp_rmdir(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (sp_priv_has_embedded_null(p)) return false;
    const char *path_str = sp_str(p);

#ifdef SP_WINDOWS
    return RemoveDirectoryA(path_str) != 0;
#else
    return rmdir(path_str) == 0;
#endif
}

SpPath sp_rename(const SpPath *p, const SpPath *target) {
    SP_ASSERT_PATH_INVARIANT(p);
    SP_ASSERT_PATH_INVARIANT(target);
    SpPath result = *target;

    if (sp_priv_has_embedded_null(p) || sp_priv_has_embedded_null(target)) {
        result.len = 0;
        result.buf[0] = SP_ERR_OTHER;
        return result;
    }

    const char *src_str = sp_str(p);
    const char *dst_str = sp_str(target);

#ifdef SP_WINDOWS
    /* MoveFileExA without MOVEFILE_REPLACE_EXISTING won't overwrite */
    if (!MoveFileExA(src_str, dst_str, 0)) {
        result.len = 0;
        result.buf[0] = SP_ERR_OTHER;
    }
#else
    /* POSIX rename does NOT replace, it fails if target exists */
    struct stat st;
    if (stat(dst_str, &st) == 0) {
        /* Target exists - check if it's the same file (same inode) */
        struct stat src_st;
        if (stat(src_str, &src_st) == 0 && src_st.st_ino == st.st_ino && src_st.st_dev == st.st_dev) {
            /* Same file, rename is a no-op */
            return result;
        }
        /* Target exists and is different - fail like Python does for rename() vs replace() */
        result.len = 0;
        result.buf[0] = SP_ERR_OTHER;
        return result;
    }
    if (rename(src_str, dst_str) != 0) {
        result.len = 0;
        result.buf[0] = SP_ERR_OTHER;
    }
#endif
    return result;
}

SpPath sp_replace(const SpPath *p, const SpPath *target) {
    SP_ASSERT_PATH_INVARIANT(p);
    SP_ASSERT_PATH_INVARIANT(target);
    SpPath result = *target;

    if (sp_priv_has_embedded_null(p) || sp_priv_has_embedded_null(target)) {
        result.len = 0;
        result.buf[0] = SP_ERR_OTHER;
        return result;
    }

    const char *src_str = sp_str(p);
    const char *dst_str = sp_str(target);

#ifdef SP_WINDOWS
    /* MOVEFILE_REPLACE_EXISTING allows atomic replace */
    if (!MoveFileExA(src_str, dst_str, MOVEFILE_REPLACE_EXISTING)) {
        result.len = 0;
        result.buf[0] = SP_ERR_OTHER;
    }
#else
    /* POSIX rename is atomic and replaces target */
    if (rename(src_str, dst_str) != 0) {
        result.len = 0;
        result.buf[0] = SP_ERR_OTHER;
    }
#endif
    return result;
}

bool sp_chmod(const SpPath *p, unsigned int mode) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (sp_priv_has_embedded_null(p)) return false;
    const char *path_str = sp_str(p);

#ifdef SP_WINDOWS
    /* Windows only supports setting the read-only attribute */
    DWORD attrs = GetFileAttributesA(path_str);
    if (attrs == INVALID_FILE_ATTRIBUTES) return false;

    /* If mode doesn't have write permission, set read-only */
    DWORD new_attrs = attrs;
    if ((mode & 0222) == 0) {
        new_attrs |= FILE_ATTRIBUTE_READONLY;
    } else {
        new_attrs &= ~FILE_ATTRIBUTE_READONLY;
    }
    if (new_attrs == attrs) return true;  /* No change needed */
    return SetFileAttributesA(path_str, new_attrs) != 0;
#else
    return chmod(path_str, SP_PRIV_CAST(mode_t, mode)) == 0;
#endif
}

/* ============ Glob Implementation ============ */

/* Include dirent for POSIX or use Windows APIs */
#ifdef SP_WINDOWS
/* Already included windows.h above */
#else
#include <dirent.h>
#endif

/* Glob segment types */
#define SP_GLOB_SEG_LITERAL 0
#define SP_GLOB_SEG_PATTERN 1
#define SP_GLOB_SEG_DOUBLESTAR 2
#define SP_GLOB_IS_DOT_OR_DOTDOT(s) ((s)[0] == '.' && ((s)[1] == '\0' || ((s)[1] == '.' && (s)[2] == '\0')))

static int sp_priv_glob_segment_type(const char *segment) {
    for (const char *p = segment; *p; p++)
        if (*p == '*' || *p == '?')
            return (segment[0] == '*' && segment[1] == '*' && segment[2] == '\0')
                ? SP_GLOB_SEG_DOUBLESTAR : SP_GLOB_SEG_PATTERN;
    return SP_GLOB_SEG_LITERAL;
}

static bool sp_priv_glob_match(const char *name, const char *pattern, int seg_type, bool case_insensitive) {
    if (seg_type == SP_GLOB_SEG_LITERAL)
        return case_insensitive
            ? sp_priv_str_eq_ci(name, strlen(name), pattern, strlen(pattern))
            : strcmp(name, pattern) == 0;
    return seg_type == SP_GLOB_SEG_PATTERN
        && sp_priv_fnmatch(pattern, strlen(pattern), name, strlen(name), case_insensitive);
}

static void sp_priv_glob_close_handle(void *handle) {
    if (!handle) return;
#ifdef SP_WINDOWS
    FindClose(SP_PRIV_CAST(HANDLE, handle));
#else
    closedir(SP_PRIV_CAST(DIR *, handle));
#endif
}

static void *sp_priv_glob_open_dir(const SpPath *dir) {
#ifdef SP_WINDOWS
    char search[SP_PATH_MAX + 3];
    size_t len = dir->len;
    if (len == 0) { search[0] = '.'; search[1] = '\\'; search[2] = '*'; search[3] = '\0'; }
    else {
        memcpy(search, dir->buf, len);
        if (!sp_priv_is_sep(search[len - 1], dir->flavor)) search[len++] = '\\';
        search[len++] = '*'; search[len] = '\0';
    }
    WIN32_FIND_DATAA fd;
    HANDLE handle = FindFirstFileA(search, &fd);
    return (handle == INVALID_HANDLE_VALUE) ? NULL : handle;
#else
    return opendir(dir->len ? dir->buf : ".");
#endif
}

static void sp_priv_glob_pop(SpGlobIter *it) {
    sp_priv_glob_close_handle(it->priv_.handles[it->depth]);
    it->priv_.handles[it->depth--] = NULL;
}

static bool sp_priv_glob_push(SpGlobIter *it, const SpPath *path, size_t seg_idx) {
    if (it->depth + 1 >= SP_GLOB_MAX_DEPTH) return false;
    void *handle = sp_priv_glob_open_dir(path);
    if (!handle) return false;
    int depth = ++it->depth;
    it->priv_.dirs[depth] = *path;
    it->priv_.seg_idxs[depth] = seg_idx;
    it->priv_.handles[depth] = handle;
    return true;
}

static size_t sp_priv_glob_parse_pattern(const char *pattern, SpFlavor flavor, char *buf,
                                         size_t *offsets, int *types, bool *dir_only) {
    size_t count = 0, len = strlen(pattern);
    if (len >= SP_GLOB_PATTERN_MAX) len = SP_GLOB_PATTERN_MAX - 1;
    memcpy(buf, pattern, len);
    buf[len] = '\0';
    *dir_only = (len > 0 && sp_priv_is_sep(pattern[len - 1], flavor));

    for (char *p = buf, *start = p; count < SP_GLOB_MAX_SEGMENTS; p++) {
        if (*p == '\0' || sp_priv_is_sep(*p, flavor)) {
            bool at_end = (*p == '\0');
            if (!at_end) *p = '\0';
            if (p > start) {
                offsets[count] = SP_PRIV_CAST(size_t, start - buf);
                types[count++] = sp_priv_glob_segment_type(start);
            }
            if (at_end) break;
            start = p + 1;
        }
    }
    return count;
}

SpGlobIter sp_glob_begin(const SpPath *base, const char *pattern, SpCaseSensitivity cs) {
    SpGlobIter it;
    memset(&it, 0, sizeof(it));
    it.depth = -1;
    if (!base || !pattern || !*pattern) return it;
    SP_ASSERT_PATH_INVARIANT(base);

    it.priv_.flavor = base->flavor;
    it.priv_.case_insensitive = (cs == SP_CASE_INSENSITIVE) ||
        (cs != SP_CASE_SENSITIVE && sp_priv_is_windows_flavor(base->flavor));
    it.priv_.seg_count = sp_priv_glob_parse_pattern(pattern, base->flavor, it.priv_.pattern_buf,
        it.priv_.seg_offsets, it.priv_.seg_types, &it.priv_.dir_only);
    if (it.priv_.seg_count == 0 || !sp_priv_glob_push(&it, base, 0)) { it.depth = -1; return it; }
    it.depth = 0;
    if (it.priv_.seg_types[0] == SP_GLOB_SEG_DOUBLESTAR && it.priv_.seg_count == 1)
        it.priv_.yield_base_pending = true;
    return it;
}

bool sp_glob_next(SpGlobIter *it, SpPath *out) {
    if (!it || it->depth < 0) return false;
    if (it->priv_.yield_base_pending) {
        it->priv_.yield_base_pending = false;
        if (!it->priv_.dir_only || sp_is_dir(&it->priv_.dirs[0])) {
            *out = it->priv_.dirs[0];
            return true;
        }
    }

    while (it->depth >= 0) {
        int depth = it->depth;
        size_t seg_idx = it->priv_.seg_idxs[depth];
        if (seg_idx >= it->priv_.seg_count) { sp_priv_glob_pop(it); continue; }

        const char *pattern = it->priv_.pattern_buf + it->priv_.seg_offsets[seg_idx];
        int seg_type = it->priv_.seg_types[seg_idx];
        bool is_last = (seg_idx == it->priv_.seg_count - 1);

        /* Literal . or .. : synthesize without readdir (opendir fails on some systems) */
        if (seg_type == SP_GLOB_SEG_LITERAL && SP_GLOB_IS_DOT_OR_DOTDOT(pattern)) {
            SpPath full = sp_join_one(&it->priv_.dirs[depth], pattern);
            if (!sp_is_dir(&full)) { sp_priv_glob_pop(it); continue; }
            if (is_last) { it->priv_.seg_idxs[depth]++; *out = full; return true; }
            /* Advance; reopen only if next segment needs readdir */
            it->priv_.seg_idxs[depth]++;
            it->priv_.dirs[depth] = full;
            const char *next_pattern = it->priv_.pattern_buf + it->priv_.seg_offsets[seg_idx + 1];
            if (it->priv_.seg_types[seg_idx + 1] == SP_GLOB_SEG_LITERAL && SP_GLOB_IS_DOT_OR_DOTDOT(next_pattern))
                continue;
            sp_priv_glob_close_handle(it->priv_.handles[depth]);
            it->priv_.handles[depth] = sp_priv_glob_open_dir(&full);
            if (!it->priv_.handles[depth]) it->depth--;
            continue;
        }

        const char *name;
#ifdef SP_WINDOWS
        WIN32_FIND_DATAA fd;
        if (!FindNextFileA(it->priv_.handles[depth], &fd)) { sp_priv_glob_pop(it); continue; }
        name = fd.cFileName;
#else
        struct dirent *de = readdir(SP_PRIV_CAST(DIR *, it->priv_.handles[depth]));
        if (!de) { sp_priv_glob_pop(it); continue; }
        name = de->d_name;
#endif
        if (name[0] == '.' && name[1] == '\0') continue;
        if (SP_GLOB_IS_DOT_OR_DOTDOT(name) && !SP_GLOB_IS_DOT_OR_DOTDOT(pattern)) continue;
        if (name[0] == '.' && pattern[0] != '.' && seg_type != SP_GLOB_SEG_DOUBLESTAR) continue;

        SpPath full = sp_join_one(&it->priv_.dirs[depth], name);
        bool isdir = sp_is_dir(&full);
        bool matches_dir_constraint = !it->priv_.dir_only || isdir;

        if (seg_type == SP_GLOB_SEG_DOUBLESTAR) {
            /* ** : try matching next segment, then recurse into directories */
            if (!is_last) {
                const char *next_pattern = it->priv_.pattern_buf + it->priv_.seg_offsets[seg_idx + 1];
                int next_type = it->priv_.seg_types[seg_idx + 1];
                bool next_is_last = (seg_idx + 1 == it->priv_.seg_count - 1);
                if (sp_priv_glob_match(name, next_pattern, next_type, it->priv_.case_insensitive)) {
                    if (next_is_last && isdir) sp_priv_glob_push(it, &full, seg_idx);
                    if (next_is_last && matches_dir_constraint) { *out = full; return true; }
                    if (!next_is_last && isdir) {
                        size_t next_seg = seg_idx + 2;
                        bool next_ds = (it->priv_.seg_types[next_seg] == SP_GLOB_SEG_DOUBLESTAR) &&
                                       (next_seg == it->priv_.seg_count - 1);
                        sp_priv_glob_push(it, &full, next_seg);
                        if (next_ds) { *out = full; return true; }
                    }
                }
            }
            if (isdir) sp_priv_glob_push(it, &full, seg_idx);
            if (is_last && isdir) { *out = full; return true; }
        } else if (sp_priv_glob_match(name, pattern, seg_type, it->priv_.case_insensitive)) {
            if (is_last && matches_dir_constraint) { *out = full; return true; }
            if (!is_last && isdir) sp_priv_glob_push(it, &full, seg_idx + 1);
        }
    }
    return false;
}

void sp_glob_end(SpGlobIter *it) {
    if (it) while (it->depth >= 0) sp_priv_glob_pop(it);
}

SpGlobIter sp_rglob_begin(const SpPath *base, const char *pattern, SpCaseSensitivity cs) {
    if (!base || !pattern) { SpGlobIter it; memset(&it, 0, sizeof(it)); it.depth = -1; return it; }
    if (pattern[0] == '*' && pattern[1] == '*' &&
        (pattern[2] == '\0' || sp_priv_is_sep(pattern[2], base->flavor)))
        return sp_glob_begin(base, pattern, cs);

    char rglob_pattern[SP_GLOB_PATTERN_MAX];
    size_t pattern_len = strlen(pattern);
    if (pattern_len + 4 >= SP_GLOB_PATTERN_MAX) pattern_len = SP_GLOB_PATTERN_MAX - 4;
    rglob_pattern[0] = '*'; rglob_pattern[1] = '*'; rglob_pattern[2] = sp_priv_sep(base->flavor);
    memcpy(rglob_pattern + 3, pattern, pattern_len);
    rglob_pattern[3 + pattern_len] = '\0';
    return sp_glob_begin(base, rglob_pattern, cs);
}

/* ============ Home Directory and User Expansion Implementation ============ */

SpPath sp_home(SpFlavor flavor) {
    SP_ASSERT_FLAVOR(flavor);
    SpPath result = SP_PRIV_ZERO;
    result.flavor = flavor;

#ifdef SP_WINDOWS
    /* On Windows, use %USERPROFILE% environment variable */
    char *userprofile = getenv("USERPROFILE");
    if (userprofile) {
        size_t len = strlen(userprofile);
        if (len < SP_PATH_MAX) {
            memcpy(result.buf, userprofile, len);
            result.len = len;
            result.buf[result.len] = '\0';
            sp_priv_normalize(result.buf, &result.len, flavor);
            return result;
        }
    }
#else
    /* On POSIX, try HOME first, then getpwuid */
    char *home = getenv("HOME");
    if (home && *home) {
        size_t len = strlen(home);
        if (len < SP_PATH_MAX) {
            memcpy(result.buf, home, len);
            result.len = len;
            result.buf[result.len] = '\0';
            sp_priv_normalize(result.buf, &result.len, flavor);
            return result;
        }
    }
    /* Fall back to passwd entry */
    struct passwd *pw = getpwuid(getuid());
    if (pw) {
        const char *dir = pw->pw_dir;
        if (dir) {
            size_t len = strlen(dir);
            if (len < SP_PATH_MAX) {
                memcpy(result.buf, dir, len);
                result.len = len;
                result.buf[result.len] = '\0';
                sp_priv_normalize(result.buf, &result.len, flavor);
                return result;
            }
        }
    }
#endif
    /* Return empty path on failure */
    result.buf[0] = SP_ERR_OTHER;
    return result;
}

SpPath sp_expanduser(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpPath result = SP_PRIV_ZERO;
    result.flavor = p->flavor;

    /* If path doesn't start with ~, return copy */
    if (p->len == 0 || p->buf[0] != '~') {
        return sp_path_copy(p);
    }

    /* Check for ~ alone or ~/... */
    bool is_current_user = (p->len == 1) ||
                          (p->len > 1 && sp_priv_is_sep(p->buf[1], p->flavor));

    if (is_current_user) {
        /* Expand ~ to current user's home */
        SpPath home = sp_home(p->flavor);
        if (sp_path_is_error(&home)) {
            result.buf[0] = SP_ERR_OTHER;
            return result;
        }
        if (p->len == 1) {
            return home;
        }
        /* Append the rest of the path after ~/ (skip both ~ and separator) */
        const char *rest = p->buf + 2;
        size_t rest_len = p->len - 2;

        /* On Windows, if rest looks like a drive letter (e.g., "a:b" from "~/a:b"),
           prefix with "./" to prevent it from being interpreted as an absolute path.
           E.g., ~/a:b should become C:/Users/foo/a:b, not just a:b */
        if (sp_priv_is_windows_flavor(p->flavor) && sp_priv_has_drive(rest, rest_len, p->flavor)) {
            char protected_path[SP_PATH_MAX];
            protected_path[0] = '.';
            protected_path[1] = '/';
            size_t copy_len = rest_len < SP_PATH_MAX - 3 ? rest_len : SP_PATH_MAX - 3;
            memcpy(protected_path + 2, rest, copy_len);
            protected_path[2 + copy_len] = '\0';
            return sp_join_one(&home, protected_path);
        }

        return sp_join_n(&home, rest, rest_len);
    }

#ifndef SP_WINDOWS
    /* On POSIX, handle ~username */
    size_t end = 1;
    while (end < p->len && !sp_priv_is_sep(p->buf[end], p->flavor)) end++;
    char username[256];
    size_t ulen = end - 1;
    if (ulen >= sizeof(username)) {
        return sp_path_copy(p);  /* Username too long, return unchanged */
    }
    memcpy(username, p->buf + 1, ulen);
    username[ulen] = '\0';

    struct passwd *pw = getpwnam(username);
    if (!pw) {
        return sp_path_copy(p);  /* User not found, return unchanged */
    }
    const char *pw_dir = pw->pw_dir;
    if (!pw_dir) {
        return sp_path_copy(p);  /* No home dir, return unchanged */
    }
    size_t home_len = strlen(pw_dir);
    if (home_len >= SP_PATH_MAX) {
        return sp_path_copy(p);
    }
    memcpy(result.buf, pw_dir, home_len);
    result.len = home_len;

    /* Append the rest of the path */
    if (end < p->len) {
        if (result.len > 0 && !sp_priv_is_sep(result.buf[result.len - 1], p->flavor)) {
            sp_priv_append_sep(&result);
        }
        sp_priv_append_cstr(&result, p->buf + end, p->len - end);
    }
    result.buf[result.len] = '\0';
    sp_priv_normalize(result.buf, &result.len, result.flavor);
    return result;
#else
    /* On Windows, ~username is not commonly supported, return unchanged */
    return sp_path_copy(p);
#endif
}

/* ============ User/Group Info Implementation ============ */

SpTerm sp_owner(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
#ifdef SP_WINDOWS
    /* owner() is not implemented on Windows - Python bindings raise NotImplementedError */
    (void)p;
    return sp_priv_term(NULL, 0);
#else
    if (sp_priv_has_embedded_null(p)) return sp_priv_term(NULL, 0);
    SpStatResult st = sp_stat(p);
    if (!st.valid) return sp_priv_term(NULL, 0);

    struct passwd *pw = getpwuid(st.sp_uid);
    if (!pw) return sp_priv_term(NULL, 0);
    const char *name = pw->pw_name;
    if (!name) return sp_priv_term(NULL, 0);
    return sp_priv_term(name, strlen(name));
#endif
}

SpTerm sp_group(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
#ifdef SP_WINDOWS
    /* group() is not implemented on Windows - Python bindings raise NotImplementedError */
    (void)p;
    return sp_priv_term(NULL, 0);
#else
    if (sp_priv_has_embedded_null(p)) return sp_priv_term(NULL, 0);
    SpStatResult st = sp_stat(p);
    if (!st.valid) return sp_priv_term(NULL, 0);

    struct group *gr = getgrgid(st.sp_gid);
    if (!gr) return sp_priv_term(NULL, 0);
    const char *name = gr->gr_name;
    if (!name) return sp_priv_term(NULL, 0);
    return sp_priv_term(name, strlen(name));
#endif
}

/* ============ Directory Iteration Implementation ============ */

SpIterdirIter sp_iterdir_begin(const SpPath *p) {
    SpIterdirIter it;
    memset(&it, 0, sizeof(it));
    it.done = -1;  /* Default to error */

    if (!p) return it;
    SP_ASSERT_PATH_INVARIANT(p);

    if (sp_priv_has_embedded_null(p)) return it;
    it.dir = sp_path_copy(p);

#ifdef SP_WINDOWS
    /* Build search pattern: dir\* */
    char search[SP_PATH_MAX + 3];
    const char *dir_str = sp_str(&it.dir);
    size_t len = it.dir.len;
    if (len == 0) {
        search[0] = '.'; search[1] = '\\'; search[2] = '*'; search[3] = '\0';
    } else {
        memcpy(search, dir_str, len);
        if (!sp_priv_is_sep(search[len - 1], it.dir.flavor)) search[len++] = '\\';
        search[len++] = '*'; search[len] = '\0';
    }
    WIN32_FIND_DATAA *fd = (WIN32_FIND_DATAA *)it.priv_.find_data;
    it.priv_.handle = FindFirstFileA(search, fd);
    if (it.priv_.handle == INVALID_HANDLE_VALUE) {
        it.priv_.handle = NULL;
        return it;
    }
    it.priv_.first = true;
    it.done = 0;
#else
    const char *dir_str = sp_str(&it.dir);
    it.priv_.handle = opendir(dir_str);
    if (!it.priv_.handle) return it;
    it.done = 0;
#endif
    return it;
}

bool sp_iterdir_next(SpIterdirIter *it, SpPath *out) {
    if (!it || it->done != 0 || !it->priv_.handle) return false;

#ifdef SP_WINDOWS
    WIN32_FIND_DATAA *fd = (WIN32_FIND_DATAA *)it->priv_.find_data;
    while (true) {
        const char *name;
        if (it->priv_.first) {
            it->priv_.first = false;
            name = fd->cFileName;
        } else {
            if (!FindNextFileA(it->priv_.handle, fd)) {
                it->done = 1;
                return false;
            }
            name = fd->cFileName;
        }
        /* Skip . and .. */
        if (name[0] == '.') {
            if (name[1] == '\0') continue;
            if (name[1] == '.' && name[2] == '\0') continue;
        }
        *out = sp_join_one(&it->dir, name);
        return true;
    }
#else
    while (true) {
        struct dirent *de = readdir(SP_PRIV_CAST(DIR *, it->priv_.handle));
        if (!de) {
            it->done = 1;
            return false;
        }
        const char *name = de->d_name;
        /* Skip . and .. */
        if (name[0] == '.') {
            if (name[1] == '\0') continue;
            if (name[1] == '.' && name[2] == '\0') continue;
        }
        *out = sp_join_one(&it->dir, name);
        return true;
    }
#endif
}

void sp_iterdir_end(SpIterdirIter *it) {
    if (!it || !it->priv_.handle) return;
#ifdef SP_WINDOWS
    FindClose(it->priv_.handle);
#else
    closedir(SP_PRIV_CAST(DIR *, it->priv_.handle));
#endif
    it->priv_.handle = NULL;
    it->done = 1;
}

/* ============ Walk Implementation (recursive, callback-based) ============ */

/* Comparison function for qsort - sort name strings */
static int sp_priv_walk_name_cmp(const void *a, const void *b) {
    return strcmp(SP_PRIV_CAST(const char *, a), SP_PRIV_CAST(const char *, b));
}

/* Helper to copy name into walk name buffer */
static void sp_priv_walk_copy_name(char dest[SP_WALK_NAME_MAX], const char *src) {
    size_t len = strlen(src);
    if (len >= SP_WALK_NAME_MAX) len = SP_WALK_NAME_MAX - 1;
    memcpy(dest, src, len);
    dest[len] = '\0';
}

/* Scan directory and populate entry with dirnames/filenames (names only) */
static bool sp_priv_walk_scan(const SpPath *dir, bool follow_symlinks,
                              char dirnames[][SP_WALK_NAME_MAX], size_t *dirname_count,
                              char filenames[][SP_WALK_NAME_MAX], size_t *filename_count) {
    *dirname_count = 0;
    *filename_count = 0;

#ifdef SP_WINDOWS
    (void)follow_symlinks;  /* Not used on Windows - FindFirstFile handles reparse points */
    char search[SP_PATH_MAX + 3];
    const char *dir_str = sp_str(dir);
    size_t len = dir->len;
    if (len == 0) {
        search[0] = '.'; search[1] = '\\'; search[2] = '*'; search[3] = '\0';
    } else {
        memcpy(search, dir_str, len);
        if (!sp_priv_is_sep(search[len - 1], dir->flavor)) search[len++] = '\\';
        search[len++] = '*'; search[len] = '\0';
    }
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(search, &fd);
    if (h == INVALID_HANDLE_VALUE) return false;

    do {
        const char *name = fd.cFileName;
        if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) continue;
        bool is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (is_dir) {
            if (*dirname_count < SP_WALK_MAX_ENTRIES)
                sp_priv_walk_copy_name(dirnames[(*dirname_count)++], name);
        } else {
            if (*filename_count < SP_WALK_MAX_ENTRIES)
                sp_priv_walk_copy_name(filenames[(*filename_count)++], name);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR *d = opendir(sp_str(dir));
    if (!d) return false;

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        const char *name = de->d_name;
        if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) continue;
        bool is_dir = false;

#ifdef DT_DIR
        if (de->d_type != DT_UNKNOWN) {
            if (de->d_type == DT_LNK) {
                SpPath child = sp_join_one(dir, name);
                SpStatResult st = follow_symlinks ? sp_stat(&child) : sp_lstat(&child);
                is_dir = st.valid && S_ISDIR(st.sp_mode);
            } else {
                is_dir = (de->d_type == DT_DIR);
            }
        } else
#endif
        {
            SpPath child = sp_join_one(dir, name);
            SpStatResult st = follow_symlinks ? sp_stat(&child) : sp_lstat(&child);
            is_dir = st.valid && S_ISDIR(st.sp_mode);
        }

        if (is_dir) {
            if (*dirname_count < SP_WALK_MAX_ENTRIES)
                sp_priv_walk_copy_name(dirnames[(*dirname_count)++], name);
        } else {
            if (*filename_count < SP_WALK_MAX_ENTRIES)
                sp_priv_walk_copy_name(filenames[(*filename_count)++], name);
        }
    }
    closedir(d);
#endif

    /* Sort for consistent ordering */
    if (*dirname_count > 1)
        qsort(dirnames, *dirname_count, SP_WALK_NAME_MAX, sp_priv_walk_name_cmp);
    if (*filename_count > 1)
        qsort(filenames, *filename_count, SP_WALK_NAME_MAX, sp_priv_walk_name_cmp);

    return true;
}

/* Recursive walk implementation */
static bool sp_priv_walk_recursive(const SpPath *dir, bool top_down, bool follow_symlinks,
                                   SpWalkFn callback, SpWalkErrorFn on_error, void *user_data) {
    /* Stack-allocated arrays for this directory level */
    char dirnames[SP_WALK_MAX_ENTRIES][SP_WALK_NAME_MAX];
    char filenames[SP_WALK_MAX_ENTRIES][SP_WALK_NAME_MAX];
    size_t dirname_count, filename_count;

    /* Scan current directory */
    if (!sp_priv_walk_scan(dir, follow_symlinks, dirnames, &dirname_count, filenames, &filename_count)) {
        if (on_error) on_error(dir, errno, user_data);
        return true;  /* Continue walking other branches */
    }

    /* Build entry for callback */
    SpWalkEntry entry;
    entry.dirpath = *dir;
    entry.dirnames = dirnames;
    entry.dirname_count = dirname_count;
    entry.filenames = filenames;
    entry.filename_count = filename_count;
    entry.user_data = user_data;

    if (top_down) {
        /* Top-down: call callback first, then recurse */
        if (!callback(&entry)) return false;  /* User requested stop */

        /* Recurse into subdirectories (entry.dirname_count may have been modified for pruning) */
        for (size_t i = 0; i < entry.dirname_count; i++) {
            SpPath subdir = sp_join_one(dir, entry.dirnames[i]);
            if (!sp_priv_walk_recursive(&subdir, top_down, follow_symlinks, callback, on_error, user_data))
                return false;
        }
    } else {
        /* Bottom-up: recurse first, then call callback */
        for (size_t i = 0; i < dirname_count; i++) {
            SpPath subdir = sp_join_one(dir, dirnames[i]);
            if (!sp_priv_walk_recursive(&subdir, top_down, follow_symlinks, callback, on_error, user_data))
                return false;
        }

        /* Re-scan to get fresh counts (directory may have changed) */
        sp_priv_walk_scan(dir, follow_symlinks, dirnames, &dirname_count, filenames, &filename_count);
        entry.dirname_count = dirname_count;
        entry.filename_count = filename_count;

        if (!callback(&entry)) return false;
    }

    return true;
}

bool sp_walk(const SpPath *p, bool top_down, bool follow_symlinks,
             SpWalkFn callback, SpWalkErrorFn on_error, void *user_data) {
    if (!p || !callback) return false;
    SP_ASSERT_PATH_INVARIANT(p);
    if (sp_priv_has_embedded_null(p)) return false;
    if (!sp_is_dir(p)) return false;

    return sp_priv_walk_recursive(p, top_down, follow_symlinks, callback, on_error, user_data);
}

/* ============ BYOS Walk Iterator Implementation ============ */

/* Pending entry format: byte 0 = marker (0=needs scan, 1=ready to yield), bytes 1+ = path */
#define SP_WALK_MARKER_SCAN 0
#define SP_WALK_MARKER_YIELD 1

/* Push a path onto the pending stack with marker */
static bool sp_priv_walk_push_ex(SpWalkIter *it, const SpPath *path, int marker) {
    if (it->priv_.pending_count >= it->priv_.pending_capacity) return false;
    char *slot = it->priv_.pending_buf + (it->priv_.pending_count * SP_PATH_MAX);
    slot[0] = SP_PRIV_CAST(char, marker);
    size_t len = path->len;
    if (len >= SP_PATH_MAX - 1) len = SP_PATH_MAX - 2;
    memcpy(slot + 1, path->buf, len);
    slot[len + 1] = '\0';
    it->priv_.pending_count++;
    return true;
}

/* Push a path for scanning (default behavior) */
static bool sp_priv_walk_push(SpWalkIter *it, const SpPath *path) {
    return sp_priv_walk_push_ex(it, path, SP_WALK_MARKER_SCAN);
}

/* Pop a path from the pending stack, returns marker via out_marker */
static bool sp_priv_walk_pop_ex(SpWalkIter *it, SpPath *out, int *out_marker) {
    if (it->priv_.pending_count == 0) return false;
    it->priv_.pending_count--;
    char *slot = it->priv_.pending_buf + (it->priv_.pending_count * SP_PATH_MAX);
    *out_marker = SP_PRIV_CAST(unsigned char, slot[0]);
    *out = sp_path_f(slot + 1, it->priv_.flavor);
    return true;
}

bool sp_walk_begin(SpWalkIter *it, const SpPath *p, bool top_down, bool follow_symlinks,
                   void *pending_buf, size_t pending_buf_size,
                   SpWalkErrorFn on_error, void *user_data) {
    /* Fast-fail validation */
    if (!it) return false;
    memset(it, 0, sizeof(*it));
    it->priv_.state = -1;  /* Mark as done initially */

    if (!p) return false;
    SP_ASSERT_PATH_INVARIANT(p);
    if (sp_priv_has_embedded_null(p)) return false;
    if (!sp_is_dir(p)) return false;
    if (!pending_buf) return false;
    if (pending_buf_size < SP_PATH_MAX) return false;

    /* Initialize iterator state */
    it->priv_.top_down = top_down;
    it->priv_.follow_symlinks = follow_symlinks;
    it->priv_.on_error = on_error;
    it->priv_.user_data = user_data;
    it->priv_.pending_buf = SP_PRIV_CAST(char *, pending_buf);
    it->priv_.pending_capacity = pending_buf_size / SP_PATH_MAX;
    it->priv_.pending_count = 0;
    it->priv_.state = 0;  /* need_scan */
    it->priv_.subdir_index = 0;
    it->priv_.flavor = p->flavor;

    /* Push initial directory onto pending stack */
    if (!sp_priv_walk_push(it, p)) {
        it->priv_.state = -1;
        return false;
    }

    return true;
}

bool sp_walk_next(SpWalkIter *it) {
    if (!it || it->priv_.state == -1) return false;

    /* State machine:
     * 0 = processing: pop from pending and handle based on marker
     * 1 = yielded_topdown: user saw it, now push subdirs and go to next
     */
    while (true) {
        if (it->priv_.state == 0) {
            int marker;
            if (!sp_priv_walk_pop_ex(it, &it->dirpath, &marker)) {
                it->priv_.state = -1;
                return false;  /* Done */
            }

            if (marker == SP_WALK_MARKER_YIELD) {
                /* Bottom-up: this directory's subdirs are done, yield it now */
                /* Re-scan to get current state (matches Python behavior) */
                sp_priv_walk_scan(&it->dirpath, it->priv_.follow_symlinks,
                                  it->dirnames, &it->dirname_count,
                                  it->filenames, &it->filename_count);
                return true;
            }

            /* marker == SP_WALK_MARKER_SCAN: scan this directory */
            if (!sp_priv_walk_scan(&it->dirpath, it->priv_.follow_symlinks,
                                   it->dirnames, &it->dirname_count,
                                   it->filenames, &it->filename_count)) {
                if (it->priv_.on_error)
                    it->priv_.on_error(&it->dirpath, errno, it->priv_.user_data);
                continue;  /* Skip unreadable directories */
            }

            if (it->priv_.top_down) {
                it->priv_.state = 1;  /* Yielded, waiting for subdirs */
                return true;  /* Yield to user */
            } else {
                /* Bottom-up: push self for later yield, then push subdirs for scan */
                sp_priv_walk_push_ex(it, &it->dirpath, SP_WALK_MARKER_YIELD);
                for (size_t i = it->dirname_count; i > 0; i--) {
                    SpPath subdir = sp_join_one(&it->dirpath, it->dirnames[i - 1]);
                    sp_priv_walk_push_ex(it, &subdir, SP_WALK_MARKER_SCAN);
                }
                /* Continue to process next (which will be first subdir or self-yield) */
            }
        } else if (it->priv_.state == 1) {
            /* Top-down: push remaining subdirs in reverse order (user may have modified dirname_count) */
            for (size_t i = it->dirname_count; i > 0; i--) {
                SpPath subdir = sp_join_one(&it->dirpath, it->dirnames[i - 1]);
                sp_priv_walk_push(it, &subdir);
            }
            it->priv_.state = 0;  /* Go process next */
        }
    }
}

void sp_walk_end(SpWalkIter *it) {
    if (!it) return;
    it->priv_.state = -1;
    it->priv_.pending_count = 0;
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
    X(exists, bool, sp_exists(&sp_priv_f_ctx))                                                                         \
    X(is_symlink, bool, sp_is_symlink(&sp_priv_f_ctx))                                                                 \
    X(is_block_device, bool, sp_is_block_device(&sp_priv_f_ctx))                                                       \
    X(is_char_device, bool, sp_is_char_device(&sp_priv_f_ctx))                                                         \
    X(is_fifo, bool, sp_is_fifo(&sp_priv_f_ctx))                                                                       \
    X(is_socket, bool, sp_is_socket(&sp_priv_f_ctx))                                                                   \
    X(is_mount, bool, sp_is_mount(&sp_priv_f_ctx))                                                                     \
    X(is_junction, bool, sp_is_junction(&sp_priv_f_ctx))
#define SP_FLUENT_TERM_TERM(X)                                                                                         \
    X(name, SpTerm, sp_name) X(stem, SpTerm, sp_stem) X(suffix, SpTerm, sp_suffix) X(drive, SpTerm, sp_drive)          \
        X(root, SpTerm, sp_root) X(anchor, SpTerm, sp_anchor) X(owner, SpTerm, sp_owner) X(group, SpTerm, sp_group)
#define SP_FLUENT_CHAIN_VOID(X) X(parent, sp_parent) X(absolute, sp_absolute) X(expanduser, sp_expanduser)
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
SP_FLUENT_TERM_TERM(SP_GEN_TERM_STR)
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

static SpPrivDontUseThisDirectly_ sp_priv_f_instance = {
    sp_priv_f_path_,
    sp_priv_f_name_,
    sp_priv_f_stem_,
    sp_priv_f_suffix_,
    sp_priv_f_suffixes_,
    sp_priv_f_drive_,
    sp_priv_f_root_,
    sp_priv_f_anchor_,
    sp_priv_f_owner_,
    sp_priv_f_group_,
    sp_priv_f_str_,
    sp_priv_f_is_absolute_,
    sp_priv_f_is_relative_to_,
    sp_priv_f_is_file_,
    sp_priv_f_is_dir_,
    sp_priv_f_exists_,
    sp_priv_f_is_symlink_,
    sp_priv_f_is_block_device_,
    sp_priv_f_is_char_device_,
    sp_priv_f_is_fifo_,
    sp_priv_f_is_socket_,
    sp_priv_f_is_mount_,
    sp_priv_f_is_junction_,
    sp_priv_f_parent_,
    sp_priv_f_join_,
    sp_priv_f_with_name_,
    sp_priv_f_with_stem_,
    sp_priv_f_with_suffix_,
    sp_priv_f_absolute_,
    sp_priv_f_expanduser_,
    sp_priv_f_relative_to_,
    sp_priv_f_relative_to_walk_up_
};

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
