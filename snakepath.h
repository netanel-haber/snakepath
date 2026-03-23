/* snakepath.h - C99 pathlib port, STB-style header-only library
 * No mallocs. POSIX and Windows compatible.
 * Note: OS functions (opendir/closedir, stat, getcwd, etc.) may allocate internally.
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

#define SP_PATH_MAX_WINDOWS 1024  /* Windows has 1MB default stack; larger values may cause stack overflow. Use /STACK linker flag to increase. */
#define SP_PATH_MAX_LINUX 4096    /* Linux PATH_MAX; typical 8MB stack handles this fine */

#ifndef SP_PATH_MAX
#error "SP_PATH_MAX must be defined before including snakepath.h. " \
       "Use: #define SP_PATH_MAX SP_PATH_MAX_WINDOWS (1024) or SP_PATH_MAX_LINUX (4096)"
#endif

#ifndef SP_MAX_SUFFIXES
#define SP_MAX_SUFFIXES 16
#endif

#define SP_ERR_NONE '\x00'         /* No error (or empty path) */
#define SP_ERR_NOT_RELATIVE '\x01' /* Not relative to other path */
#define SP_ERR_NO_NAME '\x02'      /* Path has no usable name */
#define SP_ERR_INVALID_ARG '\x03'  /* Invalid argument (name/stem/suffix) */
#define SP_ERR_OTHER '\x04'        /* Other error (I/O, permission, etc.) */

#define SP_MATCH_YES 1          /* Pattern matched */
#define SP_MATCH_NO 0           /* Pattern did not match */
#define SP_MATCH_ERR_EMPTY -1   /* Empty pattern */
#define SP_MATCH_ERR_INVALID -2 /* Invalid pattern (. or ..) */

#if defined(_WIN32) || defined(_WIN64)
#define SP_WINDOWS 1
#else
#define SP_POSIX 1
#endif

typedef enum { SP_FLAVOR_NATIVE = 0, SP_FLAVOR_POSIX, SP_FLAVOR_WINDOWS } SpFlavor;

typedef enum { SP_CASE_PLATFORM_DEFAULT = 0, SP_CASE_SENSITIVE, SP_CASE_INSENSITIVE } SpCaseSensitivity;

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

typedef struct {
    const char *data;
    size_t len;
} SpStr;

#ifndef SP_TERM_MAX
#define SP_TERM_MAX 256
#endif
typedef struct {
    char buf[SP_TERM_MAX];
    size_t len;
} SpTerm;

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

typedef struct {
    char buf[SP_PATH_MAX];
    size_t len;
    SpFlavor flavor;
} SpPath;

typedef struct {
    const SpPath *path;
    size_t pos;
    bool include_anchor;
    bool anchor_done;
} SpPartsIter;

typedef struct {
    SpStr items[SP_MAX_SUFFIXES];
    size_t count;
} SpSuffixes;

typedef struct {
    const SpPath *path;
    size_t current_len;
    bool done;
} SpParentsIter;

typedef struct {
    SpPath dir;
    int done;
    struct {
        void *handle;
    } priv_;
} SpIterdirIter;

SpIterdirIter sp_iterdir_begin(const SpPath *p);
bool sp_iterdir_next(SpIterdirIter *it, SpPath *out);  /* returns child path */
void sp_iterdir_end(SpIterdirIter *it);

#define SP_ITERDIR_FOREACH(dir, entry_var) \
    for (struct { SpIterdirIter it; int done; } sp_ictx_ = { sp_iterdir_begin(dir), 0 }; \
         !sp_ictx_.done; sp_iterdir_end(&sp_ictx_.it), sp_ictx_.done = 1) \
    for (SpPath entry_var; sp_iterdir_next(&sp_ictx_.it, &entry_var); )

#ifndef SP_GLOB_MAX_DEPTH
#define SP_GLOB_MAX_DEPTH 32
#endif
#ifndef SP_GLOB_MAX_SEGMENTS
#define SP_GLOB_MAX_SEGMENTS 64
#endif
#ifndef SP_GLOB_PATTERN_MAX
#define SP_GLOB_PATTERN_MAX 256
#endif

typedef struct {
    int depth;
    struct {
        char pattern_buf[SP_GLOB_PATTERN_MAX];
        size_t seg_offsets[SP_GLOB_MAX_SEGMENTS];
        int seg_types[SP_GLOB_MAX_SEGMENTS];
        size_t seg_count;
        bool dir_only;
        bool case_insensitive;
        bool yield_base_pending;
        SpFlavor flavor;
        SpPath current_dir;
        struct { void *handle; size_t path_len; } stack[SP_GLOB_MAX_DEPTH];
        size_t seg_idxs[SP_GLOB_MAX_DEPTH];
    } priv_;
} SpGlobIter;

typedef struct {
    SpFlavor flavor;
} SpPathOpts;
#define sp_path(s) sp_path_new((s), SP_PRIV_OPTS(SP_FLAVOR_NATIVE))
#define sp_path_f(s, f) sp_path_new((s), SP_PRIV_OPTS(f))

/* Join paths: sp_join(p, "a", "b", "c") - C only, use sp_join_one in C++ */
#ifndef __cplusplus
#define sp_join(base, ...) sp_join_impl((base), (const char *[]){__VA_ARGS__, NULL})
#endif

#define sp_eq(a, b) sp_path_eq(&(a), &(b))

#define sp_sv(s) SP_PRIV_STR((s), sizeof(s) - 1)
#define sp_sv_from(s, n) SP_PRIV_STR((s), (n))

#define SP_ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

SpPath sp_path_new(const char *s, SpPathOpts opts);
SpPath sp_path_from_n(const char *s, size_t len, SpFlavor flavor);
SpPath sp_path_convert(const char *s, SpFlavor src_flavor, SpFlavor dest_flavor);
static inline SpPath sp_path_copy(const SpPath *p) { SP_ASSERT_PATH_INVARIANT(p); return *p; }

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

SpPath sp_with_segments(const SpPath *p, const char **parts, size_t parts_count);
SpPath sp_with_name(const SpPath *p, const char *name);
SpPath sp_with_stem(const SpPath *p, const char *stem);
SpPath sp_with_suffix(const SpPath *p, const char *suffix);

SpPath sp_relative_to(const SpPath *p, const SpPath *other);
SpPath sp_relative_to_walk_up(const SpPath *p, const SpPath *other);
bool sp_is_relative_to(const SpPath *p, const SpPath *other);
SpPath sp_relative_to_parts(const SpPath *p, const char **parts, bool walk_up);
bool sp_is_relative_to_parts(const SpPath *p, const char **parts);

bool sp_is_absolute(const SpPath *p);
SpPath sp_cwd(SpFlavor flavor);
SpPath sp_absolute(const SpPath *p);
size_t sp_as_uri(const SpPath *p, char *buf, size_t buf_size);
bool sp_path_eq(const SpPath *a, const SpPath *b);
static inline bool sp_path_ne(const SpPath *a, const SpPath *b) { return !sp_path_eq(a, b); }
int sp_path_cmp(const SpPath *a, const SpPath *b);
unsigned long sp_path_hash(const SpPath *p);
int sp_match_ex(const SpPath *p, const char *pattern, int case_sensitive); /* Returns SP_MATCH_* codes */
#define SP_MATCH(p, pattern) sp_match_ex((p), (pattern), -1)
bool sp_is_reserved(const SpPath *p);
#define SP_FILE_TYPE_QUERIES(X)        \
    X(file, S_IFREG, true)             \
    X(dir, S_IFDIR, true)              \
    X(symlink, S_IFLNK, false)         \
    X(block_device, S_IFBLK, true)     \
    X(char_device, S_IFCHR, true)      \
    X(fifo, S_IFIFO, true)             \
    X(socket, S_IFSOCK, true)
#define SP_DECLARE_FILE_TYPE_QUERY(name, type_mask, follow_symlinks) bool sp_is_##name(const SpPath *p);
SP_FILE_TYPE_QUERIES(SP_DECLARE_FILE_TYPE_QUERY)
#undef SP_DECLARE_FILE_TYPE_QUERY
bool sp_exists(const SpPath *p);
bool sp_is_mount(const SpPath *p);
bool sp_is_junction(const SpPath *p);

typedef struct {
    unsigned int sp_mode;
    unsigned long long sp_ino;
    unsigned long long sp_dev;
    unsigned long long sp_nlink;
    unsigned int sp_uid;
    unsigned int sp_gid;
    long long sp_size;
    double sp_atime;
    double sp_mtime;
    double sp_ctime;
    long long sp_atime_ns;
    long long sp_mtime_ns;
    long long sp_ctime_ns;
    bool valid;
} SpStatResult;

SpStatResult sp_stat(const SpPath *p);   /* follows symlinks */
SpStatResult sp_lstat(const SpPath *p);  /* does not follow symlinks */
bool sp_stat_eq(const SpStatResult *a, const SpStatResult *b);
size_t sp_parents_count(const SpPath *p);

SpPath sp_readlink(const SpPath *p);
SpPath sp_resolve(const SpPath *p, bool strict);
bool sp_symlink_to(const SpPath *p, const SpPath *target, bool target_is_directory);
bool sp_hardlink_to(const SpPath *p, const SpPath *target);
bool sp_samefile(const SpPath *a, const SpPath *b);

#define SP_MKDIR_OK 0
#define SP_MKDIR_ERR_EXISTS 1
#define SP_MKDIR_ERR_NOT_FOUND 2
#define SP_MKDIR_ERR_NOT_DIR 3
#define SP_MKDIR_ERR_PERMISSION 4
#define SP_MKDIR_ERR_OTHER 5
#define SP_MKDIR_ERR_EXISTS_NOT_DIR 6
#define SP_MKDIR_DEF_MODE 0777

int sp_mkdir(const SpPath *p, unsigned int mode, bool parents, bool exist_ok);

enum {
    SP_OK = 0,
    SP_ERR,
    SP_ERR_EXISTS,
    SP_ERR_NOT_FOUND,
    SP_ERR_NOT_DIR,
    SP_ERR_PERMISSION,
    SP_ERR_EXISTS_NOT_DIR,
    SP_ERR_OPEN,
    SP_ERR_READ,
    SP_ERR_WRITE,
    SP_ERR_TOO_LARGE,
    SP_ERR_OTHER_OP
};

const char *sp_error_str(int error);

typedef struct { SpPath path; int error; } SpPathOp;

bool sp_touch(const SpPath *p, unsigned int mode, bool exist_ok);
bool sp_unlink(const SpPath *p, bool missing_ok);
bool sp_rmdir(const SpPath *p);
SpPath sp_rename(const SpPath *p, const SpPath *target);
SpPath sp_replace(const SpPath *p, const SpPath *target);
bool sp_chmod(const SpPath *p, unsigned int mode);

typedef struct { size_t bytes; int error; } SpIOResult;
#define SP_IO_OK            0
#define SP_IO_ERR_OPEN      1
#define SP_IO_ERR_READ      2
#define SP_IO_ERR_WRITE     3
#define SP_IO_ERR_TOO_LARGE 4

SpIOResult sp_read_file(const SpPath *p, char *buf, size_t buf_size);
SpIOResult sp_write_file(const SpPath *p, const char *data, size_t data_len);

SpPath sp_home(SpFlavor flavor);
SpPath sp_expanduser(const SpPath *p);

SpTerm sp_owner(const SpPath *p);
SpTerm sp_group(const SpPath *p);

#ifndef SP_WALK_MAX_ENTRIES
#define SP_WALK_MAX_ENTRIES 64
#endif
#ifndef SP_WALK_NAME_MAX
#define SP_WALK_NAME_MAX 128
#endif

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

/* Callback-based walk - processes entire tree, using real stack for unlimited depth.
 * Calls callback for each directory. Return false from callback to stop early.
 * For top-down pruning, modify entry->dirname_count before returning.
 */
bool sp_walk(const SpPath *p, bool top_down, bool follow_symlinks,
             SpWalkFn callback, SpWalkErrorFn on_error, void *user_data);

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
#ifdef __cplusplus
}
#endif

/* ============ Fluent API ============ */
#ifdef SNAKEPATH_FLUENT

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sp_fluent_ SpPrivDontUseThisDirectly_;

#define SP_F_TERMINATOR_METHODS(X_TERM, X_PROC)                                                                        \
    X_TERM(SpPath, path, (void), sp_priv_f_ctx)                                                                        \
    X_TERM(SpTerm, name, (void), sp_name(&sp_priv_f_ctx))                                                             \
    X_TERM(SpTerm, stem, (void), sp_stem(&sp_priv_f_ctx))                                                             \
    X_TERM(SpTerm, suffix, (void), sp_suffix(&sp_priv_f_ctx))                                                         \
    X_TERM(SpSuffixes, suffixes, (void), sp_suffixes(&sp_priv_f_ctx))                                                 \
    X_TERM(SpTerm, drive, (void), sp_drive(&sp_priv_f_ctx))                                                           \
    X_TERM(SpTerm, root, (void), sp_root(&sp_priv_f_ctx))                                                             \
    X_TERM(SpTerm, anchor, (void), sp_anchor(&sp_priv_f_ctx))                                                         \
    X_TERM(SpTerm, owner, (void), sp_owner(&sp_priv_f_ctx))                                                           \
    X_TERM(SpTerm, group, (void), sp_group(&sp_priv_f_ctx))                                                           \
    X_TERM(bool, is_absolute, (void), sp_is_absolute(&sp_priv_f_ctx))                                                 \
    X_TERM(bool, is_relative_to, (const SpPath *o), sp_is_relative_to(&sp_priv_f_ctx, o))                            \
    X_TERM(bool, is_file, (void), sp_is_file(&sp_priv_f_ctx))                                                         \
    X_TERM(bool, is_dir, (void), sp_is_dir(&sp_priv_f_ctx))                                                           \
    X_TERM(bool, exists, (void), sp_exists(&sp_priv_f_ctx))                                                           \
    X_TERM(bool, is_symlink, (void), sp_is_symlink(&sp_priv_f_ctx))                                                   \
    X_TERM(bool, is_block_device, (void), sp_is_block_device(&sp_priv_f_ctx))                                         \
    X_TERM(bool, is_char_device, (void), sp_is_char_device(&sp_priv_f_ctx))                                           \
    X_TERM(bool, is_fifo, (void), sp_is_fifo(&sp_priv_f_ctx))                                                         \
    X_TERM(bool, is_socket, (void), sp_is_socket(&sp_priv_f_ctx))                                                     \
    X_TERM(bool, is_mount, (void), sp_is_mount(&sp_priv_f_ctx))                                                       \
    X_TERM(bool, is_junction, (void), sp_is_junction(&sp_priv_f_ctx))                                                 \
    X_TERM(bool, is_reserved, (void), sp_is_reserved(&sp_priv_f_ctx))                                                 \
    X_TERM(SpStatResult, stat, (void), sp_stat(&sp_priv_f_ctx))                                                       \
    X_TERM(SpStatResult, lstat, (void), sp_lstat(&sp_priv_f_ctx))                                                     \
    X_TERM(bool, eq, (const SpPath *o), sp_path_eq(&sp_priv_f_ctx, o))                                                \
    X_TERM(bool, ne, (const SpPath *o), sp_path_ne(&sp_priv_f_ctx, o))                                                \
    X_TERM(bool, samefile, (const SpPath *o), sp_samefile(&sp_priv_f_ctx, o))                                         \
    X_TERM(SpIOResult, read_file, (char *buf, size_t buf_size), sp_read_file(&sp_priv_f_ctx, buf, buf_size))         \
    X_TERM(SpIOResult, write_file, (const char *data, size_t data_len), sp_write_file(&sp_priv_f_ctx, data, data_len)) \
    X_PROC(as_posix, (char *out, size_t out_size), sp_as_posix(&sp_priv_f_ctx, out, out_size))                       \
    X_TERM(size_t, as_uri, (char *buf, size_t buf_size), sp_as_uri(&sp_priv_f_ctx, buf, buf_size))                   \
    X_TERM(int, match, (const char *pattern), sp_match_ex(&sp_priv_f_ctx, pattern, -1))                              \
    X_TERM(SpPathOp, mkdir, (unsigned int mode, bool parents, bool exist_ok),                                         \
           sp_priv_f_pathop(sp_priv_mkdir_to_error(sp_mkdir(&sp_priv_f_ctx, mode, parents, exist_ok))))              \
    X_TERM(SpPathOp, touch, (unsigned int mode, bool exist_ok),                                                        \
           sp_priv_f_pathop(sp_touch(&sp_priv_f_ctx, mode, exist_ok) ? SP_OK : SP_ERR))                              \
    X_TERM(SpPathOp, unlink, (bool missing_ok), sp_priv_f_pathop(sp_unlink(&sp_priv_f_ctx, missing_ok) ? SP_OK : SP_ERR)) \
    X_TERM(SpPathOp, rmdir, (void), sp_priv_f_pathop(sp_rmdir(&sp_priv_f_ctx) ? SP_OK : SP_ERR))                     \
    X_TERM(SpPathOp, chmod, (unsigned int mode), sp_priv_f_pathop(sp_chmod(&sp_priv_f_ctx, mode) ? SP_OK : SP_ERR)) \
    X_TERM(SpPathOp, symlink_to, (const SpPath *target, bool target_is_directory),                                    \
           sp_priv_f_pathop(sp_symlink_to(&sp_priv_f_ctx, target, target_is_directory) ? SP_OK : SP_ERR))            \
    X_TERM(SpPathOp, hardlink_to, (const SpPath *target),                                                              \
           sp_priv_f_pathop(sp_hardlink_to(&sp_priv_f_ctx, target) ? SP_OK : SP_ERR))

#define SP_F_CHAIN_METHODS(X)                                                                                          \
    X(parent, (void), sp_parent(&sp_priv_f_ctx))                                                                       \
    X(join, (const char *s), sp_join_one(&sp_priv_f_ctx, s))                                                           \
    X(with_segments, (const char **parts, size_t parts_count), sp_with_segments(&sp_priv_f_ctx, parts, parts_count))  \
    X(with_name, (const char *s), sp_with_name(&sp_priv_f_ctx, s))                                                     \
    X(with_stem, (const char *s), sp_with_stem(&sp_priv_f_ctx, s))                                                     \
    X(with_suffix, (const char *s), sp_with_suffix(&sp_priv_f_ctx, s))                                                 \
    X(absolute, (void), sp_absolute(&sp_priv_f_ctx))                                                                   \
    X(expanduser, (void), sp_expanduser(&sp_priv_f_ctx))                                                               \
    X(relative_to, (const SpPath *o), sp_relative_to(&sp_priv_f_ctx, o))                                               \
    X(relative_to_walk_up, (const SpPath *o), sp_relative_to_walk_up(&sp_priv_f_ctx, o))                              \
    X(readlink, (void), sp_readlink(&sp_priv_f_ctx))                                                                   \
    X(resolve, (bool strict), sp_resolve(&sp_priv_f_ctx, strict))                                                      \
    X(rename, (const SpPath *target), sp_rename(&sp_priv_f_ctx, target))                                               \
    X(replace, (const SpPath *target), sp_replace(&sp_priv_f_ctx, target))

struct sp_fluent_ {
    /* Terminators - end chain and return value */
#define SP_F_TERM_FIELD(ret, name, params, expr) ret (*name) params;
#define SP_F_PROC_FIELD(name, params, expr) void (*name) params;
    SP_F_TERMINATOR_METHODS(SP_F_TERM_FIELD, SP_F_PROC_FIELD)
#undef SP_F_PROC_FIELD
#undef SP_F_TERM_FIELD
    /* Chainable - return pointer to avoid stack copies */
#define SP_F_CHAIN_FIELD(name, params, expr) SpPrivDontUseThisDirectly_ *(*name) params;
    SP_F_CHAIN_METHODS(SP_F_CHAIN_FIELD)
#undef SP_F_CHAIN_FIELD
};

#ifndef SNAKEPATH_IMPLEMENTATION
#undef SP_F_CHAIN_METHODS
#undef SP_F_TERMINATOR_METHODS
#endif

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

#ifdef SNAKEPATH_IMPLEMENTATION

#include <stdio.h>

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

static inline SpPath sp_priv_error_path(char err_code) {
    SpPath p = SP_PRIV_ZERO;
    p.buf[0] = err_code;
    return p;
}

static inline SpPath sp_priv_error_path_f(SpFlavor flavor, char err_code) {
    SpPath p = sp_priv_error_path(err_code);
    p.flavor = flavor;
    return p;
}

static inline size_t sp_priv_parent_len_raw(const char *buf, size_t len, SpFlavor flavor) {
    if (len == 0) return 0;
    size_t anchor = sp_priv_anchor_len(buf, len, flavor);
    if (len <= anchor) return len;
    size_t i = len;
    while (i > anchor && !sp_priv_is_sep(buf[i - 1], flavor)) i--;
    if (i > anchor) i--;
    if (i == 0 && anchor == 0) return 0;
    if (i <= anchor) i = anchor;
    return i;
}

static inline void sp_priv_parent_path(const SpPath *p, SpPath *out) {
    size_t parent_len = sp_priv_parent_len_raw(p->buf, p->len, p->flavor);
    out->flavor = p->flavor;
    out->len = parent_len;
    if (parent_len > 0) memcpy(out->buf, p->buf, parent_len);
    out->buf[out->len] = '\0';
}

static inline SpPath sp_priv_path_from_n_impl(const char *s, size_t len, SpFlavor flavor) {
    SP_ASSERT_FLAVOR(flavor);
    SpPath p = SP_PRIV_ZERO;
    p.flavor = flavor;
    p.len = len;
    if (p.len >= SP_PATH_MAX) p.len = SP_PATH_MAX - 1;
    if (s && p.len > 0) memcpy(p.buf, s, p.len);
    p.buf[p.len] = '\0';
    sp_priv_normalize(p.buf, &p.len, p.flavor);
    return p;
}

SpPath sp_path_new(const char *s, SpPathOpts opts) {
    return sp_priv_path_from_n_impl(s, s ? strlen(s) : 0, opts.flavor);
}

SpPath sp_path_from_n(const char *s, size_t len, SpFlavor flavor) {
    return sp_priv_path_from_n_impl(s, len, flavor);
}

SpPath sp_path_convert(const char *s, SpFlavor src_flavor, SpFlavor dest_flavor) {
    SP_ASSERT_FLAVOR(src_flavor);
    SP_ASSERT_FLAVOR(dest_flavor);
    if (!s || !*s) return sp_path_new(s, SP_PRIV_OPTS(dest_flavor));
    SpPath src = sp_path_new(s, SP_PRIV_OPTS(src_flavor));
    if (src_flavor == dest_flavor) return src;

    SpPath dest = src;
    dest.flavor = dest_flavor;
    char ssep = sp_priv_sep(src_flavor), dsep = sp_priv_sep(dest_flavor);
    for (size_t i = 0; i < dest.len; i++)
        if (dest.buf[i] == ssep) dest.buf[i] = dsep;
    sp_priv_normalize(dest.buf, &dest.len, dest_flavor);
    return dest;
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

static inline SpStr sp_priv_suffix_sv(SpStr name) {
    if (name.len == 0) return SP_PRIV_STR(SP_PRIV_NULL, 0);
    bool all_dots = true;
    for (size_t i = 0; i < name.len; i++) {
        if (name.data[i] != '.') { all_dots = false; break; }
    }
    if (all_dots) return SP_PRIV_STR(SP_PRIV_NULL, 0);
    size_t i = name.len;
    while (i > 0 && name.data[i - 1] != '.') i--;
    if (i <= 1 || i == name.len) return SP_PRIV_STR(SP_PRIV_NULL, 0);
    return SP_PRIV_STR(name.data + i - 1, name.len - i + 1);
}

SpTerm sp_suffix(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpStr name = sp_priv_name_sv(p);
    SpStr sv = sp_priv_suffix_sv(name);
    return sp_priv_term(sv.data, sv.len);
}

SpTerm sp_stem(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpStr name = sp_priv_name_sv(p);
    SpStr suf = sp_priv_suffix_sv(name);
    return sp_priv_term(name.data, name.len - suf.len);
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
    SpPath r = SP_PRIV_ZERO;
    sp_priv_parent_path(p, &r);
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
    it.path = p;
    size_t parent_len = sp_priv_parent_len_raw(p->buf, p->len, p->flavor);
    it.done = (parent_len == p->len);
    it.current_len = parent_len;
    return it;
}

bool sp_parents_next(SpParentsIter *it, SpPath *out) {
    if (it->done) return false;
    const SpPath *p = it->path;
    out->flavor = p->flavor;
    out->len = it->current_len;
    if (out->len > 0) memcpy(out->buf, p->buf, out->len);
    out->buf[out->len] = '\0';
    if (out->len == 0) {
        it->done = true;
        return true;
    }
    size_t next_len = sp_priv_parent_len_raw(p->buf, out->len, out->flavor);
    it->done = (next_len == out->len);
    it->current_len = next_len;
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
    return sp_priv_join_len(base, other, strlen(other));
}

SpPath sp_join_n(const SpPath *base, const char *s, size_t len) {
    SP_ASSERT_PATH_INVARIANT(base);
    if (len == 0 || !s) return sp_path_copy(base);
    return sp_priv_join_len(base, s, len);
}

SpPath sp_join_impl(const SpPath *base, const char **parts) {
    SP_ASSERT_PATH_INVARIANT(base);
    SpPath r = sp_path_copy(base);
    for (size_t i = 0; parts[i]; i++) {
        if (!parts[i][0]) continue;
        r = sp_priv_join_len(&r, parts[i], strlen(parts[i]));
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

static inline size_t sp_priv_parts_count(const char *const *parts) {
    if (!parts) return 0;
    size_t count = 0;
    while (parts[count]) count++;
    return count;
}

static inline SpPath sp_priv_path_from_parts(SpFlavor flavor, const char *const *parts, size_t parts_count) {
    SpPath r = SP_PRIV_ZERO;
    r.flavor = flavor;
    if (!parts) return r;
    for (size_t i = 0; i < parts_count; i++) {
        const char *part = parts[i];
        if (!part || !part[0]) continue;
        r = sp_priv_join_len(&r, part, strlen(part));
    }
    return r;
}

SpPath sp_with_segments(const SpPath *p, const char **parts, size_t parts_count) {
    SP_ASSERT_PATH_INVARIANT(p);
    return sp_priv_path_from_parts(p->flavor, parts, parts_count);
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
    SpStr n = sp_priv_name_sv(p);
    return n.len > 0 && !(n.len == 1 && n.data[0] == '.');
}

static SpPath sp_priv_with_name_impl(const SpPath *p, const char *name, size_t nlen) {
    SpPath r = SP_PRIV_ZERO;
    sp_priv_parent_path(p, &r);

    if (sp_priv_is_windows_flavor(p->flavor) && r.len == 0 && nlen >= 2 && sp_priv_is_drive_letter(name[0]) &&
        name[1] == ':') {
        r.buf[r.len++] = '.';
        r.buf[r.len++] = sp_priv_sep(p->flavor);
    }
    if (r.len > 0 && !sp_priv_is_sep(r.buf[r.len - 1], p->flavor)) sp_priv_append_sep(&r);
    sp_priv_append_cstr(&r, name, nlen);
    r.buf[r.len] = '\0';
    return r;
}

static SpPath sp_priv_with_name_parts(const SpPath *p, SpStr head, SpStr tail) {
    char name[SP_PATH_MAX];
    size_t head_len = head.len >= SP_PATH_MAX ? SP_PATH_MAX - 1 : head.len;
    size_t tail_len = tail.len;
    if (tail_len >= SP_PATH_MAX - head_len) tail_len = SP_PATH_MAX - head_len - 1;
    if (head_len > 0) memcpy(name, head.data, head_len);
    if (tail_len > 0) memcpy(name + head_len, tail.data, tail_len);
    name[head_len + tail_len] = '\0';
    return sp_priv_with_name_impl(p, name, head_len + tail_len);
}

SpPath sp_with_name(const SpPath *p, const char *name) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (!sp_priv_has_usable_name(p)) return sp_priv_error_path(SP_ERR_NO_NAME);
    size_t nlen = strlen(name);
    if (!sp_priv_is_valid_name(name, nlen, p->flavor)) return sp_priv_error_path(SP_ERR_INVALID_ARG);
    return sp_priv_with_name_impl(p, name, nlen);
}

SpPath sp_with_stem(const SpPath *p, const char *stem) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (!sp_priv_has_usable_name(p)) return sp_priv_error_path(SP_ERR_NO_NAME);
    size_t slen = strlen(stem);
    if (!sp_priv_is_valid_name(stem, slen, p->flavor)) return sp_priv_error_path(SP_ERR_INVALID_ARG);

    return sp_priv_with_name_parts(p, SP_PRIV_STR(stem, slen), sp_priv_suffix_sv(sp_priv_name_sv(p)));
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

    SpStr name_sv = sp_priv_name_sv(p);
    SpStr suf = sp_priv_suffix_sv(name_sv);
    return sp_priv_with_name_parts(
        p,
        SP_PRIV_STR(name_sv.data, name_sv.len - suf.len),
        SP_PRIV_STR(suffix, suflen)
    );
}

static inline bool sp_priv_is_absolute_path(const SpPath *p) {
    size_t drive = sp_priv_drive_len(p->buf, p->len, p->flavor);
    size_t root = sp_priv_root_len(p->buf, p->len, p->flavor);
    if (sp_priv_is_windows_flavor(p->flavor)) {
        if (drive >= 2 && sp_priv_is_sep(p->buf[0], p->flavor) && sp_priv_is_sep(p->buf[1], p->flavor)) return true;
        return drive > 0 && root > 0;
    }
    return root > 0;
}

bool sp_is_absolute(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    return sp_priv_is_absolute_path(p);
}

SpPath sp_cwd(SpFlavor flavor) {
    char buf[SP_PATH_MAX];
    if (sp_priv_getcwd(buf, SP_PATH_MAX)) {
        return sp_path_new(buf, SP_PRIV_OPTS(flavor));
    }
    return sp_path_new("", SP_PRIV_OPTS(flavor));
}

static SpPath sp_priv_absolute_path(const SpPath *p) {
    if (sp_priv_is_absolute_path(p)) return sp_path_copy(p);
    SpPath cwd = sp_cwd(p->flavor);
    if (cwd.len == 0) return sp_path_copy(p);
    return sp_priv_join_len(&cwd, p->buf, p->len);
}

SpPath sp_absolute(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    return sp_priv_absolute_path(p);
}

static size_t sp_priv_collect_parts(const SpPath *p, SpStr *out, size_t max) {
    SpPartsIter it = sp_parts_begin(p);
    SpStr part;
    size_t n = 0;
    while (sp_parts_next(&it, &part) && n < max) {
        out[n++] = part;
    }
    return n;
}

static SpPath sp_priv_relative_to_impl(const SpPath *p, const SpPath *other, bool walk_up) {
    SpStr p_parts[SP_PATH_MAX / 2], o_parts[SP_PATH_MAX / 2];
    size_t p_count = sp_priv_collect_parts(p, p_parts, SP_PATH_MAX / 2);
    size_t o_count = sp_priv_collect_parts(other, o_parts, SP_PATH_MAX / 2);

    if (walk_up) {
        for (size_t i = 0; i < o_count; i++)
            if (o_parts[i].len == 2 && o_parts[i].data[0] == '.' && o_parts[i].data[1] == '.')
                return sp_priv_error_path(SP_ERR_NOT_RELATIVE);
    }

    size_t p_anchor = sp_priv_anchor_len(p->buf, p->len, p->flavor);
    size_t o_anchor = sp_priv_anchor_len(other->buf, other->len, other->flavor);
    if (p_anchor > 0 || o_anchor > 0) {
        if (p_anchor > 0 && o_anchor > 0) {
            if (!sp_priv_sv_eq_flavor(SP_PRIV_STR(p->buf, p_anchor), SP_PRIV_STR(other->buf, o_anchor), p->flavor))
                return sp_priv_error_path(SP_ERR_NOT_RELATIVE);
        } else {
            return sp_priv_error_path(SP_ERR_NOT_RELATIVE);
        }
    }

    size_t common = 0;
    while (common < p_count && common < o_count && sp_priv_sv_eq_flavor(p_parts[common], o_parts[common], p->flavor))
        common++;

    if (!walk_up && common < o_count) return sp_priv_error_path(SP_ERR_NOT_RELATIVE);

    SpPath r = SP_PRIV_ZERO;
    r.flavor = p->flavor;
    bool first = true;
    for (size_t i = common; i < o_count; i++) {
        if (i == 0 && o_anchor > 0) continue;
        if (!first) sp_priv_append_sep(&r);
        sp_priv_append_cstr(&r, "..", 2);
        first = false;
    }
    for (size_t i = common; i < p_count; i++) {
        if (i == 0 && p_anchor > 0) continue;
        if (!first) sp_priv_append_sep(&r);
        sp_priv_append_cstr(&r, p_parts[i].data, p_parts[i].len);
        first = false;
    }
    r.buf[r.len] = '\0';
    return r;
}

bool sp_is_relative_to(const SpPath *p, const SpPath *other) {
    SP_ASSERT_PATH_INVARIANT(p);
    SP_ASSERT_PATH_INVARIANT(other);
    SpPath r = sp_priv_relative_to_impl(p, other, false);
    return !sp_path_is_error(&r);
}

SpPath sp_relative_to(const SpPath *p, const SpPath *other) {
    SP_ASSERT_PATH_INVARIANT(p);
    SP_ASSERT_PATH_INVARIANT(other);
    return sp_priv_relative_to_impl(p, other, false);
}

SpPath sp_relative_to_walk_up(const SpPath *p, const SpPath *other) {
    SP_ASSERT_PATH_INVARIANT(p);
    SP_ASSERT_PATH_INVARIANT(other);
    return sp_priv_relative_to_impl(p, other, true);
}

bool sp_is_relative_to_parts(const SpPath *p, const char **parts) {
    SpPath other = sp_priv_path_from_parts(p->flavor, parts, sp_priv_parts_count(parts));
    SpPath r = sp_priv_relative_to_impl(p, &other, false);
    return !sp_path_is_error(&r);
}

SpPath sp_relative_to_parts(const SpPath *p, const char **parts, bool walk_up) {
    SpPath other = sp_priv_path_from_parts(p->flavor, parts, sp_priv_parts_count(parts));
    return sp_priv_relative_to_impl(p, &other, walk_up);
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
    const char *str = p->len > 0 ? p->buf : ".";
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
    bool unc_root_without_drive = is_unc && pat_has_root && !pat_has_drive;
    if (unc_root_without_drive) {
        SpUncInfo info = sp_priv_parse_unc(p->buf, p->len, p->flavor);
        if (info.is_complete && !info.is_device_ns) {
            size_t server_start = info.is_unc_device ? 8 : 2; /* //?/UNC/ or // */
            size_t share_start = info.server_end ? (info.server_end + 1) : 0;
            if (info.server_end > server_start && path_count < SP_PATH_MAX / 2)
                path_parts[path_count++] = SP_PRIV_STR(p->buf + server_start, info.server_end - server_start);
            if (info.share_end > share_start && path_count < SP_PATH_MAX / 2)
                path_parts[path_count++] = SP_PRIV_STR(p->buf + share_start, info.share_end - share_start);
        }
        SpPartsIter it = sp_parts_begin(p);
        SpStr part;
        bool first = true;
        while (sp_parts_next(&it, &part) && path_count < SP_PATH_MAX / 2) {
            if (first) {
                first = false;
                continue;
            }
            path_parts[path_count++] = part;
        }
    } else {
        path_count = sp_priv_collect_parts(p, path_parts, SP_PATH_MAX / 2);
    }

    SpStr pat_parts[SP_PATH_MAX / 2];
    size_t pat_count = 0, start = pat_anchor;
    for (size_t i = start; i <= plen && pat_count < SP_PATH_MAX / 2; i++) {
        if (i == plen || pattern[i] == '/' || (is_win && pattern[i] == '\\')) {
            if (i > start) {
                pat_parts[pat_count++] = SP_PRIV_STR(pattern + start, i - start);
            }
            start = i + 1;
        }
    }

    size_t path_start = (pat_anchored && path_count > 0 && !unc_root_without_drive) ? 1 : 0;
    size_t eff_count = path_count - path_start;
    if (pat_anchored && pat_count != eff_count) return SP_MATCH_NO;
    if (pat_count > eff_count) return SP_MATCH_NO;

    for (size_t i = 0; i < pat_count; i++) {
        size_t pi = pat_count - 1 - i, si = path_start + eff_count - 1 - i;
        if (!SP_PRIV_IS_DOUBLESTAR(pat_parts[pi].data, pat_parts[pi].len) &&
            !sp_priv_fnmatch(pat_parts[pi].data, pat_parts[pi].len, path_parts[si].data, path_parts[si].len, ci))
            return SP_MATCH_NO;
    }
    return SP_MATCH_YES;
}

bool sp_is_reserved(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (!sp_priv_is_windows_flavor(p->flavor) || sp_priv_is_unc(p->buf, p->len, p->flavor)) return false;
    SpStr name = sp_priv_name_sv(p);
    if (name.len == 0 || name.len > 12) return false;
    char upper[13];
    size_t len = 0;
    for (size_t i = 0; i < name.len && name.data[i] != '.' && name.data[i] != ':' && len < 12; i++) {
        unsigned char c = SP_PRIV_CAST(unsigned char, name.data[i]);
        if (c == 0xC2 && i + 1 < name.len) {
            unsigned char c2 = SP_PRIV_CAST(unsigned char, name.data[i + 1]);
            /* UTF-8 superscript digits: ¹(0xB9)→1, ²(0xB2)→2, ³(0xB3)→3 */
            if (c2 == 0xB2 || c2 == 0xB3 || c2 == 0xB9) {
                upper[len++] = (c2 == 0xB9) ? '1' : SP_PRIV_CAST(char, '0' + (c2 - 0xB0));
                i++;
                continue;
            }
        }
        if (c != ' ') upper[len++] = SP_PRIV_CAST(char, (c >= 'a' && c <= 'z') ? c - 32 : c);
    }
    upper[len] = '\0';
    if (strcmp(upper, "CON") == 0 || strcmp(upper, "PRN") == 0 ||
        strcmp(upper, "AUX") == 0 || strcmp(upper, "NUL") == 0) return true;
    if (len == 4 && upper[3] >= '1' && upper[3] <= '9' &&
        (memcmp(upper, "COM", 3) == 0 || memcmp(upper, "LPT", 3) == 0)) return true;
    return strcmp(upper, "CONIN$") == 0 || strcmp(upper, "CONOUT$") == 0;
}

static bool sp_priv_has_type(const SpPath *p, unsigned int type_mask, bool follow_symlinks) {
    SpStatResult st = follow_symlinks ? sp_stat(p) : sp_lstat(p);
    if (!st.valid) return false;
    return (st.sp_mode & S_IFMT) == type_mask;
}

#define SP_DEFINE_FILE_TYPE_QUERY(name, type_mask, follow_symlinks) \
    bool sp_is_##name(const SpPath *p) { return sp_priv_has_type(p, type_mask, follow_symlinks); }
SP_FILE_TYPE_QUERIES(SP_DEFINE_FILE_TYPE_QUERY)
#undef SP_DEFINE_FILE_TYPE_QUERY

bool sp_exists(const SpPath *p) {
    return sp_stat(p).valid;
}

bool sp_is_mount(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (sp_priv_has_embedded_null(p)) return false;
#ifdef SP_WINDOWS
    const char *path_str = sp_str(p);
    char vol_path[SP_PATH_MAX];
    if (!GetVolumePathNameA(path_str, vol_path, SP_PATH_MAX)) return false;
    size_t vlen = strlen(vol_path);
    size_t plen = p->len > 0 ? p->len : 1;
    if (vlen > 0 && vol_path[vlen - 1] == '\\') vlen--;
    if (plen > 0 && (path_str[plen - 1] == '\\' || path_str[plen - 1] == '/')) plen--;
    return sp_priv_str_eq_ci(path_str, plen, vol_path, vlen);
#else
    SpStatResult st_path = sp_lstat(p);
    if (!st_path.valid) return false;
    if ((st_path.sp_mode & S_IFMT) != S_IFDIR) return false;
    SpPath parent = SP_PRIV_ZERO;
    sp_priv_parent_path(p, &parent);
    SpStatResult st_parent = sp_lstat(&parent);
    if (!st_parent.valid) return false;
    if (st_path.sp_dev != st_parent.sp_dev) return true;
    return st_path.sp_ino == st_parent.sp_ino;
#endif
}

bool sp_is_junction(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (sp_priv_has_embedded_null(p)) return false;
#ifdef SP_WINDOWS
    const char *path_str = sp_str(p);
    DWORD attrs = GetFileAttributesA(path_str);
    if (attrs == INVALID_FILE_ATTRIBUTES) return false;
    if (!(attrs & FILE_ATTRIBUTE_DIRECTORY)) return false;
    if (!(attrs & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(path_str, &fd);
    if (h == INVALID_HANDLE_VALUE) return false;
    FindClose(h);
    return fd.dwReserved0 == 0xA0000003;
#else
    (void)p;
    return false;
#endif
}

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
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    r->sp_atime_ns = SP_PRIV_CAST(long long, st->st_atimespec.tv_sec) * 1000000000LL + st->st_atimespec.tv_nsec;
    r->sp_mtime_ns = SP_PRIV_CAST(long long, st->st_mtimespec.tv_sec) * 1000000000LL + st->st_mtimespec.tv_nsec;
    r->sp_ctime_ns = SP_PRIV_CAST(long long, st->st_ctimespec.tv_sec) * 1000000000LL + st->st_ctimespec.tv_nsec;
#else
    r->sp_atime_ns = SP_PRIV_CAST(long long, st->st_atime) * 1000000000LL;
    r->sp_mtime_ns = SP_PRIV_CAST(long long, st->st_mtime) * 1000000000LL;
    r->sp_ctime_ns = SP_PRIV_CAST(long long, st->st_ctime) * 1000000000LL;
#endif
    r->valid = true;
}
#endif

#ifdef SP_WINDOWS
#define SP_FILETIME_TO_UNIX(ft) \
    (((SP_PRIV_CAST(double, (SP_PRIV_CAST(unsigned long long, (ft).dwHighDateTime) << 32) | (ft).dwLowDateTime)) - 116444736000000000.0) / 10000000.0)
#define SP_FILETIME_TO_NS(ft) \
    ((SP_PRIV_CAST(long long, (SP_PRIV_CAST(unsigned long long, (ft).dwHighDateTime) << 32) | (ft).dwLowDateTime) - 116444736000000000LL) * 100LL)
#endif

static SpStatResult sp_priv_stat_impl(const SpPath *p, bool follow_symlinks) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpStatResult result = SP_PRIV_ZERO;
    result.valid = false;
    if (sp_priv_has_embedded_null(p)) return result;
    const char *path_str = sp_str(p);

#ifdef SP_WINDOWS
    DWORD flags = FILE_FLAG_BACKUP_SEMANTICS;
    if (!follow_symlinks) flags |= FILE_FLAG_OPEN_REPARSE_POINT;
    HANDLE hFile = CreateFileA(path_str, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               NULL, OPEN_EXISTING, flags, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return result;

    BY_HANDLE_FILE_INFORMATION info;
    if (!GetFileInformationByHandle(hFile, &info)) {
        CloseHandle(hFile);
        return result;
    }

    DWORD file_type = GetFileType(hFile);
    result.sp_mode = (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 040777 : 0100666;
    if (file_type == FILE_TYPE_CHAR) {
        result.sp_mode = (result.sp_mode & ~S_IFMT) | S_IFCHR;
    } else if (file_type != FILE_TYPE_DISK) {
        result.sp_mode &= ~S_IFMT;
    }
    if (info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
        result.sp_mode &= ~0222;
    }
    if (!follow_symlinks && (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
        result.sp_mode = (result.sp_mode & ~S_IFMT) | S_IFLNK;
    }

    typedef struct { ULONGLONG VolumeSerialNumber; BYTE FileId[16]; } SpFileIdInfo;
    SpFileIdInfo fii;
    if (GetFileInformationByHandleEx(hFile, SP_PRIV_CAST(FILE_INFO_BY_HANDLE_CLASS, 18), &fii, sizeof(fii))) {
        result.sp_dev = fii.VolumeSerialNumber;
        memcpy(&result.sp_ino, fii.FileId, sizeof(result.sp_ino));
    } else {
        result.sp_dev = SP_PRIV_CAST(unsigned long long, info.dwVolumeSerialNumber);
        result.sp_ino = (SP_PRIV_CAST(unsigned long long, info.nFileIndexHigh) << 32) |
                        SP_PRIV_CAST(unsigned long long, info.nFileIndexLow);
    }

    result.sp_nlink = SP_PRIV_CAST(unsigned long long, info.nNumberOfLinks);
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
    CloseHandle(hFile);
#else
    struct stat st;
    if ((follow_symlinks ? stat(path_str, &st) : lstat(path_str, &st)) == 0) {
        sp_priv_fill_stat_result(&result, &st);
    }
#endif
    return result;
}

SpStatResult sp_stat(const SpPath *p) {
    return sp_priv_stat_impl(p, true);
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
    SP_ASSERT_PATH_INVARIANT(p);
    size_t curr_len = p->len;
    size_t count = 0;
    while (curr_len > 0) {
        size_t parent_len = sp_priv_parent_len_raw(p->buf, curr_len, p->flavor);
        if (parent_len == curr_len) break;
        count++;
        curr_len = parent_len;
    }
    return count;
}

SpStatResult sp_lstat(const SpPath *p) {
    return sp_priv_stat_impl(p, false);
}

SpPath sp_readlink(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpPath result = SP_PRIV_ZERO;
    result.flavor = p->flavor;
    if (sp_priv_has_embedded_null(p)) return sp_priv_error_path_f(p->flavor, SP_ERR_OTHER);
    const char *path_str = sp_str(p);

#ifdef SP_WINDOWS
    HANDLE h = CreateFileA(path_str, GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           NULL, OPEN_EXISTING,
                           FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (h == INVALID_HANDLE_VALUE) return sp_priv_error_path_f(p->flavor, SP_ERR_OTHER);

    char reparse_buf[16384];
    DWORD bytes_returned;
    if (!DeviceIoControl(h, 0x000900A8 /* FSCTL_GET_REPARSE_POINT */,
                         NULL, 0, reparse_buf, sizeof(reparse_buf),
                         &bytes_returned, NULL)) {
        CloseHandle(h);
        return sp_priv_error_path_f(p->flavor, SP_ERR_OTHER);
    }
    CloseHandle(h);

    DWORD tag = *(DWORD *)reparse_buf;
    size_t data_offset = (tag == 0xA000000C) ? 20 : (tag == 0xA0000003) ? 16 : 0;
    if (data_offset == 0) return sp_priv_error_path_f(p->flavor, SP_ERR_OTHER);

    WORD print_offset = *(WORD *)(reparse_buf + 12);
    WORD print_len = *(WORD *)(reparse_buf + 14);
    WCHAR *print_name = (WCHAR *)(reparse_buf + data_offset + print_offset);
    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, print_name, print_len / 2,
                                       result.buf, SP_PATH_MAX - 1, NULL, NULL);
    if (utf8_len <= 0) return sp_priv_error_path_f(p->flavor, SP_ERR_OTHER);
    result.len = SP_PRIV_CAST(size_t, utf8_len);
    result.buf[result.len] = '\0';
    sp_priv_normalize(result.buf, &result.len, result.flavor);
#else
    ssize_t len = readlink(path_str, result.buf, SP_PATH_MAX - 1);
    if (len < 0) return sp_priv_error_path_f(p->flavor, SP_ERR_OTHER);
    result.len = SP_PRIV_CAST(size_t, len);
    result.buf[result.len] = '\0';
#endif

    return result;
}

SpPath sp_resolve(const SpPath *p, bool strict) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpPath result = SP_PRIV_ZERO;
    result.flavor = p->flavor;
    if (sp_priv_has_embedded_null(p)) return strict ? sp_priv_error_path_f(p->flavor, SP_ERR_OTHER) : sp_priv_absolute_path(p);
    const char *path_str = sp_str(p);

#ifdef SP_WINDOWS
    char full_path[SP_PATH_MAX];
    DWORD len = GetFullPathNameA(path_str, SP_PATH_MAX, full_path, NULL);
    if (len == 0 || len >= SP_PATH_MAX)
        return strict ? sp_priv_error_path_f(p->flavor, SP_ERR_OTHER) : sp_priv_absolute_path(p);
    if (strict && GetFileAttributesA(full_path) == INVALID_FILE_ATTRIBUTES)
        return sp_priv_error_path_f(p->flavor, SP_ERR_OTHER);

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
        if (strict) return sp_priv_error_path_f(p->flavor, SP_ERR_OTHER);
        SpPath abs_path = sp_priv_absolute_path(p);
        size_t curr_len = abs_path.len;
        while (curr_len > 0) {
            char curr_path[SP_PATH_MAX];
            memcpy(curr_path, abs_path.buf, curr_len);
            curr_path[curr_len] = '\0';
            if (realpath(curr_path, resolved) != NULL) {
                result = sp_path_from_n(resolved, strlen(resolved), p->flavor);
                if (curr_len < abs_path.len) {
                    size_t rest = curr_len;
                    while (rest < abs_path.len && abs_path.buf[rest] == '/') rest++;
                    if (rest < abs_path.len)
                        result = sp_join_n(&result, abs_path.buf + rest, abs_path.len - rest);
                }
                return result;
            }
            size_t parent_len = sp_priv_parent_len_raw(abs_path.buf, curr_len, abs_path.flavor);
            if (parent_len == curr_len) break;
            curr_len = parent_len;
        }
        return abs_path;
    }
#endif

    return result;
}

static bool sp_priv_path_cstr(const SpPath *p, const char **out) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (sp_priv_has_embedded_null(p)) return false;
    *out = sp_str(p);
    return true;
}

static bool sp_priv_link_to_impl(const SpPath *p, const SpPath *target, bool symbolic, bool target_is_directory) {
    const char *link_path, *target_path;
    if (!sp_priv_path_cstr(p, &link_path) || !sp_priv_path_cstr(target, &target_path)) return false;
#ifdef SP_WINDOWS
    if (symbolic) {
        DWORD flags = target_is_directory ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0;
        return CreateSymbolicLinkA(link_path, target_path, flags | 0x2) != 0;
    }
    return CreateHardLinkA(link_path, target_path, NULL) != 0;
#else
    (void)target_is_directory;
    return symbolic ? symlink(target_path, link_path) == 0 : link(target_path, link_path) == 0;
#endif
}

bool sp_symlink_to(const SpPath *p, const SpPath *target, bool target_is_directory) {
    return sp_priv_link_to_impl(p, target, true, target_is_directory);
}

bool sp_hardlink_to(const SpPath *p, const SpPath *target) {
    return sp_priv_link_to_impl(p, target, false, false);
}

bool sp_samefile(const SpPath *a, const SpPath *b) {
    SP_ASSERT_PATH_INVARIANT(a);
    SP_ASSERT_PATH_INVARIANT(b);
    if (sp_priv_has_embedded_null(a) || sp_priv_has_embedded_null(b)) return false;
    SpStatResult stat_a = sp_stat(a);
    SpStatResult stat_b = sp_stat(b);

    if (!stat_a.valid || !stat_b.valid) return false;

    return stat_a.sp_dev == stat_b.sp_dev && stat_a.sp_ino == stat_b.sp_ino;
}

static int sp_priv_mkdir_parents(const SpPath *p) {
    SpPath parent = SP_PRIV_ZERO;
    sp_priv_parent_path(p, &parent);
    bool parent_eq_path = parent.flavor == p->flavor && parent.len == p->len &&
                          memcmp(parent.buf, p->buf, p->len) == 0;
    if (parent_eq_path || parent.len == 0) return SP_MKDIR_OK;
    int r = sp_mkdir(&parent, SP_MKDIR_DEF_MODE, true, true);
    return (r == SP_MKDIR_ERR_EXISTS_NOT_DIR) ? SP_MKDIR_ERR_NOT_DIR : r;
}

int sp_mkdir(const SpPath *p, unsigned int mode, bool parents, bool exist_ok) {
    const char *path_str;
    if (mode == 0) mode = SP_MKDIR_DEF_MODE;
    if (!sp_priv_path_cstr(p, &path_str)) return SP_MKDIR_ERR_OTHER;
    if (parents) {
        int r = sp_priv_mkdir_parents(p);
        if (r != SP_MKDIR_OK) return r;
    }
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

bool sp_touch(const SpPath *p, unsigned int mode, bool exist_ok) {
    const char *path_str;
    if (!sp_priv_path_cstr(p, &path_str)) return false;
    if (mode == 0) mode = 0666;

#ifdef SP_WINDOWS
    DWORD attrs = GetFileAttributesA(path_str);
    if (attrs != INVALID_FILE_ATTRIBUTES) {
        if (!exist_ok) return false;
        HANDLE h = CreateFileA(path_str, FILE_WRITE_ATTRIBUTES,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE) return false;
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        bool ok = SetFileTime(h, NULL, &ft, &ft) != 0;
        CloseHandle(h);
        return ok;
    }
    HANDLE h = CreateFileA(path_str, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    CloseHandle(h);
    return true;
#else
    struct stat st;
    if (stat(path_str, &st) == 0) return exist_ok && utime(path_str, SP_PRIV_NULL) == 0;
    int fd = open(path_str, O_CREAT | O_WRONLY, SP_PRIV_CAST(mode_t, mode));
    if (fd < 0) return false;
    close(fd);
    return true;
#endif
}

static bool sp_priv_remove_impl(const SpPath *p, bool is_dir, bool missing_ok) {
    const char *path_str;
    if (!sp_priv_path_cstr(p, &path_str)) return false;
#ifdef SP_WINDOWS
    if (is_dir) return RemoveDirectoryA(path_str) != 0;
    if (DeleteFileA(path_str)) return true;
    DWORD err = GetLastError();
    return missing_ok && (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND);
#else
    int rc = is_dir ? rmdir(path_str) : unlink(path_str);
    if (rc == 0) return true;
    return !is_dir && missing_ok && errno == ENOENT;
#endif
}

bool sp_unlink(const SpPath *p, bool missing_ok) {
    return sp_priv_remove_impl(p, false, missing_ok);
}

bool sp_rmdir(const SpPath *p) {
    return sp_priv_remove_impl(p, true, false);
}

static SpPath sp_priv_move(const SpPath *p, const SpPath *target, bool allow_replace) {
    SP_ASSERT_PATH_INVARIANT(p);
    SP_ASSERT_PATH_INVARIANT(target);
    SpPath result = *target;

    if (sp_priv_has_embedded_null(p) || sp_priv_has_embedded_null(target)) {
        return sp_priv_error_path(SP_ERR_OTHER);
    }

    const char *src_str = sp_str(p);
    const char *dst_str = sp_str(target);

#ifdef SP_WINDOWS
    if (!MoveFileExA(src_str, dst_str, allow_replace ? MOVEFILE_REPLACE_EXISTING : 0)) {
        return sp_priv_error_path(SP_ERR_OTHER);
    }
#else
    if (!allow_replace) {
        struct stat dst_st;
        if (stat(dst_str, &dst_st) == 0) {
            struct stat src_st;
            if (stat(src_str, &src_st) == 0 && src_st.st_dev == dst_st.st_dev && src_st.st_ino == dst_st.st_ino)
                return result;
            return sp_priv_error_path(SP_ERR_OTHER);
        }
    }
    if (rename(src_str, dst_str) != 0) {
        return sp_priv_error_path(SP_ERR_OTHER);
    }
#endif
    return result;
}

SpPath sp_rename(const SpPath *p, const SpPath *target) { return sp_priv_move(p, target, false); }
SpPath sp_replace(const SpPath *p, const SpPath *target) { return sp_priv_move(p, target, true); }

bool sp_chmod(const SpPath *p, unsigned int mode) {
    const char *path_str;
    if (!sp_priv_path_cstr(p, &path_str)) return false;

#ifdef SP_WINDOWS
    DWORD attrs = GetFileAttributesA(path_str);
    if (attrs == INVALID_FILE_ATTRIBUTES) return false;
    DWORD new_attrs = attrs;
    if ((mode & 0222) == 0) {
        new_attrs |= FILE_ATTRIBUTE_READONLY;
    } else {
        new_attrs &= ~FILE_ATTRIBUTE_READONLY;
    }
    if (new_attrs == attrs) return true;
    return SetFileAttributesA(path_str, new_attrs) != 0;
#else
    return chmod(path_str, SP_PRIV_CAST(mode_t, mode)) == 0;
#endif
}

SpIOResult sp_read_file(const SpPath *p, char *buf, size_t buf_size) {
    SpIOResult r = SP_PRIV_ZERO;
    const char *path_str;
    if (!sp_priv_path_cstr(p, &path_str)) { r.error = SP_IO_ERR_OPEN; return r; }
    SpStatResult st = sp_stat(p);
    if (!st.valid || st.sp_size < 0) { r.error = SP_IO_ERR_OPEN; return r; }
    size_t sz = SP_PRIV_CAST(size_t, st.sp_size);
    if (sz > buf_size) { r.bytes = sz; r.error = SP_IO_ERR_TOO_LARGE; return r; }

    FILE *f = fopen(path_str, "rb");
    if (!f) { r.error = SP_IO_ERR_OPEN; return r; }
    size_t got = fread(buf, 1, sz, f);
    if (got != sz) { fclose(f); r.bytes = got; r.error = SP_IO_ERR_READ; return r; }
    if (fclose(f) != 0) { r.bytes = got; r.error = SP_IO_ERR_READ; return r; }
    r.bytes = got;
    return r;
}

SpIOResult sp_write_file(const SpPath *p, const char *data, size_t data_len) {
    SpIOResult r = SP_PRIV_ZERO;
    const char *path_str;
    if (!sp_priv_path_cstr(p, &path_str)) { r.error = SP_IO_ERR_OPEN; return r; }
    FILE *f = fopen(path_str, "wb");
    if (!f) { r.error = SP_IO_ERR_OPEN; return r; }
    size_t wrote = fwrite(data, 1, data_len, f);
    if (wrote != data_len) { fclose(f); r.bytes = wrote; r.error = SP_IO_ERR_WRITE; return r; }
    if (fclose(f) != 0) { r.bytes = wrote; r.error = SP_IO_ERR_WRITE; return r; }
    r.bytes = wrote;
    return r;
}

const char *sp_error_str(int error) {
    switch (error) {
        case SP_OK:                 return "Success";
        case SP_ERR:                return "Operation failed";
        case SP_ERR_EXISTS:         return "File exists";
        case SP_ERR_NOT_FOUND:      return "No such file or directory";
        case SP_ERR_NOT_DIR:        return "Not a directory";
        case SP_ERR_PERMISSION:     return "Permission denied";
        case SP_ERR_EXISTS_NOT_DIR: return "Path exists but is not a directory";
        case SP_ERR_OPEN:           return "Could not open file";
        case SP_ERR_READ:           return "Read failed";
        case SP_ERR_WRITE:          return "Write failed";
        case SP_ERR_TOO_LARGE:      return "File too large for buffer";
        default:                    return "Unknown error";
    }
}

#ifdef SP_WINDOWS
#else
#include <dirent.h>
#endif

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

static void sp_priv_readdir_close(void **handle) {
    if (!*handle) return;
#ifdef SP_WINDOWS
    FindClose(*handle);
#else
    closedir(SP_PRIV_CAST(DIR *, *handle));
#endif
    *handle = SP_PRIV_NULL;
}

static bool sp_priv_readdir_next(void **handle, const SpPath *dir, SpPath *out) {
#ifdef SP_WINDOWS
    WIN32_FIND_DATAA fd;
    while (true) {
        if (!*handle) {
            char search[SP_PATH_MAX + 3];
            size_t len = dir->len;
            if (len == 0) {
                search[0] = '.';
                search[1] = '\\';
                search[2] = '*';
                search[3] = '\0';
            } else {
                memcpy(search, dir->buf, len);
                if (!sp_priv_is_sep(search[len - 1], dir->flavor)) search[len++] = '\\';
                search[len++] = '*';
                search[len] = '\0';
            }
            *handle = FindFirstFileA(search, &fd);
            if (*handle == INVALID_HANDLE_VALUE) { *handle = SP_PRIV_NULL; return false; }
        } else {
            if (!FindNextFileA(*handle, &fd)) return false;
        }
        const char *name = fd.cFileName;
        if (SP_GLOB_IS_DOT_OR_DOTDOT(name)) continue;
        *out = sp_priv_join_len(dir, name, strlen(name));
        return true;
    }
#else
    if (!*handle) {
        *handle = opendir(sp_str(dir));
        if (!*handle) return false;
    }
    while (true) {
        struct dirent *de = readdir(SP_PRIV_CAST(DIR *, *handle));
        if (!de) return false;
        if (SP_GLOB_IS_DOT_OR_DOTDOT(de->d_name)) continue;
        *out = sp_priv_join_len(dir, de->d_name, strlen(de->d_name));
        return true;
    }
#endif
}

static void sp_priv_glob_pop(SpGlobIter *it) {
    sp_priv_readdir_close(&it->priv_.stack[it->depth].handle);
    size_t parent_len = it->priv_.stack[it->depth].path_len;
    it->priv_.current_dir.len = parent_len;
    it->priv_.current_dir.buf[parent_len] = '\0';
    it->depth--;
}

static bool sp_priv_glob_push(SpGlobIter *it, const SpPath *path, size_t seg_idx) {
    if (it->depth + 1 >= SP_GLOB_MAX_DEPTH) return false;
    int depth = it->depth + 1;
    it->priv_.stack[depth].path_len = it->priv_.current_dir.len;
    it->priv_.stack[depth].handle = SP_PRIV_NULL;
    it->priv_.current_dir = *path;
    it->depth = depth;
    it->priv_.seg_idxs[depth] = seg_idx;
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
    SpStatResult st = sp_priv_stat_impl(base, true);
    bool is_dir = st.valid && ((st.sp_mode & S_IFMT) == S_IFDIR);
    if (it.priv_.seg_count == 0 || !is_dir || !sp_priv_glob_push(&it, base, 0)) {
        it.depth = -1;
        return it;
    }
    it.depth = 0;
    if (it.priv_.seg_types[0] == SP_GLOB_SEG_DOUBLESTAR && it.priv_.seg_count == 1)
        it.priv_.yield_base_pending = true;
    return it;
}

bool sp_glob_next(SpGlobIter *it, SpPath *out) {
    if (!it || it->depth < 0) return false;
    if (it->priv_.yield_base_pending) {
        it->priv_.yield_base_pending = false;
        if (!it->priv_.dir_only || sp_priv_has_type(&it->priv_.current_dir, S_IFDIR, true)) {
            *out = it->priv_.current_dir;
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

        if (seg_type == SP_GLOB_SEG_LITERAL && SP_GLOB_IS_DOT_OR_DOTDOT(pattern)) {
            SpPath full = sp_priv_join_len(&it->priv_.current_dir, pattern, strlen(pattern));
            if (!sp_priv_has_type(&full, S_IFDIR, true)) { sp_priv_glob_pop(it); continue; }
            if (is_last) { it->priv_.seg_idxs[depth]++; *out = full; return true; }
            it->priv_.seg_idxs[depth]++;
            it->priv_.current_dir = full;
            const char *next_pattern = it->priv_.pattern_buf + it->priv_.seg_offsets[seg_idx + 1];
            if (it->priv_.seg_types[seg_idx + 1] == SP_GLOB_SEG_LITERAL && SP_GLOB_IS_DOT_OR_DOTDOT(next_pattern))
                continue;
            sp_priv_readdir_close(&it->priv_.stack[depth].handle);
            continue;
        }

        SpPath full;
        if (!sp_priv_readdir_next(&it->priv_.stack[depth].handle, &it->priv_.current_dir, &full)) {
            sp_priv_glob_pop(it);
            continue;
        }
        SpStr name = sp_priv_name_sv(&full);
        if (name.len == 0) continue;
        if (name.data[0] == '.' && pattern[0] != '.' && seg_type != SP_GLOB_SEG_DOUBLESTAR) continue;
        bool isdir = sp_priv_has_type(&full, S_IFDIR, true);
        bool matches_dir_constraint = !it->priv_.dir_only || isdir;

        if (seg_type == SP_GLOB_SEG_DOUBLESTAR) {
            if (!is_last) {
                const char *next_pattern = it->priv_.pattern_buf + it->priv_.seg_offsets[seg_idx + 1];
                int next_type = it->priv_.seg_types[seg_idx + 1];
                bool next_is_last = (seg_idx + 1 == it->priv_.seg_count - 1);
                if (sp_priv_glob_match(name.data, next_pattern, next_type, it->priv_.case_insensitive)) {
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
        } else if (sp_priv_glob_match(name.data, pattern, seg_type, it->priv_.case_insensitive)) {
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

SpPath sp_home(SpFlavor flavor) {
    SP_ASSERT_FLAVOR(flavor);
    const char *s = NULL;
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)
#endif
#ifdef SP_WINDOWS
    s = getenv("USERPROFILE");
#else
    s = getenv("HOME");
    if (!s) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) s = pw->pw_dir;
    }
#endif
#ifdef _MSC_VER
#pragma warning(pop)
#endif
    if (s && *s) return sp_path_from_n(s, strlen(s), flavor);
    return sp_priv_error_path(SP_ERR_OTHER);
}

SpPath sp_expanduser(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (p->len == 0 || p->buf[0] != '~') return sp_path_copy(p);

    bool is_current_user = (p->len == 1) ||
                          (p->len > 1 && sp_priv_is_sep(p->buf[1], p->flavor));

    if (is_current_user) {
        SpPath home = sp_home(p->flavor);
        if (sp_path_is_error(&home)) return sp_priv_error_path(SP_ERR_OTHER);
        if (p->len == 1) return home;
        const char *rest = p->buf + 2;
        size_t rest_len = p->len - 2;

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
    size_t end = 1;
    while (end < p->len && !sp_priv_is_sep(p->buf[end], p->flavor)) end++;
    char username[256];
    size_t ulen = end - 1;
    if (ulen >= sizeof(username)) return sp_path_copy(p);
    memcpy(username, p->buf + 1, ulen);
    username[ulen] = '\0';

    struct passwd *pw = getpwnam(username);
    if (!pw || !pw->pw_dir) return sp_path_copy(p);
    const char *pw_dir = pw->pw_dir;
    SpPath home = sp_path_from_n(pw_dir, strlen(pw_dir), p->flavor);
    if (end >= p->len) return home;
    return sp_join_n(&home, p->buf + end, p->len - end);
#else
    return sp_path_copy(p);
#endif
}

#ifndef SP_WINDOWS
static SpTerm sp_priv_id_to_name(const SpPath *p, bool get_owner) {
    if (sp_priv_has_embedded_null(p)) return sp_priv_term(NULL, 0);
    SpStatResult st = sp_stat(p);
    if (!st.valid) return sp_priv_term(NULL, 0);
    const char *name = NULL;
    if (get_owner) {
        struct passwd *pw = getpwuid(st.sp_uid);
        if (pw) name = pw->pw_name;
    } else {
        struct group *gr = getgrgid(st.sp_gid);
        if (gr) name = gr->gr_name;
    }
    return name ? sp_priv_term(name, strlen(name)) : sp_priv_term(NULL, 0);
}
#endif

SpTerm sp_owner(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
#ifdef SP_WINDOWS
    (void)p;
    return sp_priv_term(NULL, 0);
#else
    return sp_priv_id_to_name(p, true);
#endif
}

SpTerm sp_group(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
#ifdef SP_WINDOWS
    (void)p;
    return sp_priv_term(NULL, 0);
#else
    return sp_priv_id_to_name(p, false);
#endif
}

SpIterdirIter sp_iterdir_begin(const SpPath *p) {
    SpIterdirIter it;
    memset(&it, 0, sizeof(it));
    it.done = -1;
    if (!p) return it;
    SP_ASSERT_PATH_INVARIANT(p);
    if (sp_priv_has_embedded_null(p)) return it;
    it.dir = sp_path_copy(p);
#ifdef SP_WINDOWS
    DWORD attr = GetFileAttributesA(it.dir.len ? it.dir.buf : ".");
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) return it;
    it.priv_.handle = SP_PRIV_NULL;
#else
    it.priv_.handle = opendir(sp_str(&it.dir));
    if (!it.priv_.handle) return it;
#endif
    it.done = 0;
    return it;
}

bool sp_iterdir_next(SpIterdirIter *it, SpPath *out) {
    if (!it || it->done != 0) return false;
    if (!sp_priv_readdir_next(&it->priv_.handle, &it->dir, out)) { it->done = 1; return false; }
    return true;
}

void sp_iterdir_end(SpIterdirIter *it) {
    if (!it) return;
    sp_priv_readdir_close(&it->priv_.handle);
    it->done = 1;
}

static int sp_priv_walk_name_cmp(const void *a, const void *b) {
    return strcmp(SP_PRIV_CAST(const char *, a), SP_PRIV_CAST(const char *, b));
}

static void sp_priv_walk_copy_name(char dest[SP_WALK_NAME_MAX], const char *src) {
    size_t len = strlen(src);
    if (len >= SP_WALK_NAME_MAX) len = SP_WALK_NAME_MAX - 1;
    memcpy(dest, src, len);
    dest[len] = '\0';
}

static bool sp_priv_walk_scan(const SpPath *dir, bool follow_symlinks,
                              char dirnames[][SP_WALK_NAME_MAX], size_t *dirname_count,
                              char filenames[][SP_WALK_NAME_MAX], size_t *filename_count) {
    *dirname_count = 0;
    *filename_count = 0;
    SpIterdirIter it = sp_iterdir_begin(dir);
    if (it.done != 0) return false;
    SpPath entry;
    while (sp_iterdir_next(&it, &entry)) {
        SpTerm name = sp_name(&entry);
        bool is_dir = sp_is_dir(&entry) && (follow_symlinks || !sp_is_symlink(&entry));
        if (is_dir) {
            if (*dirname_count < SP_WALK_MAX_ENTRIES)
                sp_priv_walk_copy_name(dirnames[(*dirname_count)++], name.buf);
        } else {
            if (*filename_count < SP_WALK_MAX_ENTRIES)
                sp_priv_walk_copy_name(filenames[(*filename_count)++], name.buf);
        }
    }
    sp_iterdir_end(&it);
    if (*dirname_count > 1)
        qsort(dirnames, *dirname_count, SP_WALK_NAME_MAX, sp_priv_walk_name_cmp);
    if (*filename_count > 1)
        qsort(filenames, *filename_count, SP_WALK_NAME_MAX, sp_priv_walk_name_cmp);
    return true;
}

static bool sp_priv_walk_recursive(const SpPath *dir, bool top_down, bool follow_symlinks,
                                   SpWalkFn callback, SpWalkErrorFn on_error, void *user_data) {
    char dirnames[SP_WALK_MAX_ENTRIES][SP_WALK_NAME_MAX];
    char filenames[SP_WALK_MAX_ENTRIES][SP_WALK_NAME_MAX];
    size_t dirname_count, filename_count;

    if (!sp_priv_walk_scan(dir, follow_symlinks, dirnames, &dirname_count, filenames, &filename_count)) {
        if (on_error) on_error(dir, errno, user_data);
        return true;
    }

    SpWalkEntry entry;
    entry.dirpath = *dir;
    entry.dirnames = dirnames;
    entry.dirname_count = dirname_count;
    entry.filenames = filenames;
    entry.filename_count = filename_count;
    entry.user_data = user_data;

    if (top_down) {
        if (!callback(&entry)) return false;
        for (size_t i = 0; i < entry.dirname_count; i++) {
            SpPath subdir = sp_priv_join_len(dir, entry.dirnames[i], strlen(entry.dirnames[i]));
            if (!sp_priv_walk_recursive(&subdir, top_down, follow_symlinks, callback, on_error, user_data))
                return false;
        }
    } else {
        for (size_t i = 0; i < dirname_count; i++) {
            SpPath subdir = sp_priv_join_len(dir, dirnames[i], strlen(dirnames[i]));
            if (!sp_priv_walk_recursive(&subdir, top_down, follow_symlinks, callback, on_error, user_data))
                return false;
        }

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

/* ============ Fluent API Implementation ============ */
#ifdef SNAKEPATH_FLUENT

#if defined(_MSC_VER) && defined(_WINDLL)
#error "snakepath fluent API uses __declspec(thread) which is unsafe in DLLs loaded via LoadLibrary"
#endif

#if defined(__cplusplus) && __cplusplus >= 201103L
  #define SP_TLS thread_local
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
  #define SP_TLS _Thread_local
#elif defined(__GNUC__) || defined(__clang__)
  #define SP_TLS __thread
#elif defined(_MSC_VER)
  #define SP_TLS __declspec(thread)
#else
  #error "snakepath fluent API requires thread-local storage (__thread, _Thread_local, or __declspec(thread))"
#endif

static SP_TLS SpPath sp_priv_f_ctx;
static SP_TLS bool sp_priv_f_ctx_active = false;

static void sp_priv_f_done(void) {
    sp_priv_f_ctx_active = false;
}

static SpPathOp sp_priv_f_pathop(int error) {
    SpPathOp r;
    r.path = sp_priv_f_ctx;
    sp_priv_f_done();
    r.error = error;
    return r;
}

static int sp_priv_mkdir_to_error(int r) {
    switch (r) {
        case SP_MKDIR_OK:                 return SP_OK;
        case SP_MKDIR_ERR_EXISTS:         return SP_ERR_EXISTS;
        case SP_MKDIR_ERR_NOT_FOUND:      return SP_ERR_NOT_FOUND;
        case SP_MKDIR_ERR_NOT_DIR:        return SP_ERR_NOT_DIR;
        case SP_MKDIR_ERR_PERMISSION:     return SP_ERR_PERMISSION;
        case SP_MKDIR_ERR_EXISTS_NOT_DIR: return SP_ERR_EXISTS_NOT_DIR;
        default:                          return SP_ERR_OTHER_OP;
    }
}

#define SP_F_TERM(ret, name, params, expr) \
    static ret sp_priv_f_##name##_ params { sp_priv_f_done(); return (expr); }
#define SP_F_PROC(name, params, expr) \
    static void sp_priv_f_##name##_ params { sp_priv_f_done(); expr; }

SP_F_TERMINATOR_METHODS(SP_F_TERM, SP_F_PROC)

#define SP_F_CHAIN_DECL(name, params, expr) static SpPrivDontUseThisDirectly_ *sp_priv_f_##name##_ params;
SP_F_CHAIN_METHODS(SP_F_CHAIN_DECL)

static SpPrivDontUseThisDirectly_ sp_priv_f_instance = {
#define SP_F_TERM_INIT(ret, name, params, expr) sp_priv_f_##name##_,
#define SP_F_PROC_INIT(name, params, expr) sp_priv_f_##name##_,
    SP_F_TERMINATOR_METHODS(SP_F_TERM_INIT, SP_F_PROC_INIT)
#undef SP_F_PROC_INIT
#undef SP_F_TERM_INIT
#define SP_F_CHAIN_INIT(name, params, expr) sp_priv_f_##name##_,
    SP_F_CHAIN_METHODS(SP_F_CHAIN_INIT)
#undef SP_F_CHAIN_INIT
};

static SpPrivDontUseThisDirectly_ *sp_priv_f_chain(SpPath path) {
    sp_priv_f_ctx = path;
    return &sp_priv_f_instance;
}

#define SP_F_CHAIN(name, params, expr) \
    static SpPrivDontUseThisDirectly_ *sp_priv_f_##name##_ params { return sp_priv_f_chain(expr); }
SP_F_CHAIN_METHODS(SP_F_CHAIN)

#undef SP_F_CHAIN
#undef SP_F_CHAIN_DECL
#undef SP_F_PROC
#undef SP_F_TERM
#undef SP_F_CHAIN_METHODS
#undef SP_F_TERMINATOR_METHODS

SpPrivDontUseThisDirectly_ *sp_fluent_init_(SpPath p) {
    assert(!sp_priv_f_ctx_active && "snakepath fluent API: previous chain not terminated");
    sp_priv_f_ctx_active = true;
    sp_priv_f_ctx = p;
    return &sp_priv_f_instance;
}

#endif /* SNAKEPATH_FLUENT */

#ifdef __cplusplus
}
#endif

#endif /* SNAKEPATH_IMPLEMENTATION */
