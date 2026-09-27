/* snakepath.h - C99 pathlib port, STB-style header-only library
 * No mallocs. POSIX and Windows compatible.
 * Note: OS functions (opendir/closedir, stat, getcwd, etc.) may allocate internally.
 *
 * Usage:
 *   #define SNAKEPATH_IMPLEMENTATION
 *   #include "snakepath.h"
 *
 * Errors: every result that can fail carries an SpError (a path's or text's .error, an iterator's .error, or the
 * return value of an action). A path with an error passes it on: joining to it, copying it or making a directory
 * there gives back the same error, so a chain needs one check at the end. Functions returning a bool or a number,
 * and sp_str(), must not get a path with an error (they assert). Nothing is silently truncated: a result that
 * doesn't fit its buffer is SP_ERR_TOO_LONG, and going past a configured limit is SP_ERR_LIMIT.
 */

#ifndef SNAKEPATH_H
#define SNAKEPATH_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
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
#define SP_PRIV_PTR_BITS(p) reinterpret_cast<uintptr_t>(p)
#else
#define SP_PRIV_STR(d, l) ((SpStr){.data = (d), .len = (l)})
#define SP_PRIV_ZERO {0}
#define SP_PRIV_NULL NULL
#define SP_PRIV_CAST(type, val) ((type)(val))
#define SP_PRIV_PTR_BITS(p) ((uintptr_t)(p))
#endif

/* Ignoring a result is a compile-time warning (an error with -Werror) */
#if defined(__cplusplus) && __cplusplus >= 201703L
#define SP_NODISCARD [[nodiscard]]
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define SP_NODISCARD [[nodiscard]]
#elif defined(__GNUC__) || defined(__clang__)
#define SP_NODISCARD __attribute__((warn_unused_result))
#else
#define SP_NODISCARD
#endif
/* clang-format on */

/* Windows has a 1MB default stack: larger values may overflow it (the /STACK linker flag raises it) */
#define SP_PATH_MAX_WINDOWS 1024
#define SP_PATH_MAX_LINUX 4096 /* Linux PATH_MAX; the typical 8MB stack handles this fine */

#if !defined(SP_PATH_MAX) || SP_PATH_MAX < 16
#error "SP_PATH_MAX must be defined (at least 16) before including snakepath.h: " \
       "#define SP_PATH_MAX SP_PATH_MAX_WINDOWS (1024) or SP_PATH_MAX_LINUX (4096)"
#endif

#ifndef SP_MAX_SUFFIXES
#define SP_MAX_SUFFIXES 16
#endif
#if SP_MAX_SUFFIXES < 1
#error "SP_MAX_SUFFIXES must be at least 1"
#endif

#if defined(_WIN32) || defined(_WIN64)
#define SP_WINDOWS 1
#endif

/* Every failure, from path logic and from the OS alike, with the text sp_error_str() gives it: one list, so the codes
 * and their messages can't drift apart */
/* clang-format off */
#define SP_ERRORS(X)                                                                                    \
    X(SP_OK, "Success")                                                                                 \
    X(SP_ERR_IO, "Input/output error")                       /* another OS failure */                   \
    X(SP_ERR_NOT_FOUND, "No such file or directory")                                                    \
    X(SP_ERR_EXISTS, "File exists")                                                                     \
    X(SP_ERR_NOT_DIR, "Not a directory")                                                                \
    X(SP_ERR_IS_DIR, "Is a directory")                                                                  \
    X(SP_ERR_NOT_EMPTY, "Directory not empty")                                                          \
    X(SP_ERR_PERMISSION, "Permission denied")                                                           \
    X(SP_ERR_LOOP, "Too many levels of symbolic links")                                                 \
    X(SP_ERR_NOT_LINK, "Not a symbolic link")                /* readlink */                             \
    X(SP_ERR_CROSS_DEVICE, "Invalid cross-device link")      /* rename across filesystems */            \
    X(SP_ERR_SAME_FILE, "Source and target are the same file") /* or the target is inside the source */ \
    X(SP_ERR_NO_HOME, "Could not determine home directory")                                             \
    X(SP_ERR_TOO_LONG, "Result too long for its buffer")     /* SP_PATH_MAX, or the caller's */         \
    X(SP_ERR_LIMIT, "Configured limit exceeded")             /* SP_MAX_SUFFIXES, SP_GLOB_MAX_DEPTH, SP_GLOB_PATTERN_MAX */ \
    X(SP_ERR_NUL, "Embedded null byte")                      /* which the OS can't take */              \
    X(SP_ERR_ENCODING, "Invalid UTF-8 or UTF-16")                                                       \
    X(SP_ERR_INVALID_ARG, "Invalid argument")                /* a name, stem, suffix, pattern or URI */ \
    X(SP_ERR_NO_NAME, "Path has an empty name")                                                         \
    X(SP_ERR_NOT_RELATIVE, "Path is not relative to the other path")                                    \
    X(SP_ERR_NOT_ABSOLUTE, "Path is not absolute")           /* a relative path has no file URI, and a file URI gives no absolute path */ \
    X(SP_ERR_UNSUPPORTED, "Operation not supported")         /* like owner() on Windows, or an anchored glob pattern */ \
    X(SP_ERR_NESTED_CHAIN, "Fluent chain started inside another chain")

typedef enum {
#define SP_ERROR_CODE(name, message) name,
    SP_ERRORS(SP_ERROR_CODE)
#undef SP_ERROR_CODE
} SpError;
/* clang-format on */

SP_NODISCARD const char *sp_error_str(SpError error);

/* A path's flavor is POSIX or WINDOWS: SP_FLAVOR_NATIVE resolves to the platform's when the path is made */
typedef enum { SP_FLAVOR_NATIVE = 0, SP_FLAVOR_POSIX, SP_FLAVOR_WINDOWS } SpFlavor;

/* SP_CASE_DEFAULT is the flavor's: insensitive for Windows paths, sensitive for POSIX ones */
typedef enum { SP_CASE_DEFAULT = 0, SP_CASE_SENSITIVE, SP_CASE_INSENSITIVE } SpCaseSensitivity;

/* One C function stands for a family of pathlib methods, and a function with more than one option takes a struct of
 * them, named after pathlib's keywords, so a wrong option is a compile error. Zero-initialized options are pathlib's
 * defaults unless a field says otherwise. In C: sp_copy(&p, &t, (SpCopyOptions){.into = true}); in C++ before C++20,
 * set the fields of a zero-initialized variable. */
typedef struct {
    bool follow_symlinks; /* pathlib's default is true */
    bool preserve_metadata;
    bool into; /* copy_into: target is the directory to copy into, under p's name */
} SpCopyOptions;

typedef struct {
    bool parents; /* make missing parents, with parent_mode (0 is a mode; pathlib's is SP_MODE_DIR) */
    bool exist_ok;
    unsigned int parent_mode;
} SpMkdirOptions;

typedef struct {
    bool bottom_up; /* os.walk's top_down=False */
    bool follow_symlinks;
} SpWalkOptions;

typedef struct {
    bool hard;                /* hardlink_to; else symlink_to */
    bool target_is_directory; /* symlink_to's */
} SpLinkOptions;

typedef struct {
    bool dir;        /* rmdir; else unlink */
    bool missing_ok; /* unlink's */
} SpRemoveOptions;

typedef struct {
    bool full; /* full_match: the whole path; else match, the last parts */
    SpCaseSensitivity case_sensitive;
} SpMatchOptions;

typedef struct {
    bool recursive;        /* rglob: "**\/" before the pattern */
    bool recurse_symlinks; /* "**" descends into symlinked directories */
    SpCaseSensitivity case_sensitive;
} SpGlobOptions;

/* What sp_is asks: exists (SP_ANY), is_file, is_dir, is_symlink, is_block_device, is_char_device, is_fifo, is_socket,
 * is_mount, is_junction */
typedef enum {
    SP_ANY = 0,
    SP_FILE,
    SP_DIR,
    SP_SYMLINK,
    SP_BLOCK_DEVICE,
    SP_CHAR_DEVICE,
    SP_FIFO,
    SP_SOCKET,
    SP_MOUNT,
    SP_JUNCTION
} SpFileType;

/* pathlib's default modes for mkdir and touch */
#define SP_MODE_DIR 0777
#define SP_MODE_FILE 0666

#define SP_ASSERT_PATH_INVARIANT(p)                                                                                    \
    do {                                                                                                               \
        assert((p) != NULL && "path pointer must not be NULL");                                                        \
        assert((p)->error == SP_OK && "path carries an error: check its .error first");                                \
        assert((p)->len < SP_PATH_MAX && "path length exceeds buffer size");                                           \
        assert((p)->buf[(p)->len] == '\0' && "path buffer not null-terminated");                                       \
        assert((p)->drive <= (p)->anchor && (p)->anchor <= (p)->len && "path anchor is inconsistent");                 \
        assert((p)->anchor == sp_priv_split_anchor((p)->buf, (p)->len, (p)->flavor, SP_PRIV_NULL) && "stale anchor");  \
        assert(((p)->flavor == SP_FLAVOR_POSIX || (p)->flavor == SP_FLAVOR_WINDOWS) && "invalid flavor value");        \
    } while (0)

typedef struct {
    const char *data;
    size_t len;
} SpStr;

typedef struct {
    char buf[SP_PATH_MAX];
    size_t len;
    size_t anchor, drive; /* the lengths of the anchor (drive + root) and of the drive, kept with the path */
    SpFlavor flavor;
    SpError error; /* SP_OK, or why there is no path (then it is empty) */
} SpPath;

/* Text copied out of a path (or looked up for one), NUL-terminated */
typedef struct {
    char buf[SP_PATH_MAX];
    size_t len;
    SpError error;
} SpTerm;

typedef struct {
    const SpPath *path;
    size_t pos, end, anchor;
} SpPartsIter;

typedef struct {
    SpStr items[SP_MAX_SUFFIXES];
    size_t count;
    SpError error; /* SP_ERR_LIMIT past SP_MAX_SUFFIXES suffixes */
} SpSuffixes;

typedef struct {
    const SpPath *path;
    size_t current_len;
} SpParentsIter;

typedef struct {
    SpPath dir;
    SpError error; /* why the listing ended early (or never started) */
    struct {
        void *handle;
        bool done;
    } priv_;
} SpIterdirIter;

SP_NODISCARD SpIterdirIter sp_iterdir_begin(const SpPath *p);
SP_NODISCARD bool sp_iterdir_next(SpIterdirIter *it, SpPath *out); /* returns child path */
void sp_iterdir_end(SpIterdirIter *it);

/* clang-format off */
/* Every path an iterator gives, closing it at the end */
#define SP_PRIV_FOREACH(Iter, ctx, begin, next, end, var) \
    for (struct { Iter it; int done; } ctx = { begin, 0 }; !ctx.done; end(&ctx.it), ctx.done = 1) \
    for (SpPath var; next(&ctx.it, &var); )
#define SP_ITERDIR_FOREACH(dir, entry_var) \
    SP_PRIV_FOREACH(SpIterdirIter, sp_ictx_, sp_iterdir_begin(dir), sp_iterdir_next, sp_iterdir_end, entry_var)
/* clang-format on */

/* Glob frames: one per directory level a recursive "**" goes down (the OS stops a symlink cycle at 40 levels on Linux,
 * 63 on Windows) */
#ifndef SP_GLOB_MAX_DEPTH
#define SP_GLOB_MAX_DEPTH 128
#endif
#ifndef SP_GLOB_PATTERN_MAX
#define SP_GLOB_PATTERN_MAX 256
#endif
#if SP_GLOB_MAX_DEPTH < 2 || SP_GLOB_PATTERN_MAX < 2 || SP_PATH_MAX > 0x7FFFFFFF || SP_GLOB_PATTERN_MAX > 0x7FFFFFFF
#error "SP_GLOB_MAX_DEPTH and SP_GLOB_PATTERN_MAX must be at least 2 and, with SP_PATH_MAX, fit 32 bits"
#endif

typedef struct {
    int depth;
    SpError error; /* SP_ERR_INVALID_ARG (empty pattern), SP_ERR_UNSUPPORTED (anchored pattern), SP_ERR_LIMIT, ... */
    struct {
        char pattern_buf[SP_GLOB_PATTERN_MAX];
        size_t pattern_len;
        bool case_insensitive;
        bool case_pedantic; /* explicit case sensitivity: literal parts are matched against listings too */
        bool recurse_symlinks;
        bool started; /* the pattern's leading literal parts are selected (on the first next) */
        bool pending; /* path is the next match */
        SpPath path;  /* each frame's directory is a prefix of it */
        /* Each frame matches pattern[from..to) below path[0..root_len), listing path[0..path_len). */
        struct {
            void *handle;
            uint32_t path_len, from, to, root_len; /* SP_PATH_MAX and SP_GLOB_PATTERN_MAX lengths */
        } stack[SP_GLOB_MAX_DEPTH];
    } priv_;
} SpGlobIter;

/* One directory of a walk: os.walk's (dirpath, dirnames, filenames), the names sorted. Top-down, the walk goes into
 * the subdirectories left in dirnames[0..dirname_count) at the next sp_walk_next: the caller may reorder or drop
 * names in place, or point dirnames at an array of its own that lives until the walk leaves this directory. */
typedef struct {
    SpPath dirpath;
    char **dirnames;
    size_t dirname_count;
    char **filenames;
    size_t filename_count;
    SpError error; /* dirpath couldn't be listed (os.walk's on_error): the walk goes on without it */
} SpWalkEntry;

typedef struct {
    SpWalkEntry entry;
    SpError error; /* why the walk stopped early: SP_ERR_TOO_LONG when the names don't fit the caller's buffer */
    struct {
        char *buf;
        size_t size, used, level;
        SpWalkOptions options;
        bool pending;  /* entry.dirpath is the next directory to list */
        bool prunable; /* the entry is the innermost level's listing, given out top-down */
    } priv_;
} SpWalkIter;

#define sp_path(s) sp_path_new((s), SP_FLAVOR_NATIVE)
#define sp_path_f(s, f) sp_path_new((s), (f))

/* Join paths: sp_join(p, "a", "b", "c") - C only, use sp_join_one in C++ */
#ifndef __cplusplus
#define sp_join(base, ...)                                                                                             \
    sp_join_impl((base), (const char *[]){__VA_ARGS__}, SP_ARRAY_LEN(((const char *[]){__VA_ARGS__})))
#endif

#define SP_ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

SP_NODISCARD SpPath sp_path_new(const char *s, SpFlavor flavor);
SP_NODISCARD SpPath sp_path_from_n(const char *s, size_t len, SpFlavor flavor);
SP_NODISCARD SpPath sp_path_convert(const char *s, SpFlavor src_flavor, SpFlavor dest_flavor);

SP_NODISCARD const char *sp_str(const SpPath *p); /* "." for the empty path */
SP_NODISCARD SpTerm sp_as_posix(const SpPath *p);

SP_NODISCARD SpTerm sp_drive(const SpPath *p);
SP_NODISCARD SpTerm sp_root(const SpPath *p);
SP_NODISCARD SpTerm sp_anchor(const SpPath *p);
SP_NODISCARD SpTerm sp_name(const SpPath *p);
SP_NODISCARD SpTerm sp_stem(const SpPath *p);
SP_NODISCARD SpTerm sp_suffix(const SpPath *p);
SP_NODISCARD SpSuffixes sp_suffixes(const SpPath *p);
SP_NODISCARD SpPath sp_parent(const SpPath *p);

SP_NODISCARD SpPartsIter sp_parts_begin(const SpPath *p);
SP_NODISCARD bool sp_parts_next(SpPartsIter *it, SpStr *out);

SP_NODISCARD SpParentsIter sp_parents_begin(const SpPath *p);
SP_NODISCARD bool sp_parents_next(SpParentsIter *it, SpPath *out);

SP_NODISCARD SpPath sp_join_one(const SpPath *base, const char *other);
SP_NODISCARD SpPath sp_join_n(const SpPath *base, const char *s, size_t len);
SP_NODISCARD SpPath sp_join_impl(const SpPath *base, const char **parts, size_t count);

SP_NODISCARD SpPath sp_with_segments(const SpPath *p, const char **parts, size_t parts_count);
SP_NODISCARD SpPath sp_with_name(const SpPath *p, const char *name);
SP_NODISCARD SpPath sp_with_stem(const SpPath *p, const char *stem);
SP_NODISCARD SpPath sp_with_suffix(const SpPath *p, const char *suffix);

SP_NODISCARD SpPath sp_relative_to(const SpPath *p, const SpPath *other, bool walk_up);
SP_NODISCARD bool sp_is_relative_to(const SpPath *p, const SpPath *other);

SP_NODISCARD bool sp_is_absolute(const SpPath *p);
SP_NODISCARD SpPath sp_cwd(SpFlavor flavor);
SP_NODISCARD SpPath sp_absolute(const SpPath *p);
SP_NODISCARD SpError sp_as_uri(const SpPath *p, char *buf, size_t buf_size); /* SP_ERR_NOT_ABSOLUTE if relative */
SP_NODISCARD SpPath sp_from_uri(const char *uri, SpFlavor flavor);
SP_NODISCARD bool sp_path_eq(const SpPath *a, const SpPath *b);
SP_NODISCARD int sp_path_cmp(const SpPath *a, const SpPath *b);
SP_NODISCARD static inline bool sp_path_ne(const SpPath *a, const SpPath *b) {
    return a->flavor != b->flavor || sp_path_cmp(a, b) != 0;
}
SP_NODISCARD unsigned long sp_path_hash(const SpPath *p);
/* match and full_match; the pattern must have a part (pathlib raises for an empty one) */
SP_NODISCARD bool sp_match(const SpPath *p, const char *pattern, SpMatchOptions options);
/* exists and the is_* predicates: follow_symlinks matters for SP_ANY, SP_FILE and SP_DIR (pathlib follows for the
 * devices, fifos and sockets, never for SP_SYMLINK and SP_JUNCTION; SP_MOUNT looks at the path itself) */
SP_NODISCARD bool sp_is(const SpPath *p, SpFileType type, bool follow_symlinks);

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
    unsigned int sp_reparse_tag; /* lstat's reparse point tag on Windows (CPython's st_reparse_tag), else 0 */
    SpError error;
} SpStatResult;

SP_NODISCARD SpStatResult sp_stat(const SpPath *p, bool follow_symlinks); /* lstat: follow_symlinks false */
SP_NODISCARD bool sp_stat_eq(const SpStatResult *a, const SpStatResult *b);

SP_NODISCARD SpPath sp_readlink(const SpPath *p);
SP_NODISCARD SpPath sp_resolve(const SpPath *p, bool strict);
SP_NODISCARD SpError sp_link_to(const SpPath *p, const SpPath *target,
                                SpLinkOptions options);          /* symlink_to, hardlink_to */
SP_NODISCARD bool sp_samefile(const SpPath *a, const SpPath *b); /* false unless both exist */

SP_NODISCARD SpError sp_mkdir(const SpPath *p, unsigned int mode, SpMkdirOptions options);
SP_NODISCARD SpError sp_touch(const SpPath *p, unsigned int mode, bool exist_ok);
SP_NODISCARD SpError sp_remove(const SpPath *p, SpRemoveOptions options); /* unlink, rmdir */
SP_NODISCARD SpError sp_chmod(const SpPath *p, unsigned int mode, bool follow_symlinks);
/* These return the target path (target / p's name with into). copy is recursive; move renames, or copies and deletes
 * across filesystems */
SP_NODISCARD SpPath sp_rename(const SpPath *p, const SpPath *target, bool replace);
SP_NODISCARD SpPath sp_copy(const SpPath *p, const SpPath *target, SpCopyOptions options); /* copy, copy_into */
SP_NODISCARD SpPath sp_move(const SpPath *p, const SpPath *target, bool into);             /* move, move_into */

typedef struct {
    size_t bytes; /* read or written; with SP_ERR_TOO_LONG from sp_read_file, the file's size */
    SpError error;
} SpIOResult;

SP_NODISCARD SpIOResult sp_read_file(const SpPath *p, char *buf, size_t buf_size);
SP_NODISCARD SpIOResult sp_write_file(const SpPath *p, const char *data, size_t data_len);

SP_NODISCARD SpPath sp_home(SpFlavor flavor);
SP_NODISCARD SpPath sp_expanduser(const SpPath *p);

SP_NODISCARD SpTerm sp_owner(const SpPath *p, bool follow_symlinks);
SP_NODISCARD SpTerm sp_group(const SpPath *p, bool follow_symlinks);

/* Path.walk as an iterator: sp_walk_next gives each directory in turn (NULL at the end, or with it.error set when the
 * walk had to stop). The names are kept in the caller's buf while their directory is being walked; an entry is valid
 * until the next call. */
SP_NODISCARD SpWalkIter sp_walk_begin(const SpPath *top, SpWalkOptions options, void *buf, size_t buf_size);
SP_NODISCARD SpWalkEntry *sp_walk_next(SpWalkIter *it);

/* glob and rglob (options.recursive) over the paths matching a relative pattern
 * sp_glob_begin: the iterator; it.error is set for a bad pattern or base
 * sp_glob_next:  the next match into out, true while there is one; false at the end, or with it.error
 * sp_glob_end:   closes the iterator (releasing directory handles) */
SP_NODISCARD SpGlobIter sp_glob_begin(const SpPath *base, const char *pattern, SpGlobOptions options);
SP_NODISCARD bool sp_glob_next(SpGlobIter *it, SpPath *out);
void sp_glob_end(SpGlobIter *it);

/* Every match, closing the iterator at the end; options is an SpGlobOptions value */
#define SP_GLOB_FOREACH(base, pattern, options, match_var)                                                             \
    SP_PRIV_FOREACH(SpGlobIter, sp_gctx_, sp_glob_begin(base, pattern, options), sp_glob_next, sp_glob_end, match_var)

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
    X_TERM(SpTerm, as_posix, (void), sp_as_posix(&sp_priv_f_ctx))                                                      \
    X_TERM(SpTerm, owner, (bool follow_symlinks), sp_owner(&sp_priv_f_ctx, follow_symlinks))                           \
    X_TERM(SpTerm, group, (bool follow_symlinks), sp_group(&sp_priv_f_ctx, follow_symlinks))                           \
    X_TERM(bool, is_absolute, (void), sp_is_absolute(&sp_priv_f_ctx))                                                  \
    X_TERM(bool, is_relative_to, (const SpPath *o), sp_is_relative_to(&sp_priv_f_ctx, o))                              \
    X_TERM(bool, is, (SpFileType type, bool follow_symlinks), sp_is(&sp_priv_f_ctx, type, follow_symlinks))            \
    X_TERM(SpStatResult, stat, (bool follow_symlinks), sp_stat(&sp_priv_f_ctx, follow_symlinks))                       \
    X_TERM(bool, eq, (const SpPath *o), sp_path_eq(&sp_priv_f_ctx, o))                                                 \
    X_TERM(bool, ne, (const SpPath *o), sp_path_ne(&sp_priv_f_ctx, o))                                                 \
    X_TERM(bool, samefile, (const SpPath *o), sp_samefile(&sp_priv_f_ctx, o))                                          \
    X_TERM(SpIOResult, read_file, (char *buf, size_t buf_size), sp_read_file(&sp_priv_f_ctx, buf, buf_size))           \
    X_TERM(SpIOResult, write_file, (const char *data, size_t data_len), sp_write_file(&sp_priv_f_ctx, data, data_len)) \
    X_TERM(SpError, as_uri, (char *buf, size_t buf_size), sp_as_uri(&sp_priv_f_ctx, buf, buf_size))                    \
    X_TERM(bool, match, (const char *pattern, SpMatchOptions options), sp_match(&sp_priv_f_ctx, pattern, options))     \
    X_TERM(SpError, mkdir, (unsigned int mode, SpMkdirOptions options), sp_mkdir(&sp_priv_f_ctx, mode, options))       \
    X_TERM(SpError, touch, (unsigned int mode, bool exist_ok), sp_touch(&sp_priv_f_ctx, mode, exist_ok))               \
    X_TERM(SpError, remove, (SpRemoveOptions options), sp_remove(&sp_priv_f_ctx, options))                             \
    X_TERM(SpError, chmod, (unsigned int mode, bool follow_symlinks), sp_chmod(&sp_priv_f_ctx, mode, follow_symlinks)) \
    X_TERM(SpError, link_to, (const SpPath *target, SpLinkOptions options), sp_link_to(&sp_priv_f_ctx, target, options))

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
    X(rename, (const SpPath *target, bool replace), sp_rename(&sp_priv_f_ctx, target, replace))                        \
    X(copy, (const SpPath *target, SpCopyOptions options), sp_copy(&sp_priv_f_ctx, target, options))                   \
    X(move, (const SpPath *target, bool into), sp_move(&sp_priv_f_ctx, target, into))

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

/* A chain started while another one is running (say, in its arguments) poisons both with SP_ERR_NESTED_CHAIN */
SP_NODISCARD SpPrivDontUseThisDirectly_ *sp_fluent_init_(SpPath);

/* SPF("/a")->join("b")->parent()->path() */
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

/* Platform-specific includes, and how each platform takes a path and a file: Windows' wide API takes UTF-16, which
 * paths are converted to in a buffer of SP_PRIV_NATIVE_MAX, and POSIX takes the path's own bytes */
#include <errno.h>
#ifdef SP_WINDOWS
#include <wchar.h>
#include <windows.h>
typedef wchar_t SpPrivChar;
typedef HANDLE SpPrivFile;
#define SP_PRIV_NATIVE_MAX SP_PATH_MAX
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h> /* For realpath */
#include <fcntl.h>  /* For O_CREAT and, where the headers show it, fchmodat's AT_SYMLINK_NOFOLLOW */
#include <utime.h>  /* For utime() */
#include <pwd.h>    /* For getpwuid, getpwnam */
#include <grp.h>    /* For getgrgid */
typedef char SpPrivChar;
typedef int SpPrivFile;
#define SP_PRIV_NATIVE_MAX 1
/* C99 workaround - these functions exist but aren't declared without feature test macros.
   C++ headers already expose them via stdlib.h/cstdlib, so only declare in C mode. */
#ifndef __cplusplus
extern int lstat(const char *path, struct stat *buf);
extern ssize_t readlink(const char *path, char *buf, size_t bufsiz);
extern char *realpath(const char *path, char *resolved_path);
extern int symlink(const char *target, const char *linkpath);
extern int link(const char *oldpath, const char *newpath);
extern int chmod(const char *path, mode_t mode);
#ifdef AT_SYMLINK_NOFOLLOW
extern int fchmodat(int dirfd, const char *path, mode_t mode, int flags);
#endif
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

/* IO_REPARSE_TAG_MOUNT_POINT: a junction's reparse tag, for the code shared with POSIX that tests it */
#define SP_PRIV_REPARSE_TAG_MOUNT_POINT 0xA0000003u

#ifdef __cplusplus
extern "C" {
#endif

/* A path's flavor is never SP_FLAVOR_NATIVE: making a path resolves it to the platform's */
static SpFlavor sp_priv_flavor(SpFlavor flavor) {
    assert((flavor == SP_FLAVOR_NATIVE || flavor == SP_FLAVOR_POSIX || flavor == SP_FLAVOR_WINDOWS) &&
           "invalid flavor");
#ifdef SP_WINDOWS
    return flavor == SP_FLAVOR_NATIVE ? SP_FLAVOR_WINDOWS : flavor;
#else
    return flavor == SP_FLAVOR_NATIVE ? SP_FLAVOR_POSIX : flavor;
#endif
}

/* The last failed OS call's error */
static SpError sp_priv_last_error(void) {
    static const struct {
        unsigned long code;
        SpError error;
    } errors[] = {
#ifdef SP_WINDOWS
        {ERROR_FILE_NOT_FOUND, SP_ERR_NOT_FOUND},      {ERROR_PATH_NOT_FOUND, SP_ERR_NOT_FOUND},
        {ERROR_INVALID_DRIVE, SP_ERR_NOT_FOUND},       {ERROR_BAD_NETPATH, SP_ERR_NOT_FOUND},
        {ERROR_ALREADY_EXISTS, SP_ERR_EXISTS},         {ERROR_FILE_EXISTS, SP_ERR_EXISTS},
        {ERROR_ACCESS_DENIED, SP_ERR_PERMISSION},      {ERROR_SHARING_VIOLATION, SP_ERR_PERMISSION},
        {ERROR_PRIVILEGE_NOT_HELD, SP_ERR_PERMISSION}, {ERROR_DIRECTORY, SP_ERR_NOT_DIR},
        {ERROR_DIR_NOT_EMPTY, SP_ERR_NOT_EMPTY},       {ERROR_CANT_RESOLVE_FILENAME, SP_ERR_LOOP},
        {ERROR_NOT_A_REPARSE_POINT, SP_ERR_NOT_LINK},  {ERROR_NOT_SAME_DEVICE, SP_ERR_CROSS_DEVICE},
        {ERROR_FILENAME_EXCED_RANGE, SP_ERR_TOO_LONG}, {ERROR_INSUFFICIENT_BUFFER, SP_ERR_TOO_LONG},
        {ERROR_INVALID_NAME, SP_ERR_INVALID_ARG},      {ERROR_INVALID_PARAMETER, SP_ERR_INVALID_ARG},
        {ERROR_NOT_SUPPORTED, SP_ERR_UNSUPPORTED},     {ERROR_INVALID_FUNCTION, SP_ERR_UNSUPPORTED},
    };
    unsigned long code = GetLastError();
#else
        {ENOENT, SP_ERR_NOT_FOUND},      {EEXIST, SP_ERR_EXISTS},       {EACCES, SP_ERR_PERMISSION},
        {EPERM, SP_ERR_PERMISSION},      {ENOTDIR, SP_ERR_NOT_DIR},     {EISDIR, SP_ERR_IS_DIR},
        {ENOTEMPTY, SP_ERR_NOT_EMPTY},   {ELOOP, SP_ERR_LOOP},          {EXDEV, SP_ERR_CROSS_DEVICE},
        {ENAMETOOLONG, SP_ERR_TOO_LONG}, {ERANGE, SP_ERR_TOO_LONG},     {EINVAL, SP_ERR_INVALID_ARG},
        {ENOSYS, SP_ERR_UNSUPPORTED},    {ENOTSUP, SP_ERR_UNSUPPORTED},
    };
    unsigned long code = SP_PRIV_CAST(unsigned long, errno);
#endif

    for (size_t i = 0; i < SP_ARRAY_LEN(errors); i++)
        if (errors[i].code == code)
            return errors[i].error;
    return SP_ERR_IO;
}

/* A path as the OS takes it ("." when empty): .path is the path's own bytes, or on Windows UTF-16 from WTF-8 in .buf */
typedef struct {
    const SpPrivChar *path;
    SpPrivChar buf[SP_PRIV_NATIVE_MAX];
} SpPrivNative;

/* Fails with the path's own error, SP_ERR_NUL for an embedded NUL, or on Windows SP_ERR_ENCODING for bytes that aren't
 * UTF-8. Encoded surrogates stand for themselves (WTF-8), which is how the bindings pass Python's lone surrogates. */
static SpError sp_priv_native(const SpPath *p, SpPrivNative *n) {
    const char *s = p->len == 0 ? "." : p->buf;
#ifdef SP_WINDOWS
    n->path = n->buf;
#else
    n->path = s;
#endif
    if (p->error != SP_OK)
        return p->error;
    if (memchr(p->buf, '\0', p->len))
        return SP_ERR_NUL;

#ifdef SP_WINDOWS
    size_t len = p->len == 0 ? 1 : p->len;
    static const unsigned long least[4] = {0, 0x80, 0x800, 0x10000}; /* shorter would be an overlong encoding */
    size_t w = 0;
    for (size_t i = 0; i < len;) {
        unsigned char lead = SP_PRIV_CAST(unsigned char, s[i]);
        size_t extra = lead >= 0xF0 ? 3 : lead >= 0xE0 ? 2 : lead >= 0xC0 ? 1 : 0;
        if ((lead >= 0x80 && lead < 0xC0) || lead > 0xF4 || len - i <= extra)
            return SP_ERR_ENCODING;

        unsigned long cp = lead & (0x7Fu >> extra);
        for (size_t k = 1; k <= extra; k++) {
            unsigned char c = SP_PRIV_CAST(unsigned char, s[i + k]);
            if ((c & 0xC0) != 0x80)
                return SP_ERR_ENCODING;
            cp = (cp << 6) | (c & 0x3F);
        }
        if (cp < least[extra] || cp > 0x10FFFF)
            return SP_ERR_ENCODING;
        i += extra + 1;

        if (w + (cp >= 0x10000 ? 2 : 1) >= SP_PATH_MAX)
            return SP_ERR_TOO_LONG;
        if (cp >= 0x10000) {
            n->buf[w++] = SP_PRIV_CAST(wchar_t, 0xD800 + ((cp - 0x10000) >> 10));
            n->buf[w++] = SP_PRIV_CAST(wchar_t, 0xDC00 + ((cp - 0x10000) & 0x3FF));
        } else {
            n->buf[w++] = SP_PRIV_CAST(wchar_t, cp);
        }
    }
    n->buf[w] = L'\0';
#endif
    return SP_OK;
}

#ifdef SP_WINDOWS
/* UTF-16 text from Windows as UTF-8 at out[0..cap), NUL-terminated; a lone surrogate is encoded as itself (WTF-8), as
 * Python's surrogatepass reads it back */
static SpError sp_priv_from_wide(const wchar_t *w, size_t wlen, char *out, size_t cap, size_t *len) {
    size_t n = 0;
    for (size_t i = 0; i < wlen; i++) {
        unsigned long cp = w[i];
        if (cp >= 0xD800 && cp < 0xDC00 && i + 1 < wlen && w[i + 1] >= 0xDC00 && w[i + 1] < 0xE000) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (w[i + 1] - 0xDC00u);
            i++;
        }

        size_t bytes = cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
        if (n + bytes >= cap)
            return SP_ERR_TOO_LONG;
        if (bytes == 1) {
            out[n++] = SP_PRIV_CAST(char, cp);
            continue;
        }
        unsigned long lead = bytes == 2 ? 0xC0 : bytes == 3 ? 0xE0 : 0xF0;
        out[n++] = SP_PRIV_CAST(char, lead | (cp >> (6 * (bytes - 1))));
        for (size_t k = bytes - 1; k-- > 0;)
            out[n++] = SP_PRIV_CAST(char, 0x80 | ((cp >> (6 * k)) & 0x3F));
    }
    out[n] = '\0';
    *len = n;
    return SP_OK;
}
#endif

/* Text copied from data[0..len), which fits: text from a path is shorter than its buffer */
static SpTerm sp_priv_term(const char *data, size_t len, SpError error) {
    SpTerm t = SP_PRIV_ZERO;
    t.error = error;
    if (error == SP_OK && len > 0) {
        memcpy(t.buf, data, len);
        t.len = len;
    }
    return t;
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
/* clang-format off */
/* Simple lowercase: runs of (first code point, count, stride, delta) */
static const long sp_priv_lower_runs[] = {0x41, 0x1a, 0x1, 0x20, 0xc0, 0x17, 0x1, 0x20, 0xd8, 0x7, 0x1, 0x20, 0x100, 0x18, 0x2, 0x1, 0x130, 0x1, 0x1, -0xc7, 0x132, 0x3, 0x2, 0x1, 0x139, 0x8, 0x2, 0x1, 0x14a, 0x17, 0x2, 0x1, 0x178, 0x1, 0x1, -0x79, 0x179, 0x3, 0x2, 0x1, 0x181, 0x1, 0x1, 0xd2, 0x182, 0x2, 0x2, 0x1, 0x186, 0x1, 0x1, 0xce, 0x187, 0x1, 0x1, 0x1, 0x189, 0x2, 0x1, 0xcd, 0x18b, 0x1, 0x1, 0x1, 0x18e, 0x1, 0x1, 0x4f, 0x18f, 0x1, 0x1, 0xca, 0x190, 0x1, 0x1, 0xcb, 0x191, 0x1, 0x1, 0x1, 0x193, 0x1, 0x1, 0xcd, 0x194, 0x1, 0x1, 0xcf, 0x196, 0x1, 0x1, 0xd3, 0x197, 0x1, 0x1, 0xd1, 0x198, 0x1, 0x1, 0x1, 0x19c, 0x1, 0x1, 0xd3, 0x19d, 0x1, 0x1, 0xd5, 0x19f, 0x1, 0x1, 0xd6, 0x1a0, 0x3, 0x2, 0x1, 0x1a6, 0x1, 0x1, 0xda, 0x1a7, 0x1, 0x1, 0x1, 0x1a9, 0x1, 0x1, 0xda, 0x1ac, 0x1, 0x1, 0x1, 0x1ae, 0x1, 0x1, 0xda, 0x1af, 0x1, 0x1, 0x1, 0x1b1, 0x2, 0x1, 0xd9, 0x1b3, 0x2, 0x2, 0x1, 0x1b7, 0x1, 0x1, 0xdb, 0x1b8, 0x1, 0x1, 0x1, 0x1bc, 0x1, 0x1, 0x1, 0x1c4, 0x1, 0x1, 0x2, 0x1c5, 0x1, 0x1, 0x1, 0x1c7, 0x1, 0x1, 0x2, 0x1c8, 0x1, 0x1, 0x1, 0x1ca, 0x1, 0x1, 0x2, 0x1cb, 0x9, 0x2, 0x1, 0x1de, 0x9, 0x2, 0x1, 0x1f1, 0x1, 0x1, 0x2, 0x1f2, 0x2, 0x2, 0x1, 0x1f6, 0x1, 0x1, -0x61, 0x1f7, 0x1, 0x1, -0x38, 0x1f8, 0x14, 0x2, 0x1, 0x220, 0x1, 0x1, -0x82, 0x222, 0x9, 0x2, 0x1, 0x23a, 0x1, 0x1, 0x2a2b, 0x23b, 0x1, 0x1, 0x1, 0x23d, 0x1, 0x1, -0xa3, 0x23e, 0x1, 0x1, 0x2a28, 0x241, 0x1, 0x1, 0x1, 0x243, 0x1, 0x1, -0xc3, 0x244, 0x1, 0x1, 0x45, 0x245, 0x1, 0x1, 0x47, 0x246, 0x5, 0x2, 0x1, 0x370, 0x2, 0x2, 0x1, 0x376, 0x1, 0x1, 0x1, 0x37f, 0x1, 0x1, 0x74, 0x386, 0x1, 0x1, 0x26, 0x388, 0x3, 0x1, 0x25, 0x38c, 0x1, 0x1, 0x40, 0x38e, 0x2, 0x1, 0x3f, 0x391, 0x11, 0x1, 0x20, 0x3a3, 0x9, 0x1, 0x20, 0x3cf, 0x1, 0x1, 0x8, 0x3d8, 0xc, 0x2, 0x1, 0x3f4, 0x1, 0x1, -0x3c, 0x3f7, 0x1, 0x1, 0x1, 0x3f9, 0x1, 0x1, -0x7, 0x3fa, 0x1, 0x1, 0x1, 0x3fd, 0x3, 0x1, -0x82, 0x400, 0x10, 0x1, 0x50, 0x410, 0x20, 0x1, 0x20, 0x460, 0x11, 0x2, 0x1, 0x48a, 0x1b, 0x2, 0x1, 0x4c0, 0x1, 0x1, 0xf, 0x4c1, 0x7, 0x2, 0x1, 0x4d0, 0x30, 0x2, 0x1, 0x531, 0x26, 0x1, 0x30, 0x10a0, 0x26, 0x1, 0x1c60, 0x10c7, 0x1, 0x1, 0x1c60, 0x10cd, 0x1, 0x1, 0x1c60, 0x13a0, 0x50, 0x1, 0x97d0, 0x13f0, 0x6, 0x1, 0x8, 0x1c89, 0x1, 0x1, 0x1, 0x1c90, 0x2b, 0x1, -0xbc0, 0x1cbd, 0x3, 0x1, -0xbc0, 0x1e00, 0x4b, 0x2, 0x1, 0x1e9e, 0x1, 0x1, -0x1dbf, 0x1ea0, 0x30, 0x2, 0x1, 0x1f08, 0x8, 0x1, -0x8, 0x1f18, 0x6, 0x1, -0x8, 0x1f28, 0x8, 0x1, -0x8, 0x1f38, 0x8, 0x1, -0x8, 0x1f48, 0x6, 0x1, -0x8, 0x1f59, 0x4, 0x2, -0x8, 0x1f68, 0x8, 0x1, -0x8, 0x1f88, 0x8, 0x1, -0x8, 0x1f98, 0x8, 0x1, -0x8, 0x1fa8, 0x8, 0x1, -0x8, 0x1fb8, 0x2, 0x1, -0x8, 0x1fba, 0x2, 0x1, -0x4a, 0x1fbc, 0x1, 0x1, -0x9, 0x1fc8, 0x4, 0x1, -0x56, 0x1fcc, 0x1, 0x1, -0x9, 0x1fd8, 0x2, 0x1, -0x8, 0x1fda, 0x2, 0x1, -0x64, 0x1fe8, 0x2, 0x1, -0x8, 0x1fea, 0x2, 0x1, -0x70, 0x1fec, 0x1, 0x1, -0x7, 0x1ff8, 0x2, 0x1, -0x80, 0x1ffa, 0x2, 0x1, -0x7e, 0x1ffc, 0x1, 0x1, -0x9, 0x2126, 0x1, 0x1, -0x1d5d, 0x212a, 0x1, 0x1, -0x20bf, 0x212b, 0x1, 0x1, -0x2046, 0x2132, 0x1, 0x1, 0x1c, 0x2160, 0x10, 0x1, 0x10, 0x2183, 0x1, 0x1, 0x1, 0x24b6, 0x1a, 0x1, 0x1a, 0x2c00, 0x30, 0x1, 0x30, 0x2c60, 0x1, 0x1, 0x1, 0x2c62, 0x1, 0x1, -0x29f7, 0x2c63, 0x1, 0x1, -0xee6, 0x2c64, 0x1, 0x1, -0x29e7, 0x2c67, 0x3, 0x2, 0x1, 0x2c6d, 0x1, 0x1, -0x2a1c, 0x2c6e, 0x1, 0x1, -0x29fd, 0x2c6f, 0x1, 0x1, -0x2a1f, 0x2c70, 0x1, 0x1, -0x2a1e, 0x2c72, 0x1, 0x1, 0x1, 0x2c75, 0x1, 0x1, 0x1, 0x2c7e, 0x2, 0x1, -0x2a3f, 0x2c80, 0x32, 0x2, 0x1, 0x2ceb, 0x2, 0x2, 0x1, 0x2cf2, 0x1, 0x1, 0x1, 0xa640, 0x17, 0x2, 0x1, 0xa680, 0xe, 0x2, 0x1, 0xa722, 0x7, 0x2, 0x1, 0xa732, 0x1f, 0x2, 0x1, 0xa779, 0x2, 0x2, 0x1, 0xa77d, 0x1, 0x1, -0x8a04, 0xa77e, 0x5, 0x2, 0x1, 0xa78b, 0x1, 0x1, 0x1, 0xa78d, 0x1, 0x1, -0xa528, 0xa790, 0x2, 0x2, 0x1, 0xa796, 0xa, 0x2, 0x1, 0xa7aa, 0x1, 0x1, -0xa544, 0xa7ab, 0x1, 0x1, -0xa54f, 0xa7ac, 0x1, 0x1, -0xa54b, 0xa7ad, 0x1, 0x1, -0xa541, 0xa7ae, 0x1, 0x1, -0xa544, 0xa7b0, 0x1, 0x1, -0xa512, 0xa7b1, 0x1, 0x1, -0xa52a, 0xa7b2, 0x1, 0x1, -0xa515, 0xa7b3, 0x1, 0x1, 0x3a0, 0xa7b4, 0x8, 0x2, 0x1, 0xa7c4, 0x1, 0x1, -0x30, 0xa7c5, 0x1, 0x1, -0xa543, 0xa7c6, 0x1, 0x1, -0x8a38, 0xa7c7, 0x2, 0x2, 0x1, 0xa7cb, 0x1, 0x1, -0xa567, 0xa7cc, 0x8, 0x2, 0x1, 0xa7dc, 0x1, 0x1, -0xa641, 0xa7f5, 0x1, 0x1, 0x1, 0xff21, 0x1a, 0x1, 0x20, 0x10400, 0x28, 0x1, 0x28, 0x104b0, 0x24, 0x1, 0x28, 0x10570, 0xb, 0x1, 0x27, 0x1057c, 0xf, 0x1, 0x27, 0x1058c, 0x7, 0x1, 0x27, 0x10594, 0x2, 0x1, 0x27, 0x10c80, 0x33, 0x1, 0x40, 0x10d50, 0x16, 0x1, 0x20, 0x118a0, 0x20, 0x1, 0x20, 0x16e40, 0x20, 0x1, 0x20, 0x16ea0, 0x19, 0x1, 0x1b, 0x1e900, 0x22, 0x1, 0x22};

/* Simple uppercase: runs of (first code point, count, stride, delta) */
static const long sp_priv_upper_runs[] = {0x61, 0x1a, 0x1, -0x20, 0xb5, 0x1, 0x1, 0x2e7, 0xe0, 0x17, 0x1, -0x20, 0xf8, 0x7, 0x1, -0x20, 0xff, 0x1, 0x1, 0x79, 0x101, 0x18, 0x2, -0x1, 0x131, 0x1, 0x1, -0xe8, 0x133, 0x3, 0x2, -0x1, 0x13a, 0x8, 0x2, -0x1, 0x14b, 0x17, 0x2, -0x1, 0x17a, 0x3, 0x2, -0x1, 0x17f, 0x1, 0x1, -0x12c, 0x180, 0x1, 0x1, 0xc3, 0x183, 0x2, 0x2, -0x1, 0x188, 0x1, 0x1, -0x1, 0x18c, 0x1, 0x1, -0x1, 0x192, 0x1, 0x1, -0x1, 0x195, 0x1, 0x1, 0x61, 0x199, 0x1, 0x1, -0x1, 0x19a, 0x1, 0x1, 0xa3, 0x19b, 0x1, 0x1, 0xa641, 0x19e, 0x1, 0x1, 0x82, 0x1a1, 0x3, 0x2, -0x1, 0x1a8, 0x1, 0x1, -0x1, 0x1ad, 0x1, 0x1, -0x1, 0x1b0, 0x1, 0x1, -0x1, 0x1b4, 0x2, 0x2, -0x1, 0x1b9, 0x1, 0x1, -0x1, 0x1bd, 0x1, 0x1, -0x1, 0x1bf, 0x1, 0x1, 0x38, 0x1c5, 0x1, 0x1, -0x1, 0x1c6, 0x1, 0x1, -0x2, 0x1c8, 0x1, 0x1, -0x1, 0x1c9, 0x1, 0x1, -0x2, 0x1cb, 0x1, 0x1, -0x1, 0x1cc, 0x1, 0x1, -0x2, 0x1ce, 0x8, 0x2, -0x1, 0x1dd, 0x1, 0x1, -0x4f, 0x1df, 0x9, 0x2, -0x1, 0x1f2, 0x1, 0x1, -0x1, 0x1f3, 0x1, 0x1, -0x2, 0x1f5, 0x1, 0x1, -0x1, 0x1f9, 0x14, 0x2, -0x1, 0x223, 0x9, 0x2, -0x1, 0x23c, 0x1, 0x1, -0x1, 0x23f, 0x2, 0x1, 0x2a3f, 0x242, 0x1, 0x1, -0x1, 0x247, 0x5, 0x2, -0x1, 0x250, 0x1, 0x1, 0x2a1f, 0x251, 0x1, 0x1, 0x2a1c, 0x252, 0x1, 0x1, 0x2a1e, 0x253, 0x1, 0x1, -0xd2, 0x254, 0x1, 0x1, -0xce, 0x256, 0x2, 0x1, -0xcd, 0x259, 0x1, 0x1, -0xca, 0x25b, 0x1, 0x1, -0xcb, 0x25c, 0x1, 0x1, 0xa54f, 0x260, 0x1, 0x1, -0xcd, 0x261, 0x1, 0x1, 0xa54b, 0x263, 0x1, 0x1, -0xcf, 0x264, 0x1, 0x1, 0xa567, 0x265, 0x1, 0x1, 0xa528, 0x266, 0x1, 0x1, 0xa544, 0x268, 0x1, 0x1, -0xd1, 0x269, 0x1, 0x1, -0xd3, 0x26a, 0x1, 0x1, 0xa544, 0x26b, 0x1, 0x1, 0x29f7, 0x26c, 0x1, 0x1, 0xa541, 0x26f, 0x1, 0x1, -0xd3, 0x271, 0x1, 0x1, 0x29fd, 0x272, 0x1, 0x1, -0xd5, 0x275, 0x1, 0x1, -0xd6, 0x27d, 0x1, 0x1, 0x29e7, 0x280, 0x1, 0x1, -0xda, 0x282, 0x1, 0x1, 0xa543, 0x283, 0x1, 0x1, -0xda, 0x287, 0x1, 0x1, 0xa52a, 0x288, 0x1, 0x1, -0xda, 0x289, 0x1, 0x1, -0x45, 0x28a, 0x2, 0x1, -0xd9, 0x28c, 0x1, 0x1, -0x47, 0x292, 0x1, 0x1, -0xdb, 0x29d, 0x1, 0x1, 0xa515, 0x29e, 0x1, 0x1, 0xa512, 0x345, 0x1, 0x1, 0x54, 0x371, 0x2, 0x2, -0x1, 0x377, 0x1, 0x1, -0x1, 0x37b, 0x3, 0x1, 0x82, 0x3ac, 0x1, 0x1, -0x26, 0x3ad, 0x3, 0x1, -0x25, 0x3b1, 0x11, 0x1, -0x20, 0x3c2, 0x1, 0x1, -0x1f, 0x3c3, 0x9, 0x1, -0x20, 0x3cc, 0x1, 0x1, -0x40, 0x3cd, 0x2, 0x1, -0x3f, 0x3d0, 0x1, 0x1, -0x3e, 0x3d1, 0x1, 0x1, -0x39, 0x3d5, 0x1, 0x1, -0x2f, 0x3d6, 0x1, 0x1, -0x36, 0x3d7, 0x1, 0x1, -0x8, 0x3d9, 0xc, 0x2, -0x1, 0x3f0, 0x1, 0x1, -0x56, 0x3f1, 0x1, 0x1, -0x50, 0x3f2, 0x1, 0x1, 0x7, 0x3f3, 0x1, 0x1, -0x74, 0x3f5, 0x1, 0x1, -0x60, 0x3f8, 0x1, 0x1, -0x1, 0x3fb, 0x1, 0x1, -0x1, 0x430, 0x20, 0x1, -0x20, 0x450, 0x10, 0x1, -0x50, 0x461, 0x11, 0x2, -0x1, 0x48b, 0x1b, 0x2, -0x1, 0x4c2, 0x7, 0x2, -0x1, 0x4cf, 0x1, 0x1, -0xf, 0x4d1, 0x30, 0x2, -0x1, 0x561, 0x26, 0x1, -0x30, 0x10d0, 0x2b, 0x1, 0xbc0, 0x10fd, 0x3, 0x1, 0xbc0, 0x13f8, 0x6, 0x1, -0x8, 0x1c80, 0x1, 0x1, -0x186e, 0x1c81, 0x1, 0x1, -0x186d, 0x1c82, 0x1, 0x1, -0x1864, 0x1c83, 0x2, 0x1, -0x1862, 0x1c85, 0x1, 0x1, -0x1863, 0x1c86, 0x1, 0x1, -0x185c, 0x1c87, 0x1, 0x1, -0x1825, 0x1c88, 0x1, 0x1, 0x89c2, 0x1c8a, 0x1, 0x1, -0x1, 0x1d79, 0x1, 0x1, 0x8a04, 0x1d7d, 0x1, 0x1, 0xee6, 0x1d8e, 0x1, 0x1, 0x8a38, 0x1e01, 0x4b, 0x2, -0x1, 0x1e9b, 0x1, 0x1, -0x3b, 0x1ea1, 0x30, 0x2, -0x1, 0x1f00, 0x8, 0x1, 0x8, 0x1f10, 0x6, 0x1, 0x8, 0x1f20, 0x8, 0x1, 0x8, 0x1f30, 0x8, 0x1, 0x8, 0x1f40, 0x6, 0x1, 0x8, 0x1f51, 0x4, 0x2, 0x8, 0x1f60, 0x8, 0x1, 0x8, 0x1f70, 0x2, 0x1, 0x4a, 0x1f72, 0x4, 0x1, 0x56, 0x1f76, 0x2, 0x1, 0x64, 0x1f78, 0x2, 0x1, 0x80, 0x1f7a, 0x2, 0x1, 0x70, 0x1f7c, 0x2, 0x1, 0x7e, 0x1f80, 0x8, 0x1, 0x8, 0x1f90, 0x8, 0x1, 0x8, 0x1fa0, 0x8, 0x1, 0x8, 0x1fb0, 0x2, 0x1, 0x8, 0x1fb3, 0x1, 0x1, 0x9, 0x1fbe, 0x1, 0x1, -0x1c25, 0x1fc3, 0x1, 0x1, 0x9, 0x1fd0, 0x2, 0x1, 0x8, 0x1fe0, 0x2, 0x1, 0x8, 0x1fe5, 0x1, 0x1, 0x7, 0x1ff3, 0x1, 0x1, 0x9, 0x214e, 0x1, 0x1, -0x1c, 0x2170, 0x10, 0x1, -0x10, 0x2184, 0x1, 0x1, -0x1, 0x24d0, 0x1a, 0x1, -0x1a, 0x2c30, 0x30, 0x1, -0x30, 0x2c61, 0x1, 0x1, -0x1, 0x2c65, 0x1, 0x1, -0x2a2b, 0x2c66, 0x1, 0x1, -0x2a28, 0x2c68, 0x3, 0x2, -0x1, 0x2c73, 0x1, 0x1, -0x1, 0x2c76, 0x1, 0x1, -0x1, 0x2c81, 0x32, 0x2, -0x1, 0x2cec, 0x2, 0x2, -0x1, 0x2cf3, 0x1, 0x1, -0x1, 0x2d00, 0x26, 0x1, -0x1c60, 0x2d27, 0x1, 0x1, -0x1c60, 0x2d2d, 0x1, 0x1, -0x1c60, 0xa641, 0x17, 0x2, -0x1, 0xa681, 0xe, 0x2, -0x1, 0xa723, 0x7, 0x2, -0x1, 0xa733, 0x1f, 0x2, -0x1, 0xa77a, 0x2, 0x2, -0x1, 0xa77f, 0x5, 0x2, -0x1, 0xa78c, 0x1, 0x1, -0x1, 0xa791, 0x2, 0x2, -0x1, 0xa794, 0x1, 0x1, 0x30, 0xa797, 0xa, 0x2, -0x1, 0xa7b5, 0x8, 0x2, -0x1, 0xa7c8, 0x2, 0x2, -0x1, 0xa7cd, 0x8, 0x2, -0x1, 0xa7f6, 0x1, 0x1, -0x1, 0xab53, 0x1, 0x1, -0x3a0, 0xab70, 0x50, 0x1, -0x97d0, 0xff41, 0x1a, 0x1, -0x20, 0x10428, 0x28, 0x1, -0x28, 0x104d8, 0x24, 0x1, -0x28, 0x10597, 0xb, 0x1, -0x27, 0x105a3, 0xf, 0x1, -0x27, 0x105b3, 0x7, 0x1, -0x27, 0x105bb, 0x2, 0x1, -0x27, 0x10cc0, 0x33, 0x1, -0x40, 0x10d70, 0x16, 0x1, -0x20, 0x118c0, 0x20, 0x1, -0x20, 0x16e60, 0x20, 0x1, -0x20, 0x16ebb, 0x19, 0x1, -0x1b, 0x1e922, 0x22, 0x1, -0x22};

/* Cased code points, as (first, last) ranges */
static const long sp_priv_cased[] = {0x41, 0x5a, 0x61, 0x7a, 0xaa, 0xaa, 0xb5, 0xb5, 0xba, 0xba, 0xc0, 0xd6, 0xd8, 0xf6, 0xf8, 0x1ba, 0x1bc, 0x1bf, 0x1c4, 0x293, 0x296, 0x2af, 0x370, 0x373, 0x376, 0x377, 0x37b, 0x37d, 0x37f, 0x37f, 0x386, 0x386, 0x388, 0x38a, 0x38c, 0x38c, 0x38e, 0x3a1, 0x3a3, 0x3f5, 0x3f7, 0x481, 0x48a, 0x52f, 0x531, 0x556, 0x560, 0x588, 0x10a0, 0x10c5, 0x10c7, 0x10c7, 0x10cd, 0x10cd, 0x10d0, 0x10fa, 0x10fd, 0x10ff, 0x13a0, 0x13f5, 0x13f8, 0x13fd, 0x1c80, 0x1c8a, 0x1c90, 0x1cba, 0x1cbd, 0x1cbf, 0x1d00, 0x1d2b, 0x1d6b, 0x1d77, 0x1d79, 0x1d9a, 0x1e00, 0x1f15, 0x1f18, 0x1f1d, 0x1f20, 0x1f45, 0x1f48, 0x1f4d, 0x1f50, 0x1f57, 0x1f59, 0x1f59, 0x1f5b, 0x1f5b, 0x1f5d, 0x1f5d, 0x1f5f, 0x1f7d, 0x1f80, 0x1fb4, 0x1fb6, 0x1fbc, 0x1fbe, 0x1fbe, 0x1fc2, 0x1fc4, 0x1fc6, 0x1fcc, 0x1fd0, 0x1fd3, 0x1fd6, 0x1fdb, 0x1fe0, 0x1fec, 0x1ff2, 0x1ff4, 0x1ff6, 0x1ffc, 0x2102, 0x2102, 0x2107, 0x2107, 0x210a, 0x2113, 0x2115, 0x2115, 0x2119, 0x211d, 0x2124, 0x2124, 0x2126, 0x2126, 0x2128, 0x2128, 0x212a, 0x212d, 0x212f, 0x2134, 0x2139, 0x2139, 0x213c, 0x213f, 0x2145, 0x2149, 0x214e, 0x214e, 0x2160, 0x217f, 0x2183, 0x2184, 0x24b6, 0x24e9, 0x2c00, 0x2c7b, 0x2c7e, 0x2ce4, 0x2ceb, 0x2cee, 0x2cf2, 0x2cf3, 0x2d00, 0x2d25, 0x2d27, 0x2d27, 0x2d2d, 0x2d2d, 0xa640, 0xa66d, 0xa680, 0xa69b, 0xa722, 0xa76f, 0xa771, 0xa787, 0xa78b, 0xa78e, 0xa790, 0xa7dc, 0xa7f5, 0xa7f6, 0xa7fa, 0xa7fa, 0xab30, 0xab5a, 0xab60, 0xab68, 0xab70, 0xabbf, 0xfb00, 0xfb06, 0xfb13, 0xfb17, 0xff21, 0xff3a, 0xff41, 0xff5a, 0x10400, 0x1044f, 0x104b0, 0x104d3, 0x104d8, 0x104fb, 0x10570, 0x1057a, 0x1057c, 0x1058a, 0x1058c, 0x10592, 0x10594, 0x10595, 0x10597, 0x105a1, 0x105a3, 0x105b1, 0x105b3, 0x105b9, 0x105bb, 0x105bc, 0x10c80, 0x10cb2, 0x10cc0, 0x10cf2, 0x10d50, 0x10d65, 0x10d70, 0x10d85, 0x118a0, 0x118df, 0x16e40, 0x16e7f, 0x16ea0, 0x16eb8, 0x16ebb, 0x16ed3, 0x1d400, 0x1d454, 0x1d456, 0x1d49c, 0x1d49e, 0x1d49f, 0x1d4a2, 0x1d4a2, 0x1d4a5, 0x1d4a6, 0x1d4a9, 0x1d4ac, 0x1d4ae, 0x1d4b9, 0x1d4bb, 0x1d4bb, 0x1d4bd, 0x1d4c3, 0x1d4c5, 0x1d505, 0x1d507, 0x1d50a, 0x1d50d, 0x1d514, 0x1d516, 0x1d51c, 0x1d51e, 0x1d539, 0x1d53b, 0x1d53e, 0x1d540, 0x1d544, 0x1d546, 0x1d546, 0x1d54a, 0x1d550, 0x1d552, 0x1d6a5, 0x1d6a8, 0x1d6c0, 0x1d6c2, 0x1d6da, 0x1d6dc, 0x1d6fa, 0x1d6fc, 0x1d714, 0x1d716, 0x1d734, 0x1d736, 0x1d74e, 0x1d750, 0x1d76e, 0x1d770, 0x1d788, 0x1d78a, 0x1d7a8, 0x1d7aa, 0x1d7c2, 0x1d7c4, 0x1d7cb, 0x1df00, 0x1df09, 0x1df0b, 0x1df1e, 0x1df25, 0x1df2a, 0x1e900, 0x1e943, 0x1f130, 0x1f149, 0x1f150, 0x1f169, 0x1f170, 0x1f189};

/* Case-ignorable code points, as (first, last) ranges */
static const long sp_priv_case_ignorable[] = {0x27, 0x27, 0x2e, 0x2e, 0x3a, 0x3a, 0x5e, 0x5e, 0x60, 0x60, 0xa8, 0xa8, 0xad, 0xad, 0xaf, 0xaf, 0xb4, 0xb4, 0xb7, 0xb8, 0x2b0, 0x36f, 0x374, 0x375, 0x37a, 0x37a, 0x384, 0x385, 0x387, 0x387, 0x483, 0x489, 0x559, 0x559, 0x55f, 0x55f, 0x591, 0x5bd, 0x5bf, 0x5bf, 0x5c1, 0x5c2, 0x5c4, 0x5c5, 0x5c7, 0x5c7, 0x5f4, 0x5f4, 0x600, 0x605, 0x610, 0x61a, 0x61c, 0x61c, 0x640, 0x640, 0x64b, 0x65f, 0x670, 0x670, 0x6d6, 0x6dd, 0x6df, 0x6e8, 0x6ea, 0x6ed, 0x70f, 0x70f, 0x711, 0x711, 0x730, 0x74a, 0x7a6, 0x7b0, 0x7eb, 0x7f5, 0x7fa, 0x7fa, 0x7fd, 0x7fd, 0x816, 0x82d, 0x859, 0x85b, 0x888, 0x888, 0x890, 0x891, 0x897, 0x89f, 0x8c9, 0x902, 0x93a, 0x93a, 0x93c, 0x93c, 0x941, 0x948, 0x94d, 0x94d, 0x951, 0x957, 0x962, 0x963, 0x971, 0x971, 0x981, 0x981, 0x9bc, 0x9bc, 0x9c1, 0x9c4, 0x9cd, 0x9cd, 0x9e2, 0x9e3, 0x9fe, 0x9fe, 0xa01, 0xa02, 0xa3c, 0xa3c, 0xa41, 0xa42, 0xa47, 0xa48, 0xa4b, 0xa4d, 0xa51, 0xa51, 0xa70, 0xa71, 0xa75, 0xa75, 0xa81, 0xa82, 0xabc, 0xabc, 0xac1, 0xac5, 0xac7, 0xac8, 0xacd, 0xacd, 0xae2, 0xae3, 0xafa, 0xaff, 0xb01, 0xb01, 0xb3c, 0xb3c, 0xb3f, 0xb3f, 0xb41, 0xb44, 0xb4d, 0xb4d, 0xb55, 0xb56, 0xb62, 0xb63, 0xb82, 0xb82, 0xbc0, 0xbc0, 0xbcd, 0xbcd, 0xc00, 0xc00, 0xc04, 0xc04, 0xc3c, 0xc3c, 0xc3e, 0xc40, 0xc46, 0xc48, 0xc4a, 0xc4d, 0xc55, 0xc56, 0xc62, 0xc63, 0xc81, 0xc81, 0xcbc, 0xcbc, 0xcbf, 0xcbf, 0xcc6, 0xcc6, 0xccc, 0xccd, 0xce2, 0xce3, 0xd00, 0xd01, 0xd3b, 0xd3c, 0xd41, 0xd44, 0xd4d, 0xd4d, 0xd62, 0xd63, 0xd81, 0xd81, 0xdca, 0xdca, 0xdd2, 0xdd4, 0xdd6, 0xdd6, 0xe31, 0xe31, 0xe34, 0xe3a, 0xe46, 0xe4e, 0xeb1, 0xeb1, 0xeb4, 0xebc, 0xec6, 0xec6, 0xec8, 0xece, 0xf18, 0xf19, 0xf35, 0xf35, 0xf37, 0xf37, 0xf39, 0xf39, 0xf71, 0xf7e, 0xf80, 0xf84, 0xf86, 0xf87, 0xf8d, 0xf97, 0xf99, 0xfbc, 0xfc6, 0xfc6, 0x102d, 0x1030, 0x1032, 0x1037, 0x1039, 0x103a, 0x103d, 0x103e, 0x1058, 0x1059, 0x105e, 0x1060, 0x1071, 0x1074, 0x1082, 0x1082, 0x1085, 0x1086, 0x108d, 0x108d, 0x109d, 0x109d, 0x10fc, 0x10fc, 0x135d, 0x135f, 0x1712, 0x1714, 0x1732, 0x1733, 0x1752, 0x1753, 0x1772, 0x1773, 0x17b4, 0x17b5, 0x17b7, 0x17bd, 0x17c6, 0x17c6, 0x17c9, 0x17d3, 0x17d7, 0x17d7, 0x17dd, 0x17dd, 0x180b, 0x180f, 0x1843, 0x1843, 0x1885, 0x1886, 0x18a9, 0x18a9, 0x1920, 0x1922, 0x1927, 0x1928, 0x1932, 0x1932, 0x1939, 0x193b, 0x1a17, 0x1a18, 0x1a1b, 0x1a1b, 0x1a56, 0x1a56, 0x1a58, 0x1a5e, 0x1a60, 0x1a60, 0x1a62, 0x1a62, 0x1a65, 0x1a6c, 0x1a73, 0x1a7c, 0x1a7f, 0x1a7f, 0x1aa7, 0x1aa7, 0x1ab0, 0x1add, 0x1ae0, 0x1aeb, 0x1b00, 0x1b03, 0x1b34, 0x1b34, 0x1b36, 0x1b3a, 0x1b3c, 0x1b3c, 0x1b42, 0x1b42, 0x1b6b, 0x1b73, 0x1b80, 0x1b81, 0x1ba2, 0x1ba5, 0x1ba8, 0x1ba9, 0x1bab, 0x1bad, 0x1be6, 0x1be6, 0x1be8, 0x1be9, 0x1bed, 0x1bed, 0x1bef, 0x1bf1, 0x1c2c, 0x1c33, 0x1c36, 0x1c37, 0x1c78, 0x1c7d, 0x1cd0, 0x1cd2, 0x1cd4, 0x1ce0, 0x1ce2, 0x1ce8, 0x1ced, 0x1ced, 0x1cf4, 0x1cf4, 0x1cf8, 0x1cf9, 0x1d2c, 0x1d6a, 0x1d78, 0x1d78, 0x1d9b, 0x1dff, 0x1fbd, 0x1fbd, 0x1fbf, 0x1fc1, 0x1fcd, 0x1fcf, 0x1fdd, 0x1fdf, 0x1fed, 0x1fef, 0x1ffd, 0x1ffe, 0x200b, 0x200f, 0x2018, 0x2019, 0x2024, 0x2024, 0x2027, 0x2027, 0x202a, 0x202e, 0x2060, 0x2064, 0x2066, 0x206f, 0x2071, 0x2071, 0x207f, 0x207f, 0x2090, 0x209c, 0x20d0, 0x20f0, 0x2c7c, 0x2c7d, 0x2cef, 0x2cf1, 0x2d6f, 0x2d6f, 0x2d7f, 0x2d7f, 0x2de0, 0x2dff, 0x2e2f, 0x2e2f, 0x3005, 0x3005, 0x302a, 0x302d, 0x3031, 0x3035, 0x303b, 0x303b, 0x3099, 0x309e, 0x30fc, 0x30fe, 0xa015, 0xa015, 0xa4f8, 0xa4fd, 0xa60c, 0xa60c, 0xa66f, 0xa672, 0xa674, 0xa67d, 0xa67f, 0xa67f, 0xa69c, 0xa69f, 0xa6f0, 0xa6f1, 0xa700, 0xa721, 0xa770, 0xa770, 0xa788, 0xa78a, 0xa7f1, 0xa7f4, 0xa7f8, 0xa7f9, 0xa802, 0xa802, 0xa806, 0xa806, 0xa80b, 0xa80b, 0xa825, 0xa826, 0xa82c, 0xa82c, 0xa8c4, 0xa8c5, 0xa8e0, 0xa8f1, 0xa8ff, 0xa8ff, 0xa926, 0xa92d, 0xa947, 0xa951, 0xa980, 0xa982, 0xa9b3, 0xa9b3, 0xa9b6, 0xa9b9, 0xa9bc, 0xa9bd, 0xa9cf, 0xa9cf, 0xa9e5, 0xa9e6, 0xaa29, 0xaa2e, 0xaa31, 0xaa32, 0xaa35, 0xaa36, 0xaa43, 0xaa43, 0xaa4c, 0xaa4c, 0xaa70, 0xaa70, 0xaa7c, 0xaa7c, 0xaab0, 0xaab0, 0xaab2, 0xaab4, 0xaab7, 0xaab8, 0xaabe, 0xaabf, 0xaac1, 0xaac1, 0xaadd, 0xaadd, 0xaaec, 0xaaed, 0xaaf3, 0xaaf4, 0xaaf6, 0xaaf6, 0xab5b, 0xab5f, 0xab69, 0xab6b, 0xabe5, 0xabe5, 0xabe8, 0xabe8, 0xabed, 0xabed, 0xfb1e, 0xfb1e, 0xfbb2, 0xfbc2, 0xfe00, 0xfe0f, 0xfe13, 0xfe13, 0xfe20, 0xfe2f, 0xfe52, 0xfe52, 0xfe55, 0xfe55, 0xfeff, 0xfeff, 0xff07, 0xff07, 0xff0e, 0xff0e, 0xff1a, 0xff1a, 0xff3e, 0xff3e, 0xff40, 0xff40, 0xff70, 0xff70, 0xff9e, 0xff9f, 0xffe3, 0xffe3, 0xfff9, 0xfffb, 0x101fd, 0x101fd, 0x102e0, 0x102e0, 0x10376, 0x1037a, 0x10780, 0x10785, 0x10787, 0x107b0, 0x107b2, 0x107ba, 0x10a01, 0x10a03, 0x10a05, 0x10a06, 0x10a0c, 0x10a0f, 0x10a38, 0x10a3a, 0x10a3f, 0x10a3f, 0x10ae5, 0x10ae6, 0x10d24, 0x10d27, 0x10d4e, 0x10d4e, 0x10d69, 0x10d6d, 0x10d6f, 0x10d6f, 0x10eab, 0x10eac, 0x10ec5, 0x10ec5, 0x10efa, 0x10eff, 0x10f46, 0x10f50, 0x10f82, 0x10f85, 0x11001, 0x11001, 0x11038, 0x11046, 0x11070, 0x11070, 0x11073, 0x11074, 0x1107f, 0x11081, 0x110b3, 0x110b6, 0x110b9, 0x110ba, 0x110bd, 0x110bd, 0x110c2, 0x110c2, 0x110cd, 0x110cd, 0x11100, 0x11102, 0x11127, 0x1112b, 0x1112d, 0x11134, 0x11173, 0x11173, 0x11180, 0x11181, 0x111b6, 0x111be, 0x111c9, 0x111cc, 0x111cf, 0x111cf, 0x1122f, 0x11231, 0x11234, 0x11234, 0x11236, 0x11237, 0x1123e, 0x1123e, 0x11241, 0x11241, 0x112df, 0x112df, 0x112e3, 0x112ea, 0x11300, 0x11301, 0x1133b, 0x1133c, 0x11340, 0x11340, 0x11366, 0x1136c, 0x11370, 0x11374, 0x113bb, 0x113c0, 0x113ce, 0x113ce, 0x113d0, 0x113d0, 0x113d2, 0x113d2, 0x113e1, 0x113e2, 0x11438, 0x1143f, 0x11442, 0x11444, 0x11446, 0x11446, 0x1145e, 0x1145e, 0x114b3, 0x114b8, 0x114ba, 0x114ba, 0x114bf, 0x114c0, 0x114c2, 0x114c3, 0x115b2, 0x115b5, 0x115bc, 0x115bd, 0x115bf, 0x115c0, 0x115dc, 0x115dd, 0x11633, 0x1163a, 0x1163d, 0x1163d, 0x1163f, 0x11640, 0x116ab, 0x116ab, 0x116ad, 0x116ad, 0x116b0, 0x116b5, 0x116b7, 0x116b7, 0x1171d, 0x1171d, 0x1171f, 0x1171f, 0x11722, 0x11725, 0x11727, 0x1172b, 0x1182f, 0x11837, 0x11839, 0x1183a, 0x1193b, 0x1193c, 0x1193e, 0x1193e, 0x11943, 0x11943, 0x119d4, 0x119d7, 0x119da, 0x119db, 0x119e0, 0x119e0, 0x11a01, 0x11a0a, 0x11a33, 0x11a38, 0x11a3b, 0x11a3e, 0x11a47, 0x11a47, 0x11a51, 0x11a56, 0x11a59, 0x11a5b, 0x11a8a, 0x11a96, 0x11a98, 0x11a99, 0x11b60, 0x11b60, 0x11b62, 0x11b64, 0x11b66, 0x11b66, 0x11c30, 0x11c36, 0x11c38, 0x11c3d, 0x11c3f, 0x11c3f, 0x11c92, 0x11ca7, 0x11caa, 0x11cb0, 0x11cb2, 0x11cb3, 0x11cb5, 0x11cb6, 0x11d31, 0x11d36, 0x11d3a, 0x11d3a, 0x11d3c, 0x11d3d, 0x11d3f, 0x11d45, 0x11d47, 0x11d47, 0x11d90, 0x11d91, 0x11d95, 0x11d95, 0x11d97, 0x11d97, 0x11dd9, 0x11dd9, 0x11ef3, 0x11ef4, 0x11f00, 0x11f01, 0x11f36, 0x11f3a, 0x11f40, 0x11f40, 0x11f42, 0x11f42, 0x11f5a, 0x11f5a, 0x13430, 0x13440, 0x13447, 0x13455, 0x1611e, 0x16129, 0x1612d, 0x1612f, 0x16af0, 0x16af4, 0x16b30, 0x16b36, 0x16b40, 0x16b43, 0x16d40, 0x16d42, 0x16d6b, 0x16d6c, 0x16f4f, 0x16f4f, 0x16f8f, 0x16f9f, 0x16fe0, 0x16fe1, 0x16fe3, 0x16fe4, 0x16ff2, 0x16ff3, 0x1aff0, 0x1aff3, 0x1aff5, 0x1affb, 0x1affd, 0x1affe, 0x1bc9d, 0x1bc9e, 0x1bca0, 0x1bca3, 0x1cf00, 0x1cf2d, 0x1cf30, 0x1cf46, 0x1d167, 0x1d169, 0x1d173, 0x1d182, 0x1d185, 0x1d18b, 0x1d1aa, 0x1d1ad, 0x1d242, 0x1d244, 0x1da00, 0x1da36, 0x1da3b, 0x1da6c, 0x1da75, 0x1da75, 0x1da84, 0x1da84, 0x1da9b, 0x1da9f, 0x1daa1, 0x1daaf, 0x1e000, 0x1e006, 0x1e008, 0x1e018, 0x1e01b, 0x1e021, 0x1e023, 0x1e024, 0x1e026, 0x1e02a, 0x1e030, 0x1e06d, 0x1e08f, 0x1e08f, 0x1e130, 0x1e13d, 0x1e2ae, 0x1e2ae, 0x1e2ec, 0x1e2ef, 0x1e4eb, 0x1e4ef, 0x1e5ee, 0x1e5ef, 0x1e6e3, 0x1e6e3, 0x1e6e6, 0x1e6e6, 0x1e6ee, 0x1e6ef, 0x1e6f5, 0x1e6f5, 0x1e6ff, 0x1e6ff, 0x1e8d0, 0x1e8d6, 0x1e944, 0x1e94b, 0x1f3fb, 0x1f3ff, 0xe0001, 0xe0001, 0xe0020, 0xe007f, 0xe0100, 0xe01ef};

/* re's IGNORECASE equivalences beyond simple lowercase: the lowercase code points in a case group, sorted, each with
 * its group's first */
static const long sp_priv_case_members[] = {0x69, 0x69, 0x73, 0x73, 0xb5, 0xb5, 0x131, 0x69, 0x17f, 0x73, 0x345, 0x345, 0x390, 0x390, 0x3b0, 0x3b0, 0x3b2, 0x3b2, 0x3b5, 0x3b5, 0x3b8, 0x3b8, 0x3b9, 0x345, 0x3ba, 0x3ba, 0x3bc, 0xb5, 0x3c0, 0x3c0, 0x3c1, 0x3c1, 0x3c2, 0x3c2, 0x3c3, 0x3c2, 0x3c6, 0x3c6, 0x3d0, 0x3b2, 0x3d1, 0x3b8, 0x3d5, 0x3c6, 0x3d6, 0x3c0, 0x3f0, 0x3ba, 0x3f1, 0x3c1, 0x3f5, 0x3b5, 0x432, 0x432, 0x434, 0x434, 0x43e, 0x43e, 0x441, 0x441, 0x442, 0x442, 0x44a, 0x44a, 0x463, 0x463, 0x1c80, 0x432, 0x1c81, 0x434, 0x1c82, 0x43e, 0x1c83, 0x441, 0x1c84, 0x442, 0x1c85, 0x442, 0x1c86, 0x44a, 0x1c87, 0x463, 0x1c88, 0x1c88, 0x1e61, 0x1e61, 0x1e9b, 0x1e61, 0x1fbe, 0x345, 0x1fd3, 0x390, 0x1fe3, 0x3b0, 0xa64b, 0x1c88, 0xfb05, 0xfb05, 0xfb06, 0xfb05};
/* clang-format on */

/* In a sorted table of entries `width` longs wide, each starting with its first code point: the number of entries
 * starting at or before cp (so the entry cp may be in is table + (count - 1) * width) */
static size_t sp_priv_entries_before(unsigned long cp, const long *table, size_t width, size_t n) {
    size_t lo = 0;
    size_t hi = n / width;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (SP_PRIV_CAST(unsigned long, table[mid * width]) <= cp)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

/* The simple case mapping of cp, from runs of (first code point, count, stride, delta) */
static unsigned long sp_priv_case_map(unsigned long cp, const long *runs, size_t n) {
    size_t at = sp_priv_entries_before(cp, runs, 4, n);
    if (at == 0)
        return cp;

    const long *run = runs + (at - 1) * 4;
    unsigned long offset = cp - SP_PRIV_CAST(unsigned long, run[0]);
    unsigned long stride = SP_PRIV_CAST(unsigned long, run[2]);
    if (offset % stride != 0 || offset / stride >= SP_PRIV_CAST(unsigned long, run[1]))
        return cp;
    return SP_PRIV_CAST(unsigned long, SP_PRIV_CAST(long, cp) + run[3]);
}

/* Whether cp lies in one of the (first, last) ranges */
static bool sp_priv_in_ranges(unsigned long cp, const long *ranges, size_t n) {
    size_t at = sp_priv_entries_before(cp, ranges, 2, n);
    return at > 0 && cp <= SP_PRIV_CAST(unsigned long, ranges[at * 2 - 1]);
}

/* The first code point of the case group holding cp, or 0 when no group holds it */
static unsigned long sp_priv_case_group(unsigned long cp) {
    size_t at = sp_priv_entries_before(cp, sp_priv_case_members, 2, SP_ARRAY_LEN(sp_priv_case_members));
    if (at == 0 || SP_PRIV_CAST(unsigned long, sp_priv_case_members[at * 2 - 2]) != cp)
        return 0;
    return SP_PRIV_CAST(unsigned long, sp_priv_case_members[at * 2 - 1]);
}

/* Whether two lowercase code points are the same letter to re's IGNORECASE (equal, or in one of its case groups) */
static bool sp_priv_case_equiv(unsigned long a, unsigned long b) {
    unsigned long group = sp_priv_case_group(a);
    return a == b || (group != 0 && group == sp_priv_case_group(b));
}

/* Whether some code point in [lo, hi] lowercases to cp: cp itself when it is its own lowercase, or one in the runs that
 * [lo, hi] overlaps (sorted and disjoint: from the run lo falls in or follows to the last one starting by hi) */
static bool sp_priv_lowered_from(unsigned long cp, unsigned long lo, unsigned long hi) {
    const long *runs = sp_priv_lower_runs;
    size_t n = SP_ARRAY_LEN(sp_priv_lower_runs);
    if (cp >= lo && cp <= hi && sp_priv_case_map(cp, runs, n) == cp)
        return true;

    size_t from = sp_priv_entries_before(lo, runs, 4, n);
    size_t to = sp_priv_entries_before(hi, runs, 4, n);
    for (size_t i = from > 0 ? from - 1 : 0; i < to; i++) {
        const long *run = runs + i * 4;
        unsigned long first = SP_PRIV_CAST(unsigned long, run[0]);
        unsigned long stride = SP_PRIV_CAST(unsigned long, run[2]);
        unsigned long last = first + (SP_PRIV_CAST(unsigned long, run[1]) - 1) * stride;
        unsigned long x = SP_PRIV_CAST(unsigned long, SP_PRIV_CAST(long, cp) - run[3]);
        if (x >= first && x <= last && (x - first) % stride == 0 && x >= lo && x <= hi)
            return true;
    }
    return false;
}

/* The next code point of str.lower() of s[0..len) from *pos: simple lowercase, except that U+0130 lowers to i and a
 * combining dot (left in *owed for the next call), and U+03A3 to a final sigma after a cased letter with no cased
 * letter following (case-ignorable letters in between don't count) */
static unsigned long sp_priv_lower_next(const char *s, size_t len, size_t *pos, unsigned long *owed) {
    unsigned long c = *owed;
    size_t start = *pos;
    *owed = 0;
    if (c != 0)
        return c;

    c = sp_priv_utf8_next(s, len, pos);
    if (c == 0x130)
        *owed = 0x307;
    if (c != 0x3a3)
        return c == 0x130 ? 'i' : sp_priv_case_map(c, sp_priv_lower_runs, SP_ARRAY_LEN(sp_priv_lower_runs));

    /* The nearest letters around it that aren't case-ignorable: cased before, not cased after */
    bool cased_before = false;
    for (size_t j = start; j > 0;) {
        size_t q = j - 1;
        while (q > 0 && j - q < 4 && (SP_PRIV_CAST(unsigned char, s[q]) & 0xC0) == 0x80)
            q--;
        size_t t = q;
        unsigned long before = sp_priv_utf8_next(s, j, &t);
        if (t != j) {
            q = j - 1;
            t = q;
            before = sp_priv_utf8_next(s, j, &t);
        }
        j = q;
        if (!sp_priv_in_ranges(before, sp_priv_case_ignorable, SP_ARRAY_LEN(sp_priv_case_ignorable))) {
            cased_before = sp_priv_in_ranges(before, sp_priv_cased, SP_ARRAY_LEN(sp_priv_cased));
            break;
        }
    }

    bool cased_after = false;
    for (size_t t = *pos; t < len;) {
        unsigned long after = sp_priv_utf8_next(s, len, &t);
        if (!sp_priv_in_ranges(after, sp_priv_case_ignorable, SP_ARRAY_LEN(sp_priv_case_ignorable))) {
            cased_after = sp_priv_in_ranges(after, sp_priv_cased, SP_ARRAY_LEN(sp_priv_cased));
            break;
        }
    }
    return cased_before && !cased_after ? 0x3c2 : 0x3c3;
}

/* How CPython orders two strings: by the code points of their str.lower() when lower (Windows paths), else by byte */
static int sp_priv_text_cmp(const char *a, size_t alen, const char *b, size_t blen, bool lower) {
    size_t i = 0;
    size_t j = 0;
    unsigned long owed_a = 0;
    unsigned long owed_b = 0;

    while ((i < alen || owed_a != 0) && (j < blen || owed_b != 0)) {
        unsigned long ca = lower ? sp_priv_lower_next(a, alen, &i, &owed_a) : SP_PRIV_CAST(unsigned char, a[i++]);
        unsigned long cb = lower ? sp_priv_lower_next(b, blen, &j, &owed_b) : SP_PRIV_CAST(unsigned char, b[j++]);
        if (ca != cb)
            return ca < cb ? -1 : 1;
    }

    bool a_left = i < alen || owed_a != 0;
    bool b_left = j < blen || owed_b != 0;
    return a_left == b_left ? 0 : a_left ? 1 : -1;
}

/* The end of the part at s[i..len): its next separator ('/' or sep), or len */
static size_t sp_priv_part_end(const char *s, size_t len, size_t i, char sep) {
    while (i < len && s[i] != '/' && s[i] != sep)
        i++;
    return i;
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

/* The empty path, carrying an error (SP_OK for a plain empty path) */
static SpPath sp_priv_error_path(SpFlavor flavor, SpError error) {
    SpPath p = SP_PRIV_ZERO;
    p.flavor = flavor;
    p.error = error;
    return p;
}

/* Append one piece, optionally separated; a path that can't hold it becomes an SP_ERR_TOO_LONG error, and a path
 * with an error stays as it is */
static void sp_priv_append(SpPath *r, const char *s, size_t len, bool separated) {
    char sep = r->flavor == SP_FLAVOR_WINDOWS ? '\\' : '/';
    bool add_sep = separated && r->len > 0 && r->buf[r->len - 1] != '/' && r->buf[r->len - 1] != sep;
    if (r->error != SP_OK)
        return;
    if (r->len + (add_sep ? 1 : 0) + len >= SP_PATH_MAX) {
        *r = sp_priv_error_path(r->flavor, SP_ERR_TOO_LONG);
        return;
    }

    if (add_sep)
        r->buf[r->len++] = sep;
    memcpy(r->buf + r->len, s, len);
    r->len += len;
    r->buf[r->len] = '\0';
}

static SpPath sp_priv_path_from_raw(const char *s, size_t len, SpFlavor flavor) {
    if (len >= SP_PATH_MAX)
        return sp_priv_error_path(flavor, SP_ERR_TOO_LONG);

    SpPath p = sp_priv_error_path(flavor, SP_OK);
    p.len = len;
    if (len > 0)
        memcpy(p.buf, s, len);
    return p;
}

/* p cut to its first len bytes, at or past its anchor, so the anchor stays right */
static SpPath sp_priv_prefix(const SpPath *p, size_t len) {
    SpPath r = *p;
    r.len = len;
    r.buf[len] = '\0';
    return r;
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
        size_t server = sp_priv_part_end(s, len, start, '\\');
        size_t share = server < len ? sp_priv_part_end(s, len, server + 1, '\\') : len;

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

/* Splits the anchor into the path's fields, then canonical separators and no repeated separators or '.' parts; a
 * complete UNC drive gets its implicit root. A leading '.' stays to protect a drive-like next part from drive parsing
 * ('./c:a' stays '.\c:a', but 'a/./c:a' becomes 'a\c:a'). */
static void sp_priv_normalize(SpPath *p) {
    p->anchor = sp_priv_split_anchor(p->buf, p->len, p->flavor, &p->drive);
    char *buf = p->buf;
    char sep = p->flavor == SP_FLAVOR_WINDOWS ? '\\' : '/';
    size_t len = p->len;
    size_t j = p->anchor < len ? p->anchor : len;

    for (size_t i = 0; i < j; i++)
        if (buf[i] == '/')
            buf[i] = sep;
    if (p->anchor > len && j + 1 >= SP_PATH_MAX) {
        *p = sp_priv_error_path(p->flavor, SP_ERR_TOO_LONG);
        return;
    }
    if (p->anchor > len)
        buf[j++] = sep;

    for (size_t i = j, end; i < len; i = end) {
        while (i < len && (buf[i] == '/' || buf[i] == sep))
            i++;
        end = sp_priv_part_end(buf, len, i, sep);

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

    if (j > p->anchor && (buf[j - 1] == '/' || buf[j - 1] == sep))
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
    sp_priv_normalize(&p);
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
    sp_priv_normalize(&dest);
    return dest;
}

/* "" for a path with an error, which release builds (where it doesn't assert) must not mistake for "." */
const char *sp_str(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    if (p->error != SP_OK)
        return "";
    return p->len == 0 ? "." : p->buf;
}

SpTerm sp_as_posix(const SpPath *p) {
    SpTerm t = sp_priv_term(p->len > 0 ? p->buf : ".", p->len > 0 ? p->len : 1, p->error);
    for (size_t i = 0; i < t.len; i++)
        if (t.buf[i] == '\\' && p->flavor == SP_FLAVOR_WINDOWS)
            t.buf[i] = '/';
    return t;
}

SpTerm sp_drive(const SpPath *p) { return sp_priv_term(p->buf, p->drive, p->error); }

SpTerm sp_root(const SpPath *p) { return sp_priv_term(p->buf + p->drive, p->anchor - p->drive, p->error); }

SpTerm sp_anchor(const SpPath *p) { return sp_priv_term(p->buf, p->anchor, p->error); }

/* The name of p as a view into its buffer */
static SpStr sp_priv_name_sv(const SpPath *p) {
    if (p->anchor >= p->len)
        return SP_PRIV_STR(p->buf + p->len, 0);

    char sep = p->flavor == SP_FLAVOR_WINDOWS ? '\\' : '/';
    size_t i = p->len;
    while (i > p->anchor && p->buf[i - 1] != '/' && p->buf[i - 1] != sep)
        i--;
    return SP_PRIV_STR(p->buf + i, p->len - i);
}

SpTerm sp_name(const SpPath *p) {
    SpStr sv = sp_priv_name_sv(p);
    return sp_priv_term(sv.data, sv.len, p->error);
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
    SpStr sv = sp_priv_suffix_sv(sp_priv_name_sv(p));
    return sp_priv_term(sv.data, sv.len, p->error);
}

SpTerm sp_stem(const SpPath *p) {
    SpStr name = sp_priv_name_sv(p);
    return sp_priv_term(name.data, name.len - sp_priv_suffix_sv(name).len, p->error);
}

SpSuffixes sp_suffixes(const SpPath *p) {
    SpSuffixes r = SP_PRIV_ZERO;
    SpStr name = sp_priv_name_sv(p);
    r.error = p->error;

    /* Each '.' after the leading dots starts a suffix, even an empty one ("a..b" -> ".", ".b") */
    size_t i = 0;
    while (i < name.len && name.data[i] == '.')
        i++;
    while (i < name.len && name.data[i] != '.')
        i++;
    while (i < name.len) {
        size_t end = i + 1;
        while (end < name.len && name.data[end] != '.')
            end++;
        if (r.count == SP_MAX_SUFFIXES) {
            r.count = 0;
            r.error = SP_ERR_LIMIT;
            return r;
        }
        r.items[r.count++] = SP_PRIV_STR(name.data + i, end - i);
        i = end;
    }
    return r;
}

SpPath sp_parent(const SpPath *p) {
    if (p->error != SP_OK)
        return *p;

    SP_ASSERT_PATH_INVARIANT(p);
    return sp_priv_prefix(p, sp_priv_parent_len(p->buf, p->len, p->flavor, p->anchor));
}

SpPartsIter sp_parts_begin(const SpPath *p) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpPartsIter it = SP_PRIV_ZERO;
    it.path = p;
    it.anchor = p->anchor;
    it.end = p->len;

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
        end = sp_priv_part_end(buf, it->end, start, sep);
    }
    it->pos = end;

    if (end <= start)
        return false;
    *out = SP_PRIV_STR(buf + start, end - start);
    return true;
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
    size_t next = sp_priv_parent_len(p->buf, it->current_len, p->flavor, p->anchor);
    if (next == it->current_len)
        return false;

    it->current_len = next;
    *out = sp_priv_prefix(p, next);
    return true;
}

/* Internal length-aware join - handles embedded nulls correctly */
static SpPath sp_priv_join_len(const SpPath *base, const char *other, size_t olen) {
    SpFlavor flavor = base->flavor;
    /* ntpath.join puts no separator after a rootless drive ending in ':' (like "c:") */
    bool add_sep = !(base->drive > 0 && base->anchor == base->drive && base->drive == base->len &&
                     base->buf[base->drive - 1] == ':');
    bool replace = false; /* by an anchored other */
    SpPath r = *base;

    if (olen > 0 && (other[0] == '/' || (flavor == SP_FLAVOR_WINDOWS && other[0] == '\\'))) {
        replace = base->drive == 0 || sp_priv_is_unc(other, olen, flavor);
        r.len = base->drive; /* Root only: keep the base drive. */
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
    sp_priv_normalize(&r);
    return r;
}

SpPath sp_join_one(const SpPath *base, const char *other) { return sp_join_n(base, other, strlen(other)); }

SpPath sp_join_n(const SpPath *base, const char *s, size_t len) {
    if (base->error != SP_OK || len == 0)
        return *base;

    SP_ASSERT_PATH_INVARIANT(base);
    return sp_priv_join_len(base, s, len);
}

SpPath sp_join_impl(const SpPath *base, const char **parts, size_t count) {
    SpPath r = *base;
    for (size_t i = 0; r.error == SP_OK && i < count; i++)
        if (parts[i][0])
            r = sp_priv_join_len(&r, parts[i], strlen(parts[i]));
    return r;
}

SpPath sp_with_segments(const SpPath *p, const char **parts, size_t parts_count) {
    SpPath empty = sp_priv_error_path(p->flavor, SP_OK);
    return sp_join_impl(&empty, parts, parts_count);
}

/* with_name(head + tail): the parent plus a name that must be non-empty, not "." and free of separators, in the order
 * CPython checks: p must have a name, and a tail (a suffix) starts with '.' and needs a head (a stem) before it */
static SpPath sp_priv_with_name_parts(const SpPath *p, SpStr head, SpStr tail) {
    if (p->error != SP_OK)
        return *p;

    SP_ASSERT_PATH_INVARIANT(p);
    if (sp_priv_name_sv(p).len == 0)
        return sp_priv_error_path(p->flavor, SP_ERR_NO_NAME);
    if (tail.len > 0 && (tail.data[0] != '.' || head.len == 0))
        return sp_priv_error_path(p->flavor, SP_ERR_INVALID_ARG);
    if (head.len + tail.len >= SP_PATH_MAX)
        return sp_priv_error_path(p->flavor, SP_ERR_TOO_LONG);

    char name[SP_PATH_MAX];
    size_t len = head.len + tail.len;
    if (head.len > 0)
        memcpy(name, head.data, head.len);
    if (tail.len > 0)
        memcpy(name + head.len, tail.data, tail.len);
    if (len == 0 || (len == 1 && name[0] == '.') || memchr(name, '/', len) ||
        (p->flavor == SP_FLAVOR_WINDOWS && memchr(name, '\\', len)))
        return sp_priv_error_path(p->flavor, SP_ERR_INVALID_ARG);

    SpPath r = sp_priv_prefix(p, sp_priv_parent_len(p->buf, p->len, p->flavor, p->anchor));
    if (r.len == 0 && sp_priv_drive_len(name, len, p->flavor) > 0)
        r.buf[r.len++] = '.'; /* keep "c:" from parsing as a drive */

    /* Past the anchor the name follows a separator; a bare drive takes it directly ("c:x" -> "c:y") */
    sp_priv_append(&r, name, len, r.len > p->anchor);
    return r;
}

SpPath sp_with_name(const SpPath *p, const char *name) {
    return sp_priv_with_name_parts(p, SP_PRIV_STR(name, strlen(name)), SP_PRIV_STR(SP_PRIV_NULL, 0));
}

SpPath sp_with_stem(const SpPath *p, const char *stem) {
    return sp_priv_with_name_parts(p, SP_PRIV_STR(stem, strlen(stem)), sp_priv_suffix_sv(sp_priv_name_sv(p)));
}

SpPath sp_with_suffix(const SpPath *p, const char *suffix) {
    SpStr name = sp_priv_name_sv(p);
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

/* os.getcwd() as a path */
SpPath sp_cwd(SpFlavor flavor) {
    flavor = sp_priv_flavor(flavor);
    SpPath p = sp_priv_error_path(flavor, SP_OK);
#ifdef SP_WINDOWS
    wchar_t wide[SP_PATH_MAX];
    DWORD n = GetCurrentDirectoryW(SP_PATH_MAX, wide);
    if (n == 0)
        p.error = sp_priv_last_error();
    else
        p.error = n < SP_PATH_MAX ? sp_priv_from_wide(wide, n, p.buf, SP_PATH_MAX, &p.len) : SP_ERR_TOO_LONG;
#else
    if (getcwd(p.buf, SP_PATH_MAX))
        p.len = strlen(p.buf);
    else
        p.error = sp_priv_last_error();
#endif
    if (p.error != SP_OK)
        return sp_priv_error_path(flavor, p.error);

    sp_priv_normalize(&p);
    return p;
}

/* A relative path joined to the current directory */
SpPath sp_absolute(const SpPath *p) {
    if (p->error != SP_OK || sp_is_absolute(p))
        return *p;

    SpPath cwd = sp_cwd(p->flavor);
    return cwd.error != SP_OK ? cwd : sp_priv_join_len(&cwd, p->buf, p->len);
}

/* The length of p's prefix that equals other (CPython: other == p or other in p.parents, comparing str.lower() on
 * Windows), or (size_t)-1 when there is none */
static size_t sp_priv_relative_len(const SpPath *p, const SpPath *other) {
    for (size_t len = p->len;;) {
        if (sp_priv_text_cmp(p->buf, len, other->buf, other->len, p->flavor == SP_FLAVOR_WINDOWS) == 0)
            return len;

        size_t parent = sp_priv_parent_len(p->buf, len, p->flavor, p->anchor);
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
    if (p->error != SP_OK || other->error != SP_OK)
        return p->error != SP_OK ? *p : *other;

    SpPath base = *other;
    SpPath r = sp_priv_error_path(p->flavor, SP_OK);
    size_t skip; /* the length of p's prefix that equals base */

    while ((skip = sp_priv_relative_len(p, &base)) == SP_PRIV_CAST(size_t, -1)) {
        SpStr name = sp_priv_name_sv(&base);
        size_t parent = sp_priv_parent_len(base.buf, base.len, base.flavor, base.anchor);
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
        if (r.error == SP_OK && r.len == 0 && sp_priv_drive_len(part.data, part.len, p->flavor) > 0)
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
SpError sp_as_uri(const SpPath *p, char *buf, size_t buf_size) {
    if (buf_size > 0)
        buf[0] = '\0';
    if (p->error != SP_OK)
        return p->error;
    if (!sp_is_absolute(p))
        return SP_ERR_NOT_ABSOLUTE;

    SpTerm posix = sp_as_posix(p);
    const char *path = posix.buf;
    size_t len = posix.len;
    size_t drive = p->drive; /* as_posix keeps every offset */

    const char *d = path;
    const char *prefix = "file://"; /* an explicitly empty authority before a POSIX root */
    if (drive > 0) {
        prefix = "file:";
        /* Device paths drop their "//?/" prefix, keeping "//" for "//?/UNC/server/share" */
        if (drive >= 4 && memcmp(d, "//?/", 4) == 0) {
            d += 4;
            drive -= 4;
            if (drive >= 4 && sp_priv_text_cmp(d, 4, "unc/", 4, true) == 0) {
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
        return SP_OK;
    if (buf_size > 0)
        buf[0] = '\0';
    return SP_ERR_TOO_LONG;
}

static bool sp_priv_is_local_authority(const char *a, size_t len) {
    if (len == 0 || (len == 9 && memcmp(a, "localhost", 9) == 0))
        return true;

    char host[256 * 3];
#ifdef SP_WINDOWS
    wchar_t wide[256];
    DWORD size = SP_ARRAY_LEN(wide);
    size_t host_len;
    if (!GetComputerNameExW(ComputerNamePhysicalDnsHostname, wide, &size) ||
        sp_priv_from_wide(wide, size, host, sizeof(host), &host_len) != SP_OK)
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

/* Append s[0..len) to buf (of SP_PATH_MAX), decoding %XX escapes (urllib.parse.unquote); when it doesn't fit, *n
 * becomes SP_PATH_MAX, which no path can be */
static void sp_priv_unquote_append(char *buf, size_t *n, const char *s, size_t len) {
    size_t i = 0;
    for (; i < len && *n + 1 < SP_PATH_MAX; i++) {
        int hi = i + 2 < len && s[i] == '%' ? sp_priv_hex_digit(s[i + 1]) : -1;
        int lo = hi >= 0 ? sp_priv_hex_digit(s[i + 2]) : -1;
        buf[(*n)++] = lo >= 0 ? SP_PRIV_CAST(char, hi * 16 + lo) : s[i];
        if (lo >= 0)
            i += 2;
    }
    if (i < len)
        *n = SP_PATH_MAX;
}

/* urllib.request.url2pathname(uri, require_scheme=True), which must give an absolute path */
SpPath sp_from_uri(const char *uri, SpFlavor flavor) {
    flavor = sp_priv_flavor(flavor);
    SpPath err = sp_priv_error_path(flavor, SP_ERR_INVALID_ARG);
    if (strlen(uri) < 5 || sp_priv_text_cmp(uri, 5, "file:", 5, true) != 0)
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
        if (plen > 0)
            sp_priv_utf8_next(path, plen, &pipe);
        pipe = plen > pipe && path[pipe] == '|' ? pipe : 0; /* older URLs use a pipe after the drive letter */
    }

    sp_priv_unquote_append(buf, &n, path, plen);
    if (pipe > 0)
        buf[pipe] = ':';
    SpPath r = sp_path_from_n(buf, n, flavor);
    if (r.error != SP_OK || sp_is_absolute(&r))
        return r;
    return sp_priv_error_path(flavor, SP_ERR_NOT_ABSOLUTE);
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
        size_t ea = sp_priv_part_end(sa, la, i, sep);
        size_t eb = sp_priv_part_end(sb, lb, j, sep);
        int c = sp_priv_text_cmp(sa + i, ea - i, sb + j, eb - j, a->flavor == SP_FLAVOR_WINDOWS);
        if (c != 0)
            return c;
        if (ea == la || eb == lb)
            return ea < la ? 1 : eb < lb ? -1 : 0; /* the path with fewer parts sorts first */
        i = ea + 1;
        j = eb + 1;
    }
}

/* Equal paths hash alike: on Windows by the code points of str.lower() */
unsigned long sp_path_hash(const SpPath *p) {
    unsigned long hash = 5381;
    const char *str = sp_str(p);
    size_t len = p->len > 0 ? p->len : 1;
    unsigned long owed = 0;
    bool lower = p->flavor == SP_FLAVOR_WINDOWS;

    for (size_t i = 0; i < len || owed != 0;) {
        hash = hash * 33 + (lower ? sp_priv_lower_next(str, len, &i, &owed) : SP_PRIV_CAST(unsigned char, str[i++]));
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

    /* The letters of its case group */
    unsigned long group = sp_priv_case_group(lower);
    for (size_t i = 0; group != 0 && lo <= top && i < SP_ARRAY_LEN(sp_priv_case_members); i += 2) {
        unsigned long member = SP_PRIV_CAST(unsigned long, sp_priv_case_members[i]);
        if (SP_PRIV_CAST(unsigned long, sp_priv_case_members[i + 1]) == group && sp_priv_lowered_from(member, lo, top))
            return true;
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
                    k = sp_priv_part_end(s, slen, k == si ? k + 1 : k, sep);
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
            size_t end = sp_priv_part_end(pat, plen, pi, sep);
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

/* full_match: the whole path against the pattern. match: the path's last parts against the pattern's parts, which must
 * be all of its parts for an anchored pattern (CPython's) */
bool sp_match(const SpPath *p, const char *pattern, SpMatchOptions options) {
    SP_ASSERT_PATH_INVARIANT(p);
    SpPath pat = sp_path_from_n(pattern, strlen(pattern), p->flavor);
    SpCaseSensitivity cs = options.case_sensitive;
    bool ci = cs == SP_CASE_INSENSITIVE || (cs == SP_CASE_DEFAULT && p->flavor == SP_FLAVOR_WINDOWS);
    if (options.full) {
        assert(pat.error == SP_OK && "pattern longer than SP_PATH_MAX");
        return sp_priv_match_path(pat.buf, pat.len, p->buf, p->len, ci, true, p->flavor);
    }

    SpPartsIter path = sp_parts_begin(p);
    SpPartsIter pattern_parts = sp_parts_begin(&pat);
    SpStr pp, sp;
    size_t count = 0;
    size_t total = 0;
    for (SpPartsIter it = pattern_parts; sp_parts_next(&it, &pp);)
        count++;
    for (SpPartsIter it = path; sp_parts_next(&it, &sp);)
        total++;
    assert(count > 0 && "empty pattern");
    if (total < count || (total > count && pattern_parts.anchor > 0))
        return false;

    for (size_t skip = total - count; sp_parts_next(&path, &sp);)
        if (skip > 0)
            skip--;
        else if (sp_parts_next(&pattern_parts, &pp) &&
                 !sp_priv_match_path(pp.data, pp.len, sp.data, sp.len, ci, false, p->flavor))
            return false;
    return true;
}

SpStatResult sp_stat(const SpPath *p, bool follow_symlinks) {
    SpStatResult result = SP_PRIV_ZERO;
    SpPrivNative native;
    result.error = sp_priv_native(p, &native);
    if (result.error != SP_OK)
        return result;

#ifdef SP_WINDOWS
    /* CPython's lstat stops at a name surrogate (a symlink or junction) and follows any other reparse point; only a
     * symlink is S_IFLNK */
    DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    DWORD flags = FILE_FLAG_BACKUP_SEMANTICS | (follow_symlinks ? 0 : FILE_FLAG_OPEN_REPARSE_POINT);
    HANDLE h = CreateFileW(native.path, FILE_READ_ATTRIBUTES, share, NULL, OPEN_EXISTING, flags, NULL);
    FILE_ATTRIBUTE_TAG_INFO tag = {0, 0};
    if (h != INVALID_HANDLE_VALUE && !follow_symlinks &&
        GetFileInformationByHandleEx(h, FileAttributeTagInfo, &tag, sizeof(tag)) &&
        (tag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) && !IsReparseTagNameSurrogate(tag.ReparseTag)) {
        CloseHandle(h);
        h = CreateFileW(native.path, FILE_READ_ATTRIBUTES, share, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS,
                        NULL);
        tag.ReparseTag = 0;
    }

    BY_HANDLE_FILE_INFORMATION info;
    if (h == INVALID_HANDLE_VALUE || !GetFileInformationByHandle(h, &info)) {
        result.error = sp_priv_last_error();
        if (h != INVALID_HANDLE_VALUE)
            CloseHandle(h);
        return result;
    }

    DWORD file_type = GetFileType(h);
    result.sp_mode = (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 040777 : 0100666;
    if (file_type == FILE_TYPE_CHAR)
        result.sp_mode = (result.sp_mode & ~SP_PRIV_IFMT) | SP_PRIV_IFCHR;
    else if (file_type != FILE_TYPE_DISK)
        result.sp_mode &= ~SP_PRIV_IFMT;
    if (info.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
        result.sp_mode &= ~0222;
    if (!follow_symlinks && tag.ReparseTag == IO_REPARSE_TAG_SYMLINK)
        result.sp_mode = (result.sp_mode & ~SP_PRIV_IFMT) | SP_PRIV_IFLNK;
    result.sp_reparse_tag = SP_PRIV_CAST(unsigned int, tag.ReparseTag);

    typedef struct {
        ULONGLONG VolumeSerialNumber;
        BYTE FileId[16];
    } SpFileIdInfo;
    SpFileIdInfo fii;
    if (GetFileInformationByHandleEx(h, SP_PRIV_CAST(FILE_INFO_BY_HANDLE_CLASS, 18), &fii, sizeof(fii))) {
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

    CloseHandle(h);
#else
    struct stat st;
    if ((follow_symlinks ? stat(native.path, &st) : lstat(native.path, &st)) != 0) {
        result.error = sp_priv_last_error();
        return result;
    }
    result.sp_mode = SP_PRIV_CAST(unsigned int, st.st_mode);
    result.sp_ino = SP_PRIV_CAST(unsigned long long, st.st_ino);
    result.sp_dev = SP_PRIV_CAST(unsigned long long, st.st_dev);
    result.sp_nlink = SP_PRIV_CAST(unsigned long long, st.st_nlink);
    result.sp_uid = SP_PRIV_CAST(unsigned int, st.st_uid);
    result.sp_gid = SP_PRIV_CAST(unsigned int, st.st_gid);
    result.sp_size = SP_PRIV_CAST(long long, st.st_size);

    /* The times' seconds and nanoseconds, where each platform keeps them */
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    long long sec[3] = {st.st_atimespec.tv_sec, st.st_mtimespec.tv_sec, st.st_ctimespec.tv_sec};
    long long nsec[3] = {st.st_atimespec.tv_nsec, st.st_mtimespec.tv_nsec, st.st_ctimespec.tv_nsec};
#elif defined(__GLIBC__) && !defined(__USE_XOPEN2K8)
    /* glibc's strict C modes hide POSIX 2008's st_atim */
    long long sec[3] = {st.st_atime, st.st_mtime, st.st_ctime};
    long long nsec[3] = {SP_PRIV_CAST(long long, st.st_atimensec), SP_PRIV_CAST(long long, st.st_mtimensec),
                         SP_PRIV_CAST(long long, st.st_ctimensec)};
#else
    long long sec[3] = {st.st_atim.tv_sec, st.st_mtim.tv_sec, st.st_ctim.tv_sec};
    long long nsec[3] = {st.st_atim.tv_nsec, st.st_mtim.tv_nsec, st.st_ctim.tv_nsec};
#endif
    long long *ns[3] = {&result.sp_atime_ns, &result.sp_mtime_ns, &result.sp_ctime_ns};
    for (int i = 0; i < 3; i++)
        *ns[i] = sec[i] * 1000000000LL + nsec[i];
#endif

    /* CPython's float times are seconds + nanoseconds * 1e-9, which rounds differently from nanoseconds / 1e9 */
    long long *nanoseconds[3] = {&result.sp_atime_ns, &result.sp_mtime_ns, &result.sp_ctime_ns};
    double *floats[3] = {&result.sp_atime, &result.sp_mtime, &result.sp_ctime};
    for (int i = 0; i < 3; i++) {
        long long seconds = *nanoseconds[i] / 1000000000LL - (*nanoseconds[i] % 1000000000LL < 0 ? 1 : 0);
        *floats[i] =
            SP_PRIV_CAST(double, seconds) + SP_PRIV_CAST(double, *nanoseconds[i] - seconds * 1000000000LL) * 1e-9;
    }
    return result;
}

bool sp_stat_eq(const SpStatResult *a, const SpStatResult *b) {
    return a->error == SP_OK && b->error == SP_OK && a->sp_mode == b->sp_mode && a->sp_ino == b->sp_ino &&
           a->sp_dev == b->sp_dev && a->sp_nlink == b->sp_nlink && a->sp_uid == b->sp_uid && a->sp_gid == b->sp_gid &&
           a->sp_size == b->sp_size;
}

/* os.path.ismount */
static bool sp_priv_is_mount(const SpPath *p) {
#ifdef SP_WINDOWS
    /* ntpath.ismount: the path is its volume's root */
    SpPrivNative native;
    wchar_t volume[SP_PATH_MAX];
    char vol[SP_PATH_MAX];
    size_t vlen;
    if (sp_priv_native(p, &native) != SP_OK || !GetVolumePathNameW(native.path, volume, SP_PATH_MAX) ||
        sp_priv_from_wide(volume, wcslen(volume), vol, SP_PATH_MAX, &vlen) != SP_OK)
        return false;

    size_t plen = p->len > 0 ? p->len : 1;
    const char *s = p->len > 0 ? p->buf : ".";
    if (vlen > 0 && vol[vlen - 1] == '\\')
        vlen--;
    if (s[plen - 1] == '\\' || s[plen - 1] == '/')
        plen--;
    return sp_priv_text_cmp(s, plen, vol, vlen, true) == 0;
#else
    SpStatResult st_path = sp_stat(p, false);
    if ((st_path.sp_mode & SP_PRIV_IFMT) != SP_PRIV_IFDIR)
        return false;

    SpPath parent = sp_parent(p);
    SpStatResult st_parent = sp_stat(&parent, false);
    return st_parent.error == SP_OK && (st_path.sp_dev != st_parent.sp_dev || st_path.sp_ino == st_parent.sp_ino);
#endif
}

/* A junction is lstat's mount point reparse tag (os.path.isjunction); the others are stat's file type */
bool sp_is(const SpPath *p, SpFileType type, bool follow_symlinks) {
    static const unsigned int modes[] = {
        0, SP_PRIV_IFREG, SP_PRIV_IFDIR, SP_PRIV_IFLNK, SP_PRIV_IFBLK, SP_PRIV_IFCHR, SP_PRIV_IFIFO, SP_PRIV_IFSOCK};
    SP_ASSERT_PATH_INVARIANT(p);
    if (type == SP_MOUNT)
        return sp_priv_is_mount(p);

    SpStatResult st = sp_stat(p, follow_symlinks && type != SP_SYMLINK && type != SP_JUNCTION);
    if (type == SP_JUNCTION)
        return st.error == SP_OK && st.sp_reparse_tag == SP_PRIV_REPARSE_TAG_MOUNT_POINT;
    return st.error == SP_OK && (type == SP_ANY || (st.sp_mode & SP_PRIV_IFMT) == modes[type]);
}

/* os.readlink(), as a path: the target of the symlink (or junction) at p. On Windows that is the substitute name,
 * with its "\??\" prefix read as "\\?\". */
SpPath sp_readlink(const SpPath *p) {
    SpPath r = sp_priv_error_path(p->flavor, SP_OK);
    SpPrivNative native;
    SpError err = sp_priv_native(p, &native);
    if (err != SP_OK)
        return sp_priv_error_path(p->flavor, err);

#ifdef SP_WINDOWS
    DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    DWORD flags = FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT;
    HANDLE h = CreateFileW(native.path, 0, share, NULL, OPEN_EXISTING, flags, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return sp_priv_error_path(p->flavor, sp_priv_last_error());

    /* REPARSE_DATA_BUFFER: the tag at 0, the substitute name's offset and length at 8, the names after the flags
     * (at 20) for a symlink, right after the lengths (at 16) for a junction */
    union {
        wchar_t wide[8192]; /* the names are UTF-16 at even offsets, read in place */
        char bytes[16384];
        DWORD align;
    } data;
    DWORD got;
    bool read = DeviceIoControl(h, 0x000900A8 /* FSCTL_GET_REPARSE_POINT */, NULL, 0, data.bytes, sizeof(data.bytes),
                                &got, NULL) != 0;
    err = read ? SP_OK : sp_priv_last_error();
    CloseHandle(h);
    if (!read)
        return sp_priv_error_path(p->flavor, err);

    DWORD tag;
    WORD offset;
    WORD bytes;
    memcpy(&tag, data.bytes, sizeof(tag));
    memcpy(&offset, data.bytes + 8, sizeof(offset));
    memcpy(&bytes, data.bytes + 10, sizeof(bytes));
    size_t names = tag == IO_REPARSE_TAG_SYMLINK ? 20 : tag == IO_REPARSE_TAG_MOUNT_POINT ? 16 : 0;
    if (names == 0)
        return sp_priv_error_path(p->flavor, SP_ERR_NOT_LINK);
    if (names + offset + bytes > got || offset % 2 != 0)
        return sp_priv_error_path(p->flavor, SP_ERR_IO);
    if (bytes / 2 >= SP_PATH_MAX)
        return sp_priv_error_path(p->flavor, SP_ERR_TOO_LONG);

    wchar_t *target = data.wide + (names + offset) / 2;
    if (bytes / 2 > 4 && memcmp(target, L"\\??\\", 4 * sizeof(wchar_t)) == 0)
        target[1] = L'\\';
    err = sp_priv_from_wide(target, bytes / 2, r.buf, SP_PATH_MAX, &r.len);
#else
    ssize_t n = readlink(native.path, r.buf, SP_PATH_MAX);
    if (n < 0)
        return sp_priv_error_path(p->flavor, errno == EINVAL ? SP_ERR_NOT_LINK : sp_priv_last_error());
    if (n >= SP_PATH_MAX)
        return sp_priv_error_path(p->flavor, SP_ERR_TOO_LONG);
    r.len = SP_PRIV_CAST(size_t, n);
    r.buf[n] = '\0';
#endif
    if (err != SP_OK)
        return sp_priv_error_path(p->flavor, err);

    sp_priv_normalize(&r);
    return r;
}

#ifdef SP_WINDOWS
/* ntpath.normpath's ".." on a normalized path, given its anchor and drive lengths: it takes the part before it away, or
 * goes right after a root */
static void sp_priv_collapse_dots(SpPath *p, size_t anchor, size_t drive) {
    bool rooted = anchor > drive;
    size_t j = anchor < p->len ? anchor : p->len;

    for (size_t i = j, end; i < p->len; i = end + 1) {
        end = sp_priv_part_end(p->buf, p->len, i, '\\');
        bool dots = end - i == 2 && p->buf[i] == '.' && p->buf[i + 1] == '.';
        size_t last = j > anchor ? j - 1 : j; /* the start of the kept part before, if any */
        while (last > anchor && p->buf[last - 1] != '\\')
            last--;
        bool after_dots = j > anchor && j - 1 - last == 2 && p->buf[last] == '.' && p->buf[last + 1] == '.';

        if (dots && j > anchor && !after_dots) {
            j = last; /* drop the part before, with its separator */
            continue;
        }
        if (dots && j == anchor && rooted)
            continue;

        memmove(p->buf + j, p->buf + i, end - i);
        j += end - i;
        p->buf[j++] = '\\';
    }

    if (j > anchor)
        j--; /* the separator after the last part */
    p->len = j;
    p->buf[j] = '\0';
}

/* ntpath's _getfinalpathname: GetFinalPathNameByHandle of the path, as "\\?\..." into out; 0, or the Windows error */
static DWORD sp_priv_final_path(const SpPath *p, SpPath *out) {
    SpPrivNative native;
    *out = sp_priv_error_path(p->flavor, SP_OK);
    if (sp_priv_native(p, &native) != SP_OK)
        return ERROR_INVALID_NAME;

    HANDLE h = CreateFileW(native.path, 0, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return GetLastError();

    wchar_t final[SP_PATH_MAX];
    DWORD n = GetFinalPathNameByHandleW(h, final, SP_PATH_MAX, 0);
    DWORD e = n == 0 ? GetLastError() : n >= SP_PATH_MAX ? ERROR_FILENAME_EXCED_RANGE : 0;
    CloseHandle(h);
    if (e == 0 && sp_priv_from_wide(final, n, out->buf, SP_PATH_MAX, &out->len) != SP_OK)
        e = ERROR_FILENAME_EXCED_RANGE;
    out->anchor = sp_priv_split_anchor(out->buf, out->len, out->flavor, &out->drive);
    return e;
}

/* ntpath._findfirstfile: the name of the path's last part as the directory spells it */
static bool sp_priv_real_name(const SpPath *p, SpPath *name) {
    SpPrivNative native;
    if (sp_priv_native(p, &native) != SP_OK)
        return false;

    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(native.path, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return false;

    FindClose(h);
    *name = sp_priv_error_path(p->flavor, SP_OK);
    return sp_priv_from_wide(fd.cFileName, wcslen(fd.cFileName), name->buf, SP_PATH_MAX, &name->len) == SP_OK;
}

/* One step of ntpath's _readlink_deep: 1 when path became its link's target (joined to the link's directory when
 * relative), 0 when there is nothing more to follow (an error it expects, or a relative target of anything but a
 * symlink), -1 when reading the link failed otherwise */
static int sp_priv_readlink_step(SpPath *path) {
    SpPath target = sp_readlink(path);
    if (target.error == SP_ERR_NOT_LINK)
        return 0;
    if (target.error != SP_OK) {
        static const DWORD stops[] = {1, 2, 3, 5, 21, 32, 50, 67, 87, 4390, 4392, 4393};
        DWORD e = GetLastError();
        for (size_t i = 0; i < SP_ARRAY_LEN(stops); i++)
            if (stops[i] == e)
                return 0;
        return -1;
    }

    if (sp_is_absolute(&target)) {
        *path = target;
        return 1;
    }
    if ((sp_stat(path, false).sp_mode & SP_PRIV_IFMT) != SP_PRIV_IFLNK)
        return 0;

    /* normpath(join(dirname(link), target)) */
    SpPath dir = sp_priv_prefix(path, sp_priv_parent_len(path->buf, path->len, path->flavor, path->anchor));
    *path = sp_priv_join_len(&dir, target.buf, target.len);
    sp_priv_collapse_dots(path, path->anchor, path->drive);
    return path->error == SP_OK ? 1 : -1;
}

/* r without its "\\?\" prefix when the path without it resolves the same (or fails the same way, initial) */
static SpPath sp_priv_without_prefix(const SpPath *r, DWORD initial) {
    bool unc = r->len >= 8 && memcmp(r->buf + 4, "UNC\\", 4) == 0;
    SpPath stripped = unc ? sp_priv_path_from_raw("\\\\", 2, r->flavor) : sp_priv_error_path(r->flavor, SP_OK);
    sp_priv_append(&stripped, r->buf + (unc ? 8 : 4), r->len - (unc ? 8 : 4), false);

    SpPath again;
    DWORD e = sp_priv_final_path(&stripped, &again);
    bool same = e == 0 ? again.len == r->len && memcmp(again.buf, r->buf, r->len) == 0 : e == initial;
    return same ? stripped : *r;
}
#endif

/* os.path.realpath of the absolute path. On POSIX, realpath() of the longest prefix it resolves (the whole path when
 * strict), followed by the rest. On Windows, ntpath.realpath: the final path of the whole path; without strict, of its
 * longest prefix Windows resolves (following a link by hand where Windows can't), then the rest. A "\\?\" prefix the
 * path didn't have comes off when the path without it resolves the same. */
SpPath sp_resolve(const SpPath *p, bool strict) {
    SpPrivNative native;
    SpError err = sp_priv_native(p, &native);
    if (err != SP_OK)
        return sp_priv_error_path(p->flavor, err);

#ifdef SP_WINDOWS
    if (sp_priv_text_cmp(p->buf, p->len, "nul", 3, true) == 0)
        return sp_path_from_n("\\\\.\\NUL", 7, p->flavor);
#endif
    SpPath abs = sp_absolute(p);
    if (abs.error != SP_OK)
        return abs;

#ifdef SP_WINDOWS
    sp_priv_collapse_dots(&abs, abs.anchor, abs.drive);
    bool had_prefix = abs.len >= 4 && memcmp(abs.buf, "\\\\?\\", 4) == 0;
    SpPath r;
    DWORD initial = sp_priv_final_path(&abs, &r);
    if (initial != 0 && strict) {
        SetLastError(initial);
        return sp_priv_error_path(p->flavor, sp_priv_last_error());
    }

    /* _getfinalpathname_nonstrict: resolve what Windows can, carrying the rest along in tail */
    SpPath rest = abs;
    SpPath tail = sp_priv_error_path(p->flavor, SP_OK);
    for (DWORD e = initial; e != 0;) {
        static const DWORD allowed[] = {1, 2, 3, 5, 21, 32, 50, 53, 65, 67, 87, 123, 161, 1005, 1920, 1921};
        bool ok = false;
        for (size_t i = 0; i < SP_ARRAY_LEN(allowed); i++)
            ok = ok || allowed[i] == e;
        if (!ok) {
            SetLastError(e);
            return sp_priv_error_path(p->flavor, sp_priv_last_error());
        }

        /* _readlink_deep: follow links until reading one stops, or a path repeats (Floyd's cycle finding gives the
         * first repeated path, which is where CPython's set of seen paths stops) */
        SpPath slow = rest;
        SpPath fast = rest;
        int step = 1;
        for (;;) {
            step = sp_priv_readlink_step(&fast);
            if (step == 1)
                step = sp_priv_readlink_step(&fast);
            if (step != 1)
                break;
            sp_priv_readlink_step(&slow);
            if (sp_priv_text_cmp(slow.buf, slow.len, fast.buf, fast.len, true) == 0)
                break;
        }
        if (step == 1) {
            slow = rest;
            while (sp_priv_text_cmp(slow.buf, slow.len, fast.buf, fast.len, true) != 0) {
                sp_priv_readlink_step(&slow);
                sp_priv_readlink_step(&fast);
            }
        }
        SpPath deep = step == -1 ? rest : step == 1 ? slow : fast;
        if (deep.len != rest.len || memcmp(deep.buf, rest.buf, rest.len) != 0) {
            r = tail.len > 0 ? sp_priv_join_len(&deep, tail.buf, tail.len) : deep;
            break;
        }

        /* Carry the last part over to tail (rest keeps abs's anchor), spelled as the directory spells it when Windows
         * couldn't open it */
        SpStr name = sp_priv_name_sv(&rest);
        SpPath real;
        bool spelled = (e == 1 || e == 5 || e == 32 || e == 50 || e == 87 || e == 1920 || e == 1921) &&
                       sp_priv_real_name(&rest, &real);
        if (spelled)
            name = SP_PRIV_STR(real.buf, real.len);
        if (name.len == 0) {
            r = tail.len > 0 ? sp_priv_join_len(&rest, tail.buf, tail.len) : rest;
            break;
        }

        SpPath joined = sp_priv_path_from_raw(name.data, name.len, p->flavor);
        sp_priv_append(&joined, tail.buf, tail.len, tail.len > 0);
        tail = joined;
        rest.len = sp_priv_parent_len(rest.buf, rest.len, rest.flavor, rest.anchor);
        rest.buf[rest.len] = '\0';
        if (tail.error != SP_OK)
            return tail;
        if (rest.len == 0) {
            r = tail;
            break;
        }
        e = sp_priv_final_path(&rest, &r);
        if (e == 0 && tail.len > 0)
            r = sp_priv_join_len(&r, tail.buf, tail.len);
    }

    if (!had_prefix && r.error == SP_OK && r.len >= 4 && memcmp(r.buf, "\\\\?\\", 4) == 0)
        r = sp_priv_without_prefix(&r, initial);
    return r.error != SP_OK ? r : sp_path_from_n(r.buf, r.len, p->flavor);
#else
    /* realpath() allocates its result: a buffer of ours could be shorter than the PATH_MAX it may fill */
    for (size_t n = abs.len;;) {
        SpPath prefix = sp_priv_prefix(&abs, n);
        char *real = realpath(prefix.len == 0 ? "." : prefix.buf, SP_PRIV_NULL);
        if (real) {
            SpPath r = sp_path_from_n(real, strlen(real), p->flavor);
            free(real);
            while (n < abs.len && abs.buf[n] == '/')
                n++;
            return n < abs.len && r.error == SP_OK ? sp_priv_join_len(&r, abs.buf + n, abs.len - n) : r;
        }
        if (strict)
            return sp_priv_error_path(p->flavor, sp_priv_last_error());

        size_t parent = sp_priv_parent_len(abs.buf, n, abs.flavor, abs.anchor);
        if (parent == n)
            return abs;
        n = parent;
    }
#endif
}

/* os.symlink() or os.link(): a link at p to target */
static SpError sp_priv_link_to_impl(const SpPath *p, const SpPath *target, bool symbolic, bool target_is_directory) {
    SpPrivNative at;
    SpPrivNative to;
    SpError err = sp_priv_native(p, &at);
    SpError target_err = sp_priv_native(target, &to);
    if (err != SP_OK || target_err != SP_OK)
        return err != SP_OK ? err : target_err;

#ifdef SP_WINDOWS
    DWORD flags = (target_is_directory ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0) | 0x2; /* unprivileged, in developer mode */
    bool ok =
        symbolic ? CreateSymbolicLinkW(at.path, to.path, flags) != 0 : CreateHardLinkW(at.path, to.path, NULL) != 0;
#else
    (void)target_is_directory;
#ifdef __ANDROID__
    if (!symbolic)
        return SP_ERR_UNSUPPORTED; /* Android forbids hard links: CPython there has no os.link */
#endif
    bool ok = (symbolic ? symlink(to.path, at.path) : link(to.path, at.path)) == 0;
#endif
    return ok ? SP_OK : sp_priv_last_error();
}

/* CPython's os.symlink: on Windows a link to an existing directory (the target taken from the link's directory) is a
 * directory link even without target_is_directory */
SpError sp_link_to(const SpPath *p, const SpPath *target, SpLinkOptions options) {
    bool target_is_directory = options.target_is_directory;
#ifdef SP_WINDOWS
    if (!options.hard && !target_is_directory) {
        SpPath dir = sp_parent(p);
        SpPath resolved = sp_priv_join_len(&dir, target->buf, target->len);
        SpPrivNative native;
        DWORD attrs = INVALID_FILE_ATTRIBUTES;
        if (sp_priv_native(&resolved, &native) == SP_OK)
            attrs = GetFileAttributesW(native.path);
        target_is_directory = attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
    }
#endif
    return sp_priv_link_to_impl(p, target, !options.hard, target_is_directory);
}

bool sp_samefile(const SpPath *a, const SpPath *b) {
    SP_ASSERT_PATH_INVARIANT(a);
    SP_ASSERT_PATH_INVARIANT(b);
    SpStatResult stat_a = sp_stat(a, true);
    SpStatResult stat_b = sp_stat(b, true);
    return stat_a.error == SP_OK && stat_b.error == SP_OK && stat_a.sp_dev == stat_b.sp_dev &&
           stat_a.sp_ino == stat_b.sp_ino;
}

/* CPython's Path.mkdir: missing parents are created (with parent_mode) only after a "not found" failure, and any
 * failure is fine with exist_ok when the path is a directory (Windows reports some of those as access denied) */
SpError sp_mkdir(const SpPath *p, unsigned int mode, SpMkdirOptions options) {
    SpPrivNative native;
    SpError err = sp_priv_native(p, &native);
    if (err != SP_OK)
        return err;

#ifdef SP_WINDOWS
    (void)mode;
    bool ok = CreateDirectoryW(native.path, NULL) != 0;
#else
    bool ok = mkdir(native.path, SP_PRIV_CAST(mode_t, mode)) == 0;
#endif
    err = ok ? SP_OK : sp_priv_last_error();
    if (err == SP_OK)
        return SP_OK;

    SpPath parent = sp_parent(p);
    if (err == SP_ERR_NOT_FOUND && options.parents && parent.len != p->len) {
        SpMkdirOptions above = options;
        above.exist_ok = true;
        err = sp_mkdir(&parent, options.parent_mode, above);
        options.parents = false;
        return err == SP_OK ? sp_mkdir(p, mode, options) : err;
    }
    /* sp_stat directly: through sp_is, is_mount's chain would put copy_tree past the frame limit */
    return options.exist_ok && (sp_stat(p, true).sp_mode & SP_PRIV_IFMT) == SP_PRIV_IFDIR ? SP_OK : err;
}

/* CPython's Path.touch: with exist_ok, bump an existing file's times; otherwise create the file */
SpError sp_touch(const SpPath *p, unsigned int mode, bool exist_ok) {
    SpPrivNative native;
    SpError err = sp_priv_native(p, &native);
    if (err != SP_OK)
        return err;

#ifdef SP_WINDOWS
    (void)mode;
    bool exists = sp_stat(p, true).error == SP_OK;
    if (exists && !exist_ok)
        return SP_ERR_EXISTS;

    DWORD access = exists ? FILE_WRITE_ATTRIBUTES : GENERIC_WRITE;
    DWORD disposition = exists ? OPEN_EXISTING : CREATE_NEW;
    DWORD share = exists ? (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE) : 0;
    HANDLE h = CreateFileW(native.path, access, share, NULL, disposition, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return sp_priv_last_error();

    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    err = !exists || SetFileTime(h, NULL, &ft, &ft) ? SP_OK : sp_priv_last_error();
    CloseHandle(h);
    return err;
#else
    if (exist_ok && utime(native.path, SP_PRIV_NULL) == 0)
        return SP_OK;

    int fd = open(native.path, O_CREAT | O_WRONLY | (exist_ok ? 0 : O_EXCL), SP_PRIV_CAST(mode_t, mode));
    return fd >= 0 && close(fd) == 0 ? SP_OK : sp_priv_last_error();
#endif
}

/* os.unlink() or os.rmdir(); a missing file is fine with missing_ok. Like CPython on Windows, unlinking a symlink or
 * junction to a directory removes the link with RemoveDirectory. */
static SpError sp_priv_remove_impl(const SpPath *p, bool is_dir, bool missing_ok) {
    SpPrivNative native;
    SpError err = sp_priv_native(p, &native);
    if (err != SP_OK)
        return err;

#ifdef SP_WINDOWS
    DWORD attrs = GetFileAttributesW(native.path);
    DWORD dir_link = FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT;
    if (!is_dir && attrs != INVALID_FILE_ATTRIBUTES && (attrs & dir_link) == dir_link) {
        WIN32_FIND_DATAW fd;
        HANDLE find = FindFirstFileW(native.path, &fd);
        if (find != INVALID_HANDLE_VALUE) {
            FindClose(find);
            is_dir = fd.dwReserved0 == IO_REPARSE_TAG_SYMLINK || fd.dwReserved0 == IO_REPARSE_TAG_MOUNT_POINT;
        }
    }
    bool ok = (is_dir ? RemoveDirectoryW(native.path) : DeleteFileW(native.path)) != 0;
#else
    bool ok = (is_dir ? rmdir(native.path) : unlink(native.path)) == 0;
#endif
    err = ok ? SP_OK : sp_priv_last_error();
    return err == SP_ERR_NOT_FOUND && missing_ok ? SP_OK : err;
}

SpError sp_remove(const SpPath *p, SpRemoveOptions options) {
    return sp_priv_remove_impl(p, options.dir, options.missing_ok);
}

/* os.chmod(). On Windows the owner's write bit clears or sets the read-only attribute: of the file a link leads to
 * (its final path) when following, of the link itself otherwise. Not following a symlink elsewhere needs fchmodat's
 * AT_SYMLINK_NOFOLLOW; where the headers don't show it, only a path that isn't a symlink can do without following. */
SpError sp_chmod(const SpPath *p, unsigned int mode, bool follow_symlinks) {
    SpPrivNative native;
    SpError err = sp_priv_native(p, &native);
    if (err != SP_OK)
        return err;

#ifdef SP_WINDOWS
    SpPath target;
    DWORD final = follow_symlinks ? sp_priv_final_path(p, &target) : 0;
    if (final != 0) {
        SetLastError(final);
        return sp_priv_last_error();
    }
    if (follow_symlinks && (err = sp_priv_native(&target, &native)) != SP_OK)
        return err;

    DWORD attrs = GetFileAttributesW(native.path);
    if (attrs == INVALID_FILE_ATTRIBUTES)
        return sp_priv_last_error();
    attrs = (mode & 0200) ? attrs & ~SP_PRIV_CAST(DWORD, FILE_ATTRIBUTE_READONLY) : attrs | FILE_ATTRIBUTE_READONLY;
    return SetFileAttributesW(native.path, attrs) ? SP_OK : sp_priv_last_error();
#else
#ifdef AT_SYMLINK_NOFOLLOW
    if (!follow_symlinks)
        return fchmodat(AT_FDCWD, native.path, SP_PRIV_CAST(mode_t, mode), AT_SYMLINK_NOFOLLOW) == 0
                   ? SP_OK
                   : sp_priv_last_error();
#else
    SpStatResult st = sp_stat(p, false);
    if (!follow_symlinks && st.error == SP_OK && (st.sp_mode & SP_PRIV_IFMT) == SP_PRIV_IFLNK)
        return SP_ERR_UNSUPPORTED;
#endif
    return chmod(native.path, SP_PRIV_CAST(mode_t, mode)) == 0 ? SP_OK : sp_priv_last_error();
#endif
}

/* A file opened for reading, or for writing (created or truncated) */
static SpError sp_priv_open(const SpPath *p, bool write, SpPrivFile *f) {
    SpPrivNative native;
    SpError err = sp_priv_native(p, &native);
    if (err != SP_OK)
        return err;

#ifdef SP_WINDOWS
    DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    *f = CreateFileW(native.path, write ? GENERIC_WRITE : GENERIC_READ, share, NULL,
                     write ? CREATE_ALWAYS : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    return *f != INVALID_HANDLE_VALUE ? SP_OK : sp_priv_last_error();
#else
    *f = open(native.path, write ? O_WRONLY | O_CREAT | O_TRUNC : O_RDONLY, 0666);
    return *f >= 0 ? SP_OK : sp_priv_last_error();
#endif
}

/* Reads until buf is full or the file ends, into *got; false on failure (with the OS's error left for
 * sp_priv_last_error) */
static bool sp_priv_read(SpPrivFile f, char *buf, size_t size, size_t *got) {
    *got = 0;
    while (*got < size) {
        size_t chunk = size - *got < 0x40000000 ? size - *got : 0x40000000;
#ifdef SP_WINDOWS
        DWORD n;
        if (!ReadFile(f, buf + *got, SP_PRIV_CAST(DWORD, chunk), &n, NULL))
            return false;
#else
        ssize_t n = read(f, buf + *got, chunk);
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0)
            return false;
#endif
        if (n == 0)
            break;
        *got += SP_PRIV_CAST(size_t, n);
    }
    return true;
}

/* Writes all of data, counting into *wrote; false on failure (with the OS's error left for sp_priv_last_error) */
static bool sp_priv_write(SpPrivFile f, const char *data, size_t size, size_t *wrote) {
    *wrote = 0;
    while (*wrote < size) {
        size_t chunk = size - *wrote < 0x40000000 ? size - *wrote : 0x40000000;
#ifdef SP_WINDOWS
        DWORD n;
        if (!WriteFile(f, data + *wrote, SP_PRIV_CAST(DWORD, chunk), &n, NULL))
            return false;
#else
        ssize_t n = write(f, data + *wrote, chunk);
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0)
            return false;
#endif
        *wrote += SP_PRIV_CAST(size_t, n);
    }
    return true;
}

static SpError sp_priv_close(SpPrivFile f) {
#ifdef SP_WINDOWS
    return CloseHandle(f) ? SP_OK : sp_priv_last_error();
#else
    return close(f) == 0 ? SP_OK : sp_priv_last_error();
#endif
}

static SpIOResult sp_priv_io_result(size_t bytes, SpError error) {
    SpIOResult r = SP_PRIV_ZERO;
    r.bytes = bytes;
    r.error = error;
    return r;
}

SpIOResult sp_read_file(const SpPath *p, char *buf, size_t buf_size) {
    SpPrivFile f;
    SpError err = sp_priv_open(p, false, &f);
    if (err != SP_OK)
        return sp_priv_io_result(0, err);

    /* A byte past the buffer means the file doesn't fit */
    size_t got;
    char more;
    size_t extra = 0;
    bool ok = sp_priv_read(f, buf, buf_size, &got) && (got < buf_size || sp_priv_read(f, &more, 1, &extra));
    err = !ok ? sp_priv_last_error() : extra > 0 ? SP_ERR_TOO_LONG : SP_OK;
    SpError closed = sp_priv_close(f);
    err = err != SP_OK ? err : closed;

    /* Too long: the file's size, for a buffer that fits */
    if (err == SP_ERR_TOO_LONG)
        got = SP_PRIV_CAST(size_t, sp_stat(p, true).sp_size);
    return sp_priv_io_result(got, err);
}

SpIOResult sp_write_file(const SpPath *p, const char *data, size_t data_len) {
    SpPrivFile f;
    SpError err = sp_priv_open(p, true, &f);
    if (err != SP_OK)
        return sp_priv_io_result(0, err);

    size_t wrote;
    err = sp_priv_write(f, data, data_len, &wrote) ? SP_OK : sp_priv_last_error();
    SpError closed = sp_priv_close(f);
    return sp_priv_io_result(wrote, err != SP_OK ? err : closed);
}

const char *sp_error_str(SpError error) {
#define SP_ERROR_MESSAGE(name, message) message,
    static const char *const messages[] = {SP_ERRORS(SP_ERROR_MESSAGE)};
#undef SP_ERROR_MESSAGE
    size_t i = SP_PRIV_CAST(size_t, error);
    return i < SP_ARRAY_LEN(messages) ? messages[i] : "Unknown error";
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
static SpError sp_priv_join_child(SpPath *dir, const char *name, size_t len) {
    char sep = dir->flavor == SP_FLAVOR_WINDOWS ? '\\' : '/';
    char last = dir->len > 0 ? dir->buf[dir->len - 1] : '/';
    size_t at = dir->len + (last != '/' && last != sep && !(sep == '\\' && last == ':') ? 1 : 0);
    if (at + len >= SP_PATH_MAX)
        return SP_ERR_TOO_LONG;

    dir->buf[dir->len] = sep;
    memcpy(dir->buf + at, name, len);
    dir->len = at + len;
    dir->buf[dir->len] = '\0';
    return SP_OK;
}

/* The next entry of dir other than "." and "..", opening the listing on the first call: out gets dir / name (as
 * sp_priv_join_child builds it), *type (unless type is NULL) the entry's type as the listing knows it without following
 * a link (SP_PRIV_IFDIR, SP_PRIV_IFLNK, ..., or 0 when only a stat can tell, as for Windows reparse points), and the
 * name length is returned. At the end it returns 0, and out is an empty path whose error says why: SP_OK when the
 * listing is complete, or what failed (SP_ERR_TOO_LONG for an entry too long to join, which ends the listing). */
static size_t sp_priv_readdir_next(void **handle, const SpPath *dir, SpPath *out, unsigned int *type) {
    char sep = dir->flavor == SP_FLAVOR_WINDOWS ? '\\' : '/';
    char last = dir->len > 0 ? dir->buf[dir->len - 1] : '/';
    size_t at = dir->len + (last != '/' && last != sep && !(sep == '\\' && last == ':') ? 1 : 0);
    SpError end = SP_OK;
    SpPrivNative native;

    for (;;) {
        *out = *dir;
        out->buf[dir->len] = sep;
#ifdef SP_WINDOWS
        WIN32_FIND_DATAW fd;
        if (!*handle) {
            end = sp_priv_native(dir, &native);
            size_t wlen = end == SP_OK ? wcslen(native.path) : 0;
            if (end == SP_OK && wlen + 2 >= SP_PATH_MAX)
                end = SP_ERR_TOO_LONG;
            if (end != SP_OK)
                break;
            native.buf[wlen] = L'\\';
            native.buf[wlen + 1] = L'*';
            native.buf[wlen + 2] = L'\0';

            /* Only an empty drive root has no "." entry to find */
            *handle = FindFirstFileW(native.buf, &fd);
            if (*handle == INVALID_HANDLE_VALUE) {
                *handle = SP_PRIV_NULL;
                end = GetLastError() == ERROR_FILE_NOT_FOUND ? SP_OK : sp_priv_last_error();
                break;
            }
        } else if (!FindNextFileW(*handle, &fd)) {
            end = GetLastError() == ERROR_NO_MORE_FILES ? SP_OK : sp_priv_last_error();
            break;
        }
        const wchar_t *wide = fd.cFileName;
        if (wide[0] == L'.' && (wide[1] == L'\0' || (wide[1] == L'.' && wide[2] == L'\0')))
            continue;

        size_t n;
        end = sp_priv_from_wide(wide, wcslen(wide), out->buf + at, SP_PATH_MAX - at, &n);
        if (end != SP_OK)
            break;
        DWORD attributes = fd.dwFileAttributes;
        unsigned int listed = (attributes & FILE_ATTRIBUTE_REPARSE_POINT) ? 0
                              : (attributes & FILE_ATTRIBUTE_DIRECTORY)   ? SP_PRIV_IFDIR
                                                                          : SP_PRIV_IFREG;
#else
        if (!*handle) {
            end = sp_priv_native(dir, &native);
            if (end == SP_OK && !(*handle = opendir(native.path)))
                end = sp_priv_last_error();
            if (end != SP_OK)
                break;
        }
        errno = 0;
        struct dirent *de = readdir(SP_PRIV_CAST(DIR *, *handle));
        if (!de) {
            end = errno != 0 ? sp_priv_last_error() : SP_OK;
            break;
        }

        const char *name = de->d_name;
        size_t n = strlen(name);
        if (name[0] == '.' && (n == 1 || (n == 2 && name[1] == '.')))
            continue;
        if (at + n >= SP_PATH_MAX) {
            end = SP_ERR_TOO_LONG;
            break;
        }
        memcpy(out->buf + at, name, n + 1);
#if defined(_DIRENT_HAVE_D_TYPE) || defined(DT_DIR)
        unsigned int listed = SP_PRIV_CAST(unsigned int, de->d_type) << 12; /* DTTOIF, where DT_UNKNOWN is 0 */
#else
        unsigned int listed = 0;
#endif
#endif
        out->len = at + n;
        if (type)
            *type = listed;
        return n;
    }

    *out = sp_priv_error_path(dir->flavor, end);
    return 0;
}

/* CPython's _copy_info for local paths: the source's access and modification times (from st, its stat result), then
 * its permissions; a symlink's (st from not following it) are left out */
static bool sp_priv_copy_metadata(const SpStatResult *st, const SpPrivChar *dst) {
    bool link = (st->sp_mode & SP_PRIV_IFMT) == SP_PRIV_IFLNK;
#ifdef SP_WINDOWS
    /* FILETIMEs count 100ns ticks since 1601 */
    long long ns[2] = {st->sp_atime_ns, st->sp_mtime_ns};
    FILETIME times[2];
    for (int i = 0; i < 2; i++) {
        unsigned long long ticks = SP_PRIV_CAST(unsigned long long, ns[i] / 100 + 116444736000000000LL);
        times[i].dwLowDateTime = SP_PRIV_CAST(DWORD, ticks);
        times[i].dwHighDateTime = SP_PRIV_CAST(DWORD, ticks >> 32);
    }
    DWORD flags = FILE_FLAG_BACKUP_SEMANTICS | (link ? FILE_FLAG_OPEN_REPARSE_POINT : 0);
    DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    HANDLE hd = CreateFileW(dst, FILE_WRITE_ATTRIBUTES, share, NULL, OPEN_EXISTING, flags, NULL);
    bool ok = hd != INVALID_HANDLE_VALUE && SetFileTime(hd, NULL, &times[0], &times[1]);
    if (hd != INVALID_HANDLE_VALUE)
        CloseHandle(hd);

    /* chmod() on Windows only sets the read-only attribute, here the source's (sp_stat leaves out its write bits) */
    DWORD dst_attrs = GetFileAttributesW(dst);
    DWORD read_only = (st->sp_mode & 0222) == 0 ? FILE_ATTRIBUTE_READONLY : 0;
    DWORD attrs = (dst_attrs & ~SP_PRIV_CAST(DWORD, FILE_ATTRIBUTE_READONLY)) | read_only;
    return ok &&
           (link || (dst_attrs != INVALID_FILE_ATTRIBUTES && (attrs == dst_attrs || SetFileAttributesW(dst, attrs))));
#else
    if (link)
        return true; /* setting times and chmod() would follow it */

    /* To the nanosecond with POSIX 2008's utimensat, where the headers show it (glibc's strict C modes don't), as
     * os.utime(ns=...) does; else to the second */
    long long ns[2] = {st->sp_atime_ns, st->sp_mtime_ns};
    long long sec[2];
    for (int i = 0; i < 2; i++)
        sec[i] = ns[i] / 1000000000LL - (ns[i] % 1000000000LL < 0 ? 1 : 0);
#ifdef AT_FDCWD
    struct timespec times[2];
    for (int i = 0; i < 2; i++) {
        times[i].tv_sec = SP_PRIV_CAST(time_t, sec[i]);
        times[i].tv_nsec = SP_PRIV_CAST(long, ns[i] - sec[i] * 1000000000LL);
    }
    bool timed = utimensat(AT_FDCWD, dst, times, 0) == 0;
#else
    struct utimbuf times;
    times.actime = SP_PRIV_CAST(time_t, sec[0]);
    times.modtime = SP_PRIV_CAST(time_t, sec[1]);
    bool timed = utime(dst, &times) == 0;
#endif
    return timed && chmod(dst, st->sp_mode & 07777) == 0;
#endif
}

#ifndef SP_WINDOWS
/* The bytes of in, written to out */
static SpError sp_priv_copy_stream(SpPrivFile in, SpPrivFile out) {
    char buf[8192];
    size_t got;
    size_t wrote;
    do {
        if (!sp_priv_read(in, buf, sizeof(buf), &got) || !sp_priv_write(out, buf, got, &wrote))
            return sp_priv_last_error();
    } while (got == sizeof(buf));
    return SP_OK;
}
#endif

/* CPython's Path._copy_from: recursively copy src to dst (extended in place for children, then restored) */
static SpError sp_priv_copy_tree(const SpPath *src, SpPath *dst, bool follow_symlinks, bool preserve_metadata) {
    SpPrivNative from;
    SpPrivNative to;
    SpError err = sp_priv_native(src, &from);
    SpError to_err = sp_priv_native(dst, &to);
    if (err != SP_OK || to_err != SP_OK)
        return err != SP_OK ? err : to_err;

    /* Like CPython, anything that can't be stat()ed as a directory or symlink is copied as a file */
    SpStatResult st = sp_stat(src, follow_symlinks);
    unsigned int type = st.error == SP_OK ? st.sp_mode & SP_PRIV_IFMT : 0;
    if (type == SP_PRIV_IFLNK) {
        SpPath target = sp_readlink(src);
        bool target_is_dir = (sp_stat(src, true).sp_mode & SP_PRIV_IFMT) == SP_PRIV_IFDIR;
        err = target.error;
        if (err == SP_OK)
            err = sp_priv_link_to_impl(dst, &target, true, target_is_dir);
    } else if (type == SP_PRIV_IFDIR) {
        /* Children are listed before dst is created, so an unreadable src leaves no dst behind */
        void *handle = SP_PRIV_NULL;
        SpPath child;
        SpMkdirOptions plain = SP_PRIV_ZERO;
        size_t n = sp_priv_readdir_next(&handle, src, &child, SP_PRIV_NULL);
        err = n == 0 ? child.error : SP_OK;
        if (err == SP_OK)
            err = sp_mkdir(dst, SP_MODE_DIR, plain);

        for (size_t len = dst->len; err == SP_OK && n > 0;
             n = sp_priv_readdir_next(&handle, src, &child, SP_PRIV_NULL)) {
            err = sp_priv_join_child(dst, child.buf + child.len - n, n);
            if (err == SP_OK)
                err = sp_priv_copy_tree(&child, dst, follow_symlinks, preserve_metadata);
            dst->len = len;
            dst->buf[len] = '\0';
        }
        if (err == SP_OK)
            err = child.error; /* why the listing ended */
        sp_priv_readdir_close(&handle);
    } else {
        SpStatResult to_st = sp_stat(dst, true);
        if (st.error == SP_OK && to_st.error == SP_OK && st.sp_dev == to_st.sp_dev && st.sp_ino == to_st.sp_ino)
            return SP_ERR_SAME_FILE;

#ifdef SP_WINDOWS
        /* CopyFile2, as CPython copies files on Windows */
        HRESULT copied = CopyFile2(from.path, to.path, NULL);
        if (FAILED(copied))
            SetLastError(SP_PRIV_CAST(DWORD, HRESULT_CODE(copied)));
        err = SUCCEEDED(copied) ? SP_OK : sp_priv_last_error();
#else
        SpPrivFile in;
        SpPrivFile out;
        err = sp_priv_open(src, false, &in);
        if (err != SP_OK)
            return err;
        err = sp_priv_open(dst, true, &out);
        if (err == SP_OK) {
            err = sp_priv_copy_stream(in, out);
            SpError closed = sp_priv_close(out);
            err = err != SP_OK ? err : closed;
        }
        SpError closed = sp_priv_close(in);
        err = err != SP_OK ? err : closed;
#endif
    }

    if (err == SP_OK && preserve_metadata)
        err = st.error; /* the source's times and permissions are its stat result's */
    if (err == SP_OK && preserve_metadata && !sp_priv_copy_metadata(&st, to.path))
        err = sp_priv_last_error();
    return err;
}

/* The target of a copy or move: target itself, or with into, target / p's name (which p must have) */
static SpPath sp_priv_destination(const SpPath *p, const SpPath *target, bool into) {
    SpStr name = sp_priv_name_sv(p);
    if (p->error != SP_OK || target->error != SP_OK || !into)
        return p->error != SP_OK ? sp_priv_error_path(target->flavor, p->error) : *target;
    if (name.len == 0)
        return sp_priv_error_path(target->flavor, SP_ERR_NO_NAME);
    return sp_priv_join_len(target, name.data, name.len);
}

SpPath sp_copy(const SpPath *p, const SpPath *target, SpCopyOptions options) {
    SpPath dst = sp_priv_destination(p, target, options.into);
    SpError err = dst.error;
    if (err == SP_OK && sp_priv_relative_len(&dst, p) != SP_PRIV_CAST(size_t, -1))
        err = SP_ERR_SAME_FILE;

    SpPath tree = dst; /* extended in place for children as they are copied */
    if (err == SP_OK)
        err = sp_priv_copy_tree(p, &tree, options.follow_symlinks, options.preserve_metadata);
    return err == SP_OK ? dst : sp_priv_error_path(target->flavor, err);
}

/* CPython's Path._delete: symlinks and junctions are unlinked, directories removed recursively */
static SpError sp_priv_delete(const SpPath *p) {
    SpStatResult lst = sp_stat(p, false);
    bool dir = (sp_stat(p, true).sp_mode & SP_PRIV_IFMT) == SP_PRIV_IFDIR;
    if (!dir || (lst.sp_mode & SP_PRIV_IFMT) == SP_PRIV_IFLNK || lst.sp_reparse_tag == SP_PRIV_REPARSE_TAG_MOUNT_POINT)
        return sp_priv_remove_impl(p, false, false);

    void *handle = SP_PRIV_NULL;
    SpPath child;
    SpError err = SP_OK;
    while (err == SP_OK) {
        if (sp_priv_readdir_next(&handle, p, &child, SP_PRIV_NULL) == 0) {
            err = child.error; /* why the listing ended */
            break;
        }
        err = sp_priv_delete(&child);
    }
    sp_priv_readdir_close(&handle);

    return err != SP_OK ? err : sp_priv_remove_impl(p, true, false);
}

/* os.rename, or os.replace (replace): on POSIX a missing replace means an existing target is an error */
SpPath sp_rename(const SpPath *p, const SpPath *target, bool replace) {
    SpPrivNative src;
    SpPrivNative dst;
    SpError err = sp_priv_native(p, &src);
    SpError dst_err = sp_priv_native(target, &dst);
    if (err != SP_OK || dst_err != SP_OK)
        return sp_priv_error_path(target->flavor, err != SP_OK ? err : dst_err);

#ifdef SP_WINDOWS
    if (MoveFileExW(src.path, dst.path, replace ? MOVEFILE_REPLACE_EXISTING : 0))
        return *target;
#else
    /* os.rename would replace an existing target here; pathlib's rename refuses one unless it is the same file */
    if (!replace && sp_stat(target, true).error == SP_OK)
        return sp_samefile(p, target) ? *target : sp_priv_error_path(target->flavor, SP_ERR_EXISTS);
    if (rename(src.path, dst.path) == 0)
        return *target;
#endif
    return sp_priv_error_path(target->flavor, sp_priv_last_error());
}

/* Path.move: rename, or across filesystems copy (keeping symlinks and metadata) and delete */
SpPath sp_move(const SpPath *p, const SpPath *target, bool into) {
    SpPath dst = sp_priv_destination(p, target, into);
    if (dst.error == SP_OK && sp_samefile(p, &dst))
        return sp_priv_error_path(target->flavor, SP_ERR_SAME_FILE);

    SpPath r = dst.error != SP_OK ? dst : sp_rename(p, &dst, true);
    if (r.error != SP_ERR_CROSS_DEVICE)
        return r;

    SpPath tree = dst;
    SpError err = sp_priv_relative_len(&dst, p) != SP_PRIV_CAST(size_t, -1) ? SP_ERR_SAME_FILE
                                                                            : sp_priv_copy_tree(p, &tree, false, true);
    if (err == SP_OK)
        err = sp_priv_delete(p);
    return err == SP_OK ? dst : sp_priv_error_path(target->flavor, err);
}

static SpStr sp_priv_glob_part(const SpGlobIter *it, size_t pos) {
    const char *buf = it->priv_.pattern_buf;
    char sep = it->priv_.path.flavor == SP_FLAVOR_WINDOWS ? '\\' : '/';
    return SP_PRIV_STR(buf + pos, sp_priv_part_end(buf, it->priv_.pattern_len, pos, sep) - pos);
}

static bool sp_priv_part_is(SpStr part, const char *s) {
    return part.len == strlen(s) && memcmp(part.data, s, part.len) == 0;
}

/* A frame deeper than SP_GLOB_MAX_DEPTH stops the glob with SP_ERR_LIMIT */
static void sp_priv_glob_push(SpGlobIter *it, size_t from, size_t to, size_t root_len) {
    if (it->depth + 1 >= SP_GLOB_MAX_DEPTH) {
        it->error = SP_ERR_LIMIT;
        return;
    }
    it->depth++;
    it->priv_.stack[it->depth].handle = SP_PRIV_NULL;
    it->priv_.stack[it->depth].path_len = SP_PRIV_CAST(uint32_t, it->priv_.path.len);
    it->priv_.stack[it->depth].from = SP_PRIV_CAST(uint32_t, from);
    it->priv_.stack[it->depth].to = SP_PRIV_CAST(uint32_t, to);
    it->priv_.stack[it->depth].root_len = SP_PRIV_CAST(uint32_t, root_len);
}

/* Literal parts extend the path without a directory listing. Wildcards and recursive groups share one frame;
 * only recursive groups (of "**" alone) can also match the current path, before any children are listed. */
static bool sp_priv_glob_select(SpGlobIter *it, size_t seg, bool exists, bool trailing) {
    SpPath *path = &it->priv_.path;
    while (seg <= it->priv_.pattern_len) {
        SpStr part = sp_priv_glob_part(it, seg);
        size_t end = seg + part.len;
        bool special = part.len == 0 || sp_priv_part_is(part, ".."); /* a trailing separator's, or ".." */
        bool walk = sp_priv_part_is(part, "**");
        bool only_walks = walk;
        bool wildcard =
            memchr(part.data, '*', part.len) || memchr(part.data, '?', part.len) || memchr(part.data, '[', part.len);

        if (walk || (!special && (it->priv_.case_pedantic || wildcard))) {
            /* Group recursive selectors to avoid duplicate visits when following symlinks. */
            while (walk && end < it->priv_.pattern_len) {
                SpStr next = sp_priv_glob_part(it, end + 1);
                if (next.len == 0 || sp_priv_part_is(next, "..") ||
                    (!it->priv_.recurse_symlinks && !sp_priv_part_is(next, "**")))
                    break;
                only_walks = only_walks && sp_priv_part_is(next, "**");
                end += 1 + next.len;
            }
            sp_priv_glob_push(it, seg, end, path->len);
            if (!only_walks)
                return false;
        } else {
            if (part.len > 0 && it->error == SP_OK)
                it->error = sp_priv_join_child(path, part.data, part.len);
            exists = exists && special;
            trailing = part.len == 0 || end < it->priv_.pattern_len;
        }
        seg = end + 1;
    }

    if (it->error != SP_OK || exists)
        return it->error == SP_OK;
    SpStatResult st = sp_stat(path, trailing);
    return trailing ? (st.sp_mode & SP_PRIV_IFMT) == SP_PRIV_IFDIR : st.error == SP_OK;
}

/* An iterator for prefix + pattern with its first match selected. The pattern is compacted once (without empty and
 * '.' parts) but keeps separators, so recursive groups are contiguous pattern slices; a trailing separator adds a final
 * empty part. */
SpGlobIter sp_glob_begin(const SpPath *base, const char *pattern, SpGlobOptions options) {
    SpGlobIter it = SP_PRIV_ZERO;
    it.depth = -1;
    SpFlavor flavor = base->flavor;
    const char *prefix =
        options.recursive ? "**/" : ""; /* glob(join('**', pattern)); an anchored pattern is rejected */
    size_t plen = strlen(prefix);
    SpCaseSensitivity cs = options.case_sensitive;
    size_t len = strlen(pattern);
    it.error = base->error;
    if (it.error == SP_OK && sp_priv_split_anchor(pattern, len, flavor, NULL) > 0)
        it.error = SP_ERR_UNSUPPORTED;
    if (it.error == SP_OK && plen + len >= SP_GLOB_PATTERN_MAX)
        it.error = SP_ERR_LIMIT;
    if (it.error != SP_OK)
        return it;

    SP_ASSERT_PATH_INVARIANT(base);
    char *buf = it.priv_.pattern_buf;
    char sep = flavor == SP_FLAVOR_WINDOWS ? '\\' : '/';
    char last = len > 0 ? pattern[len - 1] : plen > 0 ? prefix[plen - 1] : '\0';
    memcpy(buf, prefix, plen);
    memcpy(buf + plen, pattern, len);
    len += plen;

    size_t n = 0;
    for (size_t pos = 0, end; pos < len; pos = end + 1) {
        while (pos < len && (buf[pos] == '/' || buf[pos] == sep))
            pos++;
        end = sp_priv_part_end(buf, len, pos, sep);
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
    it.priv_.case_insensitive = cs == SP_CASE_INSENSITIVE || (cs == SP_CASE_DEFAULT && flavor == SP_FLAVOR_WINDOWS);
    it.priv_.case_pedantic = cs != SP_CASE_DEFAULT;
    it.priv_.recurse_symlinks = options.recurse_symlinks;
    it.priv_.path = *base;
    return it;
}

bool sp_glob_next(SpGlobIter *it, SpPath *out) {
    SpPath *path = &it->priv_.path;
    if (it->error == SP_OK && !it->priv_.started) {
        it->priv_.started = true;
        it->priv_.pending = sp_priv_glob_select(it, 0, false, true);
    }
    while (it->error == SP_OK && !it->priv_.pending && it->depth >= 0) {
        size_t from = it->priv_.stack[it->depth].from;
        size_t to = it->priv_.stack[it->depth].to;
        size_t root_len = it->priv_.stack[it->depth].root_len;
        bool walk = sp_priv_part_is(sp_priv_glob_part(it, from), "**");
        bool last = to == it->priv_.pattern_len;

        path->len = it->priv_.stack[it->depth].path_len;
        path->buf[path->len] = '\0';
        /* Like pathlib, a directory that can't be listed has no matches; an entry too long for a path is an error */
        SpPath entry;
        unsigned int type;
        size_t n = sp_priv_readdir_next(&it->priv_.stack[it->depth].handle, path, &entry, &type);
        if (n == 0 && entry.error == SP_ERR_TOO_LONG)
            it->error = SP_ERR_TOO_LONG;
        if (n == 0) {
            sp_priv_readdir_close(&it->priv_.stack[it->depth].handle);
            it->depth--;
            continue;
        }

        /* The listing's type, or for a link followed (and a type the listing doesn't know) sp_stat's, called directly:
         * through sp_is, is_mount's chain puts glob past the frame limit on Windows */
        bool follow = !walk || it->priv_.recurse_symlinks;
        if ((walk || !last) && (type == 0 || (follow && type == SP_PRIV_IFLNK)))
            type = sp_stat(&entry, follow).sp_mode & SP_PRIV_IFMT;
        bool is_dir = (walk || !last) && type == SP_PRIV_IFDIR;
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

    if (it->error != SP_OK || !it->priv_.pending)
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
/* An environment variable as a raw path, read from the process environment (which Python's os.environ also
 * updates): SP_ERR_NO_HOME when it is unset */
static SpPath sp_priv_getenv_path(const wchar_t *name, SpFlavor flavor) {
    SpPath p = sp_priv_error_path(flavor, SP_OK);
    wchar_t value[SP_PATH_MAX];
    DWORD n = GetEnvironmentVariableW(name, value, SP_PATH_MAX);
    SpError err = n == 0 ? SP_ERR_NO_HOME : n >= SP_PATH_MAX ? SP_ERR_TOO_LONG : SP_OK;
    if (err == SP_OK)
        err = sp_priv_from_wide(value, n, p.buf, SP_PATH_MAX, &p.len);
    return err == SP_OK ? p : sp_priv_error_path(flavor, err);
}
#endif

/* os.path.expanduser("~" + user) as a path: SP_ERR_NO_HOME when the home directory is unknown */
static SpPath sp_priv_user_home(const char *user, size_t ulen, SpFlavor flavor) {
    SpPath err = sp_priv_error_path(flavor, SP_ERR_NO_HOME);
#ifdef SP_WINDOWS
    /* ntpath: USERPROFILE, else HOMEDRIVE joined with HOMEPATH */
    SpPath home = sp_priv_getenv_path(L"USERPROFILE", flavor);
    if (home.error == SP_ERR_NO_HOME) {
        SpPath drive = sp_priv_getenv_path(L"HOMEDRIVE", flavor);
        SpPath path = sp_priv_getenv_path(L"HOMEPATH", flavor);
        home = drive.error == SP_ERR_NO_HOME ? sp_priv_error_path(flavor, SP_OK) : drive;
        if (path.error != SP_OK)
            home = path;

        char last = home.len > 0 ? home.buf[home.len - 1] : '/';
        bool separated = !strchr("\\/:", last) && path.buf[0] != '\\' && path.buf[0] != '/';
        sp_priv_append(&home, path.buf, path.len, separated);
    }
    if (ulen == 0 || home.error != SP_OK)
        return home;

    /* Another user's home is guessed by swapping our name, the home's last part, for theirs */
    SpPath current = sp_priv_getenv_path(L"USERNAME", flavor);
    if (current.error == SP_OK && current.len == ulen && memcmp(current.buf, user, ulen) == 0)
        return home;

    size_t base = home.len;
    while (base > 0 && home.buf[base - 1] != '\\' && home.buf[base - 1] != '/')
        base--;
    if (current.error != SP_OK || home.len - base != current.len ||
        memcmp(home.buf + base, current.buf, current.len) != 0)
        return err;
    home.len = base;
    home.buf[base] = '\0';
    sp_priv_append(&home, user, ulen, false);
    return home;
#else
    /* posixpath: HOME (else the passwd entry) for the current user, the passwd entry for others */
    const char *dir = ulen == 0 ? getenv("HOME") : SP_PRIV_NULL;
    if (!dir) {
        char name[256];
        if (ulen >= sizeof(name))
            return err;
        memcpy(name, user, ulen);
        name[ulen] = '\0';

        struct passwd *pw = ulen == 0 ? getpwuid(getuid()) : getpwnam(name);
        if (!pw || !pw->pw_dir)
            return err;
        dir = pw->pw_dir;
    }

    size_t len = strlen(dir);
    while (len > 0 && dir[len - 1] == '/')
        len--;
    return len > 0 ? sp_priv_path_from_raw(dir, len, flavor) : sp_priv_path_from_raw("/", 1, flavor);
#endif
}

SpPath sp_home(SpFlavor flavor) {
    SpPath home = sp_priv_user_home("", 0, sp_priv_flavor(flavor));
    sp_priv_normalize(&home);
    return home;
}

/* Expands a leading "~" or "~user" part of a path without drive or root; SP_ERR_NO_HOME if its home is unknown */
SpPath sp_expanduser(const SpPath *p) {
    if (p->error != SP_OK || p->len == 0 || p->buf[0] != '~' || p->anchor > 0)
        return *p;

    size_t end = sp_priv_part_end(p->buf, p->len, 1, p->flavor == SP_FLAVOR_WINDOWS ? '\\' : '/');
    SpPath r = sp_priv_user_home(p->buf + 1, end - 1, p->flavor);
    sp_priv_normalize(&r);
    if (r.error != SP_OK || end >= p->len)
        return r;

    /* The remaining parts stay parts: a leading "./" keeps one like "c:" from parsing as a drive */
    SpPath rest = sp_priv_path_from_raw("./", 2, p->flavor);
    sp_priv_append(&rest, p->buf + end + 1, p->len - end - 1, false);
    return rest.error != SP_OK ? rest : sp_priv_join_len(&r, rest.buf, rest.len);
}

/* The file owner's name in the user database: SP_ERR_NOT_FOUND when it has none there */
SpTerm sp_owner(const SpPath *p, bool follow_symlinks) {
#ifdef SP_WINDOWS
    (void)follow_symlinks;
    return sp_priv_term(NULL, 0, p->error != SP_OK ? p->error : SP_ERR_UNSUPPORTED);
#else
    SpStatResult st = sp_stat(p, follow_symlinks);
    struct passwd *pw = st.error == SP_OK ? getpwuid(st.sp_uid) : SP_PRIV_NULL;
    const char *name = pw ? pw->pw_name : SP_PRIV_NULL;
    if (!name)
        return sp_priv_term(NULL, 0, st.error != SP_OK ? st.error : SP_ERR_NOT_FOUND);

    size_t len = strlen(name);
    return sp_priv_term(name, len, len < SP_PATH_MAX ? SP_OK : SP_ERR_TOO_LONG);
#endif
}

/* The file group's name in the group database: SP_ERR_NOT_FOUND when it has none there */
SpTerm sp_group(const SpPath *p, bool follow_symlinks) {
#ifdef SP_WINDOWS
    (void)follow_symlinks;
    return sp_priv_term(NULL, 0, p->error != SP_OK ? p->error : SP_ERR_UNSUPPORTED);
#else
    SpStatResult st = sp_stat(p, follow_symlinks);
    struct group *gr = st.error == SP_OK ? getgrgid(st.sp_gid) : SP_PRIV_NULL;
    const char *name = gr ? gr->gr_name : SP_PRIV_NULL;
    if (!name)
        return sp_priv_term(NULL, 0, st.error != SP_OK ? st.error : SP_ERR_NOT_FOUND);

    size_t len = strlen(name);
    return sp_priv_term(name, len, len < SP_PATH_MAX ? SP_OK : SP_ERR_TOO_LONG);
#endif
}

SpIterdirIter sp_iterdir_begin(const SpPath *p) {
    SpIterdirIter it = SP_PRIV_ZERO;
    it.dir = *p;

    SpPrivNative native;
    it.error = sp_priv_native(p, &native);
#ifdef SP_WINDOWS
    /* FindFirstFile opens the listing with its first entry, on the first next(): check the directory now */
    SpStatResult st = sp_stat(p, true);
    if (it.error == SP_OK)
        it.error = st.error != SP_OK ? st.error : (st.sp_mode & SP_PRIV_IFMT) != SP_PRIV_IFDIR ? SP_ERR_NOT_DIR : SP_OK;
#else
    if (it.error == SP_OK && !(it.priv_.handle = opendir(native.path)))
        it.error = sp_priv_last_error();
#endif
    return it;
}

bool sp_iterdir_next(SpIterdirIter *it, SpPath *out) {
    if (it->error != SP_OK || it->priv_.done)
        return false;
    if (sp_priv_readdir_next(&it->priv_.handle, &it->dir, out, SP_PRIV_NULL) > 0)
        return true;

    it->priv_.done = true;
    it->error = out->error;
    return false;
}

void sp_iterdir_end(SpIterdirIter *it) {
    sp_priv_readdir_close(&it->priv_.handle);
    it->priv_.done = true;
}

/* A walk keeps a level for each directory on the way down in the caller's buffer: this header, then the names (each
 * NUL-terminated after a byte telling a directory, 'd', from anything else), then pointers to them */
typedef struct {
    size_t prev;    /* the level of the directory above, as an offset into the buffer */
    size_t dir_len; /* this directory's path length */
    size_t next;    /* its next subdirectory to walk */
    char **dirnames;
    size_t dirname_count;
    char **filenames;
    size_t filename_count;
} SpPrivWalkLevel;

/* The entry shows a level's names */
static void sp_priv_walk_show(SpWalkEntry *e, const SpPrivWalkLevel *level) {
    e->dirnames = level->dirnames;
    e->dirname_count = level->dirname_count;
    e->filenames = level->filenames;
    e->filename_count = level->filename_count;
}

static int sp_priv_walk_name_cmp(const void *a, const void *b) {
    return strcmp(*SP_PRIV_CAST(char *const *, a), *SP_PRIV_CAST(char *const *, b));
}

/* Lists entry.dirpath into a new level on top of the buffer's stack (levels start at pointer-aligned offsets) and points
 * the entry at its names. The listing's error leaves no level; a buffer too small for it is SP_ERR_TOO_LONG in
 * it->error. */
static SpError sp_priv_walk_list(SpWalkIter *it) {
    SpWalkEntry *e = &it->entry;
    char *buf = it->priv_.buf;
    SpPrivWalkLevel *level = SP_PRIV_CAST(SpPrivWalkLevel *, SP_PRIV_CAST(void *, buf + it->priv_.used));
    size_t names = it->priv_.used + sizeof(SpPrivWalkLevel);
    size_t used = names;
    size_t count = 0;
    bool follow = it->priv_.options.follow_symlinks;

    void *handle = SP_PRIV_NULL;
    SpPath child;
    unsigned int type;
    for (size_t n; (n = sp_priv_readdir_next(&handle, &e->dirpath, &child, &type)) > 0; count++) {
        /* The listing's type, or for a link followed (and a type the listing doesn't know) sp_stat's */
        if (type == 0 || (follow && type == SP_PRIV_IFLNK))
            type = sp_stat(&child, follow).sp_mode & SP_PRIV_IFMT;
        if (used + n + 2 > it->priv_.size) {
            it->error = SP_ERR_TOO_LONG;
            break;
        }
        buf[used] = type == SP_PRIV_IFDIR ? 'd' : 'f';
        memcpy(buf + used + 1, child.buf + child.len - n, n + 1);
        used += n + 2;
    }
    sp_priv_readdir_close(&handle);
    if (it->error != SP_OK || child.error != SP_OK)
        return it->error != SP_OK ? it->error : child.error;

    /* The pointers after the names: subdirectories from the front, the rest from the back */
    size_t ptrs = (used + sizeof(char *) - 1) / sizeof(char *) * sizeof(char *);
    if (names > it->priv_.size || ptrs + count * sizeof(char *) > it->priv_.size) {
        it->error = SP_ERR_TOO_LONG;
        return it->error;
    }
    char **list = SP_PRIV_CAST(char **, SP_PRIV_CAST(void *, buf + ptrs));
    size_t dirs = 0;
    size_t files = 0;
    for (size_t at = names; at < used; at += strlen(buf + at + 1) + 2) {
        if (buf[at] == 'd')
            list[dirs++] = buf + at + 1;
        else
            list[count - ++files] = buf + at + 1;
    }
    qsort(list, dirs, sizeof(char *), sp_priv_walk_name_cmp);
    qsort(list + dirs, files, sizeof(char *), sp_priv_walk_name_cmp);

    level->prev = it->priv_.level;
    level->dir_len = e->dirpath.len;
    level->next = 0;
    level->dirnames = list;
    level->dirname_count = dirs;
    level->filenames = list + dirs;
    level->filename_count = files;
    it->priv_.level = it->priv_.used;
    it->priv_.used = ptrs + count * sizeof(char *);
    sp_priv_walk_show(e, level);
    return SP_OK;
}

SpWalkIter sp_walk_begin(const SpPath *top, SpWalkOptions options, void *buf, size_t buf_size) {
    SpWalkIter it = SP_PRIV_ZERO;
    size_t pad = (sizeof(char *) - SP_PRIV_PTR_BITS(buf) % sizeof(char *)) % sizeof(char *);
    pad = pad < buf_size ? pad : buf_size;

    it.entry.dirpath = *top;
    it.priv_.buf = SP_PRIV_CAST(char *, buf) + pad;
    it.priv_.size = buf_size - pad;
    it.priv_.level = SP_PRIV_CAST(size_t, -1);
    it.priv_.options = options;
    it.priv_.pending = true;
    it.error = top->error;
    return it;
}

/* os.walk: a directory is listed before it is walked; top-down it is given out right after its listing (so the caller
 * can prune its dirnames), bottom-up after its subdirectories */
SpWalkEntry *sp_walk_next(SpWalkIter *it) {
    SpWalkEntry *e = &it->entry;
    bool top_down = !it->priv_.options.bottom_up;

    /* The listing given out last, top-down: its subdirectories are what the caller left in dirnames */
    if (it->priv_.prunable) {
        SpPrivWalkLevel *level = SP_PRIV_CAST(SpPrivWalkLevel *, SP_PRIV_CAST(void *, it->priv_.buf + it->priv_.level));
        level->dirnames = e->dirnames;
        level->dirname_count = e->dirname_count;
        it->priv_.prunable = false;
    }

    while (it->error == SP_OK) {
        if (it->priv_.pending) {
            it->priv_.pending = false;
            e->error = sp_priv_walk_list(it);
            if (it->error != SP_OK)
                return SP_PRIV_NULL;
            if (e->error != SP_OK) {
                SpPrivWalkLevel none = SP_PRIV_ZERO;
                sp_priv_walk_show(e, &none);
                return e;
            }
            it->priv_.prunable = top_down;
            if (top_down)
                return e;
            continue;
        }
        if (it->priv_.level == SP_PRIV_CAST(size_t, -1))
            return SP_PRIV_NULL;

        /* The innermost level's next subdirectory, or else that level is done */
        SpPrivWalkLevel *level = SP_PRIV_CAST(SpPrivWalkLevel *, SP_PRIV_CAST(void *, it->priv_.buf + it->priv_.level));
        e->dirpath.len = level->dir_len;
        e->dirpath.buf[level->dir_len] = '\0';
        if (level->next < level->dirname_count) {
            const char *name = level->dirnames[level->next++];
            it->error = sp_priv_join_child(&e->dirpath, name, strlen(name));
            it->priv_.pending = true;
            continue;
        }

        it->priv_.used = it->priv_.level;
        it->priv_.level = level->prev;
        if (!top_down) {
            e->error = SP_OK;
            sp_priv_walk_show(e, level);
            return e;
        }
    }
    return SP_PRIV_NULL;
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

/* A chain inside another (or after one left without a terminator) would overwrite the running chain's path: both
 * carry SP_ERR_NESTED_CHAIN instead, and the next terminator ends the error */
SpPrivDontUseThisDirectly_ *sp_fluent_init_(SpPath p) {
    if (sp_priv_f_ctx_active)
        return sp_priv_f_chain(sp_priv_error_path(p.flavor, SP_ERR_NESTED_CHAIN));

    sp_priv_f_ctx_active = true;
    return sp_priv_f_chain(p);
}

#endif /* SNAKEPATH_FLUENT */

#ifdef __cplusplus
}
#endif

#endif /* SNAKEPATH_IMPLEMENTATION */
