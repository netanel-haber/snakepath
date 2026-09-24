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

#define SP_MATCH_YES 1          /* Pattern matched */
#define SP_MATCH_NO 0           /* Pattern did not match */
#define SP_MATCH_ERR_EMPTY -1   /* Empty pattern */

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

static inline size_t sp_priv_copy_trunc(char *dst, size_t cap, const char *src, size_t len) {
    if (cap == 0) return 0;
    size_t n = len < cap - 1 ? len : cap - 1;
    if (n > 0) memcpy(dst, src, n);
    dst[n] = '\0';
    return n;
}

static inline SpTerm sp_priv_term(const char *data, size_t len) {
    SpTerm t = SP_PRIV_ZERO;
    if (data && len > 0) t.len = sp_priv_copy_trunc(t.buf, SP_TERM_MAX, data, len);
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
    int error; /* SP_OK, SP_ERR_INVALID_ARG (pattern has no parts) or SP_ERR_UNSUPPORTED (anchored pattern) */
    struct {
        char pattern_buf[SP_GLOB_PATTERN_MAX];
        size_t seg_offsets[SP_GLOB_MAX_SEGMENTS];
        size_t seg_count;
        bool case_insensitive;
        bool case_pedantic; /* explicit case sensitivity: literal parts are matched against listings too */
        bool recurse_symlinks;
        bool pending;       /* path is the next match */
        SpPath path;        /* each frame's directory is a prefix of it */
        /* A frame lists the directory path[0..path_len) for wildcard part `seg`, or (walk) for the '**' parts
           [from, seg), matched against paths below path[0..root_len) */
        struct { void *handle; size_t path_len, seg, from, root_len; bool walk; } stack[SP_GLOB_MAX_DEPTH];
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
size_t sp_as_uri(const SpPath *p, char *buf, size_t buf_size); /* 0 for relative paths */
SpPath sp_from_uri(const char *uri, SpFlavor flavor);           /* error path unless an absolute file: URI */
bool sp_path_eq(const SpPath *a, const SpPath *b);
static inline bool sp_path_ne(const SpPath *a, const SpPath *b) { return !sp_path_eq(a, b); }
int sp_path_cmp(const SpPath *a, const SpPath *b);
unsigned long sp_path_hash(const SpPath *p);
int sp_match_ex(const SpPath *p, const char *pattern, int case_sensitive); /* Returns SP_MATCH_* codes */
#define SP_MATCH(p, pattern) sp_match_ex((p), (pattern), -1)
bool sp_full_match(const SpPath *p, const char *pattern, int case_sensitive); /* case_sensitive: -1 = flavor default */
bool sp_is_reserved(const SpPath *p);
#define SP_FILE_TYPE_QUERIES(X)        \
    X(symlink, S_IFLNK, false)         \
    X(block_device, S_IFBLK, true)     \
    X(char_device, S_IFCHR, true)      \
    X(fifo, S_IFIFO, true)             \
    X(socket, S_IFSOCK, true)
#define SP_DECLARE_FILE_TYPE_QUERY(name, type_mask, follow_symlinks) bool sp_is_##name(const SpPath *p);
SP_FILE_TYPE_QUERIES(SP_DECLARE_FILE_TYPE_QUERY)
#undef SP_DECLARE_FILE_TYPE_QUERY
bool sp_exists(const SpPath *p, bool follow_symlinks);
bool sp_is_dir(const SpPath *p, bool follow_symlinks);
bool sp_is_file(const SpPath *p, bool follow_symlinks);
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

#define SP_MKDIR_DEF_MODE 0777

int sp_mkdir(const SpPath *p, unsigned int mode, bool parents, bool exist_ok);  /* returns SP_OK / SP_ERR_* */

/* One code space for operation results and failed SpPath results (sp_path_error_code),
 * so sp_error_str() describes either */
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
    SP_ERR_OTHER_OP,
    SP_ERR_NOT_RELATIVE,  /* Not relative to other path */
    SP_ERR_NO_NAME,       /* Path has no usable name */
    SP_ERR_INVALID_ARG,   /* Invalid argument (name/stem/suffix, empty glob pattern, same copy source and target) */
    SP_ERR_UNSUPPORTED,   /* Unsupported operation (non-relative glob pattern) */
    SP_ERR_NONE = SP_OK,  /* No error (or empty path) */
    SP_ERR_OTHER = SP_ERR /* Other error (I/O, permission, etc.) */
};

const char *sp_error_str(int error);

typedef struct { SpPath path; int error; } SpPathOp;

bool sp_touch(const SpPath *p, unsigned int mode, bool exist_ok);
bool sp_unlink(const SpPath *p, bool missing_ok);
bool sp_rmdir(const SpPath *p);
SpPath sp_rename(const SpPath *p, const SpPath *target);
SpPath sp_replace(const SpPath *p, const SpPath *target);
SpPath sp_copy(const SpPath *p, const SpPath *target, bool follow_symlinks, bool preserve_metadata); /* recursive */
SpPath sp_copy_into(const SpPath *p, const SpPath *target_dir, bool follow_symlinks, bool preserve_metadata);
SpPath sp_move(const SpPath *p, const SpPath *target); /* rename, or copy + delete across filesystems */
SpPath sp_move_into(const SpPath *p, const SpPath *target_dir);
bool sp_chmod(const SpPath *p, unsigned int mode);

typedef struct { size_t bytes; int error; } SpIOResult;  /* error: SP_OK / SP_ERR_* */

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

/* Glob iterator - iterate over paths matching a relative pattern
 * sp_glob_begin:  Initialize iterator; it.error is set for an empty or non-relative pattern
 * sp_glob_next:   Get next match, returns true if found (match written to out)
 * sp_glob_end:    Close iterator (must be called to release directory handles)
 * sp_rglob_begin: Like sp_glob_begin but prepends "**\/" to pattern
 * recurse_symlinks: whether "**" descends into symlinked directories
 */
SpGlobIter sp_glob_begin(const SpPath *base, const char *pattern, SpCaseSensitivity cs, bool recurse_symlinks);
bool sp_glob_next(SpGlobIter *it, SpPath *out);
void sp_glob_end(SpGlobIter *it);
SpGlobIter sp_rglob_begin(const SpPath *base, const char *pattern, SpCaseSensitivity cs, bool recurse_symlinks);

/* Glob foreach macro - iterates all matches, auto-closes on completion */
#define SP_GLOB_FOREACH(base, pattern, match_var) \
    for (struct { SpGlobIter it; int done; } sp_gctx_ = { sp_glob_begin(base, pattern, SP_CASE_PLATFORM_DEFAULT, false), 0 }; \
         !sp_gctx_.done; sp_glob_end(&sp_gctx_.it), sp_gctx_.done = 1) \
    for (SpPath match_var; sp_glob_next(&sp_gctx_.it, &match_var); )

#define SP_RGLOB_FOREACH(base, pattern, match_var) \
    for (struct { SpGlobIter it; int done; } sp_gctx_ = { sp_rglob_begin(base, pattern, SP_CASE_PLATFORM_DEFAULT, false), 0 }; \
         !sp_gctx_.done; sp_glob_end(&sp_gctx_.it), sp_gctx_.done = 1) \
    for (SpPath match_var; sp_glob_next(&sp_gctx_.it, &match_var); )

/* Error checking for path results */
static inline bool sp_path_is_error(const SpPath *p) { return p->len == 0 && p->buf[0] != SP_ERR_NONE; }
static inline int sp_path_error_code(const SpPath *p) {
    return p->len == 0 ? SP_PRIV_CAST(int, SP_PRIV_CAST(unsigned char, p->buf[0])) : 0;
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
    X_TERM(bool, is_file, (bool follow_symlinks), sp_is_file(&sp_priv_f_ctx, follow_symlinks))                        \
    X_TERM(bool, is_dir, (bool follow_symlinks), sp_is_dir(&sp_priv_f_ctx, follow_symlinks))                          \
    X_TERM(bool, exists, (bool follow_symlinks), sp_exists(&sp_priv_f_ctx, follow_symlinks))                          \
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
    X_TERM(bool, full_match, (const char *pattern), sp_full_match(&sp_priv_f_ctx, pattern, -1))                        \
    X_TERM(SpPathOp, mkdir, (unsigned int mode, bool parents, bool exist_ok),                                         \
           sp_priv_f_pathop(sp_mkdir(&sp_priv_f_ctx, mode, parents, exist_ok)))              \
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
    X(replace, (const SpPath *target), sp_replace(&sp_priv_f_ctx, target))                                             \
    X(copy, (const SpPath *target, bool follow_symlinks, bool preserve_metadata),                                      \
      sp_copy(&sp_priv_f_ctx, target, follow_symlinks, preserve_metadata))                                             \
    X(copy_into, (const SpPath *target_dir, bool follow_symlinks, bool preserve_metadata),                             \
      sp_copy_into(&sp_priv_f_ctx, target_dir, follow_symlinks, preserve_metadata))                                    \
    X(move, (const SpPath *target), sp_move(&sp_priv_f_ctx, target))                                                   \
    X(move_into, (const SpPath *target_dir), sp_move_into(&sp_priv_f_ctx, target_dir))

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
extern int gethostname(char *name, size_t len);
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

static inline char sp_priv_tolower(char c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

static int sp_priv_str_cmp_case(const char *a, size_t alen, const char *b, size_t blen, bool case_insensitive) {
    size_t min_len = alen < blen ? alen : blen;
    if (case_insensitive) {
        for (size_t i = 0; i < min_len; i++) {
            unsigned char ca = SP_PRIV_CAST(unsigned char, sp_priv_tolower(a[i]));
            unsigned char cb = SP_PRIV_CAST(unsigned char, sp_priv_tolower(b[i]));
            if (ca != cb) return ca < cb ? -1 : 1;
        }
    } else {
        int cmp = memcmp(a, b, min_len);
        if (cmp != 0) return cmp;
    }
    return alen < blen ? -1 : (alen > blen ? 1 : 0);
}
static int sp_priv_str_cmp_flavor(const char *a, size_t alen, const char *b, size_t blen, SpFlavor flavor) {
    return sp_priv_str_cmp_case(a, alen, b, blen, sp_priv_is_windows_flavor(flavor));
}

/* Flavor-aware string view comparison (case-insensitive for Windows) */
static inline bool sp_priv_sv_eq_flavor(SpStr a, SpStr b, SpFlavor flavor) {
    return sp_priv_str_cmp_flavor(a.data, a.len, b.data, b.len, flavor) == 0;
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
static inline void sp_priv_terminate(SpPath *p) { p->buf[p->len] = '\0'; }
static inline SpPath sp_priv_path_from_raw(const char *s, size_t len, SpFlavor flavor) {
    SpPath p = SP_PRIV_ZERO;
    p.flavor = flavor;
    p.len = len >= SP_PATH_MAX ? SP_PATH_MAX - 1 : len;
    if (s && p.len > 0) memcpy(p.buf, s, p.len);
    sp_priv_terminate(&p);
    return p;
}
static inline SpPath sp_priv_empty_path(SpFlavor flavor) { return sp_priv_path_from_raw(NULL, 0, flavor); }
static inline size_t sp_priv_find_sep(const char *s, size_t len, size_t pos, SpFlavor flavor) {
    while (pos < len && !sp_priv_is_sep(s[pos], flavor)) pos++;
    return pos;
}
static inline size_t sp_priv_skip_seps(const char *s, size_t len, size_t pos, SpFlavor flavor) {
    while (pos < len && sp_priv_is_sep(s[pos], flavor)) pos++;
    return pos;
}
static inline size_t sp_priv_rfind_sep(const char *s, size_t pos, size_t stop, SpFlavor flavor) {
    while (pos > stop && !sp_priv_is_sep(s[pos - 1], flavor)) pos--;
    return pos;
}

static inline bool sp_priv_has_drive(const char *s, size_t len, SpFlavor flavor) {
    if (!sp_priv_is_windows_flavor(flavor)) return false;
    return len >= 2 && !sp_priv_is_sep(s[0], flavor) && s[1] == ':';
}

static inline bool sp_priv_is_unc(const char *s, size_t len, SpFlavor flavor) {
    if (!sp_priv_is_windows_flavor(flavor)) return false;
    return len >= 2 && sp_priv_is_sep(s[0], flavor) && sp_priv_is_sep(s[1], flavor);
}

/* Returns the drive length and stores the root length (ntpath.splitroot plus pathlib's implicit root).
 * A UNC drive is //server/share or //?/UNC/server/share (either part possibly empty) up to the next separator;
 * without one the drive is the whole path, and a complete //server/share gets a root past len. */
static size_t sp_priv_split_anchor(const char *s, size_t len, SpFlavor flavor, size_t *root) {
    if (!sp_priv_is_windows_flavor(flavor)) {
        /* POSIX: paths starting with exactly // have root // */
        bool two = len >= 2 && s[0] == '/' && s[1] == '/' && (len == 2 || s[2] != '/');
        *root = two ? 2 : (len > 0 && s[0] == '/') ? 1 : 0;
        return 0;
    }
    if (sp_priv_is_unc(s, len, flavor)) {
        bool unc_prefix = len >= 8 && s[2] == '?' && sp_priv_is_sep(s[3], flavor) &&
                          sp_priv_str_cmp_case(s + 4, 3, "UNC", 3, true) == 0 && sp_priv_is_sep(s[7], flavor);
        size_t start = unc_prefix ? 8 : 2;
        size_t server = sp_priv_find_sep(s, len, start, flavor);
        size_t share = server < len ? sp_priv_find_sep(s, len, server + 1, flavor) : len;
        if (share < len) {
            *root = 1;
            return share;
        }
        /* Devices like //./x and //?/x (a server of "", "?", "." or "?.") get no implicit root */
        size_t n = server - start;
        bool device = !unc_prefix && (n == 0 || (n == 1 && memchr("?.", s[start], 2)) || (n == 2 && memcmp(s + start, "?.", 2) == 0));
        *root = server + 1 < len && !device ? 1 : 0;
        return len;
    }
    size_t drive = sp_priv_has_drive(s, len, flavor) ? 2 : 0;
    *root = (drive < len && sp_priv_is_sep(s[drive], flavor)) ? 1 : 0;
    return drive;
}

static size_t sp_priv_drive_len(const char *s, size_t len, SpFlavor flavor) {
    size_t root;
    return sp_priv_split_anchor(s, len, flavor, &root);
}

static size_t sp_priv_anchor_len(const char *s, size_t len, SpFlavor flavor) {
    size_t root, drive = sp_priv_split_anchor(s, len, flavor, &root);
    return drive + root;
}

static void sp_priv_normalize(char *buf, size_t *len, SpFlavor flavor) {
    char sep = sp_priv_sep(flavor);
    size_t root, drive = sp_priv_split_anchor(buf, *len, flavor, &root);
    size_t j = drive + root < *len ? drive + root : *len;

    /* Preserve anchor as-is but normalize its separators; a complete UNC drive gets its implicit root */
    for (size_t i = 0; i < j; i++)
        if (sp_priv_is_sep(buf[i], flavor)) buf[i] = sep;
    if (drive + root > *len && j + 1 < SP_PATH_MAX) buf[j++] = sep;

    /* Copy each remaining part followed by one separator, skipping '.' parts unless one leads an unanchored path
       and protects a drive-like next part from drive parsing ('./c:a' stays '.\c:a', but 'a/./c:a' becomes 'a\c:a') */
    for (size_t i = j; (i = sp_priv_skip_seps(buf, *len, i, flavor)) < *len;) {
        size_t end = sp_priv_find_sep(buf, *len, i, flavor);
        if (end - i != 1 || buf[i] != '.' ||
            (j == 0 && sp_priv_has_drive(buf + sp_priv_skip_seps(buf, *len, end, flavor),
                                         *len - sp_priv_skip_seps(buf, *len, end, flavor), flavor))) {
            memmove(buf + j, buf + i, end - i);
            j += end - i;
            if (end < *len) buf[j++] = sep;
        }
        i = end;
    }
    /* Remove trailing sep unless it's part of the anchor */
    if (j > sp_priv_anchor_len(buf, j, flavor) && sp_priv_is_sep(buf[j - 1], flavor)) j--;
    buf[j] = '\0';
    *len = j;
}

static inline SpPath sp_priv_error_path(SpFlavor flavor, int err_code) {
    SpPath p = sp_priv_empty_path(flavor);
    p.buf[0] = SP_PRIV_CAST(char, err_code);
    return p;
}

static inline size_t sp_priv_parent_len_raw(const char *buf, size_t len, SpFlavor flavor) {
    size_t anchor = sp_priv_anchor_len(buf, len, flavor);
    if (len <= anchor) return len;
    size_t i = sp_priv_rfind_sep(buf, len, anchor, flavor);
    if (i > anchor) i--;
    /* The "." protecting a drive-like part ('.\c:') is not a parent of its own: the parent is the empty path */
    if (anchor == 0 && i == 1 && buf[0] == '.') return 0;
    return i <= anchor ? anchor : i;
}

SpPath sp_path_from_n(const char *s, size_t len, SpFlavor flavor) {
    SP_ASSERT_FLAVOR(flavor);
    SpPath p = sp_priv_path_from_raw(s, len, flavor);
    sp_priv_normalize(p.buf, &p.len, p.flavor);
    return p;
}

SpPath sp_path_new(const char *s, SpPathOpts opts) {
    return sp_path_from_n(s, s ? strlen(s) : 0, opts.flavor);
}

SpPath sp_path_convert(const char *s, SpFlavor src_flavor, SpFlavor dest_flavor) {
    SP_ASSERT_FLAVOR(src_flavor);
    SP_ASSERT_FLAVOR(dest_flavor);
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
    if (out_size == 0) return;
    if (p->len == 0) {
        sp_priv_copy_trunc(out, out_size, ".", 1);
        return;
    }
    size_t n = p->len < out_size - 1 ? p->len : out_size - 1;
    for (size_t i = 0; i < n; i++) {
        out[i] = (p->buf[i] == sp_priv_sep(p->flavor)) ? '/' : p->buf[i];
    }
    out[n] = '\0';
}

SpTerm sp_drive(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    return sp_priv_term(p->buf, sp_priv_drive_len(p->buf, p->len, p->flavor));
}

SpTerm sp_root(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    size_t root, drive = sp_priv_split_anchor(p->buf, p->len, p->flavor, &root);
    return sp_priv_term(p->buf + drive, drive + root > p->len ? p->len - drive : root);
}

SpTerm sp_anchor(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    size_t alen = sp_priv_anchor_len(p->buf, p->len, p->flavor);
    return sp_priv_term(p->buf, alen > p->len ? p->len : alen);
}

/* Private: get name as SpStr pointing into path buffer (for internal use) */
static inline SpStr sp_priv_name_sv(const SpPath *p) {
    size_t anchor = sp_priv_anchor_len(p->buf, p->len, p->flavor);
    if (anchor == p->len) return SP_PRIV_STR(p->buf + p->len, 0);
    size_t i = sp_priv_rfind_sep(p->buf, p->len, anchor, p->flavor);
    if (i == 0 && p->len - i == 1 && p->buf[i] == '.') return SP_PRIV_STR(SP_PRIV_NULL, 0);
    return SP_PRIV_STR(p->buf + i, p->len - i);
}

SpTerm sp_name(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpStr sv = sp_priv_name_sv(p);
    return sp_priv_term(sv.data, sv.len);
}

/* From the last '.' after the name's leading dots; "a." has suffix "." */
static inline SpStr sp_priv_suffix_sv(SpStr name) {
    size_t lead = 0;
    while (lead < name.len && name.data[lead] == '.') lead++;
    size_t i = name.len;
    while (i > lead && name.data[i - 1] != '.') i--;
    return i > lead ? SP_PRIV_STR(name.data + i - 1, name.len - i + 1) : SP_PRIV_STR(SP_PRIV_NULL, 0);
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
    /* Each '.' after the leading dots starts a suffix, even an empty one ("a..b" -> ".", ".b") */
    size_t i = 0;
    while (i < name.len && name.data[i] == '.') i++;
    while (i < name.len && name.data[i] != '.') i++;
    while (i < name.len && r.count < SP_MAX_SUFFIXES) {
        size_t end = i + 1;
        while (end < name.len && name.data[end] != '.') end++;
        r.items[r.count].data = name.data + i;
        r.items[r.count++].len = end - i;
        i = end;
    }
    return r;
}

SpPath sp_parent(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    return sp_priv_path_from_raw(p->buf, sp_priv_parent_len_raw(p->buf, p->len, p->flavor), p->flavor);
}

SpPartsIter sp_parts_begin(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpPartsIter it = SP_PRIV_ZERO;
    it.path = p;
    it.anchor_done = sp_priv_anchor_len(p->buf, p->len, p->flavor) == 0;
    return it;
}

bool sp_parts_next(SpPartsIter *it, SpStr *out) {
    const SpPath *p = it->path;
    size_t anchor = sp_priv_anchor_len(p->buf, p->len, p->flavor);
    if (!it->anchor_done) {
        out->data = p->buf;
        out->len = anchor;
        it->anchor_done = true;
        it->pos = anchor;
        return true;
    }
retry:
    it->pos = sp_priv_skip_seps(p->buf, p->len, it->pos, p->flavor);
    if (it->pos >= p->len) return false;
    size_t start = it->pos;
    it->pos = sp_priv_find_sep(p->buf, p->len, it->pos, p->flavor);
    /* On Windows, skip leading '.' protecting a drive letter */
    if (start == 0 && it->pos - start == 1 && p->buf[start] == '.') {
        size_t ns = sp_priv_skip_seps(p->buf, p->len, it->pos, p->flavor);
        if (sp_priv_has_drive(p->buf + ns, p->len - ns, p->flavor)) {
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
    *out = sp_priv_path_from_raw(p->buf, it->current_len, p->flavor);
    if (out->len == 0) {
        it->done = true;
        return true;
    }
    size_t next_len = sp_priv_parent_len_raw(p->buf, out->len, out->flavor);
    it->done = (next_len == out->len);
    it->current_len = next_len;
    return true;
}

/* Append other to r (optionally separator-delimited), then terminate and normalize */
static SpPath sp_priv_join_finish(SpPath r, const char *s, size_t len, bool add_sep) {
    if (add_sep && r.len > 0 && !sp_priv_is_sep(r.buf[r.len - 1], r.flavor)) sp_priv_append_sep(&r);
    sp_priv_append_cstr(&r, s, len);
    sp_priv_terminate(&r);
    sp_priv_normalize(r.buf, &r.len, r.flavor);
    return r;
}

/* Internal length-aware join - handles embedded nulls correctly */
static SpPath sp_priv_join_len(const SpPath *base, const char *other, size_t olen) {
    SpFlavor flavor = base->flavor;
    size_t root, drive = sp_priv_split_anchor(base->buf, base->len, flavor, &root);
    /* ntpath.join puts no separator after a rootless drive ending in ':' (like "c:") */
    bool bare_drive = drive > 0 && drive == base->len && root == 0 && base->buf[drive - 1] == ':';

    /* Check if other has root */
    if (olen > 0 && sp_priv_is_sep(other[0], flavor)) {
        if (sp_priv_is_unc(other, olen, flavor) || drive == 0) return sp_path_from_n(other, olen, flavor);
        /* Root only - keep drive from base */
        return sp_priv_join_finish(sp_priv_path_from_raw(base->buf, drive, flavor), other, olen, false);
    }

    /* Check if other has drive */
    if (sp_priv_has_drive(other, olen, flavor)) {
        char od = other[0];
        bool same = sp_priv_has_drive(base->buf, base->len, flavor) && sp_priv_tolower(od) == sp_priv_tolower(base->buf[0]);
        if (!same || (olen > 2 && sp_priv_is_sep(other[2], flavor)))
            return sp_path_from_n(other, olen, flavor);
        /* Same drive: keep base path, adopt other's drive-letter case */
        SpPath r = sp_path_copy(base);
        r.buf[0] = od;
        if (olen == 2) return r;
        return sp_priv_join_finish(r, other + 2, olen - 2, !bare_drive);
    }

    /* Relative path - simple join */
    return sp_priv_join_finish(sp_path_copy(base), other, olen, !bare_drive);
}

SpPath sp_join_one(const SpPath *base, const char *other) {
    return sp_join_n(base, other, other ? strlen(other) : 0);
}

SpPath sp_join_n(const SpPath *base, const char *s, size_t len) {
    SP_ASSERT_PATH_INVARIANT(base);
    if (len == 0 || !s) return sp_path_copy(base);
    return sp_priv_join_len(base, s, len);
}

SpPath sp_joinpath(const SpPath *base, const SpPath *other) {
    SP_ASSERT_PATH_INVARIANT(base);
    SP_ASSERT_PATH_INVARIANT(other);
    if (other->len == 0) return sp_path_copy(base);
    if (base->len == 0) return sp_path_copy(other);
    return sp_priv_join_len(base, other->buf, other->len);
}

static SpPath sp_priv_join_parts(SpPath r, const char *const *parts, size_t parts_count, bool null_terminated) {
    if (!parts) return r;
    for (size_t i = 0; null_terminated ? parts[i] != NULL : i < parts_count; i++) {
        const char *part = parts[i];
        if (!part || !part[0]) continue;
        r = sp_priv_join_len(&r, part, strlen(part));
    }
    return r;
}

SpPath sp_join_impl(const SpPath *base, const char **parts) {
    SP_ASSERT_PATH_INVARIANT(base);
    return sp_priv_join_parts(sp_path_copy(base), parts, 0, true);
}

SpPath sp_with_segments(const SpPath *p, const char **parts, size_t parts_count) {
    SP_ASSERT_PATH_INVARIANT(p);
    return sp_priv_join_parts(sp_priv_empty_path(p->flavor), parts, parts_count, false);
}

static SpPath sp_priv_with_name_impl(const SpPath *p, const char *name, size_t nlen) {
    SpPath r = sp_parent(p);

    if (r.len == 0 && sp_priv_has_drive(name, nlen, p->flavor)) {
        r.buf[r.len++] = '.';
        r.buf[r.len++] = sp_priv_sep(p->flavor);
    }
    if (r.len > 0 && !sp_priv_is_sep(r.buf[r.len - 1], p->flavor)) sp_priv_append_sep(&r);
    sp_priv_append_cstr(&r, name, nlen);
    sp_priv_terminate(&r);
    return r;
}

/* with_name(head + tail), where the name must be non-empty, not "." and free of separators */
static SpPath sp_priv_with_name_parts(const SpPath *p, SpStr head, SpStr tail) {
    if (sp_priv_name_sv(p).len == 0) return sp_priv_error_path(p->flavor, SP_ERR_NO_NAME);
    char name[SP_PATH_MAX];
    size_t len = sp_priv_copy_trunc(name, SP_PATH_MAX, head.data, head.len);
    len += sp_priv_copy_trunc(name + len, SP_PATH_MAX - len, tail.data, tail.len);
    if (len == 0 || (len == 1 && name[0] == '.') || sp_priv_find_sep(name, len, 0, p->flavor) < len)
        return sp_priv_error_path(p->flavor, SP_ERR_INVALID_ARG);
    return sp_priv_with_name_impl(p, name, len);
}

SpPath sp_with_name(const SpPath *p, const char *name) {
    SP_ASSERT_PATH_INVARIANT(p);
    return sp_priv_with_name_parts(p, SP_PRIV_STR(name, strlen(name)), SP_PRIV_STR(SP_PRIV_NULL, 0));
}

SpPath sp_with_stem(const SpPath *p, const char *stem) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpStr suffix = sp_priv_suffix_sv(sp_priv_name_sv(p));
    /* A non-empty suffix needs a non-empty stem */
    if (suffix.len > 0 && stem[0] == '\0') return sp_priv_error_path(p->flavor, SP_ERR_INVALID_ARG);
    return sp_priv_with_name_parts(p, SP_PRIV_STR(stem, strlen(stem)), suffix);
}

SpPath sp_with_suffix(const SpPath *p, const char *suffix) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (suffix[0] != '\0' && suffix[0] != '.') return sp_priv_error_path(p->flavor, SP_ERR_INVALID_ARG);
    SpStr name = sp_priv_name_sv(p);
    return sp_priv_with_name_parts(p, SP_PRIV_STR(name.data, name.len - sp_priv_suffix_sv(name).len),
                                   SP_PRIV_STR(suffix, strlen(suffix)));
}

bool sp_is_absolute(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    /* ntpath.isabs: UNC and device paths, or ":\\" after the first character */
    if (sp_priv_is_windows_flavor(p->flavor))
        return sp_priv_is_unc(p->buf, p->len, p->flavor) || (p->len >= 3 && p->buf[1] == ':' && p->buf[2] == '\\');
    return p->len > 0 && p->buf[0] == '/';
}

SpPath sp_cwd(SpFlavor flavor) {
    char buf[SP_PATH_MAX];
    return sp_path_new(sp_priv_getcwd(buf, SP_PATH_MAX) ? buf : "", SP_PRIV_OPTS(flavor));
}

static SpPath sp_priv_absolute_path(const SpPath *p) {
    if (sp_is_absolute(p)) return sp_path_copy(p);
    SpPath cwd = sp_cwd(p->flavor);
    return cwd.len == 0 ? sp_path_copy(p) : sp_priv_join_len(&cwd, p->buf, p->len);
}

SpPath sp_absolute(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    return sp_priv_absolute_path(p);
}

static size_t sp_priv_collect_parts(const SpPath *p, SpStr *out, size_t max) {
    SpPartsIter it = sp_parts_begin(p);
    size_t n = 0;
    while (n < max && sp_parts_next(&it, &out[n])) n++;
    return n;
}

/* CPython walks `other` and its parents up to the first that is p or one of p's parents, stepping out with ".."
 * (walk_up only, and never over a ".." part). The result is p's remaining parts. */
static SpPath sp_priv_relative_to_impl(const SpPath *p, const SpPath *other, bool walk_up) {
    SpStr p_parts[SP_PATH_MAX / 2], o_parts[SP_PATH_MAX / 2];
    size_t p_count = sp_priv_collect_parts(p, p_parts, SP_PATH_MAX / 2); /* anchor first, if any */
    size_t o_count = sp_priv_collect_parts(other, o_parts, SP_PATH_MAX / 2);
    bool o_anchored = sp_priv_anchor_len(other->buf, other->len, other->flavor) > 0;
    bool same_anchoring = (sp_priv_anchor_len(p->buf, p->len, p->flavor) > 0) == o_anchored;
    size_t ups = 0, k = o_count;
    for (;; k--) {
        size_t same = 0;
        while (same < k && same < p_count && sp_priv_sv_eq_flavor(p_parts[same], o_parts[same], p->flavor)) same++;
        if (same_anchoring && same == k) break;
        if (!walk_up || k == o_anchored || (o_parts[k - 1].len == 2 && memcmp(o_parts[k - 1].data, "..", 2) == 0))
            return sp_priv_error_path(p->flavor, SP_ERR_NOT_RELATIVE);
        ups++;
    }
    /* Parts joined by separators, behind a "." if the first would parse as a drive */
    char buf[SP_PATH_MAX];
    size_t len = 0;
    if (ups == 0 && k < p_count && sp_priv_has_drive(p_parts[k].data, p_parts[k].len, p->flavor)) {
        buf[len++] = '.';
        buf[len++] = sp_priv_sep(p->flavor);
    }
    for (size_t i = 0; i < ups + p_count - k; i++) {
        SpStr part = i < ups ? SP_PRIV_STR("..", 2) : p_parts[k + i - ups];
        if (i > 0 && len + 1 < SP_PATH_MAX) buf[len++] = sp_priv_sep(p->flavor);
        len += sp_priv_copy_trunc(buf + len, SP_PATH_MAX - len, part.data, part.len);
    }
    return sp_priv_path_from_raw(buf, len, p->flavor);
}

bool sp_is_relative_to(const SpPath *p, const SpPath *other) {
    SpPath r = sp_relative_to(p, other);
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
    SpPath r = sp_relative_to_parts(p, parts, false);
    return !sp_path_is_error(&r);
}

SpPath sp_relative_to_parts(const SpPath *p, const char **parts, bool walk_up) {
    SpPath other = sp_priv_join_parts(sp_priv_empty_path(p->flavor), parts, 0, true);
    return sp_priv_relative_to_impl(p, &other, walk_up);
}

/* urllib.parse.quote: percent-encode all but letters, digits, "_.-~" and `safe`; false if buf is too small */
static bool sp_priv_quote(const char *s, size_t len, const char *safe, char *buf, size_t buf_size, size_t *pos) {
    static const char hex[] = "0123456789ABCDEF";
    for (size_t i = 0; i < len; i++) {
        unsigned char c = SP_PRIV_CAST(unsigned char, s[i]);
        bool plain = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                     (c != '\0' && (strchr("_.-~", c) || strchr(safe, c)));
        if (*pos + (plain ? 1u : 3u) >= buf_size) return false;
        if (plain) {
            buf[(*pos)++] = SP_PRIV_CAST(char, c);
        } else {
            buf[(*pos)++] = '%';
            buf[(*pos)++] = hex[c >> 4];
            buf[(*pos)++] = hex[c & 0x0F];
        }
    }
    buf[*pos] = '\0';
    return true;
}

/* urllib.request.pathname2url(str(p), add_scheme=True) */
size_t sp_as_uri(const SpPath *p, char *buf, size_t buf_size) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (buf_size > 0) buf[0] = '\0';
    if (buf_size == 0 || !sp_is_absolute(p)) return 0;
    char path[SP_PATH_MAX];
    sp_as_posix(p, path, SP_PATH_MAX);
    size_t len = strlen(path), root, drive = sp_priv_split_anchor(path, len, p->flavor, &root);
    const char *d = path, *prefix = "file://"; /* an explicitly empty authority before a POSIX root */
    if (drive > 0) {
        prefix = "file:";
        /* Device paths drop their "//?/" prefix, keeping "//" for "//?/UNC/server/share" */
        if (drive >= 4 && memcmp(d, "//?/", 4) == 0) {
            d += 4;
            drive -= 4;
            if (drive >= 4 && sp_priv_str_cmp_case(d, 4, "UNC/", 4, true) == 0) {
                d += 4;
                drive -= 4;
                prefix = "file://";
            }
        }
        if (drive == 2 && d[1] == ':') prefix = "file:///";
    }
    size_t pos = 0;
    if (sp_priv_quote(prefix, strlen(prefix), ":/", buf, buf_size, &pos) &&
        sp_priv_quote(d, drive, "/:", buf, buf_size, &pos) &&
        sp_priv_quote(d + drive, len - SP_PRIV_CAST(size_t, d + drive - path), "/", buf, buf_size, &pos))
        return pos;
    buf[0] = '\0';
    return 0;
}

static bool sp_priv_is_local_authority(const char *a, size_t len) {
    if (len == 0 || (len == 9 && memcmp(a, "localhost", 9) == 0)) return true;
    char host[256];
#ifdef SP_WINDOWS
    DWORD size = sizeof(host);
    if (!GetComputerNameExA(ComputerNamePhysicalDnsHostname, host, &size)) return false;
#else
    if (gethostname(host, sizeof(host)) != 0) return false;
    host[sizeof(host) - 1] = '\0';
#endif
    return strlen(host) == len && memcmp(host, a, len) == 0;
}

static int sp_priv_hex_digit(char c) {
    char l = sp_priv_tolower(c);
    return c >= '0' && c <= '9' ? c - '0' : l >= 'a' && l <= 'f' ? l - 'a' + 10 : -1;
}

/* Append s[0..len) to buf, decoding %XX escapes (urllib.parse.unquote) */
static void sp_priv_unquote_append(char *buf, size_t *n, const char *s, size_t len) {
    for (size_t i = 0; i < len && *n + 1 < SP_PATH_MAX; i++) {
        int hi = i + 2 < len && s[i] == '%' ? sp_priv_hex_digit(s[i + 1]) : -1;
        int lo = hi >= 0 ? sp_priv_hex_digit(s[i + 2]) : -1;
        buf[(*n)++] = lo >= 0 ? SP_PRIV_CAST(char, hi * 16 + lo) : s[i];
        if (lo >= 0) i += 2;
    }
}

/* urllib.request.url2pathname(uri, require_scheme=True), which must give an absolute path */
SpPath sp_from_uri(const char *uri, SpFlavor flavor) {
    SP_ASSERT_FLAVOR(flavor);
    SpPath err = sp_priv_error_path(flavor, SP_ERR_INVALID_ARG);
    if (strlen(uri) < 5 || sp_priv_str_cmp_case(uri, 5, "file:", 5, true) != 0) return err;
    const char *path = uri + 5, *auth = path;
    size_t alen = 0;
    if (path[0] == '/' && path[1] == '/') {
        auth = path + 2;
        alen = strcspn(auth, "/?#");
        path = auth + alen;
    }
    size_t plen = strcspn(path, "?#"), n = 0; /* query and fragment are discarded */
    bool local = sp_priv_is_local_authority(auth, alen), pipe = false;
    char buf[SP_PATH_MAX];
    if (!sp_priv_is_windows_flavor(flavor)) {
        if (!local) return err;
    } else if (alen >= 2 && auth[1] == ':') { /* file://c:/file.txt */
        sp_priv_unquote_append(buf, &n, auth, alen);
    } else if (!local) { /* file://server/share/file.txt */
        sp_priv_unquote_append(buf, &n, "//", 2);
        sp_priv_unquote_append(buf, &n, auth, alen);
    } else if (plen >= 3 && memcmp(path, "///", 3) == 0) { /* file://///server/share/file.txt */
        path++;
        plen--;
    } else {
        if (plen >= 3 && path[0] == '/' && (path[2] == ':' || path[2] == '|')) { /* file:///c:/file.txt */
            path++;
            plen--;
        }
        pipe = plen >= 2 && path[1] == '|'; /* older URLs use a pipe after the drive letter */
    }
    sp_priv_unquote_append(buf, &n, path, plen);
    if (pipe) buf[1] = ':';
    SpPath r = sp_path_from_n(buf, n, flavor);
    return sp_is_absolute(&r) ? r : err;
}

bool sp_path_eq(const SpPath *a, const SpPath *b) {
    return a->flavor == b->flavor && sp_path_cmp(a, b) == 0;
}

/* Like CPython: compare the separator-split parts of str() ("." when empty), case-folded on Windows */
int sp_path_cmp(const SpPath *a, const SpPath *b) {
    SP_ASSERT_PATH_INVARIANT(a);
    SP_ASSERT_PATH_INVARIANT(b);
    const char *sa = a->len ? a->buf : ".", *sb = b->len ? b->buf : ".";
    size_t la = a->len ? a->len : 1, lb = b->len ? b->len : 1, i = 0, j = 0;
    char sep = sp_priv_sep(a->flavor);
    for (;;) {
        size_t ea = i, eb = j;
        while (ea < la && sa[ea] != sep) ea++;
        while (eb < lb && sb[eb] != sep) eb++;
        int c = sp_priv_str_cmp_flavor(sa + i, ea - i, sb + j, eb - j, a->flavor);
        if (c != 0) return c;
        if (ea == la || eb == lb) return (eb == lb) - (ea == la); /* the path with fewer parts sorts first */
        i = ea + 1;
        j = eb + 1;
    }
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

/* Next code point of s[*i..len), advancing *i; bytes that aren't valid UTF-8 stand alone (like surrogateescape) */
static unsigned long sp_priv_utf8_next(const char *s, size_t len, size_t *i) {
    unsigned char c = SP_PRIV_CAST(unsigned char, s[(*i)++]);
    size_t extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    if (c < 0x80) return c;
    if (extra == 0 || *i + extra > len) return 0xDC00 + c;
    unsigned long cp = c & (0x3Fu >> extra);
    for (size_t k = 0; k < extra; k++) {
        unsigned char cc = SP_PRIV_CAST(unsigned char, s[*i + k]);
        if ((cc & 0xC0) != 0x80) return 0xDC00 + c;
        cp = (cp << 6) | (cc & 0x3F);
    }
    *i += extra;
    return cp;
}

static bool sp_priv_char_eq(unsigned long a, unsigned long b, bool ci) {
    if (ci && a < 0x80 && b < 0x80) return sp_priv_tolower(SP_PRIV_CAST(char, a)) == sp_priv_tolower(SP_PRIV_CAST(char, b));
    return a == b;
}

/* fnmatch bracket expression at pat[*pi] (just past '['): [seq], [!seq], ranges a-z.
 * Returns -1 (leaving *pi) when unterminated, so the '[' is literal; else whether c matched, with *pi past ']'. */
static int sp_priv_fnmatch_class(const char *pat, size_t plen, size_t *pi, unsigned long c, bool ci) {
    size_t j = *pi;
    bool negate = j < plen && pat[j] == '!';
    if (negate) j++;
    if (j < plen && pat[j] == ']') j++;
    while (j < plen && pat[j] != ']') j++;
    if (j >= plen) return -1;
    bool hit = false;
    for (size_t k = *pi + (negate ? 1 : 0); k < j;) {
        unsigned long lo = sp_priv_utf8_next(pat, j, &k), hi = lo;
        if (k + 1 < j && pat[k] == '-') {
            k++;
            hi = sp_priv_utf8_next(pat, j, &k);
        }
        for (int fold = 0; fold < (ci && c < 0x80 ? 3 : 1); fold++) {
            unsigned long f = fold == 0 ? c : fold == 1 ? SP_PRIV_CAST(unsigned long, sp_priv_tolower(SP_PRIV_CAST(char, c)))
                                                        : (c >= 'a' && c <= 'z' ? c - 32 : c);
            hit = hit || (lo <= f && f <= hi);
        }
    }
    *pi = j + 1;
    return hit != negate;
}

/* fnmatch: * ? [seq] [!seq], code point aware. As in CPython's glob.translate(), '*' and '?' never match a
 * separator, but a bracket expression like [!a] can. */
static bool sp_priv_fnmatch(const char *pat, size_t plen, const char *s, size_t slen, bool ci, SpFlavor flavor) {
    size_t pi = 0, si = 0;
    while (pi < plen) {
        if (pat[pi] == '*') {
            while (pi < plen && pat[pi] == '*') pi++;
            for (size_t k = si;; sp_priv_utf8_next(s, slen, &k)) {
                if (sp_priv_fnmatch(pat + pi, plen - pi, s + k, slen - k, ci, flavor)) return true;
                if (k == slen || sp_priv_is_sep(s[k], flavor)) return false;
            }
        }
        if (si == slen) return false;
        bool sep = sp_priv_is_sep(s[si], flavor);
        unsigned long c = sp_priv_utf8_next(s, slen, &si);
        size_t npi = pi + 1;
        int cls = pat[pi] == '[' ? sp_priv_fnmatch_class(pat, plen, &npi, c, ci) : -1;
        if (cls >= 0) {
            if (!cls) return false;
            pi = npi;
        } else if (pat[pi] == '?') {
            if (sep) return false;
            pi++;
        } else if (!sp_priv_char_eq(sp_priv_utf8_next(pat, plen, &pi), c, ci)) {
            return false;
        }
    }
    return si == slen;
}

#define SP_PRIV_IS_DOUBLESTAR(p, l) ((l) == 2 && (p)[0] == '*' && (p)[1] == '*')

/* Match a whole path string against a pattern like CPython's glob.translate() regex (include_hidden): each pattern
 * part is followed by a separator, '*' alone needs a non-empty segment, and when recursive, '**' spans any number
 * of segments. */
static bool sp_priv_match_path(const char *pat, size_t plen, const char *s, size_t slen, bool ci, bool recursive,
                               SpFlavor flavor) {
    size_t pe = sp_priv_find_sep(pat, plen, 0, flavor);
    bool last = pe == plen;
    const char *rest = pat + pe + 1;
    size_t rlen = last ? 0 : plen - pe - 1;
    if (recursive && SP_PRIV_IS_DOUBLESTAR(pat, pe)) {
        if (last) return true;
        /* A '**' followed by another '**' adds nothing; otherwise it consumes zero or more leading segments */
        if (sp_priv_match_path(rest, rlen, s, slen, ci, recursive, flavor)) return true;
        if (SP_PRIV_IS_DOUBLESTAR(rest, sp_priv_find_sep(rest, rlen, 0, flavor))) return false;
        for (size_t q = 1; q < slen; q++)
            if (sp_priv_is_sep(s[q], flavor) && sp_priv_match_path(rest, rlen, s + q + 1, slen - q - 1, ci, recursive, flavor))
                return true;
        return false;
    }
    for (size_t k = last ? slen : 0; k <= slen; k++) {
        if (!last && (k == slen || !sp_priv_is_sep(s[k], flavor))) continue;
        if ((pe != 1 || pat[0] != '*' || k > 0) && sp_priv_fnmatch(pat, pe, s, k, ci, flavor) &&
            (last || sp_priv_match_path(rest, rlen, s + k + 1, slen - k - 1, ci, recursive, flavor)))
            return true;
    }
    return false;
}

static bool sp_priv_case_insensitive(int case_sensitive, SpFlavor flavor) {
    return case_sensitive == -1 ? sp_priv_is_windows_flavor(flavor) : case_sensitive == 0;
}

bool sp_full_match(const SpPath *p, const char *pattern, int case_sensitive) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpPath pat = sp_path_new(pattern, SP_PRIV_OPTS(p->flavor));
    return sp_priv_match_path(pat.buf, pat.len, p->buf, p->len, sp_priv_case_insensitive(case_sensitive, p->flavor),
                              true, p->flavor);
}

/* Parts of p with the anchor (if any) first */
static size_t sp_priv_anchored_parts(const SpPath *p, SpStr *out, size_t max) {
    size_t n = sp_priv_collect_parts(p, out, max);
    if (n == 0 || out[0].len > 0) return n;
    memmove(out, out + 1, (n - 1) * sizeof(*out));
    return n - 1;
}

int sp_match_ex(const SpPath *p, const char *pattern, int case_sensitive) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpPath pat = sp_path_new(pattern, SP_PRIV_OPTS(p->flavor));
    SpStr path_parts[SP_PATH_MAX / 2], pat_parts[SP_PATH_MAX / 2];
    size_t path_count = sp_priv_anchored_parts(p, path_parts, SP_PATH_MAX / 2);
    size_t pat_count = sp_priv_anchored_parts(&pat, pat_parts, SP_PATH_MAX / 2);
    if (pat_count == 0) return SP_MATCH_ERR_EMPTY;
    if (path_count < pat_count || (path_count > pat_count && sp_priv_anchor_len(pat.buf, pat.len, pat.flavor) > 0))
        return SP_MATCH_NO;
    bool ci = sp_priv_case_insensitive(case_sensitive, p->flavor);
    for (size_t i = 1; i <= pat_count; i++) {
        SpStr pp = pat_parts[pat_count - i], sp = path_parts[path_count - i];
        if (!sp_priv_match_path(pp.data, pp.len, sp.data, sp.len, ci, false, p->flavor)) return SP_MATCH_NO;
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
    static const char *const reserved[] = {"CON", "PRN", "AUX", "NUL", "CONIN$", "CONOUT$"};
    for (size_t i = 0; i < SP_ARRAY_LEN(reserved); i++)
        if (strcmp(upper, reserved[i]) == 0) return true;
    if (len == 4 && upper[3] >= '1' && upper[3] <= '9' &&
        (memcmp(upper, "COM", 3) == 0 || memcmp(upper, "LPT", 3) == 0)) return true;
    return false;
}

static bool sp_priv_has_type(const SpPath *p, unsigned int type_mask, bool follow_symlinks) {
    SpStatResult st = follow_symlinks ? sp_stat(p) : sp_lstat(p);
    return st.valid && (st.sp_mode & S_IFMT) == type_mask;
}

#define SP_DEFINE_FILE_TYPE_QUERY(name, type_mask, follow_symlinks) \
    bool sp_is_##name(const SpPath *p) { return sp_priv_has_type(p, type_mask, follow_symlinks); }
SP_FILE_TYPE_QUERIES(SP_DEFINE_FILE_TYPE_QUERY)
#undef SP_DEFINE_FILE_TYPE_QUERY

bool sp_exists(const SpPath *p, bool follow_symlinks) {
    return (follow_symlinks ? sp_stat(p) : sp_lstat(p)).valid;
}

bool sp_is_dir(const SpPath *p, bool follow_symlinks) { return sp_priv_has_type(p, S_IFDIR, follow_symlinks); }
bool sp_is_file(const SpPath *p, bool follow_symlinks) { return sp_priv_has_type(p, S_IFREG, follow_symlinks); }

static bool sp_priv_path_cstr(const SpPath *p, const char **out) {
    SP_ASSERT_PATH_INVARIANT(p);
    for (size_t i = 0; i < p->len; i++)
        if (p->buf[i] == '\0') return false;
    *out = sp_str(p);
    return true;
}

bool sp_is_mount(const SpPath *p) {
    const char *path_str;
    if (!sp_priv_path_cstr(p, &path_str)) return false;
#ifdef SP_WINDOWS
    char vol_path[SP_PATH_MAX];
    if (!GetVolumePathNameA(path_str, vol_path, SP_PATH_MAX)) return false;
    size_t vlen = strlen(vol_path);
    size_t plen = p->len > 0 ? p->len : 1;
    if (vlen > 0 && vol_path[vlen - 1] == '\\') vlen--;
    if (plen > 0 && (path_str[plen - 1] == '\\' || path_str[plen - 1] == '/')) plen--;
    return sp_priv_str_cmp_case(path_str, plen, vol_path, vlen, true) == 0;
#else
    (void)path_str;
    SpStatResult st_path = sp_lstat(p);
    if (!st_path.valid) return false;
    if ((st_path.sp_mode & S_IFMT) != S_IFDIR) return false;
    SpPath parent = sp_parent(p);
    SpStatResult st_parent = sp_lstat(&parent);
    if (!st_parent.valid) return false;
    return st_path.sp_dev != st_parent.sp_dev || st_path.sp_ino == st_parent.sp_ino;
#endif
}

bool sp_is_junction(const SpPath *p) {
    const char *path_str;
    if (!sp_priv_path_cstr(p, &path_str)) return false;
#ifdef SP_WINDOWS
    DWORD attrs = GetFileAttributesA(path_str);
    if (attrs == INVALID_FILE_ATTRIBUTES) return false;
    if ((attrs & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) !=
        (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) return false;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(path_str, &fd);
    if (h == INVALID_HANDLE_VALUE) return false;
    FindClose(h);
    return fd.dwReserved0 == 0xA0000003;
#else
    (void)path_str;
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
    const char *path_str;
    if (!sp_priv_path_cstr(p, &path_str)) return result;

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
    return a->valid && b->valid &&
           a->sp_mode == b->sp_mode &&
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
    return sp_priv_stat_impl(p, false);
}

SpPath sp_readlink(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpPath result = sp_priv_empty_path(p->flavor);
    const char *path_str;
    if (!sp_priv_path_cstr(p, &path_str)) return sp_priv_error_path(p->flavor, SP_ERR_OTHER);

#ifdef SP_WINDOWS
    HANDLE h = CreateFileA(path_str, GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           NULL, OPEN_EXISTING,
                           FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (h == INVALID_HANDLE_VALUE) return sp_priv_error_path(p->flavor, SP_ERR_OTHER);

    char reparse_buf[16384];
    DWORD bytes_returned;
    if (!DeviceIoControl(h, 0x000900A8 /* FSCTL_GET_REPARSE_POINT */,
                         NULL, 0, reparse_buf, sizeof(reparse_buf),
                         &bytes_returned, NULL)) {
        CloseHandle(h);
        return sp_priv_error_path(p->flavor, SP_ERR_OTHER);
    }
    CloseHandle(h);

    DWORD tag = *(DWORD *)reparse_buf;
    size_t data_offset = (tag == 0xA000000C) ? 20 : (tag == 0xA0000003) ? 16 : 0;
    if (data_offset == 0) return sp_priv_error_path(p->flavor, SP_ERR_OTHER);

    WORD print_offset = *(WORD *)(reparse_buf + 12);
    WORD print_len = *(WORD *)(reparse_buf + 14);
    WCHAR *print_name = (WCHAR *)(reparse_buf + data_offset + print_offset);
    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, print_name, print_len / 2,
                                       result.buf, SP_PATH_MAX - 1, NULL, NULL);
    if (utf8_len <= 0) return sp_priv_error_path(p->flavor, SP_ERR_OTHER);
    result.len = SP_PRIV_CAST(size_t, utf8_len);
    sp_priv_terminate(&result);
    sp_priv_normalize(result.buf, &result.len, result.flavor);
#else
    ssize_t len = readlink(path_str, result.buf, SP_PATH_MAX - 1);
    if (len < 0) return sp_priv_error_path(p->flavor, SP_ERR_OTHER);
    result.len = SP_PRIV_CAST(size_t, len);
    sp_priv_terminate(&result);
#endif

    return result;
}

SpPath sp_resolve(const SpPath *p, bool strict) {
    SP_ASSERT_PATH_INVARIANT(p);
    const char *path_str;
    if (!sp_priv_path_cstr(p, &path_str))
        return strict ? sp_priv_error_path(p->flavor, SP_ERR_OTHER) : sp_priv_absolute_path(p);

#ifdef SP_WINDOWS
    char full_path[SP_PATH_MAX];
    DWORD len = GetFullPathNameA(path_str, SP_PATH_MAX, full_path, NULL);
    if (len == 0 || len >= SP_PATH_MAX)
        return strict ? sp_priv_error_path(p->flavor, SP_ERR_OTHER) : sp_priv_absolute_path(p);
    if (strict && GetFileAttributesA(full_path) == INVALID_FILE_ATTRIBUTES)
        return sp_priv_error_path(p->flavor, SP_ERR_OTHER);

    return sp_path_from_n(full_path, len, p->flavor);
#else
    char resolved[SP_PATH_MAX];
    if (realpath(path_str, resolved) != NULL) {
        return sp_path_from_n(resolved, strlen(resolved), p->flavor);
    } else {
        if (strict) return sp_priv_error_path(p->flavor, SP_ERR_OTHER);
        SpPath abs_path = sp_priv_absolute_path(p);
        size_t curr_len = abs_path.len;
        while (curr_len > 0) {
            SpPath curr_path = sp_priv_path_from_raw(abs_path.buf, curr_len, abs_path.flavor);
            if (realpath(sp_str(&curr_path), resolved) != NULL) {
                SpPath result = sp_path_from_n(resolved, strlen(resolved), p->flavor);
                size_t rest = curr_len;
                while (rest < abs_path.len && abs_path.buf[rest] == '/') rest++;
                if (rest < abs_path.len)
                    result = sp_join_n(&result, abs_path.buf + rest, abs_path.len - rest);
                return result;
            }
            size_t parent_len = sp_priv_parent_len_raw(abs_path.buf, curr_len, abs_path.flavor);
            if (parent_len == curr_len) break;
            curr_len = parent_len;
        }
        return abs_path;
    }
#endif
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
    SpStatResult stat_a = sp_stat(a);
    SpStatResult stat_b = sp_stat(b);

    return stat_a.valid && stat_b.valid && stat_a.sp_dev == stat_b.sp_dev && stat_a.sp_ino == stat_b.sp_ino;
}

static int sp_priv_mkdir_parents(const SpPath *p) {
    SpPath parent = sp_parent(p);
    if (sp_path_eq(&parent, p) || parent.len == 0) return SP_OK;
    int r = sp_mkdir(&parent, SP_MKDIR_DEF_MODE, true, true);
    return (r == SP_ERR_EXISTS_NOT_DIR) ? SP_ERR_NOT_DIR : r;
}

int sp_mkdir(const SpPath *p, unsigned int mode, bool parents, bool exist_ok) {
    const char *path_str;
    if (mode == 0) mode = SP_MKDIR_DEF_MODE;
    if (!sp_priv_path_cstr(p, &path_str)) return SP_ERR_OTHER_OP;
    if (parents) {
        int r = sp_priv_mkdir_parents(p);
        if (r != SP_OK) return r;
    }
#ifdef SP_WINDOWS
    (void)mode;
    DWORD attrs = GetFileAttributesA(path_str);
    if (attrs != INVALID_FILE_ATTRIBUTES)
        return (attrs & FILE_ATTRIBUTE_DIRECTORY) ? (exist_ok ? SP_OK : SP_ERR_EXISTS) : SP_ERR_EXISTS_NOT_DIR;
    if (CreateDirectoryA(path_str, NULL)) return SP_OK;
    DWORD err = GetLastError();
    if (err == ERROR_ALREADY_EXISTS) return exist_ok ? SP_OK : SP_ERR_EXISTS;
    if (err == ERROR_PATH_NOT_FOUND) return SP_ERR_NOT_FOUND;
    if (err == ERROR_ACCESS_DENIED) return SP_ERR_PERMISSION;
    return SP_ERR_OTHER_OP;
#else
    struct stat st;
    if (mkdir(path_str, SP_PRIV_CAST(mode_t, mode)) == 0) return SP_OK;
    if (errno == EEXIST)
        return (stat(path_str, &st) == 0 && S_ISDIR(st.st_mode)) ? (exist_ok ? SP_OK : SP_ERR_EXISTS)
                                                                 : SP_ERR_EXISTS_NOT_DIR;
    if (errno == ENOENT) return SP_ERR_NOT_FOUND;
    if (errno == EACCES || errno == EPERM) return SP_ERR_PERMISSION;
    if (errno == ENOTDIR) return SP_ERR_NOT_DIR;
    return SP_ERR_OTHER_OP;
#endif
}

bool sp_touch(const SpPath *p, unsigned int mode, bool exist_ok) {
    const char *path_str;
    if (!sp_priv_path_cstr(p, &path_str)) return false;
    if (mode == 0) mode = 0666;
    bool exists = sp_exists(p, true);
    if (exists && !exist_ok) return false;

#ifdef SP_WINDOWS
    DWORD access = exists ? FILE_WRITE_ATTRIBUTES : GENERIC_WRITE;
    DWORD disposition = exists ? OPEN_EXISTING : CREATE_NEW;
    DWORD share = exists ? (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE) : 0;
    HANDLE h = CreateFileA(path_str, access, share, NULL, disposition, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    bool ok = true;
    if (exists) {
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        ok = SetFileTime(h, NULL, &ft, &ft) != 0;
    }
    CloseHandle(h);
    return ok;
#else
    if (exists) return utime(path_str, SP_PRIV_NULL) == 0;
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
    return (is_dir ? rmdir(path_str) : unlink(path_str)) == 0 || (!is_dir && missing_ok && errno == ENOENT);
#endif
}

bool sp_unlink(const SpPath *p, bool missing_ok) {
    return sp_priv_remove_impl(p, false, missing_ok);
}

bool sp_rmdir(const SpPath *p) {
    return sp_priv_remove_impl(p, true, false);
}

static SpIOResult sp_priv_io_result(size_t bytes, int error) {
    SpIOResult r = SP_PRIV_ZERO;
    r.bytes = bytes;
    r.error = error;
    return r;
}

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
    const char *path_str;
    if (!sp_priv_path_cstr(p, &path_str)) return sp_priv_io_result(0, SP_ERR_OPEN);
    SpStatResult st = sp_stat(p);
    if (!st.valid || st.sp_size < 0) return sp_priv_io_result(0, SP_ERR_OPEN);
    size_t sz = SP_PRIV_CAST(size_t, st.sp_size);
    if (sz > buf_size) return sp_priv_io_result(sz, SP_ERR_TOO_LARGE);

    FILE *f = fopen(path_str, "rb");
    if (!f) return sp_priv_io_result(0, SP_ERR_OPEN);
    size_t got = fread(buf, 1, sz, f);
    if (got != sz) { fclose(f); return sp_priv_io_result(got, SP_ERR_READ); }
    return sp_priv_io_result(got, fclose(f) == 0 ? SP_OK : SP_ERR_READ);
}

SpIOResult sp_write_file(const SpPath *p, const char *data, size_t data_len) {
    const char *path_str;
    if (!sp_priv_path_cstr(p, &path_str)) return sp_priv_io_result(0, SP_ERR_OPEN);
    FILE *f = fopen(path_str, "wb");
    if (!f) return sp_priv_io_result(0, SP_ERR_OPEN);
    size_t wrote = fwrite(data, 1, data_len, f);
    if (wrote != data_len) { fclose(f); return sp_priv_io_result(wrote, SP_ERR_WRITE); }
    return sp_priv_io_result(wrote, fclose(f) == 0 ? SP_OK : SP_ERR_WRITE);
}

const char *sp_error_str(int error) {
    static const char *const messages[] = {
        "Success", "Operation failed", "File exists",
        "No such file or directory", "Not a directory", "Permission denied",
        "Path exists but is not a directory", "Could not open file", "Read failed",
        "Write failed", "File too large for buffer", "Unknown error",
        "Path is not relative to the other path", "Path has an empty name", "Invalid argument",
        "Unsupported operation"
    };
    return (error >= 0 && error < SP_PRIV_CAST(int, SP_ARRAY_LEN(messages))) ? messages[error] : "Unknown error";
}

#ifndef SP_WINDOWS
#include <dirent.h>
#endif

#define SP_GLOB_IS_DOT_OR_DOTDOT(s) ((s)[0] == '.' && ((s)[1] == '\0' || ((s)[1] == '.' && (s)[2] == '\0')))

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
    while (true) {
        const char *name;
#ifdef SP_WINDOWS
        WIN32_FIND_DATAA fd;
        if (!*handle) {
            char search[SP_PATH_MAX + 3];
            size_t len = dir->len;
            memcpy(search, dir->buf, len);
            if (len == 0) search[len++] = '.'; /* empty dir means cwd: ".\*" */
            if (!sp_priv_is_sep(search[len - 1], dir->flavor)) search[len++] = '\\';
            search[len++] = '*';
            search[len] = '\0';
            *handle = FindFirstFileA(search, &fd);
            if (*handle == INVALID_HANDLE_VALUE) { *handle = SP_PRIV_NULL; return false; }
        } else {
            if (!FindNextFileA(*handle, &fd)) return false;
        }
        name = fd.cFileName;
#else
        if (!*handle) {
            *handle = opendir(sp_str(dir));
            if (!*handle) return false;
        }
        struct dirent *de = readdir(SP_PRIV_CAST(DIR *, *handle));
        if (!de) return false;
        name = de->d_name;
#endif
        if (SP_GLOB_IS_DOT_OR_DOTDOT(name)) continue;
        *out = sp_priv_join_len(dir, name, strlen(name));
        return true;
    }
}

/* The last failed OS call's error as an SP_ERR_* code */
static int sp_priv_last_error(void) {
#ifdef SP_WINDOWS
    DWORD e = GetLastError();
    if (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND) return SP_ERR_NOT_FOUND;
    if (e == ERROR_ALREADY_EXISTS || e == ERROR_FILE_EXISTS) return SP_ERR_EXISTS;
    if (e == ERROR_ACCESS_DENIED) return SP_ERR_PERMISSION;
    if (e == ERROR_DIRECTORY) return SP_ERR_NOT_DIR;
#else
    if (errno == ENOENT) return SP_ERR_NOT_FOUND;
    if (errno == EEXIST) return SP_ERR_EXISTS;
    if (errno == EACCES || errno == EPERM) return SP_ERR_PERMISSION;
    if (errno == ENOTDIR) return SP_ERR_NOT_DIR;
    if (errno == EINVAL) return SP_ERR_INVALID_ARG;
#endif
    return SP_ERR_OTHER;
}

/* CPython's _copy_info for local paths: access and modification times, then permissions */
static int sp_priv_copy_metadata(const SpPath *src, const SpPath *dst, bool follow_symlinks) {
#ifdef SP_WINDOWS
    DWORD flags = FILE_FLAG_BACKUP_SEMANTICS | (follow_symlinks ? 0 : FILE_FLAG_OPEN_REPARSE_POINT);
    DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    HANDLE hs = CreateFileA(sp_str(src), FILE_READ_ATTRIBUTES, share, NULL, OPEN_EXISTING, flags, NULL);
    HANDLE hd = CreateFileA(sp_str(dst), FILE_WRITE_ATTRIBUTES, share, NULL, OPEN_EXISTING, flags, NULL);
    FILETIME atime, mtime;
    bool ok = hs != INVALID_HANDLE_VALUE && hd != INVALID_HANDLE_VALUE && GetFileTime(hs, NULL, &atime, &mtime) &&
              SetFileTime(hd, NULL, &atime, &mtime);
    if (hs != INVALID_HANDLE_VALUE) CloseHandle(hs);
    if (hd != INVALID_HANDLE_VALUE) CloseHandle(hd);
    ok = ok && (!follow_symlinks || sp_chmod(dst, sp_stat(src).sp_mode));
#else
    struct stat st;
    struct utimbuf times;
    bool ok = (follow_symlinks ? stat(sp_str(src), &st) : lstat(sp_str(src), &st)) == 0;
    if (ok && S_ISLNK(st.st_mode)) return SP_OK; /* utime() and chmod() would follow the link */
    times.actime = st.st_atime;
    times.modtime = st.st_mtime;
    ok = ok && utime(sp_str(dst), &times) == 0 && chmod(sp_str(dst), st.st_mode & 07777) == 0;
#endif
    return ok ? SP_OK : sp_priv_last_error();
}

static int sp_priv_copy_file(const SpPath *src, const SpPath *dst) {
    FILE *in = fopen(sp_str(src), "rb");
    if (!in) return sp_priv_last_error();
    FILE *out = fopen(sp_str(dst), "wb");
    int err = out ? SP_OK : sp_priv_last_error();
    char buf[8192];
    for (size_t n; err == SP_OK && (n = fread(buf, 1, sizeof(buf), in)) > 0;)
        if (fwrite(buf, 1, n, out) != n) err = SP_ERR_WRITE;
    if (err == SP_OK && ferror(in)) err = SP_ERR_READ;
    fclose(in);
    if (out && fclose(out) != 0 && err == SP_OK) err = SP_ERR_WRITE;
    return err;
}

/* CPython's Path._copy_from: recursively copy src to dst (extended in place for children, then restored) */
static int sp_priv_copy_tree(const SpPath *src, SpPath *dst, bool follow_symlinks, bool preserve_metadata) {
    int err;
    if (!follow_symlinks && sp_is_symlink(src)) {
        SpPath link = sp_readlink(src);
        err = sp_path_is_error(&link) || !sp_symlink_to(dst, &link, sp_is_dir(src, true)) ? sp_priv_last_error() : SP_OK;
    } else if (sp_is_dir(src, true)) {
        /* Children are listed before dst is created, so an unreadable src leaves no dst behind */
        void *handle = SP_PRIV_NULL;
        SpPath child;
        bool more = sp_priv_readdir_next(&handle, src, &child);
        if (!handle) return sp_priv_last_error();
        err = sp_mkdir(dst, SP_MKDIR_DEF_MODE, false, false);
        for (size_t len = dst->len; err == SP_OK && more; more = sp_priv_readdir_next(&handle, src, &child)) {
            SpStr name = sp_priv_name_sv(&child);
            *dst = sp_priv_join_len(dst, name.data, name.len);
            err = sp_priv_copy_tree(&child, dst, follow_symlinks, preserve_metadata);
            dst->len = len;
            sp_priv_terminate(dst);
        }
        sp_priv_readdir_close(&handle);
    } else {
        err = sp_samefile(src, dst) ? SP_ERR_INVALID_ARG : sp_priv_copy_file(src, dst);
    }
    return err == SP_OK && preserve_metadata ? sp_priv_copy_metadata(src, dst, follow_symlinks) : err;
}

static SpPath sp_priv_copy(const SpPath *p, const SpPath *target, bool follow_symlinks, bool preserve_metadata) {
    SP_ASSERT_PATH_INVARIANT(p);
    SP_ASSERT_PATH_INVARIANT(target);
    /* Lexically the same path, or inside it */
    if (sp_is_relative_to(target, p)) return sp_priv_error_path(target->flavor, SP_ERR_INVALID_ARG);
    SpPath dst = *target;
    int err = sp_priv_copy_tree(p, &dst, follow_symlinks, preserve_metadata);
    return err == SP_OK ? *target : sp_priv_error_path(target->flavor, err);
}

/* CPython's Path._delete: symlinks and junctions are unlinked, directories removed recursively */
static int sp_priv_delete(const SpPath *p) {
    if (sp_is_symlink(p) || sp_is_junction(p) || !sp_is_dir(p, true))
        return sp_unlink(p, false) ? SP_OK : sp_priv_last_error();
    void *handle = SP_PRIV_NULL;
    SpPath child;
    int err = SP_OK;
    while (err == SP_OK && sp_priv_readdir_next(&handle, p, &child)) err = sp_priv_delete(&child);
    sp_priv_readdir_close(&handle);
    return err != SP_OK ? err : sp_rmdir(p) ? SP_OK : sp_priv_last_error();
}

/* os.rename / os.replace; `copy_across_devices` falls back to copy + delete like Path.move */
static SpPath sp_priv_move(const SpPath *p, const SpPath *target, bool allow_replace, bool copy_across_devices) {
    SP_ASSERT_PATH_INVARIANT(p);
    SP_ASSERT_PATH_INVARIANT(target);
    const char *src_str, *dst_str;
    if (!sp_priv_path_cstr(p, &src_str) || !sp_priv_path_cstr(target, &dst_str))
        return sp_priv_error_path(target->flavor, SP_ERR_INVALID_ARG);
#ifdef SP_WINDOWS
    if (MoveFileExA(src_str, dst_str, allow_replace ? MOVEFILE_REPLACE_EXISTING : 0)) return *target;
    bool cross_device = GetLastError() == ERROR_NOT_SAME_DEVICE;
#else
    if (!allow_replace && sp_exists(target, true))
        return sp_samefile(p, target) ? *target : sp_priv_error_path(target->flavor, SP_ERR_EXISTS);
    if (rename(src_str, dst_str) == 0) return *target;
    bool cross_device = errno == EXDEV;
#endif
    if (!cross_device || !copy_across_devices) return sp_priv_error_path(target->flavor, sp_priv_last_error());
    SpPath r = sp_priv_copy(p, target, false, true);
    int err = sp_path_is_error(&r) ? SP_OK : sp_priv_delete(p);
    return err == SP_OK ? r : sp_priv_error_path(target->flavor, err);
}

SpPath sp_rename(const SpPath *p, const SpPath *target) { return sp_priv_move(p, target, false, false); }
SpPath sp_replace(const SpPath *p, const SpPath *target) { return sp_priv_move(p, target, true, false); }

/* target_dir / p.name, for the *_into operations */
static SpPath sp_priv_into(const SpPath *p, const SpPath *target_dir) {
    SpStr name = sp_priv_name_sv(p);
    return name.len == 0 ? sp_priv_error_path(target_dir->flavor, SP_ERR_NO_NAME)
                         : sp_priv_join_len(target_dir, name.data, name.len);
}

SpPath sp_copy(const SpPath *p, const SpPath *target, bool follow_symlinks, bool preserve_metadata) {
    return sp_priv_copy(p, target, follow_symlinks, preserve_metadata);
}

SpPath sp_copy_into(const SpPath *p, const SpPath *target_dir, bool follow_symlinks, bool preserve_metadata) {
    SpPath target = sp_priv_into(p, target_dir);
    return sp_path_is_error(&target) ? target : sp_priv_copy(p, &target, follow_symlinks, preserve_metadata);
}

static SpPath sp_priv_move_to(const SpPath *p, const SpPath *target) {
    if (sp_samefile(p, target)) return sp_priv_error_path(target->flavor, SP_ERR_INVALID_ARG);
    return sp_priv_move(p, target, true, true);
}

SpPath sp_move(const SpPath *p, const SpPath *target) { return sp_priv_move_to(p, target); }

SpPath sp_move_into(const SpPath *p, const SpPath *target_dir) {
    SpPath target = sp_priv_into(p, target_dir);
    return sp_path_is_error(&target) ? target : sp_priv_move_to(p, &target);
}

static const char *sp_priv_glob_part(const SpGlobIter *it, size_t seg) {
    return it->priv_.pattern_buf + it->priv_.seg_offsets[seg];
}

static void sp_priv_glob_push(SpGlobIter *it, size_t seg, size_t from, size_t root_len, bool walk) {
    if (it->depth + 1 >= SP_GLOB_MAX_DEPTH) return;
    it->depth++;
    it->priv_.stack[it->depth].handle = SP_PRIV_NULL;
    it->priv_.stack[it->depth].path_len = it->priv_.path.len;
    it->priv_.stack[it->depth].seg = seg;
    it->priv_.stack[it->depth].from = from;
    it->priv_.stack[it->depth].root_len = root_len;
    it->priv_.stack[it->depth].walk = walk;
}

/* Whether the path below its first root_len bytes matches the '**' parts [from, to) joined into one pattern */
static bool sp_priv_glob_walk_match(const SpGlobIter *it, size_t from, size_t to, const SpPath *path, size_t root_len) {
    char pat[SP_GLOB_PATTERN_MAX];
    size_t n = 0;
    for (size_t i = from; i < to; i++) {
        if (i > from) pat[n++] = sp_priv_sep(path->flavor);
        n += sp_priv_copy_trunc(pat + n, sizeof(pat) - n, sp_priv_glob_part(it, i), strlen(sp_priv_glob_part(it, i)));
    }
    if (root_len < path->len && sp_priv_is_sep(path->buf[root_len], path->flavor)) root_len++;
    return sp_priv_match_path(pat, n, path->buf + root_len, path->len - root_len, it->priv_.case_insensitive, true,
                              path->flavor);
}

/* CPython's glob selector chain from part `seg` on it->priv_.path: literal and special ('..', trailing '') parts
 * extend the path, wildcard and '**' parts push a frame. `trailing` marks a path ending in a separator (so it must
 * be a directory) and `exists` one already known to exist. Returns true when the path itself is a match. */
static bool sp_priv_glob_select(SpGlobIter *it, size_t seg, bool exists, bool trailing) {
    SpPath *path = &it->priv_.path;
    for (; seg < it->priv_.seg_count; seg++) {
        const char *part = sp_priv_glob_part(it, seg);
        size_t len = strlen(part);
        bool special = len == 0 || strcmp(part, "..") == 0;
        if (SP_PRIV_IS_DOUBLESTAR(part, len)) {
            while (seg + 1 < it->priv_.seg_count && strcmp(sp_priv_glob_part(it, seg + 1), "**") == 0) seg++;
            /* Following symlinks, the walk matches all following non-special parts at once */
            size_t from = seg++;
            while (it->priv_.recurse_symlinks && seg < it->priv_.seg_count && strlen(sp_priv_glob_part(it, seg)) > 0 &&
                   strcmp(sp_priv_glob_part(it, seg), "..") != 0)
                seg++;
            sp_priv_glob_push(it, seg, from, path->len, true);
            if (!sp_priv_glob_walk_match(it, from, seg, path, path->len)) return false;
            seg--;
            continue;
        }
        if (!special && (it->priv_.case_pedantic || strpbrk(part, "*?[") != SP_PRIV_NULL)) {
            sp_priv_glob_push(it, seg, seg, 0, false);
            return false;
        }
        if (len > 0) *path = sp_priv_join_len(path, part, len);
        exists = exists && special;
        trailing = len == 0 || seg + 1 < it->priv_.seg_count; /* a separator follows unless this part ends the pattern */
    }
    return exists || (trailing ? sp_priv_has_type(path, S_IFDIR, true) : sp_lstat(path).valid);
}

SpGlobIter sp_glob_begin(const SpPath *base, const char *pattern, SpCaseSensitivity cs, bool recurse_symlinks) {
    SpGlobIter it;
    memset(&it, 0, sizeof(it));
    it.depth = -1;
    SP_ASSERT_PATH_INVARIANT(base);
    SpFlavor flavor = base->flavor;
    size_t len = strlen(pattern);
    if (sp_priv_anchor_len(pattern, len, flavor) > 0) {
        it.error = SP_ERR_UNSUPPORTED;
        return it;
    }
    /* Parts without empty and '.' ones; a trailing separator adds a final empty part */
    char *buf = it.priv_.pattern_buf;
    bool trailing_sep = len > 0 && sp_priv_is_sep(pattern[len - 1], flavor);
    len = sp_priv_copy_trunc(buf, SP_GLOB_PATTERN_MAX, pattern, len);
    size_t n = 0;
    for (size_t pos = 0; n < SP_GLOB_MAX_SEGMENTS && (pos = sp_priv_skip_seps(buf, len, pos, flavor)) < len;) {
        size_t end = sp_priv_find_sep(buf, len, pos, flavor);
        if (end - pos != 1 || buf[pos] != '.') it.priv_.seg_offsets[n++] = pos;
        buf[end] = '\0';
        pos = end + 1;
    }
    if (n == 0) {
        it.error = SP_ERR_INVALID_ARG;
        return it;
    }
    if (trailing_sep && n < SP_GLOB_MAX_SEGMENTS) it.priv_.seg_offsets[n++] = len;
    it.priv_.seg_count = n;
    it.priv_.case_insensitive = cs == SP_CASE_INSENSITIVE || (cs == SP_CASE_PLATFORM_DEFAULT && sp_priv_is_windows_flavor(flavor));
    it.priv_.case_pedantic = cs != SP_CASE_PLATFORM_DEFAULT;
    it.priv_.recurse_symlinks = recurse_symlinks;
    it.priv_.path = *base;
    it.priv_.pending = sp_priv_glob_select(&it, 0, false, true);
    return it;
}

bool sp_glob_next(SpGlobIter *it, SpPath *out) {
    SpPath *path = &it->priv_.path;
    while (!it->priv_.pending && it->depth >= 0) {
        size_t seg = it->priv_.stack[it->depth].seg, from = it->priv_.stack[it->depth].from;
        size_t root_len = it->priv_.stack[it->depth].root_len;
        bool walk = it->priv_.stack[it->depth].walk;
        path->len = it->priv_.stack[it->depth].path_len;
        sp_priv_terminate(path);
        SpPath entry;
        if (!sp_priv_readdir_next(&it->priv_.stack[it->depth].handle, path, &entry)) {
            sp_priv_readdir_close(&it->priv_.stack[it->depth].handle);
            it->depth--;
            continue;
        }
        if (walk) {
            /* '**': every descendant (directories only if parts follow), each fed to the parts after it */
            bool is_dir = sp_priv_has_type(&entry, S_IFDIR, it->priv_.recurse_symlinks);
            if (!is_dir && seg < it->priv_.seg_count) continue;
            *path = entry;
            if (is_dir) sp_priv_glob_push(it, seg, from, root_len, true);
            if (!sp_priv_glob_walk_match(it, from, seg, &entry, root_len)) continue;
            it->priv_.pending = seg == it->priv_.seg_count || sp_priv_glob_select(it, seg, true, true);
        } else {
            const char *part = sp_priv_glob_part(it, seg);
            SpStr name = sp_priv_name_sv(&entry);
            if (!sp_priv_fnmatch(part, strlen(part), name.data, name.len, it->priv_.case_insensitive, entry.flavor)) continue;
            bool last = seg + 1 == it->priv_.seg_count;
            if (!last && !sp_priv_has_type(&entry, S_IFDIR, true)) continue;
            *path = entry;
            it->priv_.pending = last || sp_priv_glob_select(it, seg + 1, true, true);
        }
    }
    if (!it->priv_.pending) return false;
    it->priv_.pending = false;
    *out = *path;
    return true;
}

void sp_glob_end(SpGlobIter *it) {
    for (; it->depth >= 0; it->depth--) sp_priv_readdir_close(&it->priv_.stack[it->depth].handle);
}

SpGlobIter sp_rglob_begin(const SpPath *base, const char *pattern, SpCaseSensitivity cs, bool recurse_symlinks) {
    /* glob(join('**', pattern)); an anchored pattern replaces the '**' in the join */
    size_t len = strlen(pattern);
    if (sp_priv_anchor_len(pattern, len, base->flavor) > 0) return sp_glob_begin(base, pattern, cs, recurse_symlinks);
    char buf[SP_GLOB_PATTERN_MAX] = {'*', '*', sp_priv_sep(base->flavor)};
    sp_priv_copy_trunc(buf + 3, SP_GLOB_PATTERN_MAX - 3, pattern, len);
    return sp_glob_begin(base, buf, cs, recurse_symlinks);
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
    return sp_priv_error_path(flavor, SP_ERR_OTHER);
}

SpPath sp_expanduser(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (p->len == 0 || p->buf[0] != '~') return sp_path_copy(p);

    bool is_current_user = p->len == 1 || sp_priv_is_sep(p->buf[1], p->flavor);
    if (is_current_user) {
        SpPath home = sp_home(p->flavor);
        if (sp_path_is_error(&home)) return sp_priv_error_path(p->flavor, SP_ERR_OTHER);
        if (p->len == 1) return home;
        const char *rest = p->buf + 2;
        size_t rest_len = p->len - 2;

        if (sp_priv_is_windows_flavor(p->flavor) && sp_priv_has_drive(rest, rest_len, p->flavor)) {
            char protected_path[SP_PATH_MAX];
            protected_path[0] = '.';
            protected_path[1] = '/';
            sp_priv_copy_trunc(protected_path + 2, SP_PATH_MAX - 2, rest, rest_len);
            return sp_join_one(&home, protected_path);
        }
        return sp_join_n(&home, rest, rest_len);
    }

#ifndef SP_WINDOWS
    size_t end = sp_priv_find_sep(p->buf, p->len, 1, p->flavor);
    char username[256];
    size_t ulen = end - 1;
    if (ulen >= sizeof(username)) return sp_path_copy(p);
    memcpy(username, p->buf + 1, ulen);
    username[ulen] = '\0';

    struct passwd *pw = getpwnam(username);
    if (!pw || !pw->pw_dir) return sp_path_copy(p);
    SpPath home = sp_path_new(pw->pw_dir, SP_PRIV_OPTS(p->flavor));
    if (end >= p->len) return home;
    return sp_join_n(&home, p->buf + end, p->len - end);
#else
    return sp_path_copy(p);
#endif
}

static SpTerm sp_priv_id_to_name(const SpPath *p, bool get_owner) {
#ifdef SP_WINDOWS
    (void)p;
    (void)get_owner;
    return sp_priv_term(NULL, 0);
#else
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
#endif
}

SpTerm sp_owner(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    return sp_priv_id_to_name(p, true);
}

SpTerm sp_group(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    return sp_priv_id_to_name(p, false);
}

SpIterdirIter sp_iterdir_begin(const SpPath *p) {
    SpIterdirIter it;
    memset(&it, 0, sizeof(it));
    it.done = -1;
    if (!p) return it;
    const char *path_str;
    if (!sp_priv_path_cstr(p, &path_str)) return it;
    it.dir = sp_path_copy(p);
#ifdef SP_WINDOWS
    (void)path_str;
    if (it.dir.len > 0 && !sp_priv_has_type(&it.dir, S_IFDIR, true)) return it;
    it.priv_.handle = SP_PRIV_NULL;
#else
    it.priv_.handle = opendir(path_str);
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
        bool is_dir = sp_is_dir(&entry, follow_symlinks);
        char (*names)[SP_WALK_NAME_MAX] = is_dir ? dirnames : filenames;
        size_t *count = is_dir ? dirname_count : filename_count;
        if (*count < SP_WALK_MAX_ENTRIES)
            sp_priv_copy_trunc(names[(*count)++], SP_WALK_NAME_MAX, name.buf, name.len);
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

    if (top_down && !callback(&entry)) return false;

    char (*next_dirs)[SP_WALK_NAME_MAX] = top_down ? entry.dirnames : dirnames;
    size_t next_count = top_down ? entry.dirname_count : dirname_count;
    for (size_t i = 0; i < next_count; i++) {
        SpPath subdir = sp_priv_join_len(dir, next_dirs[i], strlen(next_dirs[i]));
        if (!sp_priv_walk_recursive(&subdir, top_down, follow_symlinks, callback, on_error, user_data))
            return false;
    }

    if (!top_down) {
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
    if (!sp_is_dir(p, true)) return false;

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

static SpPathOp sp_priv_f_pathop(int error) {
    SpPathOp r;
    r.path = sp_priv_f_ctx;
    sp_priv_f_ctx_active = false;
    r.error = error;
    return r;
}

#define SP_F_TERM(ret, name, params, expr) \
    static ret sp_priv_f_##name##_ params { sp_priv_f_ctx_active = false; return (expr); }
#define SP_F_PROC(name, params, expr) \
    static void sp_priv_f_##name##_ params { sp_priv_f_ctx_active = false; expr; }

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
