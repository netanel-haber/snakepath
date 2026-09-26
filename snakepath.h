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

/* clang-format off */
#ifdef __cplusplus
#define SP_PRIV_STR(d, l) SpStr{(d), (l)}
#define SP_PRIV_ZERO {}
#define SP_PRIV_NULL nullptr
#define SP_PRIV_CAST(type, val) static_cast<type>(val)
#else
#define SP_PRIV_STR(d, l) ((SpStr){.data = (d), .len = (l)})
#define SP_PRIV_ZERO {0}
#define SP_PRIV_NULL NULL
#define SP_PRIV_CAST(type, val) ((type)(val))
#endif
/* clang-format on */

/* Windows has a 1MB default stack: larger values may overflow it (the /STACK linker flag raises it) */
#define SP_PATH_MAX_WINDOWS 1024
#define SP_PATH_MAX_LINUX 4096 /* Linux PATH_MAX; the typical 8MB stack handles this fine */

#ifndef SP_PATH_MAX
#error "SP_PATH_MAX must be defined before including snakepath.h. " \
       "Use: #define SP_PATH_MAX SP_PATH_MAX_WINDOWS (1024) or SP_PATH_MAX_LINUX (4096)"
#endif

#ifndef SP_MAX_SUFFIXES
#define SP_MAX_SUFFIXES 16
#endif

#define SP_MATCH_YES 1        /* Pattern matched */
#define SP_MATCH_NO 0         /* Pattern did not match */
#define SP_MATCH_ERR_EMPTY -1 /* Empty pattern */

#if defined(_WIN32) || defined(_WIN64)
#define SP_WINDOWS 1
#endif

/* A path's flavor is POSIX or WINDOWS: SP_FLAVOR_NATIVE resolves to the platform's when the path is made */
typedef enum { SP_FLAVOR_NATIVE = 0, SP_FLAVOR_POSIX, SP_FLAVOR_WINDOWS } SpFlavor;

typedef enum { SP_CASE_PLATFORM_DEFAULT = 0, SP_CASE_SENSITIVE, SP_CASE_INSENSITIVE } SpCaseSensitivity;

#define SP_ASSERT_FLAVOR(f)                                                                                            \
    assert(((f) == SP_FLAVOR_NATIVE || (f) == SP_FLAVOR_POSIX || (f) == SP_FLAVOR_WINDOWS) && "invalid flavor value")
#define SP_ASSERT_PATH_INVARIANT(p)                                                                                    \
    do {                                                                                                               \
        assert((p) != NULL && "path pointer must not be NULL");                                                        \
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
    if (cap == 0)
        return 0;
    size_t n = len < cap - 1 ? len : cap - 1;
    if (n > 0)
        memcpy(dst, src, n);
    dst[n] = '\0';
    return n;
}

static inline SpTerm sp_priv_term(const char *data, size_t len) {
    SpTerm t = SP_PRIV_ZERO;
    if (data && len > 0)
        t.len = sp_priv_copy_trunc(t.buf, SP_TERM_MAX, data, len);
    return t;
}

typedef struct {
    char buf[SP_PATH_MAX];
    size_t len;
    SpFlavor flavor;
} SpPath;

typedef struct {
    const SpPath *path;
    size_t pos, end, anchor;
} SpPartsIter;

typedef struct {
    SpStr items[SP_MAX_SUFFIXES];
    size_t count;
} SpSuffixes;

typedef struct {
    const SpPath *path;
    size_t current_len;
} SpParentsIter;

typedef struct {
    SpPath dir;
    int done;
    struct {
        void *handle;
    } priv_;
} SpIterdirIter;

SpIterdirIter sp_iterdir_begin(const SpPath *p);
bool sp_iterdir_next(SpIterdirIter *it, SpPath *out); /* returns child path */
void sp_iterdir_end(SpIterdirIter *it);

/* clang-format off */
#define SP_ITERDIR_FOREACH(dir, entry_var) \
    for (struct { SpIterdirIter it; int done; } sp_ictx_ = { sp_iterdir_begin(dir), 0 }; \
         !sp_ictx_.done; sp_iterdir_end(&sp_ictx_.it), sp_ictx_.done = 1) \
    for (SpPath entry_var; sp_iterdir_next(&sp_ictx_.it, &entry_var); )
/* clang-format on */

#ifndef SP_GLOB_MAX_DEPTH
#define SP_GLOB_MAX_DEPTH 32
#endif
#ifndef SP_GLOB_PATTERN_MAX
#define SP_GLOB_PATTERN_MAX 256
#endif

typedef struct {
    int depth;
    int error; /* SP_OK, SP_ERR_INVALID_ARG (pattern has no parts) or SP_ERR_UNSUPPORTED (anchored pattern) */
    struct {
        char pattern_buf[SP_GLOB_PATTERN_MAX];
        size_t pattern_len;
        bool case_insensitive;
        bool case_pedantic; /* explicit case sensitivity: literal parts are matched against listings too */
        bool recurse_symlinks;
        bool pending; /* path is the next match */
        SpPath path;  /* each frame's directory is a prefix of it */
        /* Each frame matches pattern[from..to) below path[0..root_len), listing path[0..path_len). */
        struct {
            void *handle;
            size_t path_len, from, to, root_len;
        } stack[SP_GLOB_MAX_DEPTH];
    } priv_;
} SpGlobIter;

#define sp_path(s) sp_path_new((s), SP_FLAVOR_NATIVE)
#define sp_path_f(s, f) sp_path_new((s), (f))

/* Join paths: sp_join(p, "a", "b", "c") - C only, use sp_join_one in C++ */
#ifndef __cplusplus
#define sp_join(base, ...) sp_join_impl((base), (const char *[]){__VA_ARGS__, NULL})
#endif

#define SP_ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

SpPath sp_path_new(const char *s, SpFlavor flavor);
SpPath sp_path_from_n(const char *s, size_t len, SpFlavor flavor);
SpPath sp_path_convert(const char *s, SpFlavor src_flavor, SpFlavor dest_flavor);

const char *sp_str(const SpPath *p);
size_t sp_as_posix(const SpPath *p, char *out, size_t out_size);

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

SpPath sp_relative_to(const SpPath *p, const SpPath *other, bool walk_up);
bool sp_is_relative_to(const SpPath *p, const SpPath *other);

bool sp_is_absolute(const SpPath *p);
SpPath sp_cwd(SpFlavor flavor);
SpPath sp_absolute(const SpPath *p);
size_t sp_as_uri(const SpPath *p, char *buf, size_t buf_size); /* 0 for relative paths */
SpPath sp_from_uri(const char *uri, SpFlavor flavor);          /* error path unless an absolute file: URI */
bool sp_path_eq(const SpPath *a, const SpPath *b);
int sp_path_cmp(const SpPath *a, const SpPath *b);
static inline bool sp_path_ne(const SpPath *a, const SpPath *b) {
    return a->flavor != b->flavor || sp_path_cmp(a, b) != 0;
}
unsigned long sp_path_hash(const SpPath *p);
int sp_match_ex(const SpPath *p, const char *pattern, int case_sensitive); /* Returns SP_MATCH_* codes */
#define SP_MATCH(p, pattern) sp_match_ex((p), (pattern), -1)
bool sp_full_match(const SpPath *p, const char *pattern, int case_sensitive); /* case_sensitive: -1 = flavor default */
bool sp_is_symlink(const SpPath *p);
bool sp_is_block_device(const SpPath *p);
bool sp_is_char_device(const SpPath *p);
bool sp_is_fifo(const SpPath *p);
bool sp_is_socket(const SpPath *p);
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

SpStatResult sp_stat(const SpPath *p);  /* follows symlinks */
SpStatResult sp_lstat(const SpPath *p); /* does not follow symlinks */
bool sp_stat_eq(const SpStatResult *a, const SpStatResult *b);
size_t sp_parents_count(const SpPath *p);

SpPath sp_readlink(const SpPath *p);
SpPath sp_resolve(const SpPath *p, bool strict);
bool sp_symlink_to(const SpPath *p, const SpPath *target, bool target_is_directory);
bool sp_hardlink_to(const SpPath *p, const SpPath *target);
bool sp_samefile(const SpPath *a, const SpPath *b);

#define SP_MKDIR_DEF_MODE 0777

/* Missing parents (with `parents`) get parent_mode; returns SP_OK / SP_ERR_* */
int sp_mkdir(const SpPath *p, unsigned int mode, bool parents, bool exist_ok, unsigned int parent_mode);

/* One code space for operation results and failed SpPath results (sp_path_error_code),
 * so sp_error_str() describes either */
enum {
    SP_OK = 0,
    SP_ERR, /* Other error (I/O, permission, etc.) */
    SP_ERR_EXISTS,
    SP_ERR_NOT_FOUND,
    SP_ERR_NOT_DIR,
    SP_ERR_PERMISSION,
    SP_ERR_EXISTS_NOT_DIR,
    SP_ERR_OPEN,
    SP_ERR_READ,
    SP_ERR_WRITE,
    SP_ERR_TOO_LARGE,
    SP_ERR_NOT_RELATIVE, /* Not relative to other path */
    SP_ERR_NO_NAME,      /* Path has no usable name */
    SP_ERR_INVALID_ARG,  /* Invalid argument (name/stem/suffix, empty glob pattern, same copy source and target) */
    SP_ERR_UNSUPPORTED   /* Unsupported operation (non-relative glob pattern) */
};

const char *sp_error_str(int error);

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

typedef struct {
    size_t bytes;
    int error;
} SpIOResult; /* error: SP_OK / SP_ERR_* */

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
    char (*dirnames)[SP_WALK_NAME_MAX]; /* Pointer to array of names */
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
bool sp_walk(const SpPath *p, bool top_down, bool follow_symlinks, SpWalkFn callback, SpWalkErrorFn on_error,
             void *user_data);

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
/* clang-format off */
#define SP_GLOB_FOREACH(base, pattern, match_var) \
    for (struct { SpGlobIter it; int done; } sp_gctx_ = { sp_glob_begin(base, pattern, SP_CASE_PLATFORM_DEFAULT, false), 0 }; \
         !sp_gctx_.done; sp_glob_end(&sp_gctx_.it), sp_gctx_.done = 1) \
    for (SpPath match_var; sp_glob_next(&sp_gctx_.it, &match_var); )

#define SP_RGLOB_FOREACH(base, pattern, match_var) \
    for (struct { SpGlobIter it; int done; } sp_gctx_ = { sp_rglob_begin(base, pattern, SP_CASE_PLATFORM_DEFAULT, false), 0 }; \
         !sp_gctx_.done; sp_glob_end(&sp_gctx_.it), sp_gctx_.done = 1) \
    for (SpPath match_var; sp_glob_next(&sp_gctx_.it, &match_var); )
/* clang-format on */

/* Error checking for path results */
static inline bool sp_path_is_error(const SpPath *p) { return p->len == 0 && p->buf[0] != SP_OK; }
static inline int sp_path_error_code(const SpPath *p) {
    return p->len == 0 ? SP_PRIV_CAST(int, SP_PRIV_CAST(unsigned char, p->buf[0])) : 0;
}

/* ============ Fluent API ============ */
#ifdef SNAKEPATH_FLUENT

typedef struct sp_fluent_ SpPrivDontUseThisDirectly_;

#define SP_F_TERMINATOR_METHODS(X_TERM)                                                                                \
    X_TERM(SpPath, path, (void), sp_priv_f_ctx)                                                                        \
    X_TERM(SpTerm, name, (void), sp_name(&sp_priv_f_ctx))                                                              \
    X_TERM(SpTerm, stem, (void), sp_stem(&sp_priv_f_ctx))                                                              \
    X_TERM(SpTerm, suffix, (void), sp_suffix(&sp_priv_f_ctx))                                                          \
    X_TERM(SpSuffixes, suffixes, (void), sp_suffixes(&sp_priv_f_ctx))                                                  \
    X_TERM(SpTerm, drive, (void), sp_drive(&sp_priv_f_ctx))                                                            \
    X_TERM(SpTerm, root, (void), sp_root(&sp_priv_f_ctx))                                                              \
    X_TERM(SpTerm, anchor, (void), sp_anchor(&sp_priv_f_ctx))                                                          \
    X_TERM(SpTerm, owner, (void), sp_owner(&sp_priv_f_ctx))                                                            \
    X_TERM(SpTerm, group, (void), sp_group(&sp_priv_f_ctx))                                                            \
    X_TERM(bool, is_absolute, (void), sp_is_absolute(&sp_priv_f_ctx))                                                  \
    X_TERM(bool, is_relative_to, (const SpPath *o), sp_is_relative_to(&sp_priv_f_ctx, o))                              \
    X_TERM(bool, is_file, (bool follow_symlinks), sp_is_file(&sp_priv_f_ctx, follow_symlinks))                         \
    X_TERM(bool, is_dir, (bool follow_symlinks), sp_is_dir(&sp_priv_f_ctx, follow_symlinks))                           \
    X_TERM(bool, exists, (bool follow_symlinks), sp_exists(&sp_priv_f_ctx, follow_symlinks))                           \
    X_TERM(bool, is_symlink, (void), sp_is_symlink(&sp_priv_f_ctx))                                                    \
    X_TERM(bool, is_block_device, (void), sp_is_block_device(&sp_priv_f_ctx))                                          \
    X_TERM(bool, is_char_device, (void), sp_is_char_device(&sp_priv_f_ctx))                                            \
    X_TERM(bool, is_fifo, (void), sp_is_fifo(&sp_priv_f_ctx))                                                          \
    X_TERM(bool, is_socket, (void), sp_is_socket(&sp_priv_f_ctx))                                                      \
    X_TERM(bool, is_mount, (void), sp_is_mount(&sp_priv_f_ctx))                                                        \
    X_TERM(bool, is_junction, (void), sp_is_junction(&sp_priv_f_ctx))                                                  \
    X_TERM(SpStatResult, stat, (void), sp_stat(&sp_priv_f_ctx))                                                        \
    X_TERM(SpStatResult, lstat, (void), sp_lstat(&sp_priv_f_ctx))                                                      \
    X_TERM(bool, eq, (const SpPath *o), sp_path_eq(&sp_priv_f_ctx, o))                                                 \
    X_TERM(bool, ne, (const SpPath *o), sp_path_ne(&sp_priv_f_ctx, o))                                                 \
    X_TERM(bool, samefile, (const SpPath *o), sp_samefile(&sp_priv_f_ctx, o))                                          \
    X_TERM(SpIOResult, read_file, (char *buf, size_t buf_size), sp_read_file(&sp_priv_f_ctx, buf, buf_size))           \
    X_TERM(SpIOResult, write_file, (const char *data, size_t data_len), sp_write_file(&sp_priv_f_ctx, data, data_len)) \
    X_TERM(size_t, as_posix, (char *out, size_t out_size), sp_as_posix(&sp_priv_f_ctx, out, out_size))                 \
    X_TERM(size_t, as_uri, (char *buf, size_t buf_size), sp_as_uri(&sp_priv_f_ctx, buf, buf_size))                     \
    X_TERM(int, match, (const char *pattern), sp_match_ex(&sp_priv_f_ctx, pattern, -1))                                \
    X_TERM(bool, full_match, (const char *pattern), sp_full_match(&sp_priv_f_ctx, pattern, -1))                        \
    X_TERM(int, mkdir, (unsigned int mode, bool parents, bool exist_ok, unsigned int parent_mode),                     \
           sp_mkdir(&sp_priv_f_ctx, mode, parents, exist_ok, parent_mode))                                             \
    X_TERM(bool, touch, (unsigned int mode, bool exist_ok), sp_touch(&sp_priv_f_ctx, mode, exist_ok))                  \
    X_TERM(bool, unlink, (bool missing_ok), sp_unlink(&sp_priv_f_ctx, missing_ok))                                     \
    X_TERM(bool, rmdir, (void), sp_rmdir(&sp_priv_f_ctx))                                                              \
    X_TERM(bool, chmod, (unsigned int mode), sp_chmod(&sp_priv_f_ctx, mode))                                           \
    X_TERM(bool, symlink_to, (const SpPath *target, bool target_is_directory),                                         \
           sp_symlink_to(&sp_priv_f_ctx, target, target_is_directory))                                                 \
    X_TERM(bool, hardlink_to, (const SpPath *target), sp_hardlink_to(&sp_priv_f_ctx, target))

#define SP_F_CHAIN_METHODS(X)                                                                                          \
    X(parent, (void), sp_parent(&sp_priv_f_ctx))                                                                       \
    X(join, (const char *s), sp_join_one(&sp_priv_f_ctx, s))                                                           \
    X(with_segments, (const char **parts, size_t parts_count), sp_with_segments(&sp_priv_f_ctx, parts, parts_count))   \
    X(with_name, (const char *s), sp_with_name(&sp_priv_f_ctx, s))                                                     \
    X(with_stem, (const char *s), sp_with_stem(&sp_priv_f_ctx, s))                                                     \
    X(with_suffix, (const char *s), sp_with_suffix(&sp_priv_f_ctx, s))                                                 \
    X(absolute, (void), sp_absolute(&sp_priv_f_ctx))                                                                   \
    X(expanduser, (void), sp_expanduser(&sp_priv_f_ctx))                                                               \
    X(relative_to, (const SpPath *o, bool walk_up), sp_relative_to(&sp_priv_f_ctx, o, walk_up))                        \
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

/* clang-format off */
struct sp_fluent_ {
    /* Terminators - end chain and return value */
#define SP_F_TERM_FIELD(ret, name, params, expr) ret (*name) params;
    SP_F_TERMINATOR_METHODS(SP_F_TERM_FIELD)
#undef SP_F_TERM_FIELD
    /* Chainable - return pointer to avoid stack copies */
#define SP_F_CHAIN_FIELD(name, params, expr) SpPrivDontUseThisDirectly_ *(*name) params;
    SP_F_CHAIN_METHODS(SP_F_CHAIN_FIELD)
#undef SP_F_CHAIN_FIELD
};
/* clang-format on */

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

#endif /* SNAKEPATH_FLUENT */

#ifdef __cplusplus
}
#endif

#endif /* SNAKEPATH_H */

#ifdef SNAKEPATH_IMPLEMENTATION

#include <stdio.h>

/* Platform-specific includes, and the names both platforms' C runtimes share for getcwd and chmod */
#include <errno.h>
#ifdef SP_WINDOWS
#include <direct.h>
#include <io.h>
#include <windows.h>
#define sp_priv_getcwd _getcwd
#define sp_priv_chmod _chmod
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h> /* For realpath */
#include <fcntl.h>  /* For O_CREAT, etc. */
#include <utime.h>  /* For utime() */
#include <pwd.h>    /* For getpwuid, getpwnam */
#include <grp.h>    /* For getgrgid */
#define sp_priv_getcwd getcwd
#define sp_priv_chmod chmod
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

/* A mode's file type bits, the same everywhere (Windows and strict C99 headers leave their names out) */
enum {
    SP_PRIV_IFMT = 0170000,
    SP_PRIV_IFSOCK = 0140000,
    SP_PRIV_IFLNK = 0120000,
    SP_PRIV_IFREG = 0100000,
    SP_PRIV_IFBLK = 0060000,
    SP_PRIV_IFDIR = 0040000,
    SP_PRIV_IFCHR = 0020000,
    SP_PRIV_IFIFO = 0010000
};

#ifdef __cplusplus
extern "C" {
#endif

/* A path's flavor is never SP_FLAVOR_NATIVE: making a path resolves it to the platform's */
static SpFlavor sp_priv_flavor(SpFlavor flavor) {
    SP_ASSERT_FLAVOR(flavor);
#ifdef SP_WINDOWS
    return flavor == SP_FLAVOR_NATIVE ? SP_FLAVOR_WINDOWS : flavor;
#else
    return flavor == SP_FLAVOR_NATIVE ? SP_FLAVOR_POSIX : flavor;
#endif
}

/* Next code point of s[*i..len), advancing *i; bytes that aren't valid UTF-8 stand alone (like surrogateescape) */
static unsigned long sp_priv_utf8_next(const char *s, size_t len, size_t *i) {
    unsigned char c = SP_PRIV_CAST(unsigned char, s[(*i)++]);
    size_t extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    if (c < 0x80)
        return c;
    if (extra == 0 || *i + extra > len)
        return 0xDC00 + c;

    unsigned long cp = c & (0x3Fu >> extra);
    for (size_t k = 0; k < extra; k++) {
        unsigned char cc = SP_PRIV_CAST(unsigned char, s[*i + k]);
        if ((cc & 0xC0) != 0x80)
            return 0xDC00 + c;
        cp = (cp << 6) | (cc & 0x3F);
    }
    *i += extra;
    return cp;
}

/* Unicode case data from Python 3.15's str.lower()/str.upper(), re's simple case mappings and its IGNORECASE
 * equivalences (Unicode 17.0). "Cased" and "case-ignorable" decide str.lower()'s final sigma. */
/* Simple lowercase: runs of (first code point, count, stride, delta) */
static const long sp_priv_lower_runs[] = {
    0x41,    0x1a, 0x1, 0x20,    0xc0,    0x17, 0x1, 0x20,    0xd8,    0x7,  0x1, 0x20,    0x100,   0x18, 0x2, 0x1,
    0x130,   0x1,  0x1, -0xc7,   0x132,   0x3,  0x2, 0x1,     0x139,   0x8,  0x2, 0x1,     0x14a,   0x17, 0x2, 0x1,
    0x178,   0x1,  0x1, -0x79,   0x179,   0x3,  0x2, 0x1,     0x181,   0x1,  0x1, 0xd2,    0x182,   0x2,  0x2, 0x1,
    0x186,   0x1,  0x1, 0xce,    0x187,   0x1,  0x1, 0x1,     0x189,   0x2,  0x1, 0xcd,    0x18b,   0x1,  0x1, 0x1,
    0x18e,   0x1,  0x1, 0x4f,    0x18f,   0x1,  0x1, 0xca,    0x190,   0x1,  0x1, 0xcb,    0x191,   0x1,  0x1, 0x1,
    0x193,   0x1,  0x1, 0xcd,    0x194,   0x1,  0x1, 0xcf,    0x196,   0x1,  0x1, 0xd3,    0x197,   0x1,  0x1, 0xd1,
    0x198,   0x1,  0x1, 0x1,     0x19c,   0x1,  0x1, 0xd3,    0x19d,   0x1,  0x1, 0xd5,    0x19f,   0x1,  0x1, 0xd6,
    0x1a0,   0x3,  0x2, 0x1,     0x1a6,   0x1,  0x1, 0xda,    0x1a7,   0x1,  0x1, 0x1,     0x1a9,   0x1,  0x1, 0xda,
    0x1ac,   0x1,  0x1, 0x1,     0x1ae,   0x1,  0x1, 0xda,    0x1af,   0x1,  0x1, 0x1,     0x1b1,   0x2,  0x1, 0xd9,
    0x1b3,   0x2,  0x2, 0x1,     0x1b7,   0x1,  0x1, 0xdb,    0x1b8,   0x1,  0x1, 0x1,     0x1bc,   0x1,  0x1, 0x1,
    0x1c4,   0x1,  0x1, 0x2,     0x1c5,   0x1,  0x1, 0x1,     0x1c7,   0x1,  0x1, 0x2,     0x1c8,   0x1,  0x1, 0x1,
    0x1ca,   0x1,  0x1, 0x2,     0x1cb,   0x9,  0x2, 0x1,     0x1de,   0x9,  0x2, 0x1,     0x1f1,   0x1,  0x1, 0x2,
    0x1f2,   0x2,  0x2, 0x1,     0x1f6,   0x1,  0x1, -0x61,   0x1f7,   0x1,  0x1, -0x38,   0x1f8,   0x14, 0x2, 0x1,
    0x220,   0x1,  0x1, -0x82,   0x222,   0x9,  0x2, 0x1,     0x23a,   0x1,  0x1, 0x2a2b,  0x23b,   0x1,  0x1, 0x1,
    0x23d,   0x1,  0x1, -0xa3,   0x23e,   0x1,  0x1, 0x2a28,  0x241,   0x1,  0x1, 0x1,     0x243,   0x1,  0x1, -0xc3,
    0x244,   0x1,  0x1, 0x45,    0x245,   0x1,  0x1, 0x47,    0x246,   0x5,  0x2, 0x1,     0x370,   0x2,  0x2, 0x1,
    0x376,   0x1,  0x1, 0x1,     0x37f,   0x1,  0x1, 0x74,    0x386,   0x1,  0x1, 0x26,    0x388,   0x3,  0x1, 0x25,
    0x38c,   0x1,  0x1, 0x40,    0x38e,   0x2,  0x1, 0x3f,    0x391,   0x11, 0x1, 0x20,    0x3a3,   0x9,  0x1, 0x20,
    0x3cf,   0x1,  0x1, 0x8,     0x3d8,   0xc,  0x2, 0x1,     0x3f4,   0x1,  0x1, -0x3c,   0x3f7,   0x1,  0x1, 0x1,
    0x3f9,   0x1,  0x1, -0x7,    0x3fa,   0x1,  0x1, 0x1,     0x3fd,   0x3,  0x1, -0x82,   0x400,   0x10, 0x1, 0x50,
    0x410,   0x20, 0x1, 0x20,    0x460,   0x11, 0x2, 0x1,     0x48a,   0x1b, 0x2, 0x1,     0x4c0,   0x1,  0x1, 0xf,
    0x4c1,   0x7,  0x2, 0x1,     0x4d0,   0x30, 0x2, 0x1,     0x531,   0x26, 0x1, 0x30,    0x10a0,  0x26, 0x1, 0x1c60,
    0x10c7,  0x1,  0x1, 0x1c60,  0x10cd,  0x1,  0x1, 0x1c60,  0x13a0,  0x50, 0x1, 0x97d0,  0x13f0,  0x6,  0x1, 0x8,
    0x1c89,  0x1,  0x1, 0x1,     0x1c90,  0x2b, 0x1, -0xbc0,  0x1cbd,  0x3,  0x1, -0xbc0,  0x1e00,  0x4b, 0x2, 0x1,
    0x1e9e,  0x1,  0x1, -0x1dbf, 0x1ea0,  0x30, 0x2, 0x1,     0x1f08,  0x8,  0x1, -0x8,    0x1f18,  0x6,  0x1, -0x8,
    0x1f28,  0x8,  0x1, -0x8,    0x1f38,  0x8,  0x1, -0x8,    0x1f48,  0x6,  0x1, -0x8,    0x1f59,  0x4,  0x2, -0x8,
    0x1f68,  0x8,  0x1, -0x8,    0x1f88,  0x8,  0x1, -0x8,    0x1f98,  0x8,  0x1, -0x8,    0x1fa8,  0x8,  0x1, -0x8,
    0x1fb8,  0x2,  0x1, -0x8,    0x1fba,  0x2,  0x1, -0x4a,   0x1fbc,  0x1,  0x1, -0x9,    0x1fc8,  0x4,  0x1, -0x56,
    0x1fcc,  0x1,  0x1, -0x9,    0x1fd8,  0x2,  0x1, -0x8,    0x1fda,  0x2,  0x1, -0x64,   0x1fe8,  0x2,  0x1, -0x8,
    0x1fea,  0x2,  0x1, -0x70,   0x1fec,  0x1,  0x1, -0x7,    0x1ff8,  0x2,  0x1, -0x80,   0x1ffa,  0x2,  0x1, -0x7e,
    0x1ffc,  0x1,  0x1, -0x9,    0x2126,  0x1,  0x1, -0x1d5d, 0x212a,  0x1,  0x1, -0x20bf, 0x212b,  0x1,  0x1, -0x2046,
    0x2132,  0x1,  0x1, 0x1c,    0x2160,  0x10, 0x1, 0x10,    0x2183,  0x1,  0x1, 0x1,     0x24b6,  0x1a, 0x1, 0x1a,
    0x2c00,  0x30, 0x1, 0x30,    0x2c60,  0x1,  0x1, 0x1,     0x2c62,  0x1,  0x1, -0x29f7, 0x2c63,  0x1,  0x1, -0xee6,
    0x2c64,  0x1,  0x1, -0x29e7, 0x2c67,  0x3,  0x2, 0x1,     0x2c6d,  0x1,  0x1, -0x2a1c, 0x2c6e,  0x1,  0x1, -0x29fd,
    0x2c6f,  0x1,  0x1, -0x2a1f, 0x2c70,  0x1,  0x1, -0x2a1e, 0x2c72,  0x1,  0x1, 0x1,     0x2c75,  0x1,  0x1, 0x1,
    0x2c7e,  0x2,  0x1, -0x2a3f, 0x2c80,  0x32, 0x2, 0x1,     0x2ceb,  0x2,  0x2, 0x1,     0x2cf2,  0x1,  0x1, 0x1,
    0xa640,  0x17, 0x2, 0x1,     0xa680,  0xe,  0x2, 0x1,     0xa722,  0x7,  0x2, 0x1,     0xa732,  0x1f, 0x2, 0x1,
    0xa779,  0x2,  0x2, 0x1,     0xa77d,  0x1,  0x1, -0x8a04, 0xa77e,  0x5,  0x2, 0x1,     0xa78b,  0x1,  0x1, 0x1,
    0xa78d,  0x1,  0x1, -0xa528, 0xa790,  0x2,  0x2, 0x1,     0xa796,  0xa,  0x2, 0x1,     0xa7aa,  0x1,  0x1, -0xa544,
    0xa7ab,  0x1,  0x1, -0xa54f, 0xa7ac,  0x1,  0x1, -0xa54b, 0xa7ad,  0x1,  0x1, -0xa541, 0xa7ae,  0x1,  0x1, -0xa544,
    0xa7b0,  0x1,  0x1, -0xa512, 0xa7b1,  0x1,  0x1, -0xa52a, 0xa7b2,  0x1,  0x1, -0xa515, 0xa7b3,  0x1,  0x1, 0x3a0,
    0xa7b4,  0x8,  0x2, 0x1,     0xa7c4,  0x1,  0x1, -0x30,   0xa7c5,  0x1,  0x1, -0xa543, 0xa7c6,  0x1,  0x1, -0x8a38,
    0xa7c7,  0x2,  0x2, 0x1,     0xa7cb,  0x1,  0x1, -0xa567, 0xa7cc,  0x8,  0x2, 0x1,     0xa7dc,  0x1,  0x1, -0xa641,
    0xa7f5,  0x1,  0x1, 0x1,     0xff21,  0x1a, 0x1, 0x20,    0x10400, 0x28, 0x1, 0x28,    0x104b0, 0x24, 0x1, 0x28,
    0x10570, 0xb,  0x1, 0x27,    0x1057c, 0xf,  0x1, 0x27,    0x1058c, 0x7,  0x1, 0x27,    0x10594, 0x2,  0x1, 0x27,
    0x10c80, 0x33, 0x1, 0x40,    0x10d50, 0x16, 0x1, 0x20,    0x118a0, 0x20, 0x1, 0x20,    0x16e40, 0x20, 0x1, 0x20,
    0x16ea0, 0x19, 0x1, 0x1b,    0x1e900, 0x22, 0x1, 0x22};

/* Simple uppercase: runs of (first code point, count, stride, delta) */
static const long sp_priv_upper_runs[] = {
    0x61,    0x1a, 0x1, -0x20,   0xb5,    0x1,  0x1, 0x2e7,   0xe0,    0x17, 0x1, -0x20,   0xf8,    0x7,  0x1, -0x20,
    0xff,    0x1,  0x1, 0x79,    0x101,   0x18, 0x2, -0x1,    0x131,   0x1,  0x1, -0xe8,   0x133,   0x3,  0x2, -0x1,
    0x13a,   0x8,  0x2, -0x1,    0x14b,   0x17, 0x2, -0x1,    0x17a,   0x3,  0x2, -0x1,    0x17f,   0x1,  0x1, -0x12c,
    0x180,   0x1,  0x1, 0xc3,    0x183,   0x2,  0x2, -0x1,    0x188,   0x1,  0x1, -0x1,    0x18c,   0x1,  0x1, -0x1,
    0x192,   0x1,  0x1, -0x1,    0x195,   0x1,  0x1, 0x61,    0x199,   0x1,  0x1, -0x1,    0x19a,   0x1,  0x1, 0xa3,
    0x19b,   0x1,  0x1, 0xa641,  0x19e,   0x1,  0x1, 0x82,    0x1a1,   0x3,  0x2, -0x1,    0x1a8,   0x1,  0x1, -0x1,
    0x1ad,   0x1,  0x1, -0x1,    0x1b0,   0x1,  0x1, -0x1,    0x1b4,   0x2,  0x2, -0x1,    0x1b9,   0x1,  0x1, -0x1,
    0x1bd,   0x1,  0x1, -0x1,    0x1bf,   0x1,  0x1, 0x38,    0x1c5,   0x1,  0x1, -0x1,    0x1c6,   0x1,  0x1, -0x2,
    0x1c8,   0x1,  0x1, -0x1,    0x1c9,   0x1,  0x1, -0x2,    0x1cb,   0x1,  0x1, -0x1,    0x1cc,   0x1,  0x1, -0x2,
    0x1ce,   0x8,  0x2, -0x1,    0x1dd,   0x1,  0x1, -0x4f,   0x1df,   0x9,  0x2, -0x1,    0x1f2,   0x1,  0x1, -0x1,
    0x1f3,   0x1,  0x1, -0x2,    0x1f5,   0x1,  0x1, -0x1,    0x1f9,   0x14, 0x2, -0x1,    0x223,   0x9,  0x2, -0x1,
    0x23c,   0x1,  0x1, -0x1,    0x23f,   0x2,  0x1, 0x2a3f,  0x242,   0x1,  0x1, -0x1,    0x247,   0x5,  0x2, -0x1,
    0x250,   0x1,  0x1, 0x2a1f,  0x251,   0x1,  0x1, 0x2a1c,  0x252,   0x1,  0x1, 0x2a1e,  0x253,   0x1,  0x1, -0xd2,
    0x254,   0x1,  0x1, -0xce,   0x256,   0x2,  0x1, -0xcd,   0x259,   0x1,  0x1, -0xca,   0x25b,   0x1,  0x1, -0xcb,
    0x25c,   0x1,  0x1, 0xa54f,  0x260,   0x1,  0x1, -0xcd,   0x261,   0x1,  0x1, 0xa54b,  0x263,   0x1,  0x1, -0xcf,
    0x264,   0x1,  0x1, 0xa567,  0x265,   0x1,  0x1, 0xa528,  0x266,   0x1,  0x1, 0xa544,  0x268,   0x1,  0x1, -0xd1,
    0x269,   0x1,  0x1, -0xd3,   0x26a,   0x1,  0x1, 0xa544,  0x26b,   0x1,  0x1, 0x29f7,  0x26c,   0x1,  0x1, 0xa541,
    0x26f,   0x1,  0x1, -0xd3,   0x271,   0x1,  0x1, 0x29fd,  0x272,   0x1,  0x1, -0xd5,   0x275,   0x1,  0x1, -0xd6,
    0x27d,   0x1,  0x1, 0x29e7,  0x280,   0x1,  0x1, -0xda,   0x282,   0x1,  0x1, 0xa543,  0x283,   0x1,  0x1, -0xda,
    0x287,   0x1,  0x1, 0xa52a,  0x288,   0x1,  0x1, -0xda,   0x289,   0x1,  0x1, -0x45,   0x28a,   0x2,  0x1, -0xd9,
    0x28c,   0x1,  0x1, -0x47,   0x292,   0x1,  0x1, -0xdb,   0x29d,   0x1,  0x1, 0xa515,  0x29e,   0x1,  0x1, 0xa512,
    0x345,   0x1,  0x1, 0x54,    0x371,   0x2,  0x2, -0x1,    0x377,   0x1,  0x1, -0x1,    0x37b,   0x3,  0x1, 0x82,
    0x3ac,   0x1,  0x1, -0x26,   0x3ad,   0x3,  0x1, -0x25,   0x3b1,   0x11, 0x1, -0x20,   0x3c2,   0x1,  0x1, -0x1f,
    0x3c3,   0x9,  0x1, -0x20,   0x3cc,   0x1,  0x1, -0x40,   0x3cd,   0x2,  0x1, -0x3f,   0x3d0,   0x1,  0x1, -0x3e,
    0x3d1,   0x1,  0x1, -0x39,   0x3d5,   0x1,  0x1, -0x2f,   0x3d6,   0x1,  0x1, -0x36,   0x3d7,   0x1,  0x1, -0x8,
    0x3d9,   0xc,  0x2, -0x1,    0x3f0,   0x1,  0x1, -0x56,   0x3f1,   0x1,  0x1, -0x50,   0x3f2,   0x1,  0x1, 0x7,
    0x3f3,   0x1,  0x1, -0x74,   0x3f5,   0x1,  0x1, -0x60,   0x3f8,   0x1,  0x1, -0x1,    0x3fb,   0x1,  0x1, -0x1,
    0x430,   0x20, 0x1, -0x20,   0x450,   0x10, 0x1, -0x50,   0x461,   0x11, 0x2, -0x1,    0x48b,   0x1b, 0x2, -0x1,
    0x4c2,   0x7,  0x2, -0x1,    0x4cf,   0x1,  0x1, -0xf,    0x4d1,   0x30, 0x2, -0x1,    0x561,   0x26, 0x1, -0x30,
    0x10d0,  0x2b, 0x1, 0xbc0,   0x10fd,  0x3,  0x1, 0xbc0,   0x13f8,  0x6,  0x1, -0x8,    0x1c80,  0x1,  0x1, -0x186e,
    0x1c81,  0x1,  0x1, -0x186d, 0x1c82,  0x1,  0x1, -0x1864, 0x1c83,  0x2,  0x1, -0x1862, 0x1c85,  0x1,  0x1, -0x1863,
    0x1c86,  0x1,  0x1, -0x185c, 0x1c87,  0x1,  0x1, -0x1825, 0x1c88,  0x1,  0x1, 0x89c2,  0x1c8a,  0x1,  0x1, -0x1,
    0x1d79,  0x1,  0x1, 0x8a04,  0x1d7d,  0x1,  0x1, 0xee6,   0x1d8e,  0x1,  0x1, 0x8a38,  0x1e01,  0x4b, 0x2, -0x1,
    0x1e9b,  0x1,  0x1, -0x3b,   0x1ea1,  0x30, 0x2, -0x1,    0x1f00,  0x8,  0x1, 0x8,     0x1f10,  0x6,  0x1, 0x8,
    0x1f20,  0x8,  0x1, 0x8,     0x1f30,  0x8,  0x1, 0x8,     0x1f40,  0x6,  0x1, 0x8,     0x1f51,  0x4,  0x2, 0x8,
    0x1f60,  0x8,  0x1, 0x8,     0x1f70,  0x2,  0x1, 0x4a,    0x1f72,  0x4,  0x1, 0x56,    0x1f76,  0x2,  0x1, 0x64,
    0x1f78,  0x2,  0x1, 0x80,    0x1f7a,  0x2,  0x1, 0x70,    0x1f7c,  0x2,  0x1, 0x7e,    0x1f80,  0x8,  0x1, 0x8,
    0x1f90,  0x8,  0x1, 0x8,     0x1fa0,  0x8,  0x1, 0x8,     0x1fb0,  0x2,  0x1, 0x8,     0x1fb3,  0x1,  0x1, 0x9,
    0x1fbe,  0x1,  0x1, -0x1c25, 0x1fc3,  0x1,  0x1, 0x9,     0x1fd0,  0x2,  0x1, 0x8,     0x1fe0,  0x2,  0x1, 0x8,
    0x1fe5,  0x1,  0x1, 0x7,     0x1ff3,  0x1,  0x1, 0x9,     0x214e,  0x1,  0x1, -0x1c,   0x2170,  0x10, 0x1, -0x10,
    0x2184,  0x1,  0x1, -0x1,    0x24d0,  0x1a, 0x1, -0x1a,   0x2c30,  0x30, 0x1, -0x30,   0x2c61,  0x1,  0x1, -0x1,
    0x2c65,  0x1,  0x1, -0x2a2b, 0x2c66,  0x1,  0x1, -0x2a28, 0x2c68,  0x3,  0x2, -0x1,    0x2c73,  0x1,  0x1, -0x1,
    0x2c76,  0x1,  0x1, -0x1,    0x2c81,  0x32, 0x2, -0x1,    0x2cec,  0x2,  0x2, -0x1,    0x2cf3,  0x1,  0x1, -0x1,
    0x2d00,  0x26, 0x1, -0x1c60, 0x2d27,  0x1,  0x1, -0x1c60, 0x2d2d,  0x1,  0x1, -0x1c60, 0xa641,  0x17, 0x2, -0x1,
    0xa681,  0xe,  0x2, -0x1,    0xa723,  0x7,  0x2, -0x1,    0xa733,  0x1f, 0x2, -0x1,    0xa77a,  0x2,  0x2, -0x1,
    0xa77f,  0x5,  0x2, -0x1,    0xa78c,  0x1,  0x1, -0x1,    0xa791,  0x2,  0x2, -0x1,    0xa794,  0x1,  0x1, 0x30,
    0xa797,  0xa,  0x2, -0x1,    0xa7b5,  0x8,  0x2, -0x1,    0xa7c8,  0x2,  0x2, -0x1,    0xa7cd,  0x8,  0x2, -0x1,
    0xa7f6,  0x1,  0x1, -0x1,    0xab53,  0x1,  0x1, -0x3a0,  0xab70,  0x50, 0x1, -0x97d0, 0xff41,  0x1a, 0x1, -0x20,
    0x10428, 0x28, 0x1, -0x28,   0x104d8, 0x24, 0x1, -0x28,   0x10597, 0xb,  0x1, -0x27,   0x105a3, 0xf,  0x1, -0x27,
    0x105b3, 0x7,  0x1, -0x27,   0x105bb, 0x2,  0x1, -0x27,   0x10cc0, 0x33, 0x1, -0x40,   0x10d70, 0x16, 0x1, -0x20,
    0x118c0, 0x20, 0x1, -0x20,   0x16e60, 0x20, 0x1, -0x20,   0x16ebb, 0x19, 0x1, -0x1b,   0x1e922, 0x22, 0x1, -0x22};

/* Cased code points, as (first, last) ranges */
static const long sp_priv_cased[] = {
    0x41,    0x5a,    0x61,    0x7a,    0xaa,    0xaa,    0xb5,    0xb5,    0xba,    0xba,    0xc0,    0xd6,    0xd8,
    0xf6,    0xf8,    0x1ba,   0x1bc,   0x1bf,   0x1c4,   0x293,   0x296,   0x2af,   0x370,   0x373,   0x376,   0x377,
    0x37b,   0x37d,   0x37f,   0x37f,   0x386,   0x386,   0x388,   0x38a,   0x38c,   0x38c,   0x38e,   0x3a1,   0x3a3,
    0x3f5,   0x3f7,   0x481,   0x48a,   0x52f,   0x531,   0x556,   0x560,   0x588,   0x10a0,  0x10c5,  0x10c7,  0x10c7,
    0x10cd,  0x10cd,  0x10d0,  0x10fa,  0x10fd,  0x10ff,  0x13a0,  0x13f5,  0x13f8,  0x13fd,  0x1c80,  0x1c8a,  0x1c90,
    0x1cba,  0x1cbd,  0x1cbf,  0x1d00,  0x1d2b,  0x1d6b,  0x1d77,  0x1d79,  0x1d9a,  0x1e00,  0x1f15,  0x1f18,  0x1f1d,
    0x1f20,  0x1f45,  0x1f48,  0x1f4d,  0x1f50,  0x1f57,  0x1f59,  0x1f59,  0x1f5b,  0x1f5b,  0x1f5d,  0x1f5d,  0x1f5f,
    0x1f7d,  0x1f80,  0x1fb4,  0x1fb6,  0x1fbc,  0x1fbe,  0x1fbe,  0x1fc2,  0x1fc4,  0x1fc6,  0x1fcc,  0x1fd0,  0x1fd3,
    0x1fd6,  0x1fdb,  0x1fe0,  0x1fec,  0x1ff2,  0x1ff4,  0x1ff6,  0x1ffc,  0x2102,  0x2102,  0x2107,  0x2107,  0x210a,
    0x2113,  0x2115,  0x2115,  0x2119,  0x211d,  0x2124,  0x2124,  0x2126,  0x2126,  0x2128,  0x2128,  0x212a,  0x212d,
    0x212f,  0x2134,  0x2139,  0x2139,  0x213c,  0x213f,  0x2145,  0x2149,  0x214e,  0x214e,  0x2160,  0x217f,  0x2183,
    0x2184,  0x24b6,  0x24e9,  0x2c00,  0x2c7b,  0x2c7e,  0x2ce4,  0x2ceb,  0x2cee,  0x2cf2,  0x2cf3,  0x2d00,  0x2d25,
    0x2d27,  0x2d27,  0x2d2d,  0x2d2d,  0xa640,  0xa66d,  0xa680,  0xa69b,  0xa722,  0xa76f,  0xa771,  0xa787,  0xa78b,
    0xa78e,  0xa790,  0xa7dc,  0xa7f5,  0xa7f6,  0xa7fa,  0xa7fa,  0xab30,  0xab5a,  0xab60,  0xab68,  0xab70,  0xabbf,
    0xfb00,  0xfb06,  0xfb13,  0xfb17,  0xff21,  0xff3a,  0xff41,  0xff5a,  0x10400, 0x1044f, 0x104b0, 0x104d3, 0x104d8,
    0x104fb, 0x10570, 0x1057a, 0x1057c, 0x1058a, 0x1058c, 0x10592, 0x10594, 0x10595, 0x10597, 0x105a1, 0x105a3, 0x105b1,
    0x105b3, 0x105b9, 0x105bb, 0x105bc, 0x10c80, 0x10cb2, 0x10cc0, 0x10cf2, 0x10d50, 0x10d65, 0x10d70, 0x10d85, 0x118a0,
    0x118df, 0x16e40, 0x16e7f, 0x16ea0, 0x16eb8, 0x16ebb, 0x16ed3, 0x1d400, 0x1d454, 0x1d456, 0x1d49c, 0x1d49e, 0x1d49f,
    0x1d4a2, 0x1d4a2, 0x1d4a5, 0x1d4a6, 0x1d4a9, 0x1d4ac, 0x1d4ae, 0x1d4b9, 0x1d4bb, 0x1d4bb, 0x1d4bd, 0x1d4c3, 0x1d4c5,
    0x1d505, 0x1d507, 0x1d50a, 0x1d50d, 0x1d514, 0x1d516, 0x1d51c, 0x1d51e, 0x1d539, 0x1d53b, 0x1d53e, 0x1d540, 0x1d544,
    0x1d546, 0x1d546, 0x1d54a, 0x1d550, 0x1d552, 0x1d6a5, 0x1d6a8, 0x1d6c0, 0x1d6c2, 0x1d6da, 0x1d6dc, 0x1d6fa, 0x1d6fc,
    0x1d714, 0x1d716, 0x1d734, 0x1d736, 0x1d74e, 0x1d750, 0x1d76e, 0x1d770, 0x1d788, 0x1d78a, 0x1d7a8, 0x1d7aa, 0x1d7c2,
    0x1d7c4, 0x1d7cb, 0x1df00, 0x1df09, 0x1df0b, 0x1df1e, 0x1df25, 0x1df2a, 0x1e900, 0x1e943, 0x1f130, 0x1f149, 0x1f150,
    0x1f169, 0x1f170, 0x1f189};

/* Case-ignorable code points, as (first, last) ranges */
static const long sp_priv_case_ignorable[] = {
    0x27,    0x27,    0x2e,    0x2e,    0x3a,    0x3a,    0x5e,    0x5e,    0x60,    0x60,    0xa8,    0xa8,    0xad,
    0xad,    0xaf,    0xaf,    0xb4,    0xb4,    0xb7,    0xb8,    0x2b0,   0x36f,   0x374,   0x375,   0x37a,   0x37a,
    0x384,   0x385,   0x387,   0x387,   0x483,   0x489,   0x559,   0x559,   0x55f,   0x55f,   0x591,   0x5bd,   0x5bf,
    0x5bf,   0x5c1,   0x5c2,   0x5c4,   0x5c5,   0x5c7,   0x5c7,   0x5f4,   0x5f4,   0x600,   0x605,   0x610,   0x61a,
    0x61c,   0x61c,   0x640,   0x640,   0x64b,   0x65f,   0x670,   0x670,   0x6d6,   0x6dd,   0x6df,   0x6e8,   0x6ea,
    0x6ed,   0x70f,   0x70f,   0x711,   0x711,   0x730,   0x74a,   0x7a6,   0x7b0,   0x7eb,   0x7f5,   0x7fa,   0x7fa,
    0x7fd,   0x7fd,   0x816,   0x82d,   0x859,   0x85b,   0x888,   0x888,   0x890,   0x891,   0x897,   0x89f,   0x8c9,
    0x902,   0x93a,   0x93a,   0x93c,   0x93c,   0x941,   0x948,   0x94d,   0x94d,   0x951,   0x957,   0x962,   0x963,
    0x971,   0x971,   0x981,   0x981,   0x9bc,   0x9bc,   0x9c1,   0x9c4,   0x9cd,   0x9cd,   0x9e2,   0x9e3,   0x9fe,
    0x9fe,   0xa01,   0xa02,   0xa3c,   0xa3c,   0xa41,   0xa42,   0xa47,   0xa48,   0xa4b,   0xa4d,   0xa51,   0xa51,
    0xa70,   0xa71,   0xa75,   0xa75,   0xa81,   0xa82,   0xabc,   0xabc,   0xac1,   0xac5,   0xac7,   0xac8,   0xacd,
    0xacd,   0xae2,   0xae3,   0xafa,   0xaff,   0xb01,   0xb01,   0xb3c,   0xb3c,   0xb3f,   0xb3f,   0xb41,   0xb44,
    0xb4d,   0xb4d,   0xb55,   0xb56,   0xb62,   0xb63,   0xb82,   0xb82,   0xbc0,   0xbc0,   0xbcd,   0xbcd,   0xc00,
    0xc00,   0xc04,   0xc04,   0xc3c,   0xc3c,   0xc3e,   0xc40,   0xc46,   0xc48,   0xc4a,   0xc4d,   0xc55,   0xc56,
    0xc62,   0xc63,   0xc81,   0xc81,   0xcbc,   0xcbc,   0xcbf,   0xcbf,   0xcc6,   0xcc6,   0xccc,   0xccd,   0xce2,
    0xce3,   0xd00,   0xd01,   0xd3b,   0xd3c,   0xd41,   0xd44,   0xd4d,   0xd4d,   0xd62,   0xd63,   0xd81,   0xd81,
    0xdca,   0xdca,   0xdd2,   0xdd4,   0xdd6,   0xdd6,   0xe31,   0xe31,   0xe34,   0xe3a,   0xe46,   0xe4e,   0xeb1,
    0xeb1,   0xeb4,   0xebc,   0xec6,   0xec6,   0xec8,   0xece,   0xf18,   0xf19,   0xf35,   0xf35,   0xf37,   0xf37,
    0xf39,   0xf39,   0xf71,   0xf7e,   0xf80,   0xf84,   0xf86,   0xf87,   0xf8d,   0xf97,   0xf99,   0xfbc,   0xfc6,
    0xfc6,   0x102d,  0x1030,  0x1032,  0x1037,  0x1039,  0x103a,  0x103d,  0x103e,  0x1058,  0x1059,  0x105e,  0x1060,
    0x1071,  0x1074,  0x1082,  0x1082,  0x1085,  0x1086,  0x108d,  0x108d,  0x109d,  0x109d,  0x10fc,  0x10fc,  0x135d,
    0x135f,  0x1712,  0x1714,  0x1732,  0x1733,  0x1752,  0x1753,  0x1772,  0x1773,  0x17b4,  0x17b5,  0x17b7,  0x17bd,
    0x17c6,  0x17c6,  0x17c9,  0x17d3,  0x17d7,  0x17d7,  0x17dd,  0x17dd,  0x180b,  0x180f,  0x1843,  0x1843,  0x1885,
    0x1886,  0x18a9,  0x18a9,  0x1920,  0x1922,  0x1927,  0x1928,  0x1932,  0x1932,  0x1939,  0x193b,  0x1a17,  0x1a18,
    0x1a1b,  0x1a1b,  0x1a56,  0x1a56,  0x1a58,  0x1a5e,  0x1a60,  0x1a60,  0x1a62,  0x1a62,  0x1a65,  0x1a6c,  0x1a73,
    0x1a7c,  0x1a7f,  0x1a7f,  0x1aa7,  0x1aa7,  0x1ab0,  0x1add,  0x1ae0,  0x1aeb,  0x1b00,  0x1b03,  0x1b34,  0x1b34,
    0x1b36,  0x1b3a,  0x1b3c,  0x1b3c,  0x1b42,  0x1b42,  0x1b6b,  0x1b73,  0x1b80,  0x1b81,  0x1ba2,  0x1ba5,  0x1ba8,
    0x1ba9,  0x1bab,  0x1bad,  0x1be6,  0x1be6,  0x1be8,  0x1be9,  0x1bed,  0x1bed,  0x1bef,  0x1bf1,  0x1c2c,  0x1c33,
    0x1c36,  0x1c37,  0x1c78,  0x1c7d,  0x1cd0,  0x1cd2,  0x1cd4,  0x1ce0,  0x1ce2,  0x1ce8,  0x1ced,  0x1ced,  0x1cf4,
    0x1cf4,  0x1cf8,  0x1cf9,  0x1d2c,  0x1d6a,  0x1d78,  0x1d78,  0x1d9b,  0x1dff,  0x1fbd,  0x1fbd,  0x1fbf,  0x1fc1,
    0x1fcd,  0x1fcf,  0x1fdd,  0x1fdf,  0x1fed,  0x1fef,  0x1ffd,  0x1ffe,  0x200b,  0x200f,  0x2018,  0x2019,  0x2024,
    0x2024,  0x2027,  0x2027,  0x202a,  0x202e,  0x2060,  0x2064,  0x2066,  0x206f,  0x2071,  0x2071,  0x207f,  0x207f,
    0x2090,  0x209c,  0x20d0,  0x20f0,  0x2c7c,  0x2c7d,  0x2cef,  0x2cf1,  0x2d6f,  0x2d6f,  0x2d7f,  0x2d7f,  0x2de0,
    0x2dff,  0x2e2f,  0x2e2f,  0x3005,  0x3005,  0x302a,  0x302d,  0x3031,  0x3035,  0x303b,  0x303b,  0x3099,  0x309e,
    0x30fc,  0x30fe,  0xa015,  0xa015,  0xa4f8,  0xa4fd,  0xa60c,  0xa60c,  0xa66f,  0xa672,  0xa674,  0xa67d,  0xa67f,
    0xa67f,  0xa69c,  0xa69f,  0xa6f0,  0xa6f1,  0xa700,  0xa721,  0xa770,  0xa770,  0xa788,  0xa78a,  0xa7f1,  0xa7f4,
    0xa7f8,  0xa7f9,  0xa802,  0xa802,  0xa806,  0xa806,  0xa80b,  0xa80b,  0xa825,  0xa826,  0xa82c,  0xa82c,  0xa8c4,
    0xa8c5,  0xa8e0,  0xa8f1,  0xa8ff,  0xa8ff,  0xa926,  0xa92d,  0xa947,  0xa951,  0xa980,  0xa982,  0xa9b3,  0xa9b3,
    0xa9b6,  0xa9b9,  0xa9bc,  0xa9bd,  0xa9cf,  0xa9cf,  0xa9e5,  0xa9e6,  0xaa29,  0xaa2e,  0xaa31,  0xaa32,  0xaa35,
    0xaa36,  0xaa43,  0xaa43,  0xaa4c,  0xaa4c,  0xaa70,  0xaa70,  0xaa7c,  0xaa7c,  0xaab0,  0xaab0,  0xaab2,  0xaab4,
    0xaab7,  0xaab8,  0xaabe,  0xaabf,  0xaac1,  0xaac1,  0xaadd,  0xaadd,  0xaaec,  0xaaed,  0xaaf3,  0xaaf4,  0xaaf6,
    0xaaf6,  0xab5b,  0xab5f,  0xab69,  0xab6b,  0xabe5,  0xabe5,  0xabe8,  0xabe8,  0xabed,  0xabed,  0xfb1e,  0xfb1e,
    0xfbb2,  0xfbc2,  0xfe00,  0xfe0f,  0xfe13,  0xfe13,  0xfe20,  0xfe2f,  0xfe52,  0xfe52,  0xfe55,  0xfe55,  0xfeff,
    0xfeff,  0xff07,  0xff07,  0xff0e,  0xff0e,  0xff1a,  0xff1a,  0xff3e,  0xff3e,  0xff40,  0xff40,  0xff70,  0xff70,
    0xff9e,  0xff9f,  0xffe3,  0xffe3,  0xfff9,  0xfffb,  0x101fd, 0x101fd, 0x102e0, 0x102e0, 0x10376, 0x1037a, 0x10780,
    0x10785, 0x10787, 0x107b0, 0x107b2, 0x107ba, 0x10a01, 0x10a03, 0x10a05, 0x10a06, 0x10a0c, 0x10a0f, 0x10a38, 0x10a3a,
    0x10a3f, 0x10a3f, 0x10ae5, 0x10ae6, 0x10d24, 0x10d27, 0x10d4e, 0x10d4e, 0x10d69, 0x10d6d, 0x10d6f, 0x10d6f, 0x10eab,
    0x10eac, 0x10ec5, 0x10ec5, 0x10efa, 0x10eff, 0x10f46, 0x10f50, 0x10f82, 0x10f85, 0x11001, 0x11001, 0x11038, 0x11046,
    0x11070, 0x11070, 0x11073, 0x11074, 0x1107f, 0x11081, 0x110b3, 0x110b6, 0x110b9, 0x110ba, 0x110bd, 0x110bd, 0x110c2,
    0x110c2, 0x110cd, 0x110cd, 0x11100, 0x11102, 0x11127, 0x1112b, 0x1112d, 0x11134, 0x11173, 0x11173, 0x11180, 0x11181,
    0x111b6, 0x111be, 0x111c9, 0x111cc, 0x111cf, 0x111cf, 0x1122f, 0x11231, 0x11234, 0x11234, 0x11236, 0x11237, 0x1123e,
    0x1123e, 0x11241, 0x11241, 0x112df, 0x112df, 0x112e3, 0x112ea, 0x11300, 0x11301, 0x1133b, 0x1133c, 0x11340, 0x11340,
    0x11366, 0x1136c, 0x11370, 0x11374, 0x113bb, 0x113c0, 0x113ce, 0x113ce, 0x113d0, 0x113d0, 0x113d2, 0x113d2, 0x113e1,
    0x113e2, 0x11438, 0x1143f, 0x11442, 0x11444, 0x11446, 0x11446, 0x1145e, 0x1145e, 0x114b3, 0x114b8, 0x114ba, 0x114ba,
    0x114bf, 0x114c0, 0x114c2, 0x114c3, 0x115b2, 0x115b5, 0x115bc, 0x115bd, 0x115bf, 0x115c0, 0x115dc, 0x115dd, 0x11633,
    0x1163a, 0x1163d, 0x1163d, 0x1163f, 0x11640, 0x116ab, 0x116ab, 0x116ad, 0x116ad, 0x116b0, 0x116b5, 0x116b7, 0x116b7,
    0x1171d, 0x1171d, 0x1171f, 0x1171f, 0x11722, 0x11725, 0x11727, 0x1172b, 0x1182f, 0x11837, 0x11839, 0x1183a, 0x1193b,
    0x1193c, 0x1193e, 0x1193e, 0x11943, 0x11943, 0x119d4, 0x119d7, 0x119da, 0x119db, 0x119e0, 0x119e0, 0x11a01, 0x11a0a,
    0x11a33, 0x11a38, 0x11a3b, 0x11a3e, 0x11a47, 0x11a47, 0x11a51, 0x11a56, 0x11a59, 0x11a5b, 0x11a8a, 0x11a96, 0x11a98,
    0x11a99, 0x11b60, 0x11b60, 0x11b62, 0x11b64, 0x11b66, 0x11b66, 0x11c30, 0x11c36, 0x11c38, 0x11c3d, 0x11c3f, 0x11c3f,
    0x11c92, 0x11ca7, 0x11caa, 0x11cb0, 0x11cb2, 0x11cb3, 0x11cb5, 0x11cb6, 0x11d31, 0x11d36, 0x11d3a, 0x11d3a, 0x11d3c,
    0x11d3d, 0x11d3f, 0x11d45, 0x11d47, 0x11d47, 0x11d90, 0x11d91, 0x11d95, 0x11d95, 0x11d97, 0x11d97, 0x11dd9, 0x11dd9,
    0x11ef3, 0x11ef4, 0x11f00, 0x11f01, 0x11f36, 0x11f3a, 0x11f40, 0x11f40, 0x11f42, 0x11f42, 0x11f5a, 0x11f5a, 0x13430,
    0x13440, 0x13447, 0x13455, 0x1611e, 0x16129, 0x1612d, 0x1612f, 0x16af0, 0x16af4, 0x16b30, 0x16b36, 0x16b40, 0x16b43,
    0x16d40, 0x16d42, 0x16d6b, 0x16d6c, 0x16f4f, 0x16f4f, 0x16f8f, 0x16f9f, 0x16fe0, 0x16fe1, 0x16fe3, 0x16fe4, 0x16ff2,
    0x16ff3, 0x1aff0, 0x1aff3, 0x1aff5, 0x1affb, 0x1affd, 0x1affe, 0x1bc9d, 0x1bc9e, 0x1bca0, 0x1bca3, 0x1cf00, 0x1cf2d,
    0x1cf30, 0x1cf46, 0x1d167, 0x1d169, 0x1d173, 0x1d182, 0x1d185, 0x1d18b, 0x1d1aa, 0x1d1ad, 0x1d242, 0x1d244, 0x1da00,
    0x1da36, 0x1da3b, 0x1da6c, 0x1da75, 0x1da75, 0x1da84, 0x1da84, 0x1da9b, 0x1da9f, 0x1daa1, 0x1daaf, 0x1e000, 0x1e006,
    0x1e008, 0x1e018, 0x1e01b, 0x1e021, 0x1e023, 0x1e024, 0x1e026, 0x1e02a, 0x1e030, 0x1e06d, 0x1e08f, 0x1e08f, 0x1e130,
    0x1e13d, 0x1e2ae, 0x1e2ae, 0x1e2ec, 0x1e2ef, 0x1e4eb, 0x1e4ef, 0x1e5ee, 0x1e5ef, 0x1e6e3, 0x1e6e3, 0x1e6e6, 0x1e6e6,
    0x1e6ee, 0x1e6ef, 0x1e6f5, 0x1e6f5, 0x1e6ff, 0x1e6ff, 0x1e8d0, 0x1e8d6, 0x1e944, 0x1e94b, 0x1f3fb, 0x1f3ff, 0xe0001,
    0xe0001, 0xe0020, 0xe007f, 0xe0100, 0xe01ef};

/* re's IGNORECASE equivalences beyond simple lowercase: groups of lowercase code points, each ended by 0 */
static const long sp_priv_case_groups[] = {
    0x69,   0x131, 0x0,    0x73,   0x17f, 0x0,    0xb5,   0x3bc, 0x0,    0x345,  0x3b9, 0x1fbe, 0x0,    0x390, 0x1fd3,
    0x0,    0x3b0, 0x1fe3, 0x0,    0x3b2, 0x3d0,  0x0,    0x3b5, 0x3f5,  0x0,    0x3b8, 0x3d1,  0x0,    0x3ba, 0x3f0,
    0x0,    0x3c0, 0x3d6,  0x0,    0x3c1, 0x3f1,  0x0,    0x3c2, 0x3c3,  0x0,    0x3c6, 0x3d5,  0x0,    0x432, 0x1c80,
    0x0,    0x434, 0x1c81, 0x0,    0x43e, 0x1c82, 0x0,    0x441, 0x1c83, 0x0,    0x442, 0x1c84, 0x1c85, 0x0,   0x44a,
    0x1c86, 0x0,   0x463,  0x1c87, 0x0,   0x1c88, 0xa64b, 0x0,   0x1e61, 0x1e9b, 0x0,   0xfb05, 0xfb06, 0x0};

/* The simple case mapping of cp, from runs of (first code point, count, stride, delta) */
static unsigned long sp_priv_case_map(unsigned long cp, const long *runs, size_t n) {
    size_t lo = 0;
    size_t hi = n / 4;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (SP_PRIV_CAST(unsigned long, runs[mid * 4]) <= cp)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo == 0)
        return cp;

    const long *run = runs + (lo - 1) * 4;
    unsigned long offset = cp - SP_PRIV_CAST(unsigned long, run[0]);
    unsigned long stride = SP_PRIV_CAST(unsigned long, run[2]);
    if (offset % stride != 0 || offset / stride >= SP_PRIV_CAST(unsigned long, run[1]))
        return cp;
    return SP_PRIV_CAST(unsigned long, SP_PRIV_CAST(long, cp) + run[3]);
}

/* Whether cp lies in one of the (first, last) ranges */
static bool sp_priv_in_ranges(unsigned long cp, const long *ranges, size_t n) {
    size_t lo = 0;
    size_t hi = n / 2;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (SP_PRIV_CAST(unsigned long, ranges[mid * 2]) <= cp)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo > 0 && cp <= SP_PRIV_CAST(unsigned long, ranges[lo * 2 - 1]);
}

/* Whether two lowercase code points are the same letter to re's IGNORECASE (equal, or in one of its case groups) */
static bool sp_priv_case_equiv(unsigned long a, unsigned long b) {
    if (a == b)
        return true;

    bool in_a = false;
    bool in_b = false;
    for (size_t i = 0; i < SP_ARRAY_LEN(sp_priv_case_groups); i++) {
        unsigned long c = SP_PRIV_CAST(unsigned long, sp_priv_case_groups[i]);
        if (c == 0 && in_a && in_b)
            return true;
        in_a = c != 0 && (in_a || c == a);
        in_b = c != 0 && (in_b || c == b);
    }
    return false;
}

/* Whether some code point in [lo, hi] lowercases to cp */
static bool sp_priv_lowered_from(unsigned long cp, unsigned long lo, unsigned long hi) {
    bool lowercase = true; /* cp is its own lowercase */
    for (size_t i = 0; i < SP_ARRAY_LEN(sp_priv_lower_runs); i += 4) {
        const long *run = sp_priv_lower_runs + i;
        unsigned long first = SP_PRIV_CAST(unsigned long, run[0]);
        unsigned long stride = SP_PRIV_CAST(unsigned long, run[2]);
        unsigned long last = first + (SP_PRIV_CAST(unsigned long, run[1]) - 1) * stride;
        unsigned long x = SP_PRIV_CAST(unsigned long, SP_PRIV_CAST(long, cp) - run[3]);
        if (cp >= first && cp <= last && (cp - first) % stride == 0)
            lowercase = false;
        if (x >= first && x <= last && (x - first) % stride == 0 && x >= lo && x <= hi)
            return true;
    }
    return lowercase && cp >= lo && cp <= hi;
}

/* How CPython orders str(a).lower() and str(b).lower() by code point: simple lowercase, except that U+0130 lowers
 * to i and a combining dot, and U+03A3 to a final sigma after a cased letter with no cased letter following
 * (case-ignorable letters in between don't count) */
static int sp_priv_fold_cmp(const char *a, size_t alen, const char *b, size_t blen) {
    const char *s[2] = {a, b};
    size_t len[2] = {alen, blen};
    size_t pos[2] = {0, 0};
    unsigned long owed[2] = {0, 0}; /* the combining dot after U+0130's i */

    for (;;) {
        unsigned long c[2] = {0, 0};
        bool ended[2] = {false, false};
        for (int k = 0; k < 2; k++) {
            if (owed[k] != 0) {
                c[k] = owed[k];
                owed[k] = 0;
                continue;
            }
            if (pos[k] == len[k]) {
                ended[k] = true;
                continue;
            }

            size_t start = pos[k];
            c[k] = sp_priv_utf8_next(s[k], len[k], &pos[k]);
            if (c[k] == 0x130) {
                c[k] = 'i';
                owed[k] = 0x307;
            } else if (c[k] == 0x3a3) {
                /* The nearest letters around it that aren't case-ignorable: cased before, not cased after */
                bool cased_before = false;
                for (size_t j = start; j > 0;) {
                    size_t q = j - 1;
                    while (q > 0 && j - q < 4 && (SP_PRIV_CAST(unsigned char, s[k][q]) & 0xC0) == 0x80)
                        q--;
                    size_t t = q;
                    unsigned long before = sp_priv_utf8_next(s[k], j, &t);
                    if (t != j) {
                        q = j - 1;
                        t = q;
                        before = sp_priv_utf8_next(s[k], j, &t);
                    }
                    j = q;
                    if (!sp_priv_in_ranges(before, sp_priv_case_ignorable, SP_ARRAY_LEN(sp_priv_case_ignorable))) {
                        cased_before = sp_priv_in_ranges(before, sp_priv_cased, SP_ARRAY_LEN(sp_priv_cased));
                        break;
                    }
                }

                bool cased_after = false;
                for (size_t t = pos[k]; t < len[k];) {
                    unsigned long after = sp_priv_utf8_next(s[k], len[k], &t);
                    if (!sp_priv_in_ranges(after, sp_priv_case_ignorable, SP_ARRAY_LEN(sp_priv_case_ignorable))) {
                        cased_after = sp_priv_in_ranges(after, sp_priv_cased, SP_ARRAY_LEN(sp_priv_cased));
                        break;
                    }
                }
                c[k] = cased_before && !cased_after ? 0x3c2 : 0x3c3;
            } else {
                c[k] = sp_priv_case_map(c[k], sp_priv_lower_runs, SP_ARRAY_LEN(sp_priv_lower_runs));
            }
        }

        if (ended[0] || ended[1])
            return ended[0] && ended[1] ? 0 : ended[0] ? -1 : 1;
        if (c[0] != c[1])
            return c[0] < c[1] ? -1 : 1;
    }
}

/* The length of a leading Windows drive like "c:": any one character but a separator, then ':'; 0 when there is none
 * (or the flavor is POSIX). A character is a UTF-8 sequence, or a byte that isn't part of one; like CPython, whose
 * ntpath.splitroot is native C on a Windows host, what counts as one character depends on the host. */
static size_t sp_priv_drive_len(const char *s, size_t len, SpFlavor flavor) {
    if (flavor != SP_FLAVOR_WINDOWS || len < 2 || s[0] == '/' || s[0] == '\\')
        return 0;

    unsigned char lead = SP_PRIV_CAST(unsigned char, s[0]);
    size_t extra = lead >= 0xF0 ? 3 : lead >= 0xE0 ? 2 : lead >= 0xC0 ? 1 : 0;
    size_t n = 1;
    while (n <= extra && n < len && (SP_PRIV_CAST(unsigned char, s[n]) & 0xC0) == 0x80)
        n++;
    if (n != extra + 1)
        n = 1;
#ifdef SP_WINDOWS
    /* Windows' native splitroot counts UTF-16 units, where a character past the BMP takes two: never a drive there */
    if (n == 4)
        return 0;
#endif
    return n < len && s[n] == ':' ? n + 1 : 0;
}

/* Byte order, with ASCII letters lowercased when case-insensitive */
static int sp_priv_str_cmp_case(const char *a, size_t alen, const char *b, size_t blen, bool case_insensitive) {
    for (size_t i = 0; i < alen && i < blen; i++) {
        int ca = SP_PRIV_CAST(unsigned char, a[i]);
        int cb = SP_PRIV_CAST(unsigned char, b[i]);
        if (case_insensitive && ca >= 'A' && ca <= 'Z')
            ca += 32;
        if (case_insensitive && cb >= 'A' && cb <= 'Z')
            cb += 32;
        if (ca != cb)
            return ca < cb ? -1 : 1;
    }

    return alen < blen ? -1 : (alen > blen ? 1 : 0);
}

/* Append one bounded piece, optionally separated; an oversized piece is skipped as a whole. */
static void sp_priv_append(SpPath *r, const char *s, size_t len, bool separated) {
    char sep = r->flavor == SP_FLAVOR_WINDOWS ? '\\' : '/';
    if (separated && r->len > 0 && r->buf[r->len - 1] != '/' && r->buf[r->len - 1] != sep && r->len + 1 < SP_PATH_MAX)
        r->buf[r->len++] = sep;

    if (r->len + len < SP_PATH_MAX) {
        memcpy(r->buf + r->len, s, len);
        r->len += len;
    }
    r->buf[r->len] = '\0';
}

static SpPath sp_priv_path_from_raw(const char *s, size_t len, SpFlavor flavor) {
    SpPath p = SP_PRIV_ZERO;
    p.flavor = flavor;
    p.len = len >= SP_PATH_MAX ? SP_PATH_MAX - 1 : len;
    if (s && p.len > 0)
        memcpy(p.buf, s, p.len);
    p.buf[p.len] = '\0';
    return p;
}

/* The empty path, carrying an error code for sp_path_error_code() (SP_OK for a plain empty path) */
static SpPath sp_priv_error_path(SpFlavor flavor, int err_code) {
    SpPath p = SP_PRIV_ZERO;
    p.flavor = flavor;
    p.buf[0] = SP_PRIV_CAST(char, err_code);
    return p;
}

static inline bool sp_priv_is_unc(const char *s, size_t len, SpFlavor flavor) {
    return flavor == SP_FLAVOR_WINDOWS && len >= 2 && (s[0] == '/' || s[0] == '\\') && (s[1] == '/' || s[1] == '\\');
}

/* Returns the anchor (drive + root) length and stores the drive length if asked: ntpath.splitroot plus pathlib's
 * implicit root. A UNC drive is //server/share or //?/UNC/server/share (either part possibly empty) up to the next
 * separator; without one the drive is the whole path, and a complete //server/share gets a root past len. */
static size_t sp_priv_split_anchor(const char *s, size_t len, SpFlavor flavor, size_t *drive_len) {
    size_t drive = 0;
    size_t root;

    if (flavor != SP_FLAVOR_WINDOWS) {
        /* POSIX: paths starting with exactly // have root // */
        bool two = len >= 2 && s[0] == '/' && s[1] == '/' && (len == 2 || s[2] != '/');
        root = two ? 2 : (len > 0 && s[0] == '/') ? 1 : 0;
    } else if (len >= 2 && (s[0] == '/' || s[0] == '\\') && (s[1] == '/' || s[1] == '\\')) {
        bool unc_prefix = len >= 8 && s[2] == '?' && (s[3] == '/' || s[3] == '\\') && (s[4] == 'U' || s[4] == 'u') &&
                          (s[5] == 'N' || s[5] == 'n') && (s[6] == 'C' || s[6] == 'c') && (s[7] == '/' || s[7] == '\\');
        size_t start = unc_prefix ? 8 : 2;
        size_t server = start;
        while (server < len && s[server] != '/' && s[server] != '\\')
            server++;
        size_t share = server < len ? server + 1 : len;
        while (share < len && s[share] != '/' && s[share] != '\\')
            share++;

        /* Devices like //./x and //?/x (a server of "", "?", "." or "?.") get no implicit root */
        size_t n = server - start;
        bool device = !unc_prefix &&
                      (n == 0 || (n == 1 && memchr("?.", s[start], 2)) || (n == 2 && memcmp(s + start, "?.", 2) == 0));
        drive = share < len ? share : len;
        root = share < len || (server + 1 < len && !device) ? 1 : 0;
    } else {
        drive = sp_priv_drive_len(s, len, flavor);
        root = drive < len && (s[drive] == '/' || s[drive] == '\\') ? 1 : 0;
    }

    if (drive_len)
        *drive_len = drive;
    return drive + root;
}

/* Canonical separators and no repeated separators or '.' parts, for a path with the given anchor length; a complete
 * UNC drive gets its implicit root. A leading '.' stays to protect a drive-like next part from drive parsing
 * ('./c:a' stays '.\c:a', but 'a/./c:a' becomes 'a\c:a'). */
static void sp_priv_normalize(SpPath *p, size_t anchor) {
    char *buf = p->buf;
    char sep = p->flavor == SP_FLAVOR_WINDOWS ? '\\' : '/';
    size_t len = p->len;
    size_t j = anchor < len ? anchor : len;

    for (size_t i = 0; i < j; i++)
        if (buf[i] == '/')
            buf[i] = sep;
    if (anchor > len && j + 1 < SP_PATH_MAX)
        buf[j++] = sep;

    for (size_t i = j, end; i < len; i = end) {
        while (i < len && (buf[i] == '/' || buf[i] == sep))
            i++;
        end = i;
        while (end < len && buf[end] != '/' && buf[end] != sep)
            end++;

        size_t next = end;
        while (next < len && (buf[next] == '/' || buf[next] == sep))
            next++;
        bool protects = j == 0 && sep == '\\' && sp_priv_drive_len(buf + next, len - next, p->flavor) > 0;
        if (i == end || (end - i == 1 && buf[i] == '.' && !protects))
            continue;

        memmove(buf + j, buf + i, end - i);
        j += end - i;
        if (end < len)
            buf[j++] = sep;
    }

    if (j > anchor && (buf[j - 1] == '/' || buf[j - 1] == sep))
        j--;
    buf[j] = '\0';
    p->len = j;
}

/* Length of the parent of buf[0..len), given its anchor length: the anchor (or the empty path) is its own parent */
static size_t sp_priv_parent_len(const char *buf, size_t len, SpFlavor flavor, size_t anchor) {
    if (len <= anchor)
        return len;

    char sep = flavor == SP_FLAVOR_WINDOWS ? '\\' : '/';
    size_t i = len;
    while (i > anchor && buf[i - 1] != '/' && buf[i - 1] != sep)
        i--;
    if (i > anchor)
        i--;

    /* The "." protecting a drive-like part ('.\c:') is not a parent of its own: the parent is the empty path */
    if (anchor == 0 && i == 1 && buf[0] == '.')
        return 0;
    return i <= anchor ? anchor : i;
}

SpPath sp_path_from_n(const char *s, size_t len, SpFlavor flavor) {
    SpPath p = sp_priv_path_from_raw(s, len, sp_priv_flavor(flavor));
    sp_priv_normalize(&p, sp_priv_split_anchor(p.buf, p.len, p.flavor, NULL));
    return p;
}

SpPath sp_path_new(const char *s, SpFlavor flavor) { return sp_path_from_n(s, s ? strlen(s) : 0, flavor); }

SpPath sp_path_convert(const char *s, SpFlavor src_flavor, SpFlavor dest_flavor) {
    SpPath src = sp_path_from_n(s, s ? strlen(s) : 0, src_flavor);
    SpPath dest = src;
    dest.flavor = sp_priv_flavor(dest_flavor);
    if (src.flavor == dest.flavor)
        return src;

    char ssep = src.flavor == SP_FLAVOR_WINDOWS ? '\\' : '/';
    char dsep = dest.flavor == SP_FLAVOR_WINDOWS ? '\\' : '/';
    for (size_t i = 0; i < dest.len; i++)
        if (dest.buf[i] == ssep)
            dest.buf[i] = dsep;
    sp_priv_normalize(&dest, sp_priv_split_anchor(dest.buf, dest.len, dest.flavor, NULL));
    return dest;
}

const char *sp_str(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    return p->len == 0 ? "." : p->buf;
}

size_t sp_as_posix(const SpPath *p, char *out, size_t out_size) {
    size_t n = sp_priv_copy_trunc(out, out_size, sp_str(p), p->len > 0 ? p->len : 1);
    for (size_t i = 0; i < n; i++)
        if (out[i] == '\\' && p->flavor == SP_FLAVOR_WINDOWS)
            out[i] = '/';
    return n;
}

SpTerm sp_drive(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    size_t drive;
    sp_priv_split_anchor(p->buf, p->len, p->flavor, &drive);
    return sp_priv_term(p->buf, drive);
}

SpTerm sp_root(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    size_t drive;
    size_t anchor = sp_priv_split_anchor(p->buf, p->len, p->flavor, &drive);
    return sp_priv_term(p->buf + drive, (anchor > p->len ? p->len : anchor) - drive);
}

SpTerm sp_anchor(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    size_t anchor = sp_priv_split_anchor(p->buf, p->len, p->flavor, NULL);
    return sp_priv_term(p->buf, anchor > p->len ? p->len : anchor);
}

/* The name of p, given its anchor length, as a view into its buffer */
static SpStr sp_priv_name_sv(const SpPath *p, size_t anchor) {
    if (anchor >= p->len)
        return SP_PRIV_STR(p->buf + p->len, 0);

    char sep = p->flavor == SP_FLAVOR_WINDOWS ? '\\' : '/';
    size_t i = p->len;
    while (i > anchor && p->buf[i - 1] != '/' && p->buf[i - 1] != sep)
        i--;
    return SP_PRIV_STR(p->buf + i, p->len - i);
}

SpTerm sp_name(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpStr sv = sp_priv_name_sv(p, sp_priv_split_anchor(p->buf, p->len, p->flavor, NULL));
    return sp_priv_term(sv.data, sv.len);
}

/* From the last '.' after the name's leading dots; "a." has suffix "." */
static inline SpStr sp_priv_suffix_sv(SpStr name) {
    size_t lead = 0;
    while (lead < name.len && name.data[lead] == '.')
        lead++;

    size_t i = name.len;
    while (i > lead && name.data[i - 1] != '.')
        i--;
    return i > lead ? SP_PRIV_STR(name.data + i - 1, name.len - i + 1) : SP_PRIV_STR(SP_PRIV_NULL, 0);
}

SpTerm sp_suffix(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpStr sv = sp_priv_suffix_sv(sp_priv_name_sv(p, sp_priv_split_anchor(p->buf, p->len, p->flavor, NULL)));
    return sp_priv_term(sv.data, sv.len);
}

SpTerm sp_stem(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpStr name = sp_priv_name_sv(p, sp_priv_split_anchor(p->buf, p->len, p->flavor, NULL));
    return sp_priv_term(name.data, name.len - sp_priv_suffix_sv(name).len);
}

SpSuffixes sp_suffixes(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpSuffixes r = SP_PRIV_ZERO;
    SpStr name = sp_priv_name_sv(p, sp_priv_split_anchor(p->buf, p->len, p->flavor, NULL));

    /* Each '.' after the leading dots starts a suffix, even an empty one ("a..b" -> ".", ".b") */
    size_t i = 0;
    while (i < name.len && name.data[i] == '.')
        i++;
    while (i < name.len && name.data[i] != '.')
        i++;
    while (i < name.len && r.count < SP_MAX_SUFFIXES) {
        size_t end = i + 1;
        while (end < name.len && name.data[end] != '.')
            end++;
        r.items[r.count++] = SP_PRIV_STR(name.data + i, end - i);
        i = end;
    }
    return r;
}

SpPath sp_parent(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    size_t anchor = sp_priv_split_anchor(p->buf, p->len, p->flavor, NULL);
    return sp_priv_path_from_raw(p->buf, sp_priv_parent_len(p->buf, p->len, p->flavor, anchor), p->flavor);
}

SpPartsIter sp_parts_begin(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpPartsIter it = SP_PRIV_ZERO;
    it.path = p;
    it.anchor = sp_priv_split_anchor(p->buf, p->len, p->flavor, NULL);
    it.end = p->len > it.anchor ? p->len : it.anchor;

    /* The leading '.' in '.\c:' protects a drive-like part, but is not itself a part. */
    size_t next = 1;
    while (next < p->len && (p->buf[next] == '/' || p->buf[next] == '\\'))
        next++;
    if (it.anchor == 0 && p->len > 1 && p->buf[0] == '.' && next > 1 &&
        sp_priv_drive_len(p->buf + next, p->len - next, p->flavor) > 0)
        it.pos = next;
    return it;
}

/* The anchor is one part; the rest are split at separators */
bool sp_parts_next(SpPartsIter *it, SpStr *out) {
    const char *buf = it->path->buf;
    char sep = it->path->flavor == SP_FLAVOR_WINDOWS ? '\\' : '/';
    size_t start = it->pos;
    size_t end = it->anchor;

    if (start > 0 || end == 0) {
        while (start < it->end && (buf[start] == '/' || buf[start] == sep))
            start++;
        end = start;
        while (end < it->end && buf[end] != '/' && buf[end] != sep)
            end++;
    }
    it->pos = end;

    if (end <= start)
        return false;
    *out = SP_PRIV_STR(buf + start, end - start);
    return true;
}

size_t sp_parts_count(const SpPath *p) {
    SpPartsIter it = sp_parts_begin(p);
    SpStr part;
    size_t c = 0;
    while (sp_parts_next(&it, &part))
        c++;
    return c;
}

SpParentsIter sp_parents_begin(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpParentsIter it = SP_PRIV_ZERO;
    it.path = p;
    it.current_len = p->len;
    return it;
}

/* Each parent is a prefix of the path; the anchor (or the empty path) is its own parent, which ends the walk */
bool sp_parents_next(SpParentsIter *it, SpPath *out) {
    const SpPath *p = it->path;
    size_t anchor = sp_priv_split_anchor(p->buf, p->len, p->flavor, NULL);
    size_t next = sp_priv_parent_len(p->buf, it->current_len, p->flavor, anchor);
    if (next == it->current_len)
        return false;

    it->current_len = next;
    *out = sp_priv_path_from_raw(p->buf, next, p->flavor);
    return true;
}

size_t sp_parents_count(const SpPath *p) {
    SpParentsIter it = sp_parents_begin(p);
    SpPath parent;
    size_t count = 0;
    while (sp_parents_next(&it, &parent))
        count++;
    return count;
}

/* Internal length-aware join - handles embedded nulls correctly */
static SpPath sp_priv_join_len(const SpPath *base, const char *other, size_t olen) {
    SpFlavor flavor = base->flavor;
    size_t drive;
    size_t anchor = sp_priv_split_anchor(base->buf, base->len, flavor, &drive);
    /* ntpath.join puts no separator after a rootless drive ending in ':' (like "c:") */
    bool add_sep = !(drive > 0 && drive == base->len && anchor == drive && base->buf[drive - 1] == ':');
    bool replace = false; /* by an anchored other */
    SpPath r = *base;

    if (olen > 0 && (other[0] == '/' || (flavor == SP_FLAVOR_WINDOWS && other[0] == '\\'))) {
        replace = drive == 0 || sp_priv_is_unc(other, olen, flavor);
        r.len = drive; /* Root only: keep the base drive. */
        add_sep = false;
    } else if (sp_priv_drive_len(other, olen, flavor) > 0) {
        /* ntpath.join compares drives by str.lower(), where U+0130 (two characters lowered) matches only itself */
        size_t odrive = sp_priv_drive_len(other, olen, flavor);
        size_t bdrive = sp_priv_drive_len(base->buf, base->len, flavor);
        size_t oi = 0;
        size_t bi = 0;
        unsigned long oc = sp_priv_utf8_next(other, odrive, &oi);
        unsigned long bc = bdrive > 0 ? sp_priv_utf8_next(base->buf, bdrive, &bi) : 0;
        bool same = oc == bc || (oc != 0x130 && bc != 0x130 &&
                                 sp_priv_case_map(oc, sp_priv_lower_runs, SP_ARRAY_LEN(sp_priv_lower_runs)) ==
                                     sp_priv_case_map(bc, sp_priv_lower_runs, SP_ARRAY_LEN(sp_priv_lower_runs)));
        replace = bdrive == 0 || !same || (olen > odrive && (other[odrive] == '/' || other[odrive] == '\\'));
        if (!replace) { /* Same drive: keep the base path, spelling the drive like the other. */
            r = sp_priv_path_from_raw(other, odrive, flavor);
            sp_priv_append(&r, base->buf + bdrive, base->len - bdrive, false);
            other += odrive;
            olen -= odrive;
        }
    }

    if (replace)
        r = sp_priv_path_from_raw(other, olen, flavor);
    else
        sp_priv_append(&r, other, olen, add_sep);
    sp_priv_normalize(&r, sp_priv_split_anchor(r.buf, r.len, flavor, NULL));
    return r;
}

SpPath sp_join_one(const SpPath *base, const char *other) {
    SP_ASSERT_PATH_INVARIANT(base);
    return other && other[0] ? sp_priv_join_len(base, other, strlen(other)) : *base;
}

SpPath sp_join_n(const SpPath *base, const char *s, size_t len) {
    SP_ASSERT_PATH_INVARIANT(base);
    return s && len > 0 ? sp_priv_join_len(base, s, len) : *base;
}

SpPath sp_joinpath(const SpPath *base, const SpPath *other) {
    SP_ASSERT_PATH_INVARIANT(base);
    SP_ASSERT_PATH_INVARIANT(other);
    if (other->len == 0)
        return *base;
    return base->len == 0 ? *other : sp_priv_join_len(base, other->buf, other->len);
}

SpPath sp_join_impl(const SpPath *base, const char **parts) {
    SP_ASSERT_PATH_INVARIANT(base);
    SpPath r = *base;
    for (; *parts; parts++)
        if ((*parts)[0])
            r = sp_priv_join_len(&r, *parts, strlen(*parts));
    return r;
}

SpPath sp_with_segments(const SpPath *p, const char **parts, size_t parts_count) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpPath r = sp_priv_error_path(p->flavor, SP_OK);
    for (size_t i = 0; i < parts_count; i++)
        if (parts[i][0])
            r = sp_priv_join_len(&r, parts[i], strlen(parts[i]));
    return r;
}

/* with_name(head + tail): the parent plus a name that must be non-empty, not "." and free of separators */
static SpPath sp_priv_with_name_parts(const SpPath *p, SpStr head, SpStr tail) {
    size_t anchor = sp_priv_split_anchor(p->buf, p->len, p->flavor, NULL);
    if (sp_priv_name_sv(p, anchor).len == 0)
        return sp_priv_error_path(p->flavor, SP_ERR_NO_NAME);

    char name[SP_PATH_MAX];
    size_t len = sp_priv_copy_trunc(name, SP_PATH_MAX, head.data, head.len);
    len += sp_priv_copy_trunc(name + len, SP_PATH_MAX - len, tail.data, tail.len);
    if (len == 0 || (len == 1 && name[0] == '.') || memchr(name, '/', len) ||
        (p->flavor == SP_FLAVOR_WINDOWS && memchr(name, '\\', len)))
        return sp_priv_error_path(p->flavor, SP_ERR_INVALID_ARG);

    SpPath r = sp_priv_path_from_raw(p->buf, sp_priv_parent_len(p->buf, p->len, p->flavor, anchor), p->flavor);
    if (r.len == 0 && sp_priv_drive_len(name, len, p->flavor) > 0)
        r.buf[r.len++] = '.'; /* keep "c:" from parsing as a drive */

    /* Past the anchor the name follows a separator; a bare drive takes it directly ("c:x" -> "c:y") */
    sp_priv_append(&r, name, len, r.len > anchor);
    return r;
}

SpPath sp_with_name(const SpPath *p, const char *name) {
    SP_ASSERT_PATH_INVARIANT(p);
    return sp_priv_with_name_parts(p, SP_PRIV_STR(name, strlen(name)), SP_PRIV_STR(SP_PRIV_NULL, 0));
}

SpPath sp_with_stem(const SpPath *p, const char *stem) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpStr suffix = sp_priv_suffix_sv(sp_priv_name_sv(p, sp_priv_split_anchor(p->buf, p->len, p->flavor, NULL)));

    /* A non-empty suffix needs a non-empty stem */
    if (suffix.len > 0 && stem[0] == '\0')
        return sp_priv_error_path(p->flavor, SP_ERR_INVALID_ARG);
    return sp_priv_with_name_parts(p, SP_PRIV_STR(stem, strlen(stem)), suffix);
}

SpPath sp_with_suffix(const SpPath *p, const char *suffix) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (suffix[0] != '\0' && suffix[0] != '.')
        return sp_priv_error_path(p->flavor, SP_ERR_INVALID_ARG);

    SpStr name = sp_priv_name_sv(p, sp_priv_split_anchor(p->buf, p->len, p->flavor, NULL));
    SpStr stem = SP_PRIV_STR(name.data, name.len - sp_priv_suffix_sv(name).len);
    return sp_priv_with_name_parts(p, stem, SP_PRIV_STR(suffix, strlen(suffix)));
}

bool sp_is_absolute(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (p->flavor != SP_FLAVOR_WINDOWS)
        return p->len > 0 && p->buf[0] == '/';

    /* ntpath.isabs: UNC and device paths, or ":\\" after the first character (whatever it is) */
    size_t after = 0;
    if (p->len > 0)
        sp_priv_utf8_next(p->buf, p->len, &after);
    return sp_priv_is_unc(p->buf, p->len, p->flavor) ||
           (p->len > after + 1 && p->buf[after] == ':' && p->buf[after + 1] == '\\');
}

SpPath sp_cwd(SpFlavor flavor) {
    char buf[SP_PATH_MAX];
    return sp_path_from_n(buf, sp_priv_getcwd(buf, SP_PATH_MAX) ? strlen(buf) : 0, flavor);
}

/* A relative path joined to the current directory, when that is known */
SpPath sp_absolute(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    char buf[SP_PATH_MAX];
    if (sp_is_absolute(p) || !sp_priv_getcwd(buf, SP_PATH_MAX))
        return *p;

    SpPath cwd = sp_path_from_n(buf, strlen(buf), p->flavor);
    return cwd.len == 0 ? *p : sp_priv_join_len(&cwd, p->buf, p->len);
}

/* The length of p's prefix that equals other (CPython: other == p or other in p.parents, comparing str.lower() on
 * Windows), or (size_t)-1 when there is none */
static size_t sp_priv_relative_len(const SpPath *p, const SpPath *other) {
    size_t anchor = sp_priv_split_anchor(p->buf, p->len, p->flavor, NULL);
    for (size_t len = p->len;;) {
        if (p->flavor == SP_FLAVOR_WINDOWS ? sp_priv_fold_cmp(p->buf, len, other->buf, other->len) == 0
                                           : len == other->len && memcmp(p->buf, other->buf, len) == 0)
            return len;

        size_t parent = sp_priv_parent_len(p->buf, len, p->flavor, anchor);
        if (parent == len)
            return SP_PRIV_CAST(size_t, -1);
        len = parent;
    }
}

bool sp_is_relative_to(const SpPath *p, const SpPath *other) {
    SP_ASSERT_PATH_INVARIANT(p);
    SP_ASSERT_PATH_INVARIANT(other);
    return sp_priv_relative_len(p, other) != SP_PRIV_CAST(size_t, -1);
}

/* CPython walks `other` and its parents up to the first that p is relative to, stepping out with ".." (walk_up only,
 * and never over a ".." part). The result is p's remaining parts. */
SpPath sp_relative_to(const SpPath *p, const SpPath *other, bool walk_up) {
    SpPath base = *other;
    SpPath r = sp_priv_error_path(p->flavor, SP_OK);
    size_t anchor = sp_priv_split_anchor(other->buf, other->len, other->flavor, NULL);
    size_t skip; /* the length of p's prefix that equals base */

    while ((skip = sp_priv_relative_len(p, &base)) == SP_PRIV_CAST(size_t, -1)) {
        SpStr name = sp_priv_name_sv(&base, anchor);
        size_t parent = sp_priv_parent_len(base.buf, base.len, base.flavor, anchor);
        if (!walk_up || parent == base.len || (name.len == 2 && memcmp(name.data, "..", 2) == 0))
            return sp_priv_error_path(p->flavor, SP_ERR_NOT_RELATIVE);

        sp_priv_append(&r, "..", 2, true);
        base.len = parent;
        base.buf[parent] = '\0';
    }

    SpPartsIter it = sp_parts_begin(p);
    SpStr part;
    if (skip > it.pos)
        it.pos = skip;
    while (sp_parts_next(&it, &part)) {
        if (r.len == 0 && sp_priv_drive_len(part.data, part.len, p->flavor) > 0)
            r.buf[r.len++] = '.';
        sp_priv_append(&r, part.data, part.len, true);
    }
    return r;
}

/* urllib.parse.quote: percent-encode all but letters, digits, "_.-~" and `safe`; false if buf is too small */
static bool sp_priv_quote(const char *s, size_t len, const char *safe, char *buf, size_t buf_size, size_t *pos) {
    for (size_t i = 0; i < len; i++) {
        unsigned char c = SP_PRIV_CAST(unsigned char, s[i]);
        bool plain = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                     (c != '\0' && (strchr("_.-~", c) || strchr(safe, c)));
        if (*pos + (plain ? 1u : 3u) >= buf_size)
            return false;

        if (plain)
            buf[(*pos)++] = SP_PRIV_CAST(char, c);
        else
            *pos += SP_PRIV_CAST(size_t, snprintf(buf + *pos, 4, "%%%02X", c));
    }

    buf[*pos] = '\0';
    return true;
}

/* urllib.request.pathname2url(str(p), add_scheme=True) */
size_t sp_as_uri(const SpPath *p, char *buf, size_t buf_size) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (buf_size > 0)
        buf[0] = '\0';
    if (buf_size == 0 || !sp_is_absolute(p))
        return 0;

    char path[SP_PATH_MAX];
    size_t len = sp_as_posix(p, path, SP_PATH_MAX);
    size_t drive;
    sp_priv_split_anchor(path, len, p->flavor, &drive);

    const char *d = path;
    const char *prefix = "file://"; /* an explicitly empty authority before a POSIX root */
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
        if (drive > 0 && sp_priv_drive_len(d, drive, p->flavor) == drive)
            prefix = "file:///";
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
    if (len == 0 || (len == 9 && memcmp(a, "localhost", 9) == 0))
        return true;

    char host[256];
#ifdef SP_WINDOWS
    DWORD size = sizeof(host);
    if (!GetComputerNameExA(ComputerNamePhysicalDnsHostname, host, &size))
        return false;
#else
    if (gethostname(host, sizeof(host)) != 0)
        return false;
    host[sizeof(host) - 1] = '\0';
#endif
    return strlen(host) == len && memcmp(host, a, len) == 0;
}

static int sp_priv_hex_digit(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

/* Append s[0..len) to buf, decoding %XX escapes (urllib.parse.unquote) */
static void sp_priv_unquote_append(char *buf, size_t *n, const char *s, size_t len) {
    for (size_t i = 0; i < len && *n + 1 < SP_PATH_MAX; i++) {
        int hi = i + 2 < len && s[i] == '%' ? sp_priv_hex_digit(s[i + 1]) : -1;
        int lo = hi >= 0 ? sp_priv_hex_digit(s[i + 2]) : -1;
        buf[(*n)++] = lo >= 0 ? SP_PRIV_CAST(char, hi * 16 + lo) : s[i];
        if (lo >= 0)
            i += 2;
    }
}

/* urllib.request.url2pathname(uri, require_scheme=True), which must give an absolute path */
SpPath sp_from_uri(const char *uri, SpFlavor flavor) {
    flavor = sp_priv_flavor(flavor);
    SpPath err = sp_priv_error_path(flavor, SP_ERR_INVALID_ARG);
    if (strlen(uri) < 5 || sp_priv_str_cmp_case(uri, 5, "file:", 5, true) != 0)
        return err;

    const char *path = uri + 5;
    const char *auth = path;
    size_t alen = 0;
    if (path[0] == '/' && path[1] == '/') {
        auth = path + 2;
        alen = strcspn(auth, "/?#");
        path = auth + alen;
    }
    size_t plen = strcspn(path, "?#"); /* query and fragment are discarded */

    bool local = sp_priv_is_local_authority(auth, alen);
    size_t pipe = 0; /* where a drive letter's pipe goes, when the URL has one */
    char buf[SP_PATH_MAX];
    size_t n = 0;
    if (flavor != SP_FLAVOR_WINDOWS) {
        if (!local)
            return err;
    } else if (sp_priv_drive_len(auth, alen, flavor) > 0) { /* file://c:/file.txt */
        sp_priv_unquote_append(buf, &n, auth, alen);
    } else if (!local) { /* file://server/share/file.txt */
        sp_priv_unquote_append(buf, &n, "//", 2);
        sp_priv_unquote_append(buf, &n, auth, alen);
    } else if (plen >= 3 && memcmp(path, "///", 3) == 0) { /* file://///server/share/file.txt */
        path++;
        plen--;
    } else {
        /* Characters, not bytes: the drive letter may be any one character */
        size_t after = 1;
        if (plen > 1)
            sp_priv_utf8_next(path, plen, &after);
        if (plen > after && path[0] == '/' && (path[after] == ':' || path[after] == '|')) { /* file:///c:/file.txt */
            path++;
            plen--;
        }
        pipe = 0;
        if (plen > 0)
            sp_priv_utf8_next(path, plen, &pipe);
        pipe = plen > pipe && path[pipe] == '|' ? pipe : 0; /* older URLs use a pipe after the drive letter */
    }

    sp_priv_unquote_append(buf, &n, path, plen);
    if (pipe > 0)
        buf[pipe] = ':';
    SpPath r = sp_path_from_n(buf, n, flavor);
    return sp_is_absolute(&r) ? r : err;
}

bool sp_path_eq(const SpPath *a, const SpPath *b) { return a->flavor == b->flavor && sp_path_cmp(a, b) == 0; }

/* Like CPython: compare the separator-split parts of str() ("." when empty), lowered like str.lower() on Windows */
int sp_path_cmp(const SpPath *a, const SpPath *b) {
    SP_ASSERT_PATH_INVARIANT(a);
    SP_ASSERT_PATH_INVARIANT(b);
    const char *sa = a->len ? a->buf : ".";
    const char *sb = b->len ? b->buf : ".";
    size_t la = a->len ? a->len : 1;
    size_t lb = b->len ? b->len : 1;
    char sep = a->flavor == SP_FLAVOR_WINDOWS ? '\\' : '/';

    for (size_t i = 0, j = 0;;) {
        size_t ea = i;
        size_t eb = j;
        while (ea < la && sa[ea] != sep)
            ea++;
        while (eb < lb && sb[eb] != sep)
            eb++;

        int c = a->flavor == SP_FLAVOR_WINDOWS ? sp_priv_fold_cmp(sa + i, ea - i, sb + j, eb - j)
                                               : sp_priv_str_cmp_case(sa + i, ea - i, sb + j, eb - j, false);
        if (c != 0)
            return c;
        if (ea == la || eb == lb)
            return ea < la ? 1 : eb < lb ? -1 : 0; /* the path with fewer parts sorts first */
        i = ea + 1;
        j = eb + 1;
    }
}

/* Equal paths hash alike: on Windows by code point lowered like str.lower() (U+0130 to i and a combining dot), with
 * both sigmas hashing alike since str.lower() picks between them by context */
unsigned long sp_path_hash(const SpPath *p) {
    unsigned long hash = 5381;
    const char *str = sp_str(p);
    size_t len = p->len > 0 ? p->len : 1;

    for (size_t i = 0; i < len;) {
        if (p->flavor != SP_FLAVOR_WINDOWS) {
            hash = hash * 33 + SP_PRIV_CAST(unsigned char, str[i++]);
            continue;
        }

        unsigned long c = sp_priv_utf8_next(str, len, &i);
        if (c == 0x130)
            hash = hash * 33 + 'i';
        c = c == 0x130 ? 0x307 : sp_priv_case_map(c, sp_priv_lower_runs, SP_ARRAY_LEN(sp_priv_lower_runs));
        hash = hash * 33 + (c == 0x3c2 ? 0x3c3 : c);
    }
    return hash;
}

/* re's IGNORECASE test of a character, by its lowercase, against a class item lo..hi (single when not a range): a
 * character of the item's BMP part lowers into the character's case group; past the BMP, a range must hold the
 * lowercase or its uppercase, and a single character must equal the lowercase */
static bool sp_priv_class_hit(unsigned long lower, unsigned long lo, unsigned long hi, bool single) {
    unsigned long top = hi > 0xFFFF ? 0xFFFF : hi;
    if (lo <= top && sp_priv_lowered_from(lower, lo, top))
        return true;

    for (size_t start = 0, i = 0; lo <= top && i < SP_ARRAY_LEN(sp_priv_case_groups); i++) {
        if (sp_priv_case_groups[i] != 0)
            continue;
        bool member = false;
        for (size_t j = start; j < i; j++)
            member = member || SP_PRIV_CAST(unsigned long, sp_priv_case_groups[j]) == lower;
        for (size_t j = start; member && j < i; j++)
            if (sp_priv_lowered_from(SP_PRIV_CAST(unsigned long, sp_priv_case_groups[j]), lo, top))
                return true;
        start = i + 1;
    }

    if (hi <= 0xFFFF)
        return false;
    if (single)
        return lower == lo;
    unsigned long upper = sp_priv_case_map(lower, sp_priv_upper_runs, SP_ARRAY_LEN(sp_priv_upper_runs));
    return (lo <= lower && lower <= hi) || (lo <= upper && upper <= hi);
}

/* One code-point matcher for names and paths. '*' and '?' stop at separators; a whole-part '*' also requires
 * a non-empty part. Recursive '**' consumes whole parts, while bracket expressions may match separators. */
static bool sp_priv_match_path(const char *pat, size_t plen, const char *s, size_t slen, bool ci, bool recursive,
                               SpFlavor flavor) {
    char sep = flavor == SP_FLAVOR_WINDOWS ? '\\' : '/';
    size_t pi = 0;
    size_t si = 0;
    while (pi < plen) {
        if (pat[pi] == '*') {
            size_t start = pi;
            while (pi < plen && pat[pi] == '*')
                pi++;
            bool whole = (start == 0 || pat[start - 1] == '/' || pat[start - 1] == sep) &&
                         (pi == plen || pat[pi] == '/' || pat[pi] == sep);
            bool walk = recursive && whole && pi - start == 2;
            while (walk && pi + 3 <= plen && pat[pi + 1] == '*' && pat[pi + 2] == '*' &&
                   (pi + 3 == plen || pat[pi + 3] == '/' || pat[pi + 3] == sep))
                pi += 3;
            if (walk && pi == plen)
                return true;

            size_t k = si;
            if (whole && pi - start == 1) {
                if (k == slen || s[k] == '/' || s[k] == sep)
                    return false;
                sp_priv_utf8_next(s, slen, &k);
            }
            if (walk)
                pi++; /* Skip the separator too: recursive stars may consume no parts. */

            for (;;) {
                if (sp_priv_match_path(pat + pi, plen - pi, s + k, slen - k, ci, recursive, flavor))
                    return true;
                if (walk) {
                    k = k == si ? k + 1 : k;
                    while (k < slen && s[k] != '/' && s[k] != sep)
                        k++;
                    if (k >= slen)
                        return false;
                    k++;
                    continue;
                }
                if (k == slen || s[k] == '/' || s[k] == sep)
                    return false;
                sp_priv_utf8_next(s, slen, &k);
            }
        }

        if (si == slen)
            return false;
        bool at_sep = s[si] == '/' || s[si] == sep;
        unsigned long c = sp_priv_utf8_next(s, slen, &si);

        if (pat[pi] == '[') {
            /* fnmatch bracket expression within the part: [seq], [!seq], ranges a-z; unterminated, the '[' is literal */
            size_t end = pi;
            while (end < plen && pat[end] != '/' && pat[end] != sep)
                end++;
            bool negate = pi + 1 < end && pat[pi + 1] == '!';
            size_t first = pi + 1 + (negate ? 1 : 0);
            size_t close = first < end && pat[first] == ']' ? first + 1 : first;
            while (close < end && pat[close] != ']')
                close++;

            if (close < end) {
                unsigned long lower = sp_priv_case_map(c, sp_priv_lower_runs, SP_ARRAY_LEN(sp_priv_lower_runs));
                bool hit = false;
                for (size_t k = first; k < close;) {
                    unsigned long lo = sp_priv_utf8_next(pat, close, &k);
                    unsigned long hi = lo;
                    bool single = !(k + 1 < close && pat[k] == '-');
                    if (!single) {
                        k++;
                        hi = sp_priv_utf8_next(pat, close, &k);
                    }
                    hit = hit || (ci ? sp_priv_class_hit(lower, lo, hi, single) : lo <= c && c <= hi);
                }
                if (hit == negate)
                    return false;
                pi = close + 1;
                continue;
            }
        }

        if (pat[pi] == '?') {
            if (at_sep)
                return false;
            pi++;
        } else if (pat[pi] == '/' || pat[pi] == sep) {
            if (!at_sep)
                return false;
            pi++;
        } else {
            /* re.IGNORECASE: the same simple lowercase, or lowercases in one of its case groups */
            unsigned long pc = sp_priv_utf8_next(pat, plen, &pi);
            if (ci) {
                pc = sp_priv_case_map(pc, sp_priv_lower_runs, SP_ARRAY_LEN(sp_priv_lower_runs));
                c = sp_priv_case_map(c, sp_priv_lower_runs, SP_ARRAY_LEN(sp_priv_lower_runs));
            }
            if (pc != c && !(ci && sp_priv_case_equiv(pc, c)))
                return false;
        }
    }
    return si == slen;
}

static bool sp_priv_case_insensitive(int case_sensitive, SpFlavor flavor) {
    return case_sensitive == -1 ? flavor == SP_FLAVOR_WINDOWS : case_sensitive == 0;
}

bool sp_full_match(const SpPath *p, const char *pattern, int case_sensitive) {
    SpPath pat = sp_path_from_n(pattern, pattern ? strlen(pattern) : 0, p->flavor);
    bool ci = sp_priv_case_insensitive(case_sensitive, p->flavor);
    return sp_priv_match_path(pat.buf, pat.len, p->buf, p->len, ci, true, p->flavor);
}

/* CPython: the path's last parts match the pattern's parts, which must be all of its parts for an anchored pattern */
int sp_match_ex(const SpPath *p, const char *pattern, int case_sensitive) {
    SpPath pat = sp_path_from_n(pattern, pattern ? strlen(pattern) : 0, p->flavor);
    SpPartsIter path = sp_parts_begin(p);
    SpPartsIter pattern_parts = sp_parts_begin(&pat);
    SpStr pp, sp;
    size_t count = 0;
    size_t total = 0;
    for (SpPartsIter it = pattern_parts; sp_parts_next(&it, &pp);)
        count++;
    for (SpPartsIter it = path; sp_parts_next(&it, &sp);)
        total++;
    if (count == 0)
        return SP_MATCH_ERR_EMPTY;
    if (total < count || (total > count && pattern_parts.anchor > 0))
        return SP_MATCH_NO;

    bool ci = sp_priv_case_insensitive(case_sensitive, p->flavor);
    for (size_t skip = total - count; sp_parts_next(&path, &sp);)
        if (skip > 0)
            skip--;
        else if (sp_parts_next(&pattern_parts, &pp) &&
                 !sp_priv_match_path(pp.data, pp.len, sp.data, sp.len, ci, false, p->flavor))
            return SP_MATCH_NO;
    return SP_MATCH_YES;
}

/* The path as a C string ("." when empty); false when it has an embedded NUL */
static bool sp_priv_path_cstr(const SpPath *p, const char **out) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (memchr(p->buf, '\0', p->len))
        return false;
    *out = p->len == 0 ? "." : p->buf;
    return true;
}

static SpStatResult sp_priv_stat_impl(const SpPath *p, bool follow_symlinks) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpStatResult result = SP_PRIV_ZERO;
    if (memchr(p->buf, '\0', p->len))
        return result;
    const char *path_str = p->len == 0 ? "." : p->buf;

#ifdef SP_WINDOWS
    DWORD flags = FILE_FLAG_BACKUP_SEMANTICS;
    if (!follow_symlinks)
        flags |= FILE_FLAG_OPEN_REPARSE_POINT;
    HANDLE hFile = CreateFileA(path_str, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                               flags, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return result;

    BY_HANDLE_FILE_INFORMATION info;
    if (!GetFileInformationByHandle(hFile, &info)) {
        CloseHandle(hFile);
        return result;
    }

    DWORD file_type = GetFileType(hFile);
    result.sp_mode = (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 040777 : 0100666;
    if (file_type == FILE_TYPE_CHAR) {
        result.sp_mode = (result.sp_mode & ~SP_PRIV_IFMT) | SP_PRIV_IFCHR;
    } else if (file_type != FILE_TYPE_DISK) {
        result.sp_mode &= ~SP_PRIV_IFMT;
    }
    if (info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
        result.sp_mode &= ~0222;
    }
    if (!follow_symlinks && (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
        result.sp_mode = (result.sp_mode & ~SP_PRIV_IFMT) | SP_PRIV_IFLNK;
    }

    typedef struct {
        ULONGLONG VolumeSerialNumber;
        BYTE FileId[16];
    } SpFileIdInfo;
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
    result.sp_size = (SP_PRIV_CAST(long long, info.nFileSizeHigh) << 32) | SP_PRIV_CAST(long long, info.nFileSizeLow);

    /* FILETIMEs count 100ns ticks since 1601 */
    FILETIME times[3] = {info.ftLastAccessTime, info.ftLastWriteTime, info.ftCreationTime};
    long long *ns[3] = {&result.sp_atime_ns, &result.sp_mtime_ns, &result.sp_ctime_ns};
    for (int i = 0; i < 3; i++) {
        unsigned long long ticks =
            (SP_PRIV_CAST(unsigned long long, times[i].dwHighDateTime) << 32) | times[i].dwLowDateTime;
        *ns[i] = (SP_PRIV_CAST(long long, ticks) - 116444736000000000LL) * 100;
    }

    CloseHandle(hFile);
#else
    struct stat st;
    if ((follow_symlinks ? stat(path_str, &st) : lstat(path_str, &st)) != 0)
        return result;
    result.sp_mode = SP_PRIV_CAST(unsigned int, st.st_mode);
    result.sp_ino = SP_PRIV_CAST(unsigned long long, st.st_ino);
    result.sp_dev = SP_PRIV_CAST(unsigned long long, st.st_dev);
    result.sp_nlink = SP_PRIV_CAST(unsigned long long, st.st_nlink);
    result.sp_uid = SP_PRIV_CAST(unsigned int, st.st_uid);
    result.sp_gid = SP_PRIV_CAST(unsigned int, st.st_gid);
    result.sp_size = SP_PRIV_CAST(long long, st.st_size);
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    result.sp_atime_ns = SP_PRIV_CAST(long long, st.st_atimespec.tv_sec) * 1000000000LL + st.st_atimespec.tv_nsec;
    result.sp_mtime_ns = SP_PRIV_CAST(long long, st.st_mtimespec.tv_sec) * 1000000000LL + st.st_mtimespec.tv_nsec;
    result.sp_ctime_ns = SP_PRIV_CAST(long long, st.st_ctimespec.tv_sec) * 1000000000LL + st.st_ctimespec.tv_nsec;
#else
    result.sp_atime_ns = SP_PRIV_CAST(long long, st.st_atime) * 1000000000LL;
    result.sp_mtime_ns = SP_PRIV_CAST(long long, st.st_mtime) * 1000000000LL;
    result.sp_ctime_ns = SP_PRIV_CAST(long long, st.st_ctime) * 1000000000LL;
#endif
#endif
    result.sp_atime = SP_PRIV_CAST(double, result.sp_atime_ns) / 1e9;
    result.sp_mtime = SP_PRIV_CAST(double, result.sp_mtime_ns) / 1e9;
    result.sp_ctime = SP_PRIV_CAST(double, result.sp_ctime_ns) / 1e9;
    result.valid = true;
    return result;
}

SpStatResult sp_stat(const SpPath *p) { return sp_priv_stat_impl(p, true); }

SpStatResult sp_lstat(const SpPath *p) { return sp_priv_stat_impl(p, false); }

bool sp_stat_eq(const SpStatResult *a, const SpStatResult *b) {
    return a->valid && b->valid && a->sp_mode == b->sp_mode && a->sp_ino == b->sp_ino && a->sp_dev == b->sp_dev &&
           a->sp_nlink == b->sp_nlink && a->sp_uid == b->sp_uid && a->sp_gid == b->sp_gid && a->sp_size == b->sp_size;
}

static bool sp_priv_has_type(const SpPath *p, unsigned int type_mask, bool follow_symlinks) {
    SpStatResult st = sp_priv_stat_impl(p, follow_symlinks);
    return st.valid && (st.sp_mode & SP_PRIV_IFMT) == type_mask;
}

bool sp_exists(const SpPath *p, bool follow_symlinks) { return sp_priv_stat_impl(p, follow_symlinks).valid; }

bool sp_is_dir(const SpPath *p, bool follow_symlinks) { return sp_priv_has_type(p, SP_PRIV_IFDIR, follow_symlinks); }
bool sp_is_file(const SpPath *p, bool follow_symlinks) { return sp_priv_has_type(p, SP_PRIV_IFREG, follow_symlinks); }
bool sp_is_symlink(const SpPath *p) { return sp_priv_has_type(p, SP_PRIV_IFLNK, false); }
bool sp_is_block_device(const SpPath *p) { return sp_priv_has_type(p, SP_PRIV_IFBLK, true); }
bool sp_is_char_device(const SpPath *p) { return sp_priv_has_type(p, SP_PRIV_IFCHR, true); }
bool sp_is_fifo(const SpPath *p) { return sp_priv_has_type(p, SP_PRIV_IFIFO, true); }
bool sp_is_socket(const SpPath *p) { return sp_priv_has_type(p, SP_PRIV_IFSOCK, true); }

bool sp_is_mount(const SpPath *p) {
#ifdef SP_WINDOWS
    const char *path_str;
    if (!sp_priv_path_cstr(p, &path_str))
        return false;

    char vol_path[SP_PATH_MAX];
    if (!GetVolumePathNameA(path_str, vol_path, SP_PATH_MAX))
        return false;

    size_t vlen = strlen(vol_path);
    size_t plen = p->len > 0 ? p->len : 1;
    if (vlen > 0 && vol_path[vlen - 1] == '\\')
        vlen--;
    if (plen > 0 && (path_str[plen - 1] == '\\' || path_str[plen - 1] == '/'))
        plen--;
    return sp_priv_fold_cmp(path_str, plen, vol_path, vlen) == 0;
#else
    SpStatResult st_path = sp_priv_stat_impl(p, false);
    if ((st_path.sp_mode & SP_PRIV_IFMT) != SP_PRIV_IFDIR)
        return false;

    SpPath parent = sp_parent(p);
    SpStatResult st_parent = sp_priv_stat_impl(&parent, false);
    return st_parent.valid && (st_path.sp_dev != st_parent.sp_dev || st_path.sp_ino == st_parent.sp_ino);
#endif
}

bool sp_is_junction(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
#ifdef SP_WINDOWS
    if (memchr(p->buf, '\0', p->len))
        return false;

    const char *path_str = p->len == 0 ? "." : p->buf;
    DWORD attrs = GetFileAttributesA(path_str);
    if (attrs == INVALID_FILE_ATTRIBUTES)
        return false;
    if ((attrs & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) !=
        (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))
        return false;

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(path_str, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    FindClose(h);
    return fd.dwReserved0 == 0xA0000003;
#else
    (void)p;
    return false;
#endif
}

/* os.readlink(): the unparsed target of the symlink (or junction) at path, into out; its length, 0 on failure */
static size_t sp_priv_readlink_impl(const char *path, char *out) {
#ifdef SP_WINDOWS
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                           OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return 0;

    char reparse_buf[16384];
    DWORD bytes_returned;
    if (!DeviceIoControl(h, 0x000900A8 /* FSCTL_GET_REPARSE_POINT */, NULL, 0, reparse_buf, sizeof(reparse_buf),
                         &bytes_returned, NULL)) {
        CloseHandle(h);
        return 0;
    }
    CloseHandle(h);

    DWORD tag = *(DWORD *)reparse_buf;
    size_t data_offset = (tag == 0xA000000C) ? 20 : (tag == 0xA0000003) ? 16 : 0;
    if (data_offset == 0)
        return 0;

    WORD print_offset = *(WORD *)(reparse_buf + 12);
    WORD print_len = *(WORD *)(reparse_buf + 14);
    WCHAR *print_name = (WCHAR *)(reparse_buf + data_offset + print_offset);
    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, print_name, print_len / 2, out, SP_PATH_MAX - 1, NULL, NULL);
    return utf8_len > 0 ? SP_PRIV_CAST(size_t, utf8_len) : 0;
#else
    ssize_t len = readlink(path, out, SP_PATH_MAX - 1);
    return len > 0 ? SP_PRIV_CAST(size_t, len) : 0;
#endif
}

SpPath sp_readlink(const SpPath *p) {
    SpPath r = sp_priv_error_path(p->flavor, SP_OK);
    const char *path_str;
    if (sp_priv_path_cstr(p, &path_str))
        r.len = sp_priv_readlink_impl(path_str, r.buf);
    if (r.len == 0)
        return sp_priv_error_path(p->flavor, SP_ERR);

    r.buf[r.len] = '\0';
    sp_priv_normalize(&r, sp_priv_split_anchor(r.buf, r.len, r.flavor, NULL));
    return r;
}

/* os.path.realpath of the absolute path: GetFullPathName on Windows; on POSIX, realpath() of the longest prefix it
 * resolves (the whole path when strict), followed by the rest */
SpPath sp_resolve(const SpPath *p, bool strict) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpPath abs = *p;
    char buf[SP_PATH_MAX];
    if (!sp_is_absolute(p) && sp_priv_getcwd(buf, SP_PATH_MAX)) {
        SpPath cwd = sp_path_from_n(buf, strlen(buf), p->flavor);
        if (cwd.len > 0)
            abs = sp_priv_join_len(&cwd, p->buf, p->len);
    }

    const char *path_str;
    if (!sp_priv_path_cstr(p, &path_str))
        return strict ? sp_priv_error_path(p->flavor, SP_ERR) : abs;

#ifdef SP_WINDOWS
    DWORD len = GetFullPathNameA(path_str, SP_PATH_MAX, buf, NULL);
    if (len == 0 || len >= SP_PATH_MAX)
        return strict ? sp_priv_error_path(p->flavor, SP_ERR) : abs;
    if (strict && GetFileAttributesA(buf) == INVALID_FILE_ATTRIBUTES)
        return sp_priv_error_path(p->flavor, SP_ERR);
    return sp_path_from_n(buf, len, p->flavor);
#else
    size_t anchor = sp_priv_split_anchor(abs.buf, abs.len, abs.flavor, NULL);
    size_t n = abs.len;
    do {
        SpPath prefix = sp_priv_path_from_raw(abs.buf, n, abs.flavor);
        if (realpath(prefix.len == 0 ? "." : prefix.buf, buf)) {
            SpPath r = sp_path_from_n(buf, strlen(buf), p->flavor);
            while (n < abs.len && abs.buf[n] == '/')
                n++;
            return n < abs.len ? sp_priv_join_len(&r, abs.buf + n, abs.len - n) : r;
        }
        if (strict)
            return sp_priv_error_path(p->flavor, SP_ERR);

        size_t parent = sp_priv_parent_len(abs.buf, n, abs.flavor, anchor);
        if (parent == n)
            break;
        n = parent;
    } while (n > 0);
    return abs;
#endif
}

/* os.symlink() or os.link(): a link at link_path to target_path */
static bool sp_priv_link_to_impl(const char *link_path, const char *target_path, bool symbolic,
                                 bool target_is_directory) {
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
    const char *link_path, *target_path;
    return sp_priv_path_cstr(p, &link_path) && sp_priv_path_cstr(target, &target_path) &&
           sp_priv_link_to_impl(link_path, target_path, true, target_is_directory);
}

bool sp_hardlink_to(const SpPath *p, const SpPath *target) {
    const char *link_path, *target_path;
    return sp_priv_path_cstr(p, &link_path) && sp_priv_path_cstr(target, &target_path) &&
           sp_priv_link_to_impl(link_path, target_path, false, false);
}

bool sp_samefile(const SpPath *a, const SpPath *b) {
    SpStatResult stat_a = sp_priv_stat_impl(a, true);
    SpStatResult stat_b = sp_priv_stat_impl(b, true);
    return stat_a.valid && stat_b.valid && stat_a.sp_dev == stat_b.sp_dev && stat_a.sp_ino == stat_b.sp_ino;
}

/* The last failed OS call's error as an SP_ERR_* code */
static int sp_priv_last_error(void) {
#ifdef SP_WINDOWS
    DWORD e = GetLastError();
    if (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND)
        return SP_ERR_NOT_FOUND;
    if (e == ERROR_ALREADY_EXISTS || e == ERROR_FILE_EXISTS)
        return SP_ERR_EXISTS;
    if (e == ERROR_ACCESS_DENIED)
        return SP_ERR_PERMISSION;
    if (e == ERROR_DIRECTORY)
        return SP_ERR_NOT_DIR;
#else
    if (errno == ENOENT)
        return SP_ERR_NOT_FOUND;
    if (errno == EEXIST)
        return SP_ERR_EXISTS;
    if (errno == EACCES || errno == EPERM)
        return SP_ERR_PERMISSION;
    if (errno == ENOTDIR)
        return SP_ERR_NOT_DIR;
    if (errno == EINVAL)
        return SP_ERR_INVALID_ARG;
#endif
    return SP_ERR;
}

static bool sp_priv_mkdir_impl(const char *path, unsigned int mode) {
#ifdef SP_WINDOWS
    (void)mode;
    return CreateDirectoryA(path, NULL) != 0;
#else
    return mkdir(path, SP_PRIV_CAST(mode_t, mode)) == 0;
#endif
}

/* CPython's Path.mkdir: missing parents are created (with parent_mode) only after a "not found" failure, and any
 * failure is fine with exist_ok when the path is a directory (Windows reports some of those as access denied) */
int sp_mkdir(const SpPath *p, unsigned int mode, bool parents, bool exist_ok, unsigned int parent_mode) {
    const char *path_str;
    if (mode == 0)
        mode = SP_MKDIR_DEF_MODE;
    if (!sp_priv_path_cstr(p, &path_str))
        return SP_ERR;
    if (sp_priv_mkdir_impl(path_str, mode))
        return SP_OK;

    int err = sp_priv_last_error();
    SpPath parent = sp_parent(p);
    if (err == SP_ERR_NOT_FOUND && parents && parent.len != p->len) {
        err = sp_mkdir(&parent, parent_mode, true, true, parent_mode);
        return err == SP_OK ? sp_mkdir(p, mode, false, exist_ok, parent_mode) : err;
    }

    bool is_dir = sp_priv_has_type(p, SP_PRIV_IFDIR, true);
    if (exist_ok && is_dir)
        return SP_OK;
    return err == SP_ERR_EXISTS && !is_dir ? SP_ERR_EXISTS_NOT_DIR : err;
}

/* CPython's Path.touch: with exist_ok, bump an existing file's times; otherwise create the file */
bool sp_touch(const SpPath *p, unsigned int mode, bool exist_ok) {
    const char *path_str;
    if (!sp_priv_path_cstr(p, &path_str))
        return false;
    if (mode == 0)
        mode = 0666;

#ifdef SP_WINDOWS
    bool exists = sp_priv_stat_impl(p, true).valid;
    if (exists && !exist_ok)
        return false;

    DWORD access = exists ? FILE_WRITE_ATTRIBUTES : GENERIC_WRITE;
    DWORD disposition = exists ? OPEN_EXISTING : CREATE_NEW;
    DWORD share = exists ? (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE) : 0;
    HANDLE h = CreateFileA(path_str, access, share, NULL, disposition, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return false;

    bool ok = true;
    if (exists) {
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        ok = SetFileTime(h, NULL, &ft, &ft) != 0;
    }
    CloseHandle(h);
    return ok;
#else
    if (exist_ok && utime(path_str, SP_PRIV_NULL) == 0)
        return true;

    int fd = open(path_str, O_CREAT | O_WRONLY | (exist_ok ? 0 : O_EXCL), SP_PRIV_CAST(mode_t, mode));
    return fd >= 0 && close(fd) == 0;
#endif
}

/* os.unlink() or os.rmdir(); a missing file is fine with missing_ok */
static bool sp_priv_remove_impl(const char *path, bool is_dir, bool missing_ok) {
#ifdef SP_WINDOWS
    if (is_dir)
        return RemoveDirectoryA(path) != 0;
    if (DeleteFileA(path))
        return true;

    DWORD err = GetLastError();
    return missing_ok && (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND);
#else
    return (is_dir ? rmdir(path) : unlink(path)) == 0 || (!is_dir && missing_ok && errno == ENOENT);
#endif
}

bool sp_unlink(const SpPath *p, bool missing_ok) {
    const char *path_str;
    return sp_priv_path_cstr(p, &path_str) && sp_priv_remove_impl(path_str, false, missing_ok);
}

bool sp_rmdir(const SpPath *p) {
    const char *path_str;
    return sp_priv_path_cstr(p, &path_str) && sp_priv_remove_impl(path_str, true, false);
}

static SpIOResult sp_priv_io_result(size_t bytes, int error) {
    SpIOResult r = SP_PRIV_ZERO;
    r.bytes = bytes;
    r.error = error;
    return r;
}

/* os.chmod(): on Windows, the owner's write bit clears or sets the read-only attribute */
bool sp_chmod(const SpPath *p, unsigned int mode) {
    const char *path_str;
    return sp_priv_path_cstr(p, &path_str) && sp_priv_chmod(path_str, mode) == 0;
}

SpIOResult sp_read_file(const SpPath *p, char *buf, size_t buf_size) {
    const char *path_str;
    if (!sp_priv_path_cstr(p, &path_str))
        return sp_priv_io_result(0, SP_ERR_OPEN);

    SpStatResult st = sp_stat(p);
    if (!st.valid || st.sp_size < 0)
        return sp_priv_io_result(0, SP_ERR_OPEN);
    size_t sz = SP_PRIV_CAST(size_t, st.sp_size);
    if (sz > buf_size)
        return sp_priv_io_result(sz, SP_ERR_TOO_LARGE);

    FILE *f = fopen(path_str, "rb");
    if (!f)
        return sp_priv_io_result(0, SP_ERR_OPEN);
    size_t got = fread(buf, 1, sz, f);
    bool closed = fclose(f) == 0;
    return sp_priv_io_result(got, got == sz && closed ? SP_OK : SP_ERR_READ);
}

SpIOResult sp_write_file(const SpPath *p, const char *data, size_t data_len) {
    const char *path_str;
    FILE *f = sp_priv_path_cstr(p, &path_str) ? fopen(path_str, "wb") : SP_PRIV_NULL;
    if (!f)
        return sp_priv_io_result(0, SP_ERR_OPEN);
    size_t wrote = fwrite(data, 1, data_len, f);
    bool closed = fclose(f) == 0;
    return sp_priv_io_result(wrote, wrote == data_len && closed ? SP_OK : SP_ERR_WRITE);
}

const char *sp_error_str(int error) {
    static const char *const messages[] = {
        "Success",
        "Operation failed",
        "File exists",
        "No such file or directory",
        "Not a directory",
        "Permission denied",
        "Path exists but is not a directory",
        "Could not open file",
        "Read failed",
        "Write failed",
        "File too large for buffer",
        "Path is not relative to the other path",
        "Path has an empty name",
        "Invalid argument",
        "Unsupported operation",
    };
    return (error >= 0 && error < SP_PRIV_CAST(int, SP_ARRAY_LEN(messages))) ? messages[error] : "Unknown error";
}

static void sp_priv_readdir_close(void **handle) {
    if (!*handle)
        return;
#ifdef SP_WINDOWS
    FindClose(*handle);
#else
    closedir(SP_PRIV_CAST(DIR *, *handle));
#endif
    *handle = SP_PRIV_NULL;
}

/* dir / name for a directory entry or literal glob part (a single part), like join: a separator unless dir is
 * empty or ends in one or, on Windows, in a drive's ':' (which no directory name can end in). Too long, dir stays. */
static void sp_priv_join_child(SpPath *dir, const char *name, size_t len) {
    char sep = dir->flavor == SP_FLAVOR_WINDOWS ? '\\' : '/';
    char last = dir->len > 0 ? dir->buf[dir->len - 1] : '/';
    size_t at = dir->len + (last != '/' && last != sep && !(sep == '\\' && last == ':') ? 1 : 0);
    if (at + len >= SP_PATH_MAX)
        return;

    dir->buf[dir->len] = sep;
    memcpy(dir->buf + at, name, len);
    dir->len = at + len;
    dir->buf[dir->len] = '\0';
}

/* The next entry of dir other than "." and "..", opening the listing on the first call: out gets dir / name (as
 * sp_priv_join_child builds it; entries too long for that are skipped) and the name length is returned, 0 at the end */
static size_t sp_priv_readdir_next(void **handle, const SpPath *dir, SpPath *out) {
    char sep = dir->flavor == SP_FLAVOR_WINDOWS ? '\\' : '/';
    char last = dir->len > 0 ? dir->buf[dir->len - 1] : '/';
    size_t at = dir->len + (last != '/' && last != sep && !(sep == '\\' && last == ':') ? 1 : 0);

    for (;;) {
        const char *name;
#ifdef SP_WINDOWS
        WIN32_FIND_DATAA fd;
        if (!*handle) {
            char search[SP_PATH_MAX];
            if (at + 1 >= SP_PATH_MAX)
                return 0;
            memcpy(search, dir->buf, dir->len);
            search[dir->len] = '\\';
            search[at] = '*';
            search[at + 1] = '\0';

            *handle = FindFirstFileA(search, &fd);
            if (*handle == INVALID_HANDLE_VALUE) {
                *handle = SP_PRIV_NULL;
                return 0;
            }
        } else if (!FindNextFileA(*handle, &fd)) {
            return 0;
        }
        name = fd.cFileName;
#else
        if (!*handle && !(*handle = opendir(dir->len == 0 ? "." : dir->buf)))
            return 0;
        struct dirent *de = readdir(SP_PRIV_CAST(DIR *, *handle));
        if (!de)
            return 0;
        name = de->d_name;
#endif

        size_t n = strlen(name);
        if ((name[0] == '.' && (n == 1 || (n == 2 && name[1] == '.'))) || at + n >= SP_PATH_MAX)
            continue;
        *out = *dir;
        out->buf[dir->len] = sep;
        memcpy(out->buf + at, name, n + 1);
        out->len = at + n;
        return n;
    }
}

/* CPython's _copy_info for local paths: access and modification times, then permissions */
static bool sp_priv_copy_metadata(const char *src, const char *dst, bool follow_symlinks) {
#ifdef SP_WINDOWS
    DWORD flags = FILE_FLAG_BACKUP_SEMANTICS | (follow_symlinks ? 0 : FILE_FLAG_OPEN_REPARSE_POINT);
    DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    HANDLE hs = CreateFileA(src, FILE_READ_ATTRIBUTES, share, NULL, OPEN_EXISTING, flags, NULL);
    HANDLE hd = CreateFileA(dst, FILE_WRITE_ATTRIBUTES, share, NULL, OPEN_EXISTING, flags, NULL);
    FILETIME atime, mtime;
    bool ok = hs != INVALID_HANDLE_VALUE && hd != INVALID_HANDLE_VALUE && GetFileTime(hs, NULL, &atime, &mtime) &&
              SetFileTime(hd, NULL, &atime, &mtime);
    if (hs != INVALID_HANDLE_VALUE)
        CloseHandle(hs);
    if (hd != INVALID_HANDLE_VALUE)
        CloseHandle(hd);

    /* chmod() on Windows only sets the read-only attribute, here the source's */
    DWORD src_attrs = GetFileAttributesA(src);
    DWORD dst_attrs = GetFileAttributesA(dst);
    DWORD attrs = (dst_attrs & ~SP_PRIV_CAST(DWORD, FILE_ATTRIBUTE_READONLY)) | (src_attrs & FILE_ATTRIBUTE_READONLY);
    return ok && (!follow_symlinks ||
                  (dst_attrs != INVALID_FILE_ATTRIBUTES && (attrs == dst_attrs || SetFileAttributesA(dst, attrs))));
#else
    struct stat st;
    if ((follow_symlinks ? stat(src, &st) : lstat(src, &st)) != 0)
        return false;
    if (S_ISLNK(st.st_mode))
        return true; /* utime() and chmod() would follow the link */

    struct utimbuf times;
    times.actime = st.st_atime;
    times.modtime = st.st_mtime;
    return utime(dst, &times) == 0 && chmod(dst, st.st_mode & 07777) == 0;
#endif
}

/* The bytes of in, written to out: SP_OK, SP_ERR_READ or SP_ERR_WRITE */
static int sp_priv_copy_stream(FILE *in, FILE *out) {
    char buf[8192];
    for (size_t n; (n = fread(buf, 1, sizeof(buf), in)) > 0;)
        if (fwrite(buf, 1, n, out) != n)
            return SP_ERR_WRITE;
    return ferror(in) ? SP_ERR_READ : SP_OK;
}

/* CPython's Path._copy_from: recursively copy src to dst (extended in place for children, then restored) */
static int sp_priv_copy_tree(const SpPath *src, SpPath *dst, bool follow_symlinks, bool preserve_metadata) {
    const char *from, *to;
    if (!sp_priv_path_cstr(src, &from) || !sp_priv_path_cstr(dst, &to))
        return SP_ERR_INVALID_ARG;

    SpStatResult st = sp_priv_stat_impl(src, follow_symlinks);
    int err;
    if (!follow_symlinks && (st.sp_mode & SP_PRIV_IFMT) == SP_PRIV_IFLNK) {
        char link[SP_PATH_MAX];
        size_t n = sp_priv_readlink_impl(from, link);
        link[n] = '\0';
        bool target_is_dir = n > 0 && (sp_priv_stat_impl(src, true).sp_mode & SP_PRIV_IFMT) == SP_PRIV_IFDIR;
        err = n > 0 && sp_priv_link_to_impl(to, link, true, target_is_dir) ? SP_OK : sp_priv_last_error();
    } else if ((st.sp_mode & SP_PRIV_IFMT) == SP_PRIV_IFDIR) {
        /* Children are listed before dst is created, so an unreadable src leaves no dst behind */
        void *handle = SP_PRIV_NULL;
        SpPath child;
        size_t n = sp_priv_readdir_next(&handle, src, &child);
        if (!handle)
            return sp_priv_last_error();

        err = sp_priv_mkdir_impl(to, SP_MKDIR_DEF_MODE) ? SP_OK : sp_priv_last_error();
        if (err == SP_ERR_EXISTS && (sp_priv_stat_impl(dst, true).sp_mode & SP_PRIV_IFMT) != SP_PRIV_IFDIR)
            err = SP_ERR_EXISTS_NOT_DIR;
        for (size_t len = dst->len; err == SP_OK && n > 0; n = sp_priv_readdir_next(&handle, src, &child)) {
            sp_priv_join_child(dst, child.buf + child.len - n, n);
            err = sp_priv_copy_tree(&child, dst, follow_symlinks, preserve_metadata);
            dst->len = len;
            dst->buf[len] = '\0';
        }
        sp_priv_readdir_close(&handle);
    } else {
        SpStatResult to_st = sp_priv_stat_impl(dst, true);
        if (st.valid && to_st.valid && st.sp_dev == to_st.sp_dev && st.sp_ino == to_st.sp_ino)
            return SP_ERR_INVALID_ARG;

        FILE *in = fopen(from, "rb");
        FILE *out = in ? fopen(to, "wb") : SP_PRIV_NULL;
        err = out ? sp_priv_copy_stream(in, out) : sp_priv_last_error();
        if (in)
            fclose(in);
        if (out && fclose(out) != 0 && err == SP_OK)
            err = SP_ERR_WRITE;
    }

    if (err == SP_OK && preserve_metadata && !sp_priv_copy_metadata(from, to, follow_symlinks))
        err = sp_priv_last_error();
    return err;
}

SpPath sp_copy(const SpPath *p, const SpPath *target, bool follow_symlinks, bool preserve_metadata) {
    SpPath dst = *target;
    int err = sp_priv_relative_len(target, p) != SP_PRIV_CAST(size_t, -1)
                  ? SP_ERR_INVALID_ARG
                  : sp_priv_copy_tree(p, &dst, follow_symlinks, preserve_metadata);
    return err == SP_OK ? *target : sp_priv_error_path(target->flavor, err);
}

/* CPython's Path._delete: symlinks and junctions are unlinked, directories removed recursively */
static int sp_priv_delete(const SpPath *p) {
    const char *path_str;
    if (!sp_priv_path_cstr(p, &path_str))
        return SP_ERR_INVALID_ARG;

    bool dir = (sp_priv_stat_impl(p, true).sp_mode & SP_PRIV_IFMT) == SP_PRIV_IFDIR;
    if (!dir || (sp_priv_stat_impl(p, false).sp_mode & SP_PRIV_IFMT) == SP_PRIV_IFLNK || sp_is_junction(p))
        return sp_priv_remove_impl(path_str, false, false) ? SP_OK : sp_priv_last_error();

    void *handle = SP_PRIV_NULL;
    SpPath child;
    int err = SP_OK;
    while (err == SP_OK && sp_priv_readdir_next(&handle, p, &child) > 0)
        err = sp_priv_delete(&child);
    sp_priv_readdir_close(&handle);

    if (err != SP_OK)
        return err;
    return sp_priv_remove_impl(path_str, true, false) ? SP_OK : sp_priv_last_error();
}

/* sp_priv_rename's error when only copying and deleting can move across filesystems */
enum { SP_PRIV_ERR_CROSS_DEVICE = 0x7F };

/* os.rename / os.replace, or with `move` Path.move's first step: the same file is an error */
static SpPath sp_priv_rename(const SpPath *p, const SpPath *target, bool allow_replace, bool move) {
    SpStatResult from = sp_priv_stat_impl(p, true);
    SpStatResult to = sp_priv_stat_impl(target, true);
    bool same = from.valid && to.valid && from.sp_dev == to.sp_dev && from.sp_ino == to.sp_ino;
    const char *src_str, *dst_str;
    if (!sp_priv_path_cstr(p, &src_str) || !sp_priv_path_cstr(target, &dst_str) || (move && same))
        return sp_priv_error_path(target->flavor, SP_ERR_INVALID_ARG);

#ifdef SP_WINDOWS
    if (MoveFileExA(src_str, dst_str, allow_replace ? MOVEFILE_REPLACE_EXISTING : 0))
        return *target;
    bool cross_device = GetLastError() == ERROR_NOT_SAME_DEVICE;
#else
    if (!allow_replace && to.valid)
        return same ? *target : sp_priv_error_path(target->flavor, SP_ERR_EXISTS);
    if (rename(src_str, dst_str) == 0)
        return *target;
    bool cross_device = errno == EXDEV;
#endif
    return sp_priv_error_path(target->flavor, cross_device && move ? SP_PRIV_ERR_CROSS_DEVICE : sp_priv_last_error());
}

SpPath sp_rename(const SpPath *p, const SpPath *target) { return sp_priv_rename(p, target, false, false); }
SpPath sp_replace(const SpPath *p, const SpPath *target) { return sp_priv_rename(p, target, true, false); }

/* Path.move: rename, or across filesystems copy (keeping symlinks and metadata) and delete */
SpPath sp_move(const SpPath *p, const SpPath *target) {
    SpPath r = sp_priv_rename(p, target, true, true);
    if (sp_path_error_code(&r) != SP_PRIV_ERR_CROSS_DEVICE)
        return r;

    SpPath dst = *target;
    int err = sp_priv_relative_len(target, p) != SP_PRIV_CAST(size_t, -1) ? SP_ERR_INVALID_ARG
                                                                          : sp_priv_copy_tree(p, &dst, false, true);
    if (err == SP_OK)
        err = sp_priv_delete(p);
    return err == SP_OK ? *target : sp_priv_error_path(target->flavor, err);
}

/* The *_into operations: target_dir / p.name, then the operation itself */
SpPath sp_copy_into(const SpPath *p, const SpPath *target_dir, bool follow_symlinks, bool preserve_metadata) {
    SpStr name = sp_priv_name_sv(p, sp_priv_split_anchor(p->buf, p->len, p->flavor, NULL));
    if (name.len == 0)
        return sp_priv_error_path(target_dir->flavor, SP_ERR_NO_NAME);

    /* sp_copy's body: calling it would take a fifth frame (the target check folds case on Windows) */
    SpPath target = sp_priv_join_len(target_dir, name.data, name.len);
    SpPath dst = target;
    int err = sp_priv_relative_len(&target, p) != SP_PRIV_CAST(size_t, -1)
                  ? SP_ERR_INVALID_ARG
                  : sp_priv_copy_tree(p, &dst, follow_symlinks, preserve_metadata);
    return err == SP_OK ? target : sp_priv_error_path(target.flavor, err);
}

SpPath sp_move_into(const SpPath *p, const SpPath *target_dir) {
    SpStr name = sp_priv_name_sv(p, sp_priv_split_anchor(p->buf, p->len, p->flavor, NULL));
    if (name.len == 0)
        return sp_priv_error_path(target_dir->flavor, SP_ERR_NO_NAME);

    /* sp_move's body, for the same reason */
    SpPath target = sp_priv_join_len(target_dir, name.data, name.len);
    SpPath r = sp_priv_rename(p, &target, true, true);
    if (sp_path_error_code(&r) != SP_PRIV_ERR_CROSS_DEVICE)
        return r;

    SpPath dst = target;
    int err = sp_priv_relative_len(&target, p) != SP_PRIV_CAST(size_t, -1) ? SP_ERR_INVALID_ARG
                                                                           : sp_priv_copy_tree(p, &dst, false, true);
    if (err == SP_OK)
        err = sp_priv_delete(p);
    return err == SP_OK ? target : sp_priv_error_path(target.flavor, err);
}

static SpStr sp_priv_glob_part(const SpGlobIter *it, size_t pos) {
    const char *buf = it->priv_.pattern_buf;
    char sep = it->priv_.path.flavor == SP_FLAVOR_WINDOWS ? '\\' : '/';
    size_t end = pos;
    while (end < it->priv_.pattern_len && buf[end] != sep)
        end++;
    return SP_PRIV_STR(buf + pos, end - pos);
}

static bool sp_priv_part_is(SpStr part, const char *s) {
    return part.len == strlen(s) && memcmp(part.data, s, part.len) == 0;
}

/* The empty part a trailing separator adds, or ".." */
static bool sp_priv_glob_special(SpStr part) {
    return part.len == 0 || (part.len == 2 && part.data[0] == '.' && part.data[1] == '.');
}

static void sp_priv_glob_push(SpGlobIter *it, size_t from, size_t to, size_t root_len) {
    if (it->depth + 1 >= SP_GLOB_MAX_DEPTH)
        return;
    it->depth++;
    it->priv_.stack[it->depth].handle = SP_PRIV_NULL;
    it->priv_.stack[it->depth].path_len = it->priv_.path.len;
    it->priv_.stack[it->depth].from = from;
    it->priv_.stack[it->depth].to = to;
    it->priv_.stack[it->depth].root_len = root_len;
}

/* Literal parts extend the path without a directory listing. Wildcards and recursive groups share one frame;
 * only recursive groups (of "**" alone) can also match the current path, before any children are listed. */
static bool sp_priv_glob_select(SpGlobIter *it, size_t seg, bool exists, bool trailing) {
    SpPath *path = &it->priv_.path;
    while (seg <= it->priv_.pattern_len) {
        SpStr part = sp_priv_glob_part(it, seg);
        size_t end = seg + part.len;
        bool special = sp_priv_glob_special(part);
        bool walk = sp_priv_part_is(part, "**");
        bool only_walks = walk;
        bool wildcard =
            memchr(part.data, '*', part.len) || memchr(part.data, '?', part.len) || memchr(part.data, '[', part.len);

        if (walk || (!special && (it->priv_.case_pedantic || wildcard))) {
            /* Group recursive selectors to avoid duplicate visits when following symlinks. */
            while (walk && end < it->priv_.pattern_len) {
                SpStr next = sp_priv_glob_part(it, end + 1);
                if (sp_priv_glob_special(next) || (!it->priv_.recurse_symlinks && !sp_priv_part_is(next, "**")))
                    break;
                only_walks = only_walks && sp_priv_part_is(next, "**");
                end += 1 + next.len;
            }
            sp_priv_glob_push(it, seg, end, path->len);
            if (!only_walks)
                return false;
        } else {
            if (part.len > 0)
                sp_priv_join_child(path, part.data, part.len);
            exists = exists && special;
            trailing = part.len == 0 || end < it->priv_.pattern_len;
        }
        seg = end + 1;
    }

    if (exists)
        return true;
    SpStatResult st = sp_priv_stat_impl(path, trailing);
    return trailing ? (st.sp_mode & SP_PRIV_IFMT) == SP_PRIV_IFDIR : st.valid;
}

/* An iterator for prefix + pattern with its first match selected. The pattern is compacted once (without empty and
 * '.' parts) but keeps separators, so recursive groups are contiguous pattern slices; a trailing separator adds a final
 * empty part. */
static SpGlobIter sp_priv_glob_init(const SpPath *base, const char *prefix, const char *pattern, SpCaseSensitivity cs,
                                    bool recurse_symlinks) {
    SpGlobIter it = SP_PRIV_ZERO;
    it.depth = -1;
    SP_ASSERT_PATH_INVARIANT(base);
    SpFlavor flavor = base->flavor;
    size_t plen = strlen(prefix);
    size_t len = strlen(pattern);
    if (sp_priv_split_anchor(pattern, len, flavor, NULL) > 0) {
        it.error = SP_ERR_UNSUPPORTED;
        return it;
    }

    char *buf = it.priv_.pattern_buf;
    char sep = flavor == SP_FLAVOR_WINDOWS ? '\\' : '/';
    char last = len > 0 ? pattern[len - 1] : plen > 0 ? prefix[plen - 1] : '\0';
    size_t n = sp_priv_copy_trunc(buf, SP_GLOB_PATTERN_MAX, prefix, plen);
    len = n + sp_priv_copy_trunc(buf + n, SP_GLOB_PATTERN_MAX - n, pattern, len);

    n = 0;
    for (size_t pos = 0, end; pos < len; pos = end + 1) {
        while (pos < len && (buf[pos] == '/' || buf[pos] == sep))
            pos++;
        end = pos;
        while (end < len && buf[end] != '/' && buf[end] != sep)
            end++;
        if (pos == end || (end - pos == 1 && buf[pos] == '.'))
            continue;

        if (n > 0)
            buf[n++] = sep;
        memmove(buf + n, buf + pos, end - pos);
        n += end - pos;
    }
    if (n == 0) {
        it.error = SP_ERR_INVALID_ARG;
        return it;
    }
    if (last == '/' || last == sep)
        buf[n++] = sep;

    it.priv_.pattern_len = n;
    it.priv_.case_insensitive =
        cs == SP_CASE_INSENSITIVE || (cs == SP_CASE_PLATFORM_DEFAULT && flavor == SP_FLAVOR_WINDOWS);
    it.priv_.case_pedantic = cs != SP_CASE_PLATFORM_DEFAULT;
    it.priv_.recurse_symlinks = recurse_symlinks;
    it.priv_.path = *base;
    it.priv_.pending = sp_priv_glob_select(&it, 0, false, true);
    return it;
}

SpGlobIter sp_glob_begin(const SpPath *base, const char *pattern, SpCaseSensitivity cs, bool recurse_symlinks) {
    return sp_priv_glob_init(base, "", pattern, cs, recurse_symlinks);
}

/* glob(join('**', pattern)); an anchored pattern replaces the '**' in the join, which glob rejects */
SpGlobIter sp_rglob_begin(const SpPath *base, const char *pattern, SpCaseSensitivity cs, bool recurse_symlinks) {
    return sp_priv_glob_init(base, "**/", pattern, cs, recurse_symlinks);
}

bool sp_glob_next(SpGlobIter *it, SpPath *out) {
    SpPath *path = &it->priv_.path;
    while (!it->priv_.pending && it->depth >= 0) {
        size_t from = it->priv_.stack[it->depth].from;
        size_t to = it->priv_.stack[it->depth].to;
        size_t root_len = it->priv_.stack[it->depth].root_len;
        bool walk = sp_priv_part_is(sp_priv_glob_part(it, from), "**");
        bool last = to == it->priv_.pattern_len;

        path->len = it->priv_.stack[it->depth].path_len;
        path->buf[path->len] = '\0';
        SpPath entry;
        size_t n = sp_priv_readdir_next(&it->priv_.stack[it->depth].handle, path, &entry);
        if (n == 0) {
            sp_priv_readdir_close(&it->priv_.stack[it->depth].handle);
            it->depth--;
            continue;
        }

        bool is_dir = (walk || !last) && sp_priv_has_type(&entry, SP_PRIV_IFDIR, !walk || it->priv_.recurse_symlinks);
        if (!last && !is_dir)
            continue;
        *path = entry;
        if (walk && is_dir)
            sp_priv_glob_push(it, from, to, root_len);

        /* A wildcard matches the entry's name; a recursive group, its path below the walk's root */
        SpStr subject = SP_PRIV_STR(entry.buf + entry.len - n, n);
        if (walk) {
            if (root_len < entry.len && (entry.buf[root_len] == '/' || entry.buf[root_len] == '\\'))
                root_len++;
            subject = SP_PRIV_STR(entry.buf + root_len, entry.len - root_len);
        }
        if (sp_priv_match_path(it->priv_.pattern_buf + from, to - from, subject.data, subject.len,
                               it->priv_.case_insensitive, walk, entry.flavor))
            it->priv_.pending = last || sp_priv_glob_select(it, to + 1, true, true);
    }

    if (!it->priv_.pending)
        return false;
    it->priv_.pending = false;
    *out = *path;
    return true;
}

void sp_glob_end(SpGlobIter *it) {
    for (; it->depth >= 0; it->depth--)
        sp_priv_readdir_close(&it->priv_.stack[it->depth].handle);
}

#ifdef SP_WINDOWS
/* An environment variable, read from the process environment (which Python's os.environ also updates) */
static bool sp_priv_getenv(const char *name, char *buf, DWORD size) {
    DWORD n = GetEnvironmentVariableA(name, buf, size);
    return n > 0 && n < size;
}
#endif

/* os.path.expanduser("~" + user) into home (SP_PATH_MAX); its length, or 0 when the home directory is unknown */
static size_t sp_priv_user_home(const char *user, size_t ulen, char *home) {
#ifdef SP_WINDOWS
    /* ntpath: USERPROFILE, else HOMEDRIVE joined with HOMEPATH */
    size_t len = 0;
    if (!sp_priv_getenv("USERPROFILE", home, SP_PATH_MAX)) {
        char path[SP_PATH_MAX];
        if (!sp_priv_getenv("HOMEPATH", path, SP_PATH_MAX))
            return 0;
        if (sp_priv_getenv("HOMEDRIVE", home, SP_PATH_MAX))
            len = strlen(home);
        if (len > 0 && !strchr("\\/:", home[len - 1]) && path[0] != '\\' && path[0] != '/' && len + 1 < SP_PATH_MAX)
            home[len++] = '\\';
        sp_priv_copy_trunc(home + len, SP_PATH_MAX - len, path, strlen(path));
    }
    len = strlen(home);
    if (ulen == 0)
        return len;

    /* Another user's home is guessed by swapping our name, the home's last part, for theirs */
    char current[256];
    bool has_current = sp_priv_getenv("USERNAME", current, sizeof(current));
    size_t clen = has_current ? strlen(current) : 0;
    if (has_current && clen == ulen && memcmp(current, user, ulen) == 0)
        return len;

    size_t base = len;
    while (base > 0 && home[base - 1] != '\\' && home[base - 1] != '/')
        base--;
    if (!has_current || len - base != clen || memcmp(home + base, current, clen) != 0)
        return 0;
    return base + sp_priv_copy_trunc(home + base, SP_PATH_MAX - base, user, ulen);
#else
    /* posixpath: HOME (else the passwd entry) for the current user, the passwd entry for others */
    const char *dir = ulen == 0 ? getenv("HOME") : SP_PRIV_NULL;
    if (!dir) {
        char name[256];
        if (ulen >= sizeof(name))
            return 0;
        memcpy(name, user, ulen);
        name[ulen] = '\0';

        struct passwd *pw = ulen == 0 ? getpwuid(getuid()) : getpwnam(name);
        if (!pw || !pw->pw_dir)
            return 0;
        dir = pw->pw_dir;
    }

    size_t len = strlen(dir);
    while (len > 0 && dir[len - 1] == '/')
        len--;
    return len > 0 ? sp_priv_copy_trunc(home, SP_PATH_MAX, dir, len) : sp_priv_copy_trunc(home, SP_PATH_MAX, "/", 1);
#endif
}

SpPath sp_home(SpFlavor flavor) {
    char home[SP_PATH_MAX];
    size_t len = sp_priv_user_home("", 0, home);
    return len > 0 ? sp_path_from_n(home, len, flavor) : sp_priv_error_path(sp_priv_flavor(flavor), SP_ERR);
}

/* Expands a leading "~" or "~user" part of a path without drive or root; an error if its home is unknown */
SpPath sp_expanduser(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (p->len == 0 || p->buf[0] != '~' || sp_priv_split_anchor(p->buf, p->len, p->flavor, NULL) > 0)
        return *p;

    char sep = p->flavor == SP_FLAVOR_WINDOWS ? '\\' : '/';
    size_t end = 1;
    while (end < p->len && p->buf[end] != sep)
        end++;
    char home[SP_PATH_MAX];
    size_t len = sp_priv_user_home(p->buf + 1, end - 1, home);
    if (len == 0)
        return sp_priv_error_path(p->flavor, SP_ERR);

    SpPath r = sp_path_from_n(home, len, p->flavor);
    if (end >= p->len)
        return r;

    /* The remaining parts stay parts: a leading "./" keeps one like "c:" from parsing as a drive */
    char rest[SP_PATH_MAX] = {'.', '/'};
    sp_priv_copy_trunc(rest + 2, SP_PATH_MAX - 2, p->buf + end + 1, p->len - end - 1);
    return sp_priv_join_len(&r, rest, strlen(rest));
}

SpTerm sp_owner(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
#ifdef SP_WINDOWS
    (void)p;
    return sp_priv_term(NULL, 0);
#else
    SpStatResult st = sp_stat(p);
    struct passwd *pw = st.valid ? getpwuid(st.sp_uid) : SP_PRIV_NULL;
    const char *name = pw ? pw->pw_name : SP_PRIV_NULL;
    return sp_priv_term(name, name ? strlen(name) : 0);
#endif
}

SpTerm sp_group(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
#ifdef SP_WINDOWS
    (void)p;
    return sp_priv_term(NULL, 0);
#else
    SpStatResult st = sp_stat(p);
    struct group *gr = st.valid ? getgrgid(st.sp_gid) : SP_PRIV_NULL;
    const char *name = gr ? gr->gr_name : SP_PRIV_NULL;
    return sp_priv_term(name, name ? strlen(name) : 0);
#endif
}

SpIterdirIter sp_iterdir_begin(const SpPath *p) {
    SpIterdirIter it = SP_PRIV_ZERO;
    it.dir = *p;

    const char *path_str;
    bool ok = sp_priv_path_cstr(p, &path_str);
#ifdef SP_WINDOWS
    ok = ok && (p->len == 0 || sp_priv_has_type(p, SP_PRIV_IFDIR, true)); /* FindFirstFile opens on the first next() */
#else
    ok = ok && (it.priv_.handle = opendir(path_str)) != SP_PRIV_NULL;
#endif
    it.done = ok ? 0 : -1;
    return it;
}

bool sp_iterdir_next(SpIterdirIter *it, SpPath *out) {
    if (!it || it->done != 0)
        return false;
    it->done = sp_priv_readdir_next(&it->priv_.handle, &it->dir, out) == 0;
    return !it->done;
}

void sp_iterdir_end(SpIterdirIter *it) {
    if (!it)
        return;
    sp_priv_readdir_close(&it->priv_.handle);
    it->done = 1;
}

static int sp_priv_walk_name_cmp(const void *a, const void *b) {
    return strcmp(SP_PRIV_CAST(const char *, a), SP_PRIV_CAST(const char *, b));
}

/* os.walk: each directory's sorted listing (split into directories and files) goes to the callback before or after
 * its subdirectories are walked, and a directory that can't be listed goes to on_error */
bool sp_walk(const SpPath *p, bool top_down, bool follow_symlinks, SpWalkFn callback, SpWalkErrorFn on_error,
             void *user_data) {
    const char *path_str;
    if (!p || !callback || !sp_priv_path_cstr(p, &path_str))
        return false;

    char dirnames[SP_WALK_MAX_ENTRIES][SP_WALK_NAME_MAX];
    char filenames[SP_WALK_MAX_ENTRIES][SP_WALK_NAME_MAX];
    SpWalkEntry entry = SP_PRIV_ZERO;
    entry.dirpath = *p;
    entry.dirnames = dirnames;
    entry.filenames = filenames;
    entry.user_data = user_data;

    void *handle = SP_PRIV_NULL;
    SpPath child;
    size_t n = sp_priv_readdir_next(&handle, p, &child);
    if (!handle) {
        if (on_error)
            on_error(p, errno, user_data);
        return true;
    }
    for (; n > 0; n = sp_priv_readdir_next(&handle, p, &child)) {
        bool is_dir = (sp_priv_stat_impl(&child, follow_symlinks).sp_mode & SP_PRIV_IFMT) == SP_PRIV_IFDIR;
        char (*names)[SP_WALK_NAME_MAX] = is_dir ? dirnames : filenames;
        size_t *count = is_dir ? &entry.dirname_count : &entry.filename_count;
        if (*count < SP_WALK_MAX_ENTRIES)
            sp_priv_copy_trunc(names[(*count)++], SP_WALK_NAME_MAX, child.buf + child.len - n, n);
    }
    sp_priv_readdir_close(&handle);
    qsort(dirnames, entry.dirname_count, SP_WALK_NAME_MAX, sp_priv_walk_name_cmp);
    qsort(filenames, entry.filename_count, SP_WALK_NAME_MAX, sp_priv_walk_name_cmp);

    if (top_down && !callback(&entry))
        return false;

    /* Top-down callbacks may prune entry.dirname_count */
    for (size_t i = 0; i < entry.dirname_count; i++) {
        SpPath subdir = *p;
        sp_priv_join_child(&subdir, dirnames[i], strlen(dirnames[i]));
        if (!sp_walk(&subdir, top_down, follow_symlinks, callback, on_error, user_data))
            return false;
    }
    return top_down || callback(&entry);
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

#define SP_F_TERM(ret, name, params, expr)                                                                             \
    static ret sp_priv_f_##name##_ params {                                                                            \
        sp_priv_f_ctx_active = false;                                                                                  \
        return (expr);                                                                                                 \
    }

SP_F_TERMINATOR_METHODS(SP_F_TERM)

#define SP_F_CHAIN_DECL(name, params, expr) static SpPrivDontUseThisDirectly_ *sp_priv_f_##name##_ params;
SP_F_CHAIN_METHODS(SP_F_CHAIN_DECL)

/* clang-format off */
static SpPrivDontUseThisDirectly_ sp_priv_f_instance = {
#define SP_F_TERM_INIT(ret, name, params, expr) sp_priv_f_##name##_,
    SP_F_TERMINATOR_METHODS(SP_F_TERM_INIT)
#undef SP_F_TERM_INIT
#define SP_F_CHAIN_INIT(name, params, expr) sp_priv_f_##name##_,
    SP_F_CHAIN_METHODS(SP_F_CHAIN_INIT)
#undef SP_F_CHAIN_INIT
};
/* clang-format on */

static SpPrivDontUseThisDirectly_ *sp_priv_f_chain(SpPath path) {
    sp_priv_f_ctx = path;
    return &sp_priv_f_instance;
}

#define SP_F_CHAIN(name, params, expr)                                                                                 \
    static SpPrivDontUseThisDirectly_ *sp_priv_f_##name##_ params { return sp_priv_f_chain(expr); }
SP_F_CHAIN_METHODS(SP_F_CHAIN)

#undef SP_F_CHAIN
#undef SP_F_CHAIN_DECL
#undef SP_F_TERM
#undef SP_F_CHAIN_METHODS
#undef SP_F_TERMINATOR_METHODS

SpPrivDontUseThisDirectly_ *sp_fluent_init_(SpPath p) {
    assert(!sp_priv_f_ctx_active && "snakepath fluent API: previous chain not terminated");
    sp_priv_f_ctx_active = true;
    return sp_priv_f_chain(p);
}

#endif /* SNAKEPATH_FLUENT */

#ifdef __cplusplus
}
#endif

#endif /* SNAKEPATH_IMPLEMENTATION */
