/* test.c - Rigorous pathlib tests for snakepath.h (plus the fluent API with -DSNAKEPATH_FLUENT).
 * With -DSP_FFI -shared it builds the library the Python bindings (snakepath.py) load instead. */
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS  /* Disable fopen deprecation warning on MSVC */
#endif
#ifdef _WIN32
#define SP_PATH_MAX SP_PATH_MAX_WINDOWS
#else
#define SP_PATH_MAX SP_PATH_MAX_LINUX
#endif
#define SNAKEPATH_IMPLEMENTATION
#include "snakepath.h"

#ifdef SP_FFI

#ifdef _WIN32
#define SP_EXPORT __declspec(dllexport)
#else
#define SP_EXPORT __attribute__((visibility("default")))
#endif

/* One wrapper per library function the Python bindings (snakepath.py) call, results through pointers */

#define WRAP_CONST(name) \
    SP_EXPORT int sp_const_##name(void) { return name; }

#define WRAP_TERM(fn) \
    SP_EXPORT void sp_##fn##_wrap(const SpPath *p, SpTerm *out) { *out = sp_##fn(p); }

#define WRAP_PATH_UNARY(fn) \
    SP_EXPORT void sp_##fn##_wrap(const SpPath *p, SpPath *out) { *out = sp_##fn(p); }

#define WRAP_PATH_CSTR(fn) \
    SP_EXPORT void sp_##fn##_wrap(const SpPath *p, const char *s, SpPath *out) { *out = sp_##fn(p, s); }

#define WRAP_PATH_BINARY(fn) \
    SP_EXPORT void sp_##fn##_wrap(const SpPath *p, const SpPath *other, SpPath *out) { *out = sp_##fn(p, other); }

#define WRAP_BOOL_UNARY(fn) \
    SP_EXPORT int sp_##fn##_wrap(const SpPath *p) { return sp_##fn(p) ? 1 : 0; }

#define WRAP_BOOL_FOLLOW(fn) \
    SP_EXPORT int sp_##fn##_wrap(const SpPath *p, int follow_symlinks) { return sp_##fn(p, follow_symlinks != 0) ? 1 : 0; }

#define WRAP_BOOL_BINARY(fn) \
    SP_EXPORT int sp_##fn##_wrap(const SpPath *a, const SpPath *b) { return sp_##fn(a, b) ? 1 : 0; }

/* Constants */
WRAP_CONST(SP_OK)
WRAP_CONST(SP_ERR_IO)
WRAP_CONST(SP_ERR_NOT_FOUND)
WRAP_CONST(SP_ERR_EXISTS)
WRAP_CONST(SP_ERR_NOT_DIR)
WRAP_CONST(SP_ERR_IS_DIR)
WRAP_CONST(SP_ERR_NOT_EMPTY)
WRAP_CONST(SP_ERR_PERMISSION)
WRAP_CONST(SP_ERR_LOOP)
WRAP_CONST(SP_ERR_NOT_LINK)
WRAP_CONST(SP_ERR_CROSS_DEVICE)
WRAP_CONST(SP_ERR_SAME_FILE)
WRAP_CONST(SP_ERR_NO_HOME)
WRAP_CONST(SP_ERR_TOO_LONG)
WRAP_CONST(SP_ERR_LIMIT)
WRAP_CONST(SP_ERR_NUL)
WRAP_CONST(SP_ERR_INVALID_ARG)
WRAP_CONST(SP_ERR_NO_NAME)
WRAP_CONST(SP_ERR_NOT_RELATIVE)
WRAP_CONST(SP_ERR_NOT_ABSOLUTE)
WRAP_CONST(SP_ERR_UNSUPPORTED)
WRAP_CONST(SP_MKDIR_PARENTS)
WRAP_CONST(SP_MKDIR_EXIST_OK)
WRAP_CONST(SP_COPY_FOLLOW_SYMLINKS)
WRAP_CONST(SP_COPY_PRESERVE_METADATA)
WRAP_CONST(SP_WALK_TOP_DOWN)
WRAP_CONST(SP_WALK_FOLLOW_SYMLINKS)
WRAP_CONST(SP_CASE_DEFAULT)
WRAP_CONST(SP_CASE_SENSITIVE)
WRAP_CONST(SP_CASE_INSENSITIVE)
WRAP_CONST(SP_PATH_MAX)
WRAP_CONST(SP_MAX_SUFFIXES)
SP_EXPORT const char *sp_error_str_wrap(int error) { return sp_error_str((SpError)error); }

/* Struct sizes, for the opaque ones */
SP_EXPORT size_t sp_sizeof_parts_iter(void) { return sizeof(SpPartsIter); }
SP_EXPORT size_t sp_sizeof_parents_iter(void) { return sizeof(SpParentsIter); }
SP_EXPORT size_t sp_sizeof_glob_iter(void) { return sizeof(SpGlobIter); }
SP_EXPORT size_t sp_sizeof_iterdir_iter(void) { return sizeof(SpIterdirIter); }
SP_EXPORT size_t sp_sizeof_walk_iter(void) { return sizeof(SpWalkIter); }

/* Making and converting paths */
SP_EXPORT void sp_path_new_len_wrap(const char *s, size_t len, int flavor, SpPath *out) {
    *out = sp_path_from_n(s, len, (SpFlavor)flavor);
}
SP_EXPORT void sp_path_convert_wrap(const char *s, int src_flavor, int dest_flavor, SpPath *out) {
    *out = sp_path_convert(s, (SpFlavor)src_flavor, (SpFlavor)dest_flavor);
}
SP_EXPORT void sp_path_copy_wrap(const SpPath *p, SpPath *out) { *out = *p; }
SP_EXPORT const char *sp_str_wrap(const SpPath *p) { return sp_str(p); }
SP_EXPORT void sp_cwd_wrap(int flavor, SpPath *out) { *out = sp_cwd((SpFlavor)flavor); }
SP_EXPORT void sp_home_wrap(int flavor, SpPath *out) { *out = sp_home((SpFlavor)flavor); }
SP_EXPORT void sp_from_uri_wrap(const char *uri, int flavor, SpPath *out) { *out = sp_from_uri(uri, (SpFlavor)flavor); }

/* Path -> text */
WRAP_TERM(drive)
WRAP_TERM(root)
WRAP_TERM(anchor)
WRAP_TERM(name)
WRAP_TERM(stem)
WRAP_TERM(suffix)
WRAP_TERM(as_posix)
SP_EXPORT void sp_suffixes_wrap(const SpPath *p, SpSuffixes *out) { *out = sp_suffixes(p); }
SP_EXPORT int sp_as_uri_wrap(const SpPath *p, char *buf, size_t buf_size) { return sp_as_uri(p, buf, buf_size); }
SP_EXPORT void sp_owner_wrap(const SpPath *p, int follow_symlinks, SpTerm *out) { *out = sp_owner(p, follow_symlinks != 0); }
SP_EXPORT void sp_group_wrap(const SpPath *p, int follow_symlinks, SpTerm *out) { *out = sp_group(p, follow_symlinks != 0); }

/* Path -> path */
WRAP_PATH_UNARY(parent)
WRAP_PATH_UNARY(absolute)
WRAP_PATH_UNARY(expanduser)
WRAP_PATH_UNARY(readlink)
WRAP_PATH_CSTR(with_name)
WRAP_PATH_CSTR(with_stem)
WRAP_PATH_CSTR(with_suffix)
WRAP_PATH_BINARY(joinpath)
WRAP_PATH_BINARY(rename)
WRAP_PATH_BINARY(replace)
WRAP_PATH_BINARY(move)
WRAP_PATH_BINARY(move_into)
SP_EXPORT void sp_join_one_len_wrap(const SpPath *p, const char *s, size_t len, SpPath *out) {
    *out = sp_join_n(p, s, len);
}
SP_EXPORT void sp_with_segments_wrap(const SpPath *p, const char **parts, size_t parts_count, SpPath *out) {
    *out = sp_with_segments(p, parts, parts_count);
}
SP_EXPORT void sp_relative_to_wrap(const SpPath *p, const SpPath *other, int walk_up, SpPath *out) {
    *out = sp_relative_to(p, other, walk_up != 0);
}
SP_EXPORT void sp_resolve_wrap(const SpPath *p, int strict, SpPath *out) { *out = sp_resolve(p, strict != 0); }
SP_EXPORT void sp_copy_wrap(const SpPath *p, const SpPath *target, unsigned int flags, SpPath *out) {
    *out = sp_copy(p, target, flags);
}
SP_EXPORT void sp_copy_into_wrap(const SpPath *p, const SpPath *target_dir, unsigned int flags, SpPath *out) {
    *out = sp_copy_into(p, target_dir, flags);
}

/* Parts and parents */
SP_EXPORT void sp_parts_iter_begin_wrap(const SpPath *p, SpPartsIter *out) { *out = sp_parts_begin(p); }
SP_EXPORT int sp_parts_iter_next_wrap(SpPartsIter *it, SpStr *out) { return sp_parts_next(it, out) ? 1 : 0; }
SP_EXPORT void sp_parents_iter_begin_wrap(const SpPath *p, SpParentsIter *out) { *out = sp_parents_begin(p); }
SP_EXPORT int sp_parents_iter_next_wrap(SpParentsIter *it, SpPath *out) { return sp_parents_next(it, out) ? 1 : 0; }

/* Comparisons and matching */
WRAP_BOOL_UNARY(is_absolute)
WRAP_BOOL_BINARY(path_eq)
WRAP_BOOL_BINARY(is_relative_to)
SP_EXPORT int sp_path_cmp_wrap(const SpPath *a, const SpPath *b) { return sp_path_cmp(a, b); }
SP_EXPORT unsigned long sp_path_hash_wrap(const SpPath *p) { return sp_path_hash(p); }
SP_EXPORT int sp_match_wrap(const SpPath *p, const char *pattern, int cs) {
    return sp_match(p, pattern, (SpCaseSensitivity)cs) ? 1 : 0;
}
SP_EXPORT int sp_full_match_wrap(const SpPath *p, const char *pattern, int cs) {
    return sp_full_match(p, pattern, (SpCaseSensitivity)cs) ? 1 : 0;
}

/* The file system */
WRAP_BOOL_FOLLOW(is_file)
WRAP_BOOL_FOLLOW(is_dir)
WRAP_BOOL_FOLLOW(exists)
WRAP_BOOL_UNARY(is_symlink)
WRAP_BOOL_UNARY(is_block_device)
WRAP_BOOL_UNARY(is_char_device)
WRAP_BOOL_UNARY(is_fifo)
WRAP_BOOL_UNARY(is_socket)
WRAP_BOOL_UNARY(is_mount)
WRAP_BOOL_UNARY(is_junction)
WRAP_BOOL_BINARY(samefile)
SP_EXPORT void sp_stat_wrap(const SpPath *p, SpStatResult *out) { *out = sp_stat(p); }
SP_EXPORT void sp_lstat_wrap(const SpPath *p, SpStatResult *out) { *out = sp_lstat(p); }
SP_EXPORT int sp_stat_eq_wrap(const SpStatResult *a, const SpStatResult *b) { return sp_stat_eq(a, b) ? 1 : 0; }
SP_EXPORT int sp_mkdir_wrap(const SpPath *p, unsigned int mode, unsigned int flags, unsigned int parent_mode) {
    return sp_mkdir(p, mode, flags, parent_mode);
}
SP_EXPORT int sp_touch_wrap(const SpPath *p, unsigned int mode, int exist_ok) { return sp_touch(p, mode, exist_ok != 0); }
SP_EXPORT int sp_unlink_wrap(const SpPath *p, int missing_ok) { return sp_unlink(p, missing_ok != 0); }
SP_EXPORT int sp_rmdir_wrap(const SpPath *p) { return sp_rmdir(p); }
SP_EXPORT int sp_chmod_wrap(const SpPath *p, unsigned int mode, int follow_symlinks) {
    return sp_chmod(p, mode, follow_symlinks != 0);
}
SP_EXPORT int sp_symlink_to_wrap(const SpPath *p, const SpPath *target, int target_is_directory) {
    return sp_symlink_to(p, target, target_is_directory != 0);
}
SP_EXPORT int sp_hardlink_to_wrap(const SpPath *p, const SpPath *target) { return sp_hardlink_to(p, target); }
SP_EXPORT void sp_read_file_wrap(const SpPath *p, char *buf, size_t buf_size, SpIOResult *out) {
    *out = sp_read_file(p, buf, buf_size);
}
SP_EXPORT void sp_write_file_wrap(const SpPath *p, const char *data, size_t data_len, SpIOResult *out) {
    *out = sp_write_file(p, data, data_len);
}

/* Iterators */
SP_EXPORT void sp_iterdir_begin_wrap(const SpPath *p, SpIterdirIter *out) { *out = sp_iterdir_begin(p); }
SP_EXPORT int sp_iterdir_next_wrap(SpIterdirIter *it, SpPath *out) { return sp_iterdir_next(it, out) ? 1 : 0; }
SP_EXPORT void sp_iterdir_end_wrap(SpIterdirIter *it) { sp_iterdir_end(it); }
SP_EXPORT int sp_iterdir_error_wrap(const SpIterdirIter *it) { return it->error; }
SP_EXPORT void sp_glob_begin_wrap(const SpPath *p, const char *pattern, int cs, int recurse_symlinks, SpGlobIter *out) {
    *out = sp_glob_begin(p, pattern, (SpCaseSensitivity)cs, recurse_symlinks != 0);
}
SP_EXPORT void sp_rglob_begin_wrap(const SpPath *p, const char *pattern, int cs, int recurse_symlinks, SpGlobIter *out) {
    *out = sp_rglob_begin(p, pattern, (SpCaseSensitivity)cs, recurse_symlinks != 0);
}
SP_EXPORT int sp_glob_next_wrap(SpGlobIter *it, SpPath *out) { return sp_glob_next(it, out) ? 1 : 0; }
SP_EXPORT void sp_glob_end_wrap(SpGlobIter *it) { sp_glob_end(it); }
SP_EXPORT int sp_glob_error_wrap(const SpGlobIter *it) { return it->error; }
SP_EXPORT void sp_walk_begin_wrap(const SpPath *top, unsigned int flags, void *buf, size_t buf_size, SpWalkIter *out) {
    *out = sp_walk_begin(top, flags, buf, buf_size);
}
SP_EXPORT SpWalkEntry *sp_walk_next_wrap(SpWalkIter *it) { return sp_walk_next(it); }
SP_EXPORT int sp_walk_error_wrap(const SpWalkIter *it) { return it->error; }

#else /* the tests */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Platform-specific rmdir and getpid for test cleanup (not the library's sp_rmdir) */
#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define test_rmdir _rmdir
#define test_getpid _getpid
#else
#include <unistd.h>
#define test_rmdir rmdir
#define test_getpid getpid
#endif

static int tests_run = 0;
static char test_dir_mkdir_temp[64];
static char test_dir_mkdir_nested[64];
static char test_dir_glob[64];

/* Inline string view comparison (like nob_sv_eq) */
static int sv_eq(SpStr a, const char *b) {
    size_t blen = strlen(b);
    return a.len == blen && (a.len == 0 || memcmp(a.data, b, a.len) == 0);
}

/* SpTerm comparison */
static int term_eq(SpTerm a, const char *b) {
    size_t blen = strlen(b);
    return a.len == blen && (a.len == 0 || memcmp(a.buf, b, a.len) == 0);
}

#define ARRAY_LEN(a) (sizeof(a)/sizeof((a)[0]))
/* ASSERT_SIZE: like ASSERT but suppresses MSVC C4127 (constant conditional) for sizeof checks */
#define ASSERT_SIZE(actual, expected) do { size_t a_ = (actual), e_ = (expected); ASSERT(a_ == e_); } while(0)

#define ASSERT(cond) do { tests_run++; if (!(cond)) { fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); exit(1); } } while(0)
#define ASSERT_SV(sv, exp) ASSERT(sv_eq(sv, exp))
#define ASSERT_TERM(t, exp) ASSERT(term_eq(t, exp))
#define ASSERT_PATH(p, exp) do { SpPath _p = (p); ASSERT(strcmp(sp_str(&_p), exp) == 0); } while(0)
#define ASSERT_ABS(path, flav, expected) do { SpPath _p = sp_path_f(path, flav); ASSERT(sp_is_absolute(&_p) == expected); } while(0)

/* Table-driven tests */
typedef struct { const char *path; const char *expected; } SVTest;
typedef struct { const char *path; const char *arg; const char *expected; } JoinTest;

#define P SP_FLAVOR_POSIX
#define W SP_FLAVOR_WINDOWS

static void test_term(SpFlavor f, SpTerm (*fn)(const SpPath*), SVTest *tests, size_t n) {
    for (size_t i = 0; i < n; i++) {
        SpPath p = sp_path_f(tests[i].path, f);
        ASSERT_TERM(fn(&p), tests[i].expected);
    }
}

static void test_path(SpFlavor f, SpPath (*fn)(const SpPath*), SVTest *tests, size_t n) {
    for (size_t i = 0; i < n; i++) {
        SpPath p = sp_path_f(tests[i].path, f);
        ASSERT_PATH(fn(&p), tests[i].expected);
    }
}

static void test_join(SpFlavor f, JoinTest *tests, size_t n) {
    for (size_t i = 0; i < n; i++) {
        SpPath p = sp_path_f(tests[i].path, f);
        ASSERT_PATH(sp_join_one(&p, tests[i].arg), tests[i].expected);
    }
}

static void test_with(SpFlavor f, SpPath (*fn)(const SpPath*, const char*), JoinTest *tests, size_t n) {
    for (size_t i = 0; i < n; i++) {
        SpPath p = sp_path_f(tests[i].path, f);
        ASSERT_PATH(fn(&p, tests[i].arg), tests[i].expected);
    }
}

#ifdef SNAKEPATH_FLUENT
/* Fluent API tests: built with -DSNAKEPATH_FLUENT, run after everything in main().
 * Each test references Python pathlib documentation snippets.
 * https://docs.python.org/3/library/pathlib.html */
#define ASSERT_STR(got, exp) ASSERT(strcmp(got, exp) == 0)
#define ASSERT_FLUENT(f, exp) do { SpPath _p = (f)->path(); ASSERT(strcmp(sp_str(&_p), exp) == 0); } while(0)

static void test_fluent_api(void) {
    printf("\nFluent API Tests:\n");

    /* ============ Pure Path Creation ============ */

    /* >>> PurePath('setup.py') -> PurePosixPath('setup.py') */
    ASSERT_FLUENT(SPF("setup.py"), "setup.py");

    /* A chain started while another is running (here, one left without a terminator) carries SP_ERR_NESTED_CHAIN,
     * and its terminator ends the error */
    {
        SpPrivDontUseThisDirectly_ *unterminated = SPF("a")->join("b");
        ASSERT(unterminated != NULL);
        SpPath nested = SPF("c")->join("d")->path();
        ASSERT(nested.error == SP_ERR_NESTED_CHAIN);
        ASSERT_FLUENT(SPF("f"), "f");
    }

    /* >>> PurePath('foo', 'some/path', 'bar') -> PurePosixPath('foo/some/path/bar') */
    ASSERT_FLUENT(SPF_P("foo")->join("some/path")->join("bar"), "foo/some/path/bar");

    /* >>> PurePosixPath('/etc/hosts') -> PurePosixPath('/etc/hosts') */
    ASSERT_FLUENT(SPF_P("/etc/hosts"), "/etc/hosts");

    /* >>> PureWindowsPath('c:/', 'Users', 'Ximénez') -> PureWindowsPath('c:/Users/Ximénez') */
    ASSERT_FLUENT(SPF_W("c:/")->join("Users")->join("Ximenez"), "c:\\Users\\Ximenez");

    /* SPF_PATH: Start fluent chain from existing SpPath */
    { SpPath p = sp_path_f("/existing/path", SP_FLAVOR_POSIX); ASSERT_FLUENT(SPF_PATH(p)->join("child"), "/existing/path/child"); }
    { SpPath p = sp_path_f("C:/existing/path", SP_FLAVOR_WINDOWS); ASSERT_FLUENT(SPF_PATH(p)->join("child"), "C:\\existing\\path\\child"); }

    /* ============ Drive ============ */

    /* >>> PureWindowsPath('c:/Program Files/').drive -> 'c:' */
    { SpPath p = SPF_W("c:/Program Files/")->path(); ASSERT_TERM(sp_drive(&p), "c:"); }

    /* >>> PurePosixPath('/etc').drive -> '' */
    { SpPath p = SPF_P("/etc")->path(); ASSERT_TERM(sp_drive(&p), ""); }

    /* >>> PureWindowsPath('//host/share/foo.txt').drive -> '\\\\host\\share' */
    { SpPath p = SPF_W("//host/share/foo.txt")->path(); ASSERT_TERM(sp_drive(&p), "\\\\host\\share"); }

    /* ============ Root ============ */

    /* >>> PureWindowsPath('c:/Program Files/').root -> '\\' */
    { SpPath p = SPF_W("c:/Program Files/")->path(); ASSERT_TERM(sp_root(&p), "\\"); }

    /* >>> PurePosixPath('/etc').root -> '/' */
    { SpPath p = SPF_P("/etc")->path(); ASSERT_TERM(sp_root(&p), "/"); }

    /* >>> PureWindowsPath('c:Program Files/').root -> '' */
    { SpPath p = SPF_W("c:Program Files/")->path(); ASSERT_TERM(sp_root(&p), ""); }

    /* ============ Anchor ============ */

    /* >>> PureWindowsPath('c:/Program Files/').anchor -> 'c:\\' */
    { SpPath p = SPF_W("c:/Program Files/")->path(); ASSERT_TERM(sp_anchor(&p), "c:\\"); }

    /* >>> PurePosixPath('/etc').anchor -> '/' */
    { SpPath p = SPF_P("/etc")->path(); ASSERT_TERM(sp_anchor(&p), "/"); }

    /* ============ Parent ============ */

    /* >>> PurePosixPath('/a/b/c/d').parent -> PurePosixPath('/a/b/c') */
    ASSERT_FLUENT(SPF_P("/a/b/c/d")->parent(), "/a/b/c");

    /* >>> PurePosixPath('/').parent -> PurePosixPath('/') */
    ASSERT_FLUENT(SPF_P("/")->parent(), "/");

    /* >>> PurePosixPath('.').parent -> PurePosixPath('.') */
    ASSERT_FLUENT(SPF_P(".")->parent(), ".");

    /* ============ Name ============ */

    /* >>> PurePosixPath('my/library/setup.py').name -> 'setup.py' */
    { SpPath p = SPF_P("my/library/setup.py")->path(); ASSERT_TERM(sp_name(&p), "setup.py"); }

    /* >>> PureWindowsPath('//some/share/setup.py').name -> 'setup.py' */
    { SpPath p = SPF_W("//some/share/setup.py")->path(); ASSERT_TERM(sp_name(&p), "setup.py"); }

    /* >>> PureWindowsPath('//some/share').name -> '' */
    { SpPath p = SPF_W("//some/share")->path(); ASSERT_TERM(sp_name(&p), ""); }

    /* ============ Suffix ============ */

    /* >>> PurePosixPath('my/library/setup.py').suffix -> '.py' */
    { SpPath p = SPF_P("my/library/setup.py")->path(); ASSERT_TERM(sp_suffix(&p), ".py"); }

    /* >>> PurePosixPath('my/library.tar.gz').suffix -> '.gz' */
    { SpPath p = SPF_P("my/library.tar.gz")->path(); ASSERT_TERM(sp_suffix(&p), ".gz"); }

    /* >>> PurePosixPath('my/library').suffix -> '' */
    { SpPath p = SPF_P("my/library")->path(); ASSERT_TERM(sp_suffix(&p), ""); }

    /* ============ Suffixes ============ */

    /* >>> PurePosixPath('my/library.tar.gz').suffixes -> ['.tar', '.gz'] */
    { SpPath p = SPF_P("my/library.tar.gz")->path();
      SpSuffixes s = sp_suffixes(&p);
      ASSERT(s.count == 2); ASSERT_SV(s.items[0], ".tar"); ASSERT_SV(s.items[1], ".gz"); }

    /* >>> PurePosixPath('my/library').suffixes -> [] */
    { SpPath p = SPF_P("my/library")->path(); ASSERT(sp_suffixes(&p).count == 0); }

    /* ============ Stem ============ */

    /* >>> PurePosixPath('my/library.tar.gz').stem -> 'library.tar' */
    { SpPath p = SPF_P("my/library.tar.gz")->path(); ASSERT_TERM(sp_stem(&p), "library.tar"); }

    /* >>> PurePosixPath('my/library.tar').stem -> 'library' */
    { SpPath p = SPF_P("my/library.tar")->path(); ASSERT_TERM(sp_stem(&p), "library"); }

    /* ============ as_posix ============ */

    /* >>> PureWindowsPath('c:\\windows').as_posix() -> 'c:/windows' */
    { SpPath p = SPF_W("c:\\windows")->path(); ASSERT_TERM(sp_as_posix(&p), "c:/windows"); }

    /* ============ is_absolute ============ */

    /* >>> PurePosixPath('/a/b').is_absolute() -> True */
    { SpPath p = SPF_P("/a/b")->path(); ASSERT(sp_is_absolute(&p) == true); }

    /* >>> PurePosixPath('a/b').is_absolute() -> False */
    { SpPath p = SPF_P("a/b")->path(); ASSERT(sp_is_absolute(&p) == false); }

    /* >>> PureWindowsPath('c:/a/b').is_absolute() -> True */
    { SpPath p = SPF_W("c:/a/b")->path(); ASSERT(sp_is_absolute(&p) == true); }

    /* >>> PureWindowsPath('/a/b').is_absolute() -> False */
    { SpPath p = SPF_W("/a/b")->path(); ASSERT(sp_is_absolute(&p) == false); }

    /* >>> PureWindowsPath('c:').is_absolute() -> False */
    { SpPath p = SPF_W("c:")->path(); ASSERT(sp_is_absolute(&p) == false); }

    /* >>> PureWindowsPath('//some/share').is_absolute() -> True */
    { SpPath p = SPF_W("//server/share/a")->path(); ASSERT(sp_is_absolute(&p) == true); }

    /* ============ joinpath ============ */

    /* >>> PurePosixPath('/etc').joinpath('passwd') -> PurePosixPath('/etc/passwd') */
    ASSERT_FLUENT(SPF_P("/etc")->join("passwd"), "/etc/passwd");

    /* >>> PurePosixPath('/etc').joinpath('init.d', 'apache2') -> PurePosixPath('/etc/init.d/apache2') */
    ASSERT_FLUENT(SPF_P("/etc")->join("init.d")->join("apache2"), "/etc/init.d/apache2");

    /* >>> PureWindowsPath('c:').joinpath('/Program Files') -> PureWindowsPath('c:/Program Files') */
    ASSERT_FLUENT(SPF_W("c:")->join("/Program Files"), "c:\\Program Files");

    /* ============ with_segments ============ */

    { const char *parts[] = {"foo", "bar"};
      ASSERT_FLUENT(SPF_P("ignored")->with_segments(parts, SP_ARRAY_LEN(parts)), "foo/bar"); }

    /* ============ with_name ============ */

    /* >>> PureWindowsPath('c:/Downloads/pathlib.tar.gz').with_name('setup.py') -> 'c:/Downloads/setup.py' */
    ASSERT_FLUENT(SPF_W("c:/Downloads/pathlib.tar.gz")->with_name("setup.py"), "c:\\Downloads\\setup.py");

    /* ============ with_stem ============ */

    /* >>> PureWindowsPath('c:/Downloads/draft.txt').with_stem('final') -> 'c:/Downloads/final.txt' */
    ASSERT_FLUENT(SPF_W("c:/Downloads/draft.txt")->with_stem("final"), "c:\\Downloads\\final.txt");

    /* >>> PureWindowsPath('c:/Downloads/pathlib.tar.gz').with_stem('lib') -> 'c:/Downloads/lib.gz' */
    ASSERT_FLUENT(SPF_W("c:/Downloads/pathlib.tar.gz")->with_stem("lib"), "c:\\Downloads\\lib.gz");

    /* ============ with_suffix ============ */

    /* >>> PureWindowsPath('c:/Downloads/pathlib.tar.gz').with_suffix('.bz2') -> 'c:/Downloads/pathlib.tar.bz2' */
    ASSERT_FLUENT(SPF_W("c:/Downloads/pathlib.tar.gz")->with_suffix(".bz2"), "c:\\Downloads\\pathlib.tar.bz2");

    /* >>> PureWindowsPath('README').with_suffix('.txt') -> PureWindowsPath('README.txt') */
    ASSERT_FLUENT(SPF_P("a/README")->with_suffix(".txt"), "a/README.txt");

    /* >>> PureWindowsPath('README.txt').with_suffix('') -> PureWindowsPath('README') */
    ASSERT_FLUENT(SPF_P("a/README.txt")->with_suffix(""), "a/README");

    /* ============ is_relative_to ============ */

    /* >>> PurePath('/etc/passwd').is_relative_to('/etc') -> True */
    { SpPath p = SPF_P("/etc/passwd")->path(); SpPath base = sp_path_f("/etc", SP_FLAVOR_POSIX);
      ASSERT(sp_is_relative_to(&p, &base) == true); }

    /* >>> PurePath('/etc/passwd').is_relative_to('/usr') -> False */
    { SpPath p = SPF_P("/etc/passwd")->path(); SpPath base = sp_path_f("/usr", SP_FLAVOR_POSIX);
      ASSERT(sp_is_relative_to(&p, &base) == false); }

    /* ============ eq / ne (fluent terminators) ============ */

    /* Equal POSIX paths */
    { SpPath other = sp_path_f("/etc/passwd", SP_FLAVOR_POSIX);
      ASSERT(SPF_P("/etc/passwd")->eq(&other) == true);
      ASSERT(SPF_P("/etc/passwd")->ne(&other) == false); }

    /* Unequal POSIX paths */
    { SpPath other = sp_path_f("/etc/shadow", SP_FLAVOR_POSIX);
      ASSERT(SPF_P("/etc/passwd")->eq(&other) == false);
      ASSERT(SPF_P("/etc/passwd")->ne(&other) == true); }

    /* Windows case-insensitive equality */
    { SpPath other = sp_path_f("C:\\Users\\FOO", SP_FLAVOR_WINDOWS);
      ASSERT(SPF_W("C:\\Users\\foo")->eq(&other) == true);
      ASSERT(SPF_W("C:\\Users\\foo")->ne(&other) == false); }

    /* eq/ne after chaining */
    { SpPath expected = sp_path_f("/a/b", SP_FLAVOR_POSIX);
      ASSERT(SPF_P("/a/b/c")->parent()->eq(&expected) == true);
      ASSERT(SPF_P("/a/b/c")->parent()->ne(&expected) == false); }

    /* ============ samefile (fluent terminator) ============ */

    /* Same file via same path */
    { SpPath self = sp_path(__FILE__);
      ASSERT(SPF(__FILE__)->samefile(&self) == true); }

    /* ============ relative_to (fluent chainable) ============ */

    /* >>> PurePosixPath('/etc/passwd').relative_to('/') -> PurePosixPath('etc/passwd') */
    { SpPath base = sp_path_f("/", SP_FLAVOR_POSIX);
      ASSERT_FLUENT(SPF_P("/etc/passwd")->relative_to(&base, false), "etc/passwd"); }

    /* >>> PurePosixPath('/etc/passwd').relative_to('/etc') -> PurePosixPath('passwd') */
    { SpPath base = sp_path_f("/etc", SP_FLAVOR_POSIX);
      ASSERT_FLUENT(SPF_P("/etc/passwd")->relative_to(&base, false), "passwd"); }

    /* >>> PurePosixPath('/etc/passwd').relative_to('/usr', walk_up=True) -> '../etc/passwd' */
    { SpPath base = sp_path_f("/usr", SP_FLAVOR_POSIX);
      ASSERT_FLUENT(SPF_P("/etc/passwd")->relative_to(&base, true), "../etc/passwd"); }

    /* ============ absolute (fluent chainable) ============ */

    /* Path('foo/bar').absolute() returns absolute path */
    /* Use native flavor - POSIX flavor on Windows CWD won't be a valid POSIX absolute */
    { SpPath p = SPF("foo/bar")->absolute()->path();
      ASSERT(sp_is_absolute(&p) == true); }

    /* ============ expanduser (fluent chainable) ============ */

    /* Path('~/foo').expanduser() returns expanded path */
    { SpPath p = SPF("~")->expanduser()->path();
      ASSERT(sp_is_absolute(&p) == true); }

    /* Path('~/subdir').expanduser() returns absolute path */
    { SpPath p = SPF("~/subdir")->expanduser()->path();
      ASSERT(sp_is_absolute(&p) == true);
      ASSERT_TERM(sp_name(&p), "subdir"); }

    /* ============ owner/group (fluent terminators) ============ */

#ifndef _WIN32
    /* Path(__FILE__).owner() returns non-empty string (POSIX only) */
    { SpTerm o = SPF(__FILE__)->owner(true);
      ASSERT(o.len > 0); }

    /* Path(__FILE__).group() returns non-empty string (POSIX only) */
    { SpTerm g = SPF(__FILE__)->group(true);
      ASSERT(g.len > 0); }
#endif

    /* ============ Chaining ============ */

    /* Path('/a/b/c.txt').parent.name -> 'b' */
    { SpPath p = SPF_P("/a/b/c.txt")->parent()->path(); ASSERT_TERM(sp_name(&p), "b"); }

    /* Path('/a/b/c').parent.parent -> '/a' */
    ASSERT_FLUENT(SPF_P("/a/b/c")->parent()->parent(), "/a");

    /* (Path('/a') / 'b' / 'c').parent -> '/a/b' */
    ASSERT_FLUENT(SPF_P("/a")->join("b")->join("c")->parent(), "/a/b");

    /* Path('a/b.txt').with_name('c.py').suffix -> '.py' */
    { SpPath p = SPF_P("a/b.txt")->with_name("c.py")->path(); ASSERT_TERM(sp_suffix(&p), ".py"); }

    /* Path('a/b.txt').with_suffix('.md').stem -> 'b' */
    { SpPath p = SPF_P("a/b.txt")->with_suffix(".md")->path(); ASSERT_TERM(sp_stem(&p), "b"); }

    /* ============ Branching from common base ============ */

    /* Use non-fluent API or repeat prefix for branching */
    SpPath base = SPF_P("/home/user")->path();
    SpPath docs = sp_join_one(&base, "Documents");
    SpPath pics = sp_join_one(&base, "Pictures");
    ASSERT_STR(sp_str(&docs), "/home/user/Documents");
    ASSERT_STR(sp_str(&pics), "/home/user/Pictures");

    /* ============ is_file (fluent) ============ */

    /* Existing file should return true */
    { SpPath p = SPF(__FILE__)->path(); ASSERT(sp_is_file(&p, true) == true); }

    /* Non-existent file should return false */
    ASSERT(SPF("/nonexistent/file.txt")->is_file(true) == false);

    /* ============ is_dir (fluent) ============ */

    /* Existing directory should return true */
    { SpPath p = SPF(".")->path(); ASSERT(sp_is_dir(&p, true) == true); }

    /* Non-existent directory should return false */
    ASSERT(SPF("/nonexistent/dir")->is_dir(true) == false);

    /* ============ exists (fluent) ============ */

    /* Existing file should return true */
    ASSERT(SPF(__FILE__)->exists(true) == true);

    /* Existing directory should return true */
    ASSERT(SPF(".")->exists(true) == true);

    /* Non-existent path should return false */
    ASSERT(SPF("/nonexistent/path")->exists(true) == false);

    /* ============ read_file / write_file (fluent) ============ */

    {
        long fpid = (long)test_getpid();
        char fluent_rw_path[128];
        snprintf(fluent_rw_path, sizeof(fluent_rw_path), "./test_fluent_rw_%ld.tmp", fpid);

        SpIOResult wr = SPF(fluent_rw_path)->write_file("fluent io", 9);
        ASSERT(wr.error == SP_OK);
        ASSERT(wr.bytes == 9);

        char rbuf[64];
        SpIOResult rd = SPF(fluent_rw_path)->read_file(rbuf, sizeof(rbuf));
        ASSERT(rd.error == SP_OK);
        ASSERT(rd.bytes == 9);
        ASSERT(memcmp(rbuf, "fluent io", 9) == 0);

        SpPath cleanup = sp_path(fluent_rw_path);
        ASSERT(sp_unlink(&cleanup, true) == SP_OK);
    }

    /* ============ sp_error_str / sp_error_print ============ */

    ASSERT_STR(sp_error_str(SP_OK), "Success");
    ASSERT_STR(sp_error_str(SP_ERR_IO), "Input/output error");
    ASSERT_STR(sp_error_str(SP_ERR_EXISTS), "File exists");
    ASSERT_STR(sp_error_str(SP_ERR_NOT_FOUND), "No such file or directory");
    ASSERT_STR(sp_error_str(SP_ERR_NOT_DIR), "Not a directory");
    ASSERT_STR(sp_error_str(SP_ERR_PERMISSION), "Permission denied");
    ASSERT_STR(sp_error_str(SP_ERR_TOO_LONG), "Result too long for its buffer");
    ASSERT_STR(sp_error_str(SP_ERR_NOT_RELATIVE), "Path is not relative to the other path");
    ASSERT_STR(sp_error_str(SP_ERR_NO_NAME), "Path has an empty name");
    ASSERT_STR(sp_error_str(SP_ERR_INVALID_ARG), "Invalid argument");
    ASSERT_STR(sp_error_str(SP_ERR_NESTED_CHAIN), "Fluent chain started inside another chain");
    ASSERT_STR(sp_error_str((SpError)9999), "Unknown error");

    /* ============ stat / lstat (fluent) ============ */

    { SpStatResult st = SPF(__FILE__)->stat();
      ASSERT(st.error == SP_OK);
      ASSERT(st.sp_size > 0); }

    { SpStatResult st = SPF(__FILE__)->lstat();
      ASSERT(st.error == SP_OK);
      ASSERT(st.sp_size > 0); }

    /* Non-existent file stat */
    { SpStatResult st = SPF("/nonexistent_stat_test")->stat();
      ASSERT(st.error != SP_OK); }

    /* ============ as_posix (fluent) ============ */

    ASSERT_TERM(SPF_W("c:\\windows\\system32")->as_posix(), "c:/windows/system32");

    /* ============ as_uri (fluent) ============ */

    { char buf[SP_PATH_MAX];
      ASSERT(SPF_P("/etc/hosts")->as_uri(buf, sizeof(buf)) == SP_OK);
      ASSERT_STR(buf, "file:///etc/hosts"); }

    /* ============ match (fluent) ============ */

    ASSERT(SPF_P("/foo/bar.py")->match("*.py"));
    ASSERT(SPF_P("/foo/bar.py")->match("*.txt") == false);
    ASSERT(SPF_P("/foo/bar.py")->match("foo/*.py"));
    ASSERT(SPF_P("/foo/bar.py")->full_match("/**/*.py"));
    ASSERT(!SPF_P("/foo/bar.py")->full_match("*.py"));

    /* ============ resolve (fluent chainable) ============ */

    { SpPath p = SPF(".")->resolve(false)->path();
      ASSERT(sp_is_absolute(&p) == true); }

    /* resolve then chain */
    { SpPath p = SPF(".")->resolve(false)->join("child")->path();
      ASSERT(sp_is_absolute(&p) == true); }

    /* ============ readlink (fluent chainable) ============ */

#ifndef _WIN32
    {
        long fpid = (long)test_getpid();
        char link_path[128], target_path[128];
        snprintf(target_path, sizeof(target_path), "test_readlink_target_%ld.tmp", fpid);
        snprintf(link_path, sizeof(link_path), "test_readlink_link_%ld.tmp", fpid);

        /* Create target file and symlink */
        SpPath tp = sp_path(target_path);
        ASSERT(sp_touch(&tp, 0644, true) == SP_OK);
        SpPath lp = sp_path(link_path);
        ASSERT(sp_symlink_to(&lp, &tp, false) == SP_OK);

        /* readlink should return the target */
        { SpPath rl = SPF(link_path)->readlink()->path();
          ASSERT(rl.len > 0); }

        ASSERT(sp_unlink(&lp, true) == SP_OK);
        ASSERT(sp_unlink(&tp, true) == SP_OK);
    }
#endif

    /* ============ rename / replace (fluent chainable) ============ */

    {
        long fpid = (long)test_getpid();
        char src_path[128], dst_path[128];
        snprintf(src_path, sizeof(src_path), "test_rename_src_%ld.tmp", fpid);
        snprintf(dst_path, sizeof(dst_path), "test_rename_dst_%ld.tmp", fpid);

        /* Create source, rename it */
        SpPath srcp = sp_path(src_path);
        ASSERT(sp_touch(&srcp, 0644, true) == SP_OK);
        SpPath dp = sp_path(dst_path);

        SpPath result = SPF(src_path)->rename(&dp)->path();
        ASSERT_STR(sp_str(&result), dst_path);

        /* Clean up */
        SpPath cleanup = sp_path(dst_path);
        ASSERT(sp_unlink(&cleanup, true) == SP_OK);
    }

    {
        long fpid = (long)test_getpid();
        char src_path[128], dst_path[128];
        snprintf(src_path, sizeof(src_path), "test_replace_src_%ld.tmp", fpid);
        snprintf(dst_path, sizeof(dst_path), "test_replace_dst_%ld.tmp", fpid);

        /* Create both files, replace dst with src */
        SpPath srcp2 = sp_path(src_path);
        ASSERT(sp_touch(&srcp2, 0644, true) == SP_OK);
        SpPath dp2 = sp_path(dst_path);
        ASSERT(sp_touch(&dp2, 0644, true) == SP_OK);

        SpPath result = SPF(src_path)->replace(&dp2)->path();
        ASSERT_STR(sp_str(&result), dst_path);

        /* Clean up */
        SpPath cleanup = sp_path(dst_path);
        ASSERT(sp_unlink(&cleanup, true) == SP_OK);
    }

    /* ============ mkdir (fluent) ============ */

    {
        long fpid = (long)test_getpid();
        char dir_path[128];
        snprintf(dir_path, sizeof(dir_path), "test_fluent_mkdir_%ld", fpid);

        ASSERT(SPF(dir_path)->mkdir(0755, 0, SP_MODE_DIR) == SP_OK);

        /* mkdir again without exist_ok should fail */
        ASSERT(SPF(dir_path)->mkdir(0755, 0, SP_MODE_DIR) == SP_ERR_EXISTS);

        /* mkdir with exist_ok should succeed */
        ASSERT(SPF(dir_path)->mkdir(0755, SP_MKDIR_EXIST_OK, SP_MODE_DIR) == SP_OK);

        SpPath cleanup = sp_path(dir_path);
        ASSERT(sp_rmdir(&cleanup) == SP_OK);
    }

    /* ============ touch (fluent) ============ */

    {
        long fpid = (long)test_getpid();
        char touch_path[128];
        snprintf(touch_path, sizeof(touch_path), "test_fluent_touch_%ld.tmp", fpid);

        ASSERT(SPF(touch_path)->touch(0644, true) == SP_OK);

        /* Verify file exists */
        ASSERT(SPF(touch_path)->exists(true) == true);

        SpPath cleanup = sp_path(touch_path);
        ASSERT(sp_unlink(&cleanup, true) == SP_OK);
    }

    /* ============ unlink (fluent) ============ */

    {
        long fpid = (long)test_getpid();
        char unlink_path[128];
        snprintf(unlink_path, sizeof(unlink_path), "test_fluent_unlink_%ld.tmp", fpid);

        SpPath p = sp_path(unlink_path);
        ASSERT(sp_touch(&p, 0644, true) == SP_OK);

        ASSERT(SPF(unlink_path)->unlink(false) == SP_OK);

        /* Unlink non-existent without missing_ok should fail */
        ASSERT(SPF(unlink_path)->unlink(false) == SP_ERR_NOT_FOUND);

        /* Unlink non-existent with missing_ok should succeed */
        ASSERT(SPF(unlink_path)->unlink(true) == SP_OK);
    }

    /* ============ rmdir (fluent) ============ */

    {
        long fpid = (long)test_getpid();
        char rmdir_path[128];
        snprintf(rmdir_path, sizeof(rmdir_path), "test_fluent_rmdir_%ld", fpid);

        SpPath p = sp_path(rmdir_path);
        ASSERT(sp_mkdir(&p, 0755, 0, SP_MODE_DIR) == SP_OK);

        ASSERT(SPF(rmdir_path)->rmdir() == SP_OK);

        /* Rmdir non-existent should fail */
        ASSERT(SPF(rmdir_path)->rmdir() == SP_ERR_NOT_FOUND);
    }

    /* ============ chmod (fluent) ============ */

#ifndef _WIN32
    {
        long fpid = (long)test_getpid();
        char chmod_path[128];
        snprintf(chmod_path, sizeof(chmod_path), "test_fluent_chmod_%ld.tmp", fpid);

        SpPath p = sp_path(chmod_path);
        ASSERT(sp_touch(&p, 0644, true) == SP_OK);

        ASSERT(SPF(chmod_path)->chmod(0600, true) == SP_OK);

        ASSERT(sp_unlink(&p, true) == SP_OK);
    }
#endif

    /* ============ symlink_to / hardlink_to (fluent) ============ */

#ifndef _WIN32
    {
        long fpid = (long)test_getpid();
        char target_path[128], sym_path[128], hard_path[128];
        snprintf(target_path, sizeof(target_path), "test_fluent_symtgt_%ld.tmp", fpid);
        snprintf(sym_path, sizeof(sym_path), "test_fluent_sym_%ld.tmp", fpid);
        snprintf(hard_path, sizeof(hard_path), "test_fluent_hard_%ld.tmp", fpid);

        SpPath tp = sp_path(target_path);
        ASSERT(sp_touch(&tp, 0644, true) == SP_OK);

        /* symlink_to */
        ASSERT(SPF(sym_path)->symlink_to(&tp, false) == SP_OK);
        ASSERT(SPF(sym_path)->is_symlink() == true);

        /* hardlink_to */
#ifdef __ANDROID__
        ASSERT(SPF(hard_path)->hardlink_to(&tp) == SP_ERR_UNSUPPORTED);
#else
        ASSERT(SPF(hard_path)->hardlink_to(&tp) == SP_OK);
#endif

        SpPath sp2 = sp_path(sym_path);
        SpPath hp = sp_path(hard_path);
        ASSERT(sp_unlink(&sp2, true) == SP_OK);
        ASSERT(sp_unlink(&hp, true) == SP_OK);
        ASSERT(sp_unlink(&tp, true) == SP_OK);
    }
#endif

    printf("  All fluent API tests OK\n");
}
#endif

int main(void) {
    /* Initialize unique test directory names based on PID to avoid race conditions */
#ifdef __cplusplus
    long pid = static_cast<long>(test_getpid());
#else
    long pid = (long)test_getpid();
#endif
    snprintf(test_dir_mkdir_temp, sizeof(test_dir_mkdir_temp), "./test_mkdir_temp_%ld", pid);
    snprintf(test_dir_mkdir_nested, sizeof(test_dir_mkdir_nested), "./test_mkdir_nested_%ld", pid);
    snprintf(test_dir_glob, sizeof(test_dir_glob), "./test_glob_dir_%ld", pid);

    printf("POSIX Tests:\n");
    
    SVTest posix_anchor[] = {{"", ""}, {"a/b", ""}, {"/", "/"}, {"/a/b", "/"}};
    test_term(P, sp_anchor, posix_anchor, ARRAY_LEN(posix_anchor));

    SVTest posix_drive[] = {{"/a/b", ""}};
    test_term(P, sp_drive, posix_drive, ARRAY_LEN(posix_drive));

    SVTest posix_root[] = {{"a/b", ""}, {"/a/b", "/"}};
    test_term(P, sp_root, posix_root, ARRAY_LEN(posix_root));

    SVTest posix_name[] = {{"", ""}, {".", ""}, {"..", ".."}, {"/", ""}, {"a/b", "b"}, {"a/b.py", "b.py"}};
    test_term(P, sp_name, posix_name, ARRAY_LEN(posix_name));

    SVTest posix_stem[] = {{"", ""}, {"a/b", "b"}, {"a/b.py", "b"}, {"a/.hgrc", ".hgrc"}, {"a/b.tar.gz", "b.tar"}};
    test_term(P, sp_stem, posix_stem, ARRAY_LEN(posix_stem));

    SVTest posix_suffix[] = {{"a/b.py", ".py"}, {"a/.hgrc", ""}, {"a/.hg.rc", ".rc"}, {"a/b.tar.gz", ".gz"}};
    test_term(P, sp_suffix, posix_suffix, ARRAY_LEN(posix_suffix));
    
    SVTest posix_parent[] = {{"a/b/c", "a/b"}, {"/a/b/c", "/a/b"}, {"/", "/"}, {"a", "."}};
    test_path(P, sp_parent, posix_parent, ARRAY_LEN(posix_parent));
    
    JoinTest posix_join[] = {{"a/b", "c", "a/b/c"}, {"a/b", "/c", "/c"}};
    test_join(P, posix_join, ARRAY_LEN(posix_join));
    
    JoinTest posix_with_name[] = {{"a/b", "d.xml", "a/d.xml"}};
    test_with(P, sp_with_name, posix_with_name, ARRAY_LEN(posix_with_name));
    
    JoinTest posix_with_stem[] = {{"a/b.py", "d", "a/d.py"}};
    test_with(P, sp_with_stem, posix_with_stem, ARRAY_LEN(posix_with_stem));
    
    JoinTest posix_with_suffix[] = {{"a/b.py", ".gz", "a/b.gz"}, {"a/b", ".gz", "a/b.gz"}, {"a/b.py", "", "a/b"}};
    test_with(P, sp_with_suffix, posix_with_suffix, ARRAY_LEN(posix_with_suffix));
    
    SpPath posix_ws_base = sp_path_f("ignored", P);
    const char *posix_ws1[] = {"a", "b"};
    ASSERT_PATH(sp_with_segments(&posix_ws_base, posix_ws1, ARRAY_LEN(posix_ws1)), "a/b");
    const char *posix_ws2[] = {"a", "/b", "c"};
    ASSERT_PATH(sp_with_segments(&posix_ws_base, posix_ws2, ARRAY_LEN(posix_ws2)), "/b/c");
    const char **posix_ws3 = NULL;
    ASSERT_PATH(sp_with_segments(&posix_ws_base, posix_ws3, 0), ".");

    ASSERT_ABS("/a/b", P, true); ASSERT_ABS("a/b", P, false);
    
    SpPath ps1 = sp_path_f("a/b.py", P); SpSuffixes ss1 = sp_suffixes(&ps1);
    ASSERT(ss1.count == 1); ASSERT_SV(ss1.items[0], ".py");
    
    SpPath ps2 = sp_path_f("a/b.tar.gz", P); SpSuffixes ss2 = sp_suffixes(&ps2);
    ASSERT(ss2.count == 2); ASSERT_SV(ss2.items[0], ".tar"); ASSERT_SV(ss2.items[1], ".gz");
    
    SpPath ps3 = sp_path_f("a/.hgrc", P); ASSERT(sp_suffixes(&ps3).count == 0);
    
    SpPath pp1 = sp_path_f("a/b", P); SpPartsIter it1 = sp_parts_begin(&pp1); SpStr part;
    ASSERT(sp_parts_next(&it1, &part)); ASSERT_SV(part, "a");
    ASSERT(sp_parts_next(&it1, &part)); ASSERT_SV(part, "b");
    ASSERT(!sp_parts_next(&it1, &part));
    ASSERT_SV(part, "b"); /* Exhaustion must leave the output untouched. */
    
    SpPath pp2 = sp_path_f("/a/b", P); SpPartsIter it2 = sp_parts_begin(&pp2);
    ASSERT(sp_parts_next(&it2, &part)); ASSERT_SV(part, "/");
    ASSERT(sp_parts_next(&it2, &part)); ASSERT_SV(part, "a");
    ASSERT(sp_parts_next(&it2, &part)); ASSERT_SV(part, "b");
    ASSERT(!sp_parts_next(&it2, &part));
    
    SpPath ppc = sp_path_f("/a/b/c", P); ASSERT(sp_parts_count(&ppc) == 4);
    
    SpPath pp3 = sp_path_f("a/b/c", P); SpParentsIter pit = sp_parents_begin(&pp3); SpPath parent;
    ASSERT(sp_parents_next(&pit, &parent)); ASSERT_PATH(parent, "a/b");
    ASSERT(sp_parents_next(&pit, &parent)); ASSERT_PATH(parent, "a");
    ASSERT(sp_parents_next(&pit, &parent)); ASSERT_PATH(parent, ".");
    ASSERT(!sp_parents_next(&pit, &parent));
    
#ifndef __cplusplus
    SpPath pjm = sp_path_f("a", P); ASSERT_PATH(sp_join(&pjm, "b", "c"), "a/b/c");
#endif
    
    SpPath pr1 = sp_path_f("a/b", P), po1 = sp_path_f("a", P); ASSERT(sp_is_relative_to(&pr1, &po1));
    SpPath pr2 = sp_path_f("a/b", P), po2 = sp_path_f("c", P); ASSERT(!sp_is_relative_to(&pr2, &po2));
    SpPath pr3 = sp_path_f("a/b/c", P), po3 = sp_path_f("a", P); ASSERT_PATH(sp_relative_to(&pr3, &po3, false), "b/c");
    SpPath pr4 = sp_path_f("a/b", P), po4 = sp_path_f("a/b", P); ASSERT_PATH(sp_relative_to(&pr4, &po4, false), ".");  /* C returns "." for display, but Python returns "" */
    
    ASSERT_PATH(sp_path_f("a//b", P), "a/b");
    ASSERT_PATH(sp_path_f("a/b/", P), "a/b");
    
    SpPath ea = sp_path_f("a/b", P), eb = sp_path_f("a/b", P); ASSERT(sp_path_eq(&ea, &eb));
    
    SpPath pap = sp_path_f("a/b/c", P);
    ASSERT_TERM(sp_as_posix(&pap), "a/b/c");
    
    printf("  POSIX tests OK\n");
    
    printf("\nWindows Tests:\n");
    
    SVTest win_drive[] = {{"C:/a/b", "C:"}, {"/a/b", ""}};
    test_term(W, sp_drive, win_drive, ARRAY_LEN(win_drive));

    SVTest win_root[] = {{"C:/a/b", "\\"}, {"C:a/b", ""}};
    test_term(W, sp_root, win_root, ARRAY_LEN(win_root));

    SVTest win_anchor[] = {{"C:/a/b", "C:\\"}, {"C:a/b", "C:"}};
    test_term(W, sp_anchor, win_anchor, ARRAY_LEN(win_anchor));

    SVTest win_name[] = {{"C:/a/b.py", "b.py"}};
    test_term(W, sp_name, win_name, ARRAY_LEN(win_name));

    SVTest win_suffix[] = {{"C:/a/b.py", ".py"}};
    test_term(W, sp_suffix, win_suffix, ARRAY_LEN(win_suffix));
    
    SVTest win_parent[] = {{"C:/a/b/c", "C:\\a\\b"}};
    test_path(W, sp_parent, win_parent, ARRAY_LEN(win_parent));
    
    JoinTest win_join[] = {
        {"C:/a/b", "x/y", "C:\\a\\b\\x\\y"}, {"C:/a/b", "/x/y", "C:\\x\\y"},
        {"C:/a/b", "D:/x/y", "D:\\x\\y"}, {"C:/a/b", "c:x/y", "c:\\a\\b\\x\\y"}
    };
    test_join(W, win_join, ARRAY_LEN(win_join));
    
    /* A drive without a root takes the name directly, like PureWindowsPath('c:x').with_name('y') -> 'c:y' */
    JoinTest win_with_name[] = {{"C:/a/b", "d.xml", "C:\\a\\d.xml"}, {"c:x", "y", "c:y"}, {"c:x/y", "z", "c:x\\z"},
                                {"c:/x", "y", "c:\\y"}, {"//s/h/x", "y", "\\\\s\\h\\y"}};
    test_with(W, sp_with_name, win_with_name, ARRAY_LEN(win_with_name));
    
    JoinTest win_with_stem[] = {{"c:x.py", "y", "c:y.py"}};
    test_with(W, sp_with_stem, win_with_stem, ARRAY_LEN(win_with_stem));
    
    JoinTest win_with_suffix[] = {{"C:/a/b.py", ".gz", "C:\\a\\b.gz"}, {"c:x.py", ".gz", "c:x.gz"}};
    test_with(W, sp_with_suffix, win_with_suffix, ARRAY_LEN(win_with_suffix));
    
    /* Windows compares like str.lower(): every script, U+0130 to i + combining dot, and a word-final sigma */
    {
        SpPath cafe = sp_path_f("Caf\xc3\xa9", W), upper = sp_path_f("CAF\xc3\x89", W);
        SpPath dotted = sp_path_f("\xc4\xb0", W), plain_i = sp_path_f("I", W), i_dot = sp_path_f("i\xcc\x87", W);
        SpPath final_sigma = sp_path_f("\xce\x91\xce\xa3", W), final_small = sp_path_f("\xce\xb1\xcf\x82", W);
        SpPath medial_small = sp_path_f("\xce\xb1\xcf\x83", W);
        ASSERT(sp_path_eq(&cafe, &upper) && sp_path_hash(&cafe) == sp_path_hash(&upper));
        ASSERT(!sp_path_eq(&dotted, &plain_i) && sp_path_eq(&dotted, &i_dot));
        ASSERT(sp_path_eq(&final_sigma, &final_small) && !sp_path_eq(&final_sigma, &medial_small));
        SpPath sub = sp_path_f("CAF\xc3\x89\\x", W);
        ASSERT(sp_is_relative_to(&sub, &cafe));
        ASSERT_PATH(sp_relative_to(&sub, &cafe, false), "x");
    }
    /* A drive is any one character, then ':' */
    {
        SpPath e_drive = sp_path_f("\xc3\xa9:\\x", W);
        ASSERT_TERM(sp_drive(&e_drive), "\xc3\xa9:");
        ASSERT(sp_is_absolute(&e_drive));
    }
    /* Case-insensitive matching follows re.IGNORECASE, including its extra case groups (s and long s, k and Kelvin) */
    {
        SpPath longs = sp_path_f("\xc5\xbf", P), kelvin = sp_path_f("\xe2\x84\xaa", P), sigma = sp_path_f("\xce\xa3", P);
        ASSERT(sp_full_match(&longs, "S", SP_CASE_INSENSITIVE) && sp_full_match(&kelvin, "k", SP_CASE_INSENSITIVE) && sp_full_match(&sigma, "[\xcf\x83]", SP_CASE_INSENSITIVE));
        ASSERT(!sp_full_match(&kelvin, "k", SP_CASE_SENSITIVE));
    }

    SpPath win_ws_base = sp_path_f("ignored", W);
    const char *win_ws1[] = {"C:/", "Users", "Bob"};
    ASSERT_PATH(sp_with_segments(&win_ws_base, win_ws1, ARRAY_LEN(win_ws1)), "C:\\Users\\Bob");
    const char *win_ws2[] = {"C:/a", "D:/x", "y"};
    ASSERT_PATH(sp_with_segments(&win_ws_base, win_ws2, ARRAY_LEN(win_ws2)), "D:\\x\\y");
    const char **win_ws3 = NULL;
    ASSERT_PATH(sp_with_segments(&win_ws_base, win_ws3, 0), ".");

    SpPath wp1 = sp_path_f("c:a/b", W); SpPartsIter wit1 = sp_parts_begin(&wp1);
    ASSERT(sp_parts_next(&wit1, &part)); ASSERT_SV(part, "c:");
    ASSERT(sp_parts_next(&wit1, &part)); ASSERT_SV(part, "a");
    ASSERT(sp_parts_next(&wit1, &part)); ASSERT_SV(part, "b");
    ASSERT(!sp_parts_next(&wit1, &part));
    
    SpPath wp2 = sp_path_f("c:/a/b", W); SpPartsIter wit2 = sp_parts_begin(&wp2);
    ASSERT(sp_parts_next(&wit2, &part)); ASSERT_SV(part, "c:\\");
    ASSERT(sp_parts_next(&wit2, &part)); ASSERT_SV(part, "a");
    ASSERT(sp_parts_next(&wit2, &part)); ASSERT_SV(part, "b");
    ASSERT(!sp_parts_next(&wit2, &part));
    
    SpPath wp3 = sp_path_f("//server/share/a/b", W); SpPartsIter wit3 = sp_parts_begin(&wp3);
    ASSERT(sp_parts_next(&wit3, &part)); ASSERT_SV(part, "\\\\server\\share\\");
    ASSERT(sp_parts_next(&wit3, &part)); ASSERT_SV(part, "a");
    ASSERT(sp_parts_next(&wit3, &part)); ASSERT_SV(part, "b");
    ASSERT(!sp_parts_next(&wit3, &part));
    
    SpPath ws = sp_path_f("c:a/b.tar.gz", W); SpSuffixes wss = sp_suffixes(&ws);
    ASSERT(wss.count == 2); ASSERT_SV(wss.items[0], ".tar"); ASSERT_SV(wss.items[1], ".gz");
    
    ASSERT_ABS("C:/a/b", W, true); ASSERT_ABS("C:a/b", W, false);
    ASSERT_ABS("/a/b", W, false); ASSERT_ABS("//server/share/a", W, true);
    
    ASSERT_PATH(sp_path_f("C:/a\\b/c", W), "C:\\a\\b\\c");
    
    SpPath wap = sp_path_f("C:\\a\\b\\c", W);
    ASSERT_TERM(sp_as_posix(&wap), "C:/a/b/c");
    
    SpPath wr1 = sp_path_f("C:/a/b/c", W), wo1 = sp_path_f("C:/a", W);
    ASSERT_PATH(sp_relative_to(&wr1, &wo1, false), "b\\c");
    
    SpPath wr2 = sp_path_f("C:/a/b", W), wo2 = sp_path_f("D:/a", W);
    ASSERT(!sp_is_relative_to(&wr2, &wo2));
    
    printf("  Windows tests OK\n");
    
    printf("\nEdge Cases:\n");
    
    SpPath e1 = sp_path_f("", P); ASSERT_PATH(e1, "."); ASSERT_TERM(sp_name(&e1), ""); ASSERT_TERM(sp_suffix(&e1), "");
    SpPath e2 = sp_path_f(".", P); ASSERT_PATH(e2, "."); ASSERT_TERM(sp_name(&e2), "");
    SpPath e3 = sp_path_f("..", P); ASSERT_PATH(e3, ".."); ASSERT_TERM(sp_name(&e3), "..");
    SpPath e4 = sp_path_f("...", P); ASSERT_TERM(sp_suffix(&e4), "");

    SpPath e5 = sp_path_f("a/b.c.d.e", P); SpSuffixes es5 = sp_suffixes(&e5);
    ASSERT(es5.count == 3); ASSERT_SV(es5.items[0], ".c"); ASSERT_SV(es5.items[1], ".d"); ASSERT_SV(es5.items[2], ".e");

    SpPath e6 = sp_path_f(".tar.gz", P); ASSERT_TERM(sp_stem(&e6), ".tar"); ASSERT_TERM(sp_suffix(&e6), ".gz");

    SpPath e7 = sp_path_f("file.txt", P);
    ASSERT_TERM(sp_name(&e7), "file.txt"); ASSERT_TERM(sp_stem(&e7), "file");
    ASSERT_TERM(sp_suffix(&e7), ".txt"); ASSERT_PATH(sp_parent(&e7), ".");
    
    SpPath e8 = sp_path_f("/a/b/c/d/e/f/g", P);
    ASSERT(sp_parts_count(&e8) == 8); ASSERT_PATH(sp_parent(&e8), "/a/b/c/d/e/f");
    
    SpPath ej1 = sp_path_f("a/b", P); ASSERT_PATH(sp_join_one(&ej1, ""), "a/b");
    SpPath ej2 = sp_path_f("", P); ASSERT_PATH(sp_join_one(&ej2, "a"), "a");

    /* Failed path results are described by sp_error_str (their codes must not collide with SP_ERR_*) */
    SpPath er1 = sp_path_f("/", P), er2 = sp_path_f("/a/b", P), er3 = sp_path_f("/c", P);
    SpPath ee1 = sp_with_name(&er1, "x"), ee2 = sp_with_name(&er2, ""), ee3 = sp_relative_to(&er2, &er3, false);
    ASSERT(strcmp(sp_error_str(ee1.error), "Path has an empty name") == 0);
    ASSERT(strcmp(sp_error_str(ee2.error), "Invalid argument") == 0);
    ASSERT(strcmp(sp_error_str(ee3.error), "Path is not relative to the other path") == 0);

    printf("  Edge cases OK\n");

    printf("\nis_file Tests:\n");

    /* Test is_file - existing file */
    SpPath existing_file = sp_path_f(__FILE__, SP_FLAVOR_NATIVE);
    ASSERT(sp_is_file(&existing_file, true) == true);

    /* Test is_file - nonexistent path */
    SpPath nonexistent = sp_path_f("/nonexistent/path/file.txt", P);
    ASSERT(sp_is_file(&nonexistent, true) == false);

    /* Test is_file - directory (not a file) */
    SpPath dir = sp_path_f(".", P);
    ASSERT(sp_is_file(&dir, true) == false);

    printf("  is_file tests OK\n");

    printf("\nis_dir Tests:\n");

    /* Test is_dir - existing directory */
    SpPath existing_dir = sp_path_f(".", SP_FLAVOR_NATIVE);
    ASSERT(sp_is_dir(&existing_dir, true) == true);

    /* Test is_dir - nonexistent path */
    SpPath nonexistent_dir = sp_path_f("/nonexistent/path/dir", P);
    ASSERT(sp_is_dir(&nonexistent_dir, true) == false);

    /* Test is_dir - file (not a directory) */
    SpPath file_path = sp_path_f(__FILE__, SP_FLAVOR_NATIVE);
    ASSERT(sp_is_dir(&file_path, true) == false);

    printf("  is_dir tests OK\n");

    printf("\nexists Tests:\n");

    /* Test exists - existing file */
    SpPath exists_file = sp_path_f(__FILE__, SP_FLAVOR_NATIVE);
    ASSERT(sp_exists(&exists_file, true) == true);

    /* Test exists - existing directory */
    SpPath exists_dir = sp_path_f(".", SP_FLAVOR_NATIVE);
    ASSERT(sp_exists(&exists_dir, true) == true);

    /* Test exists - nonexistent path */
    SpPath not_exists = sp_path_f("/nonexistent/path/file.txt", P);
    ASSERT(sp_exists(&not_exists, true) == false);

    printf("  exists tests OK\n");

    printf("\nstat Tests:\n");

    /* Test stat - existing file */
    SpPath stat_file = sp_path_f(__FILE__, SP_FLAVOR_NATIVE);
    SpStatResult stat_result = sp_stat(&stat_file);
    ASSERT(stat_result.error == SP_OK);
    ASSERT(stat_result.sp_size > 0);
    ASSERT((stat_result.sp_mode & 0170000) == 0100000);  /* S_IFREG - regular file */

    /* Test stat - existing directory */
    SpPath stat_dir = sp_path_f(".", SP_FLAVOR_NATIVE);
    SpStatResult dir_result = sp_stat(&stat_dir);
    ASSERT(dir_result.error == SP_OK);
    ASSERT((dir_result.sp_mode & 0170000) == 0040000);  /* S_IFDIR - directory */

    /* Test stat - nonexistent path */
    SpPath stat_nonexistent = sp_path_f("/nonexistent/path/file.txt", P);
    SpStatResult nonexistent_result = sp_stat(&stat_nonexistent);
    ASSERT(nonexistent_result.error != SP_OK);

    /* Test sp_stat_eq */
    ASSERT(sp_stat_eq(&stat_result, &stat_result) == true);
    ASSERT(sp_stat_eq(&stat_result, &dir_result) == false);
    ASSERT(sp_stat_eq(&stat_result, &nonexistent_result) == false);  /* invalid stat */

    printf("  stat tests OK\n");

    printf("\nparents_count Tests:\n");

    SpPath pc1 = sp_path_f("/a/b/c/d", P);
    ASSERT(sp_parents_count(&pc1) == 4);  /* /a/b/c, /a/b, /a, / */

    SpPath pc2 = sp_path_f("a/b/c", P);
    ASSERT(sp_parents_count(&pc2) == 3);  /* a/b, a, . */

    SpPath pc3 = sp_path_f("/", P);
    ASSERT(sp_parents_count(&pc3) == 0);  /* root has no parents */

    SpPath pc4 = sp_path_f(".", P);
    ASSERT(sp_parents_count(&pc4) == 0);  /* current dir has no parents */

    printf("  parents_count tests OK\n");

    printf("\nmkdir Tests:\n");

    /* Test mkdir - create and cleanup a temporary directory */
    SpPath mkdir_test = sp_path_f(test_dir_mkdir_temp, SP_FLAVOR_NATIVE);

    /* First ensure it doesn't exist (cleanup from previous failed runs) */
    test_rmdir(sp_str(&mkdir_test));

    /* Test basic mkdir */
    SpError mkdir_result = sp_mkdir(&mkdir_test, 0755, 0, SP_MODE_DIR);
    ASSERT(mkdir_result == SP_OK);
    ASSERT(sp_is_dir(&mkdir_test, true) == true);

    /* Test mkdir with exist_ok=false should fail when dir exists */
    mkdir_result = sp_mkdir(&mkdir_test, 0755, 0, SP_MODE_DIR);
    ASSERT(mkdir_result == SP_ERR_EXISTS);

    /* Test mkdir with exist_ok=true should succeed when dir exists */
    mkdir_result = sp_mkdir(&mkdir_test, 0755, SP_MKDIR_EXIST_OK, SP_MODE_DIR);
    ASSERT(mkdir_result == SP_OK);

    /* Cleanup */
    test_rmdir(sp_str(&mkdir_test));

    /* Test mkdir with parents=true */
    char nested_path[128], nested_sub[128];
    snprintf(nested_path, sizeof(nested_path), "%s/subdir/deep", test_dir_mkdir_nested);
    snprintf(nested_sub, sizeof(nested_sub), "%s/subdir", test_dir_mkdir_nested);
    SpPath mkdir_nested = sp_path_f(nested_path, SP_FLAVOR_NATIVE);
    mkdir_result = sp_mkdir(&mkdir_nested, 0755, SP_MKDIR_PARENTS, SP_MODE_DIR);
    ASSERT(mkdir_result == SP_OK);
    ASSERT(sp_is_dir(&mkdir_nested, true) == true);

    /* Cleanup nested dirs */
    test_rmdir(nested_path);
    test_rmdir(nested_sub);
    test_rmdir(test_dir_mkdir_nested);

    /* Test mkdir without parents should fail if parent doesn't exist */
    SpPath mkdir_no_parent = sp_path_f("./nonexistent_parent/subdir", SP_FLAVOR_NATIVE);
    mkdir_result = sp_mkdir(&mkdir_no_parent, 0755, 0, SP_MODE_DIR);
    ASSERT(mkdir_result == SP_ERR_NOT_FOUND);

    printf("  mkdir tests OK\n");

    /* lstat tests */
    printf("\nlstat Tests:\n");

    /* Test lstat on regular file */
    SpPath lstat_file = sp_path_f(__FILE__, SP_FLAVOR_NATIVE);
    SpStatResult lstat_res = sp_lstat(&lstat_file);
    ASSERT(lstat_res.error == SP_OK);
    ASSERT(lstat_res.sp_size > 0);

    /* Test lstat on nonexistent file */
    SpPath lstat_nonexist = sp_path_f("/nonexistent/path/file.txt", P);
    SpStatResult lstat_nonexist_res = sp_lstat(&lstat_nonexist);
    ASSERT(lstat_nonexist_res.error != SP_OK);

    printf("  lstat tests OK\n");

    /* resolve tests */
    printf("\nresolve Tests:\n");

    /* Test resolve on current directory */
    SpPath resolve_cwd = sp_path_f(".", SP_FLAVOR_NATIVE);
    SpPath resolved = sp_resolve(&resolve_cwd, false);
    ASSERT(sp_is_absolute(&resolved));
    ASSERT(resolved.len > 0);

    /* Test resolve strict mode on existing file */
    SpPath resolve_file = sp_path_f(__FILE__, SP_FLAVOR_NATIVE);
    SpPath resolved_file = sp_resolve(&resolve_file, true);
    ASSERT(sp_is_absolute(&resolved_file));
    ASSERT(resolved_file.error == SP_OK);

    /* Test resolve strict mode on nonexistent path */
    SpPath resolve_nonexist = sp_path_f("/nonexistent/path/file.txt", P);
    SpPath resolved_nonexist = sp_resolve(&resolve_nonexist, true);
    ASSERT(resolved_nonexist.error == SP_ERR_NOT_FOUND);

#ifndef SP_WINDOWS
    /* A symlink loop: strict resolve reports it, and non-strict resolve leaves the looping part as it is */
    {
        char loop_name[64];
        snprintf(loop_name, sizeof(loop_name), "test_resolve_loop_%ld", pid);
        SpPath loop = sp_path(loop_name);
        ASSERT(sp_symlink_to(&loop, &loop, false) == SP_OK);
        ASSERT(sp_resolve(&loop, true).error == SP_ERR_LOOP);
        SpPath loop_child = sp_join_one(&loop, "x");
        SpPath abs_child = sp_absolute(&loop_child);
        SpPath lenient = sp_resolve(&loop_child, false);
        ASSERT(lenient.error == SP_OK && sp_path_eq(&lenient, &abs_child));
        ASSERT(sp_readlink(&loop).error == SP_OK);
        ASSERT(sp_readlink(&resolve_file).error == SP_ERR_NOT_LINK);
        ASSERT(sp_unlink(&loop, false) == SP_OK);
    }
#endif

    printf("  resolve tests OK\n");

    /* samefile tests */
    printf("\nsamefile Tests:\n");

    /* Test samefile with same path */
    SpPath same1 = sp_path_f(__FILE__, SP_FLAVOR_NATIVE);
    SpPath same2 = sp_path_f(__FILE__, SP_FLAVOR_NATIVE);
    ASSERT(sp_samefile(&same1, &same2) == true);

    /* Test samefile with different paths */
    SpPath diff1 = sp_path_f(".", SP_FLAVOR_NATIVE);
    SpPath diff2 = sp_path_f(__FILE__, SP_FLAVOR_NATIVE);
    ASSERT(sp_samefile(&diff1, &diff2) == false);

    /* Test samefile with nonexistent path */
    SpPath nonexist1 = sp_path_f("/nonexistent1", P);
    SpPath nonexist2 = sp_path_f("/nonexistent2", P);
    ASSERT(sp_samefile(&nonexist1, &nonexist2) == false);

    printf("  samefile tests OK\n");

    /* Glob tests */
    printf("\nGlob Tests:\n");

    /* Create test directory structure with unique names to avoid parallel test races */
    char glob_sub_path[128], glob_f1[128], glob_f2[128], glob_f3[128];
    snprintf(glob_sub_path, sizeof(glob_sub_path), "%s/subdir", test_dir_glob);
    snprintf(glob_f1, sizeof(glob_f1), "%s/file1.txt", test_dir_glob);
    snprintf(glob_f2, sizeof(glob_f2), "%s/file2.py", test_dir_glob);
    snprintf(glob_f3, sizeof(glob_f3), "%s/subdir/file3.txt", test_dir_glob);

    SpPath glob_base = sp_path_f(test_dir_glob, SP_FLAVOR_NATIVE);
    ASSERT(sp_mkdir(&glob_base, 0755, SP_MKDIR_PARENTS | SP_MKDIR_EXIST_OK, SP_MODE_DIR) == SP_OK);

    SpPath glob_sub = sp_path_f(glob_sub_path, SP_FLAVOR_NATIVE);
    ASSERT(sp_mkdir(&glob_sub, 0755, SP_MKDIR_PARENTS | SP_MKDIR_EXIST_OK, SP_MODE_DIR) == SP_OK);

    /* Create test files by touching them (just need the glob to find them) */
    FILE *f1 = fopen(glob_f1, "w");
    if (f1) fclose(f1);
    FILE *f2 = fopen(glob_f2, "w");
    if (f2) fclose(f2);
    FILE *f3 = fopen(glob_f3, "w");
    if (f3) fclose(f3);

    /* Test basic glob *.txt using iterator */
    int txt_count = 0;
    SpGlobIter git = sp_glob_begin(&glob_base, "*.txt", SP_CASE_DEFAULT, false);
    SpPath gmatch;
    while (sp_glob_next(&git, &gmatch)) {
        if (strstr(sp_str(&gmatch), ".txt")) txt_count++;
    }
    sp_glob_end(&git);
    ASSERT(txt_count == 1);  /* Should find file1.txt */

    /* Test recursive glob using foreach macro */
    txt_count = 0;
    SP_GLOB_FOREACH(&glob_base, "**/*.txt", m) {
        if (strstr(sp_str(&m), ".txt")) txt_count++;
    }
    ASSERT(txt_count >= 1);  /* Should find file1.txt and file3.txt */

    /* Test rglob */
    txt_count = 0;
    SP_RGLOB_FOREACH(&glob_base, "*.txt", m2) {
        if (strstr(sp_str(&m2), ".txt")) txt_count++;
    }
    ASSERT(txt_count >= 1);

    /* Cleanup */
    remove(glob_f1);
    remove(glob_f2);
    remove(glob_f3);
    test_rmdir(glob_sub_path);
    test_rmdir(test_dir_glob);

    printf("  glob tests OK\n");

    /* touch/unlink/chmod tests */
    printf("\ntouch/unlink/chmod Tests:\n");

    /* Create a unique test file path */
    char touch_file_path[128];
    snprintf(touch_file_path, sizeof(touch_file_path), "./test_touch_%ld.tmp", pid);
    SpPath touch_file = sp_path_f(touch_file_path, SP_FLAVOR_NATIVE);

    /* Test touch creates new file */
    ASSERT(sp_touch(&touch_file, 0644, true) == SP_OK);
    ASSERT(sp_exists(&touch_file, true) == true);
    ASSERT(sp_is_file(&touch_file, true) == true);

    /* Test touch with exist_ok=false on existing file fails */
    ASSERT(sp_touch(&touch_file, 0644, false) == SP_ERR_EXISTS);

    /* Test touch with exist_ok=true on existing file succeeds */
    ASSERT(sp_touch(&touch_file, 0644, true) == SP_OK);

    /* Test chmod */
    ASSERT(sp_chmod(&touch_file, 0444, true) == SP_OK);

    /* Restore write permission before unlink (required on Windows) */
    ASSERT(sp_chmod(&touch_file, 0644, true) == SP_OK);

    /* Test unlink */
    ASSERT(sp_unlink(&touch_file, false) == SP_OK);
    ASSERT(sp_exists(&touch_file, true) == false);

    /* Test unlink with missing_ok=false on nonexistent file fails */
    ASSERT(sp_unlink(&touch_file, false) == SP_ERR_NOT_FOUND);

    /* Test unlink with missing_ok=true on nonexistent file succeeds */
    ASSERT(sp_unlink(&touch_file, true) == SP_OK);

    printf("  touch/unlink/chmod tests OK\n");

    /* rename/replace tests */
    printf("\nrename/replace Tests:\n");

    char rename_src_path[128], rename_dst_path[128];
    snprintf(rename_src_path, sizeof(rename_src_path), "./test_rename_src_%ld.tmp", pid);
    snprintf(rename_dst_path, sizeof(rename_dst_path), "./test_rename_dst_%ld.tmp", pid);
    SpPath rename_src = sp_path_f(rename_src_path, SP_FLAVOR_NATIVE);
    SpPath rename_dst = sp_path_f(rename_dst_path, SP_FLAVOR_NATIVE);

    /* Create source file */
    ASSERT(sp_touch(&rename_src, 0644, true) == SP_OK);

    /* Test rename */
    SpPath rename_result = sp_rename(&rename_src, &rename_dst);
    ASSERT(rename_result.error == SP_OK);
    ASSERT(sp_exists(&rename_src, true) == false);
    ASSERT(sp_exists(&rename_dst, true) == true);

    /* Test replace (create src again, replace dst) */
    ASSERT(sp_touch(&rename_src, 0644, true) == SP_OK);
    SpPath replace_result = sp_replace(&rename_src, &rename_dst);
    ASSERT(replace_result.error == SP_OK);
    ASSERT(sp_exists(&rename_src, true) == false);
    ASSERT(sp_exists(&rename_dst, true) == true);

    /* Cleanup */
    ASSERT(sp_unlink(&rename_dst, true) == SP_OK);

    printf("  rename/replace tests OK\n");

    /* rmdir tests */
    printf("\nrmdir Tests:\n");

    char rmdir_path[128];
    snprintf(rmdir_path, sizeof(rmdir_path), "./test_rmdir_%ld", pid);
    SpPath rmdir_dir = sp_path_f(rmdir_path, SP_FLAVOR_NATIVE);

    /* Create directory */
    ASSERT(sp_mkdir(&rmdir_dir, 0755, 0, SP_MODE_DIR) == SP_OK);
    ASSERT(sp_is_dir(&rmdir_dir, true) == true);

    /* Test rmdir on empty directory */
    ASSERT(sp_rmdir(&rmdir_dir) == SP_OK);
    ASSERT(sp_exists(&rmdir_dir, true) == false);

    /* Test rmdir on nonexistent directory fails */
    ASSERT(sp_rmdir(&rmdir_dir) == SP_ERR_NOT_FOUND);

    printf("  rmdir tests OK\n");

    /* Python 3.15 pathlib: match/full_match, URIs, Windows anchors, glob, copy/move */
    printf("\npathlib 3.15 Tests:\n");
    {
        SpPath p = sp_path_f("/a/b/c.py", SP_FLAVOR_POSIX), empty = sp_path_f("", SP_FLAVOR_POSIX);
        ASSERT(!sp_match(&p, "/**/*.py", SP_CASE_DEFAULT)); /* '**' is an ordinary wildcard in match() */
        ASSERT(sp_match(&p, "/a/**/*.py", SP_CASE_DEFAULT));
        ASSERT(!sp_match(&empty, "**", SP_CASE_DEFAULT));
        ASSERT(sp_full_match(&p, "/a/**", SP_CASE_DEFAULT));
        ASSERT(sp_full_match(&p, "**/*.py", SP_CASE_DEFAULT));
        ASSERT(!sp_full_match(&p, "*.py", SP_CASE_DEFAULT));
        ASSERT(sp_full_match(&p, "/[a-c]/?/[!x]*", SP_CASE_DEFAULT));
        ASSERT(sp_full_match(&empty, "**", SP_CASE_DEFAULT));
        ASSERT(!sp_full_match(&empty, "*", SP_CASE_DEFAULT));
        ASSERT(sp_full_match(&empty, "***", SP_CASE_DEFAULT));
        ASSERT(sp_full_match(&p, "**/**/**/c.py", SP_CASE_DEFAULT));
        ASSERT(!sp_full_match(&p, "**/**/**/absent", SP_CASE_DEFAULT));
        SpPath rooted = sp_path_f("/a", SP_FLAVOR_POSIX);
        ASSERT(sp_full_match(&rooted, "[!x]a", SP_CASE_DEFAULT)); /* Classes, unlike '*' and '?', may match a separator. */
        ASSERT(!sp_full_match(&rooted, "?a", SP_CASE_DEFAULT));
        SpPath w = sp_path_f("c:/a/B.Py", SP_FLAVOR_WINDOWS);
        ASSERT(sp_full_match(&w, "C:/A/*.pY", SP_CASE_DEFAULT));
        ASSERT(!sp_full_match(&w, "C:/A/*.pY", SP_CASE_SENSITIVE));
        ASSERT(sp_match(&w, "*:/*/*.py", SP_CASE_DEFAULT));
        ASSERT_PATH(sp_path_f("//a//b", SP_FLAVOR_WINDOWS), "\\\\a\\\\b"); /* //server/share with an empty share */
        SpPath dotted = sp_path_f("a/x.", SP_FLAVOR_POSIX), back = sp_path_f("a\\b", SP_FLAVOR_POSIX);
        ASSERT_TERM(sp_suffix(&dotted), ".");                     /* a trailing dot is a suffix */
        ASSERT_TERM(sp_stem(&dotted), "x");
        ASSERT_PATH(sp_with_suffix(&dotted, "."), "a/x.");
        ASSERT_TERM(sp_as_posix(&back), "a\\b");
        SpPath dotdot = sp_path_f("../a/b", SP_FLAVOR_POSIX), up = sp_path_f("../c", SP_FLAVOR_POSIX);
        ASSERT_PATH(sp_relative_to(&dotdot, &up, true), "../a/b"); /* ".." shared by both is not walked */
        SpPath dot = sp_path_f("", SP_FLAVOR_POSIX), star = sp_path_f("*", SP_FLAVOR_POSIX);
        ASSERT(sp_path_cmp(&dot, &star) > 0);                     /* compares "." with "*", part by part */
        SpPath protected_drive = sp_path_f("./c:", SP_FLAVOR_WINDOWS);
        ASSERT_PATH(sp_parent(&protected_drive), ".");
        SpPath drive_child = sp_join_one(&protected_drive, "x"), win_empty = sp_path_f("", SP_FLAVOR_WINDOWS);
        ASSERT(sp_parts_count(&drive_child) == 2);
        ASSERT(sp_match(&drive_child, "./c:/*", SP_CASE_DEFAULT));
        ASSERT(!sp_match(&drive_child, "c:/*", SP_CASE_DEFAULT));
        ASSERT_PATH(sp_relative_to(&drive_child, &protected_drive, false), "x");
        ASSERT_PATH(sp_relative_to(&drive_child, &win_empty, false), ".\\c:\\x");
    }
    {
        /* readlink can return unnormalized paths; both directions of parts traversal must skip empty parts. */
        SpPath raw = sp_path_f("", SP_FLAVOR_POSIX), base = sp_path_f("a", SP_FLAVOR_POSIX);
        memcpy(raw.buf, "a///b//", 8);
        raw.len = 7;
        ASSERT(sp_parts_count(&raw) == 2);
        ASSERT(sp_match(&raw, "a/b", SP_CASE_DEFAULT));
        ASSERT(sp_match(&raw, "b", SP_CASE_DEFAULT));
        ASSERT(sp_full_match(&raw, "a/**/b/**", SP_CASE_DEFAULT));
        ASSERT_PATH(sp_relative_to(&raw, &base, false), "b");
        SpPath above = sp_path_f("a/../c", SP_FLAVOR_POSIX), result = sp_relative_to(&raw, &above, true);
        ASSERT(result.error == SP_ERR_NOT_RELATIVE);
    }
    {
        /* A result too long for SP_PATH_MAX is an error, including when a rooted join keeps a Windows drive, and the
         * error passes through what comes after */
        char full[SP_PATH_MAX + 1];
        memset(full, 'x', sizeof(full) - 1);
        full[sizeof(full) - 1] = '\0';
        ASSERT(sp_path_f(full, SP_FLAVOR_POSIX).error == SP_ERR_TOO_LONG);
        full[sizeof(full) - 2] = '\0';
        SpPath base = sp_path_f("a", SP_FLAVOR_POSIX);
        SpPath too_long = sp_join_one(&base, full);
        ASSERT(too_long.error == SP_ERR_TOO_LONG && too_long.len == 0);
        SpPath still = sp_join_one(&too_long, "b");
        SpPath parent_of = sp_parent(&still);
        ASSERT(sp_with_name(&parent_of, "c").error == SP_ERR_TOO_LONG);
        ASSERT(sp_name(&still).error == SP_ERR_TOO_LONG);
        ASSERT(sp_mkdir(&still, 0777, 0, 0777) == SP_ERR_TOO_LONG);
        ASSERT(sp_stat(&still).error == SP_ERR_TOO_LONG);
        full[0] = '/';
        base = sp_path_f("C:/base", SP_FLAVOR_WINDOWS);
        ASSERT(sp_join_one(&base, full).error == SP_ERR_TOO_LONG);
        /* Exercise streaming comparisons with the maximum number of parts. */
        for (size_t i = 0; i + 2 < sizeof(full); i++) full[i] = i % 2 ? '/' : 'a';
        SpPath deep = sp_path_f(full, SP_FLAVOR_POSIX), empty = sp_path_f("", SP_FLAVOR_POSIX);
        SpPath relative = sp_relative_to(&deep, &empty, false);
        ASSERT(sp_path_eq(&relative, &deep));
        ASSERT(sp_match(&deep, "a/a/a", SP_CASE_DEFAULT));
        ASSERT(!sp_full_match(&deep, "**/**/**/**/absent", SP_CASE_DEFAULT));
    }
    {
        char buf[SP_PATH_MAX * 3];
        const char *uris[][3] = {
            {"/a/b%#c", "p", "file:///a/b%25%23c"},   {"//a/b", "p", "file:////a/b"},
            {"c:/a b", "w", "file:///c:/a%20b"},       {"//some/share/a", "w", "file://some/share/a"},
            {"//?/UNC/srv/sh/x", "w", "file://srv/sh/x"}, {"a/b", "p", ""},
        };
        for (size_t i = 0; i < ARRAY_LEN(uris); i++) {
            SpPath u = sp_path_f(uris[i][0], uris[i][1][0] == 'p' ? SP_FLAVOR_POSIX : SP_FLAVOR_WINDOWS);
            SpError err = sp_as_uri(&u, buf, sizeof(buf));
            ASSERT(err == (uris[i][2][0] ? SP_OK : SP_ERR_NOT_ABSOLUTE));
            ASSERT(strcmp(buf, uris[i][2]) == 0);
        }
        SpPath long_uri = sp_path_f("/a b", SP_FLAVOR_POSIX);
        ASSERT(sp_as_uri(&long_uri, buf, 13) == SP_ERR_TOO_LONG && buf[0] == '\0');
        ASSERT(sp_as_uri(&long_uri, buf, 14) == SP_OK);
        ASSERT_PATH(sp_from_uri("file:///foo/bar", SP_FLAVOR_POSIX), "/foo/bar");
        ASSERT_PATH(sp_from_uri("file://localhost/foo", SP_FLAVOR_POSIX), "/foo");
        ASSERT_PATH(sp_from_uri("file:////foo/bar", SP_FLAVOR_POSIX), "//foo/bar");
        ASSERT_PATH(sp_from_uri("FILE:///a%20b?q#f", SP_FLAVOR_POSIX), "/a b");
        ASSERT_PATH(sp_from_uri("file:///c|/x", SP_FLAVOR_WINDOWS), "c:\\x");
        ASSERT_PATH(sp_from_uri("file://server/share/x", SP_FLAVOR_WINDOWS), "\\\\server\\share\\x");
        const char *bad_uris[] = {"file://host/x", "http://x/y", "/foo"};
        for (size_t i = 0; i < ARRAY_LEN(bad_uris); i++) {
            SpPath u = sp_from_uri(bad_uris[i], SP_FLAVOR_POSIX);
            ASSERT(u.error == SP_ERR_INVALID_ARG);
        }
        ASSERT(sp_from_uri("file:foo", SP_FLAVOR_POSIX).error == SP_ERR_NOT_ABSOLUTE);
    }
    {
        char root_path[64];
        snprintf(root_path, sizeof(root_path), "./test_315_%ld", pid);
        SpPath root = sp_path(root_path), sub = sp_join_one(&root, "sub"), txt = sp_join_one(&root, "a.txt");
        SpPath hidden = sp_join_one(&root, ".hidden"), sub_txt = sp_join_one(&sub, "b.txt");
        ASSERT(sp_mkdir(&sub, 0755, SP_MKDIR_PARENTS, SP_MODE_DIR) == SP_OK);
        ASSERT(sp_touch(&txt, 0644, true) == SP_OK && sp_touch(&hidden, 0644, true) == SP_OK);
        ASSERT(sp_touch(&sub_txt, 0644, true) == SP_OK);
        ASSERT(sp_is_dir(&sub, false) && !sp_is_file(&sub, false) && sp_exists(&txt, false));

        struct { const char *pattern; bool rglob; int count, follow_count; } globs[] = {
            {"*", false, 3, 3}, /* hidden files match too */
            {"*/", false, 1, 1}, {"sub/../a.txt", false, 1, 1}, {"**", false, 5, 5}, {"*.txt", true, 2, 2}, {"", true, 2, 2},
            {"**/**", false, 5, 5}, {"**/**/sub/**", false, 2, 1}, {"sub//./*.txt", false, 1, 1}, {"sub/", false, 1, 1},
        };
        for (size_t i = 0; i < ARRAY_LEN(globs); i++) {
            for (int follow = 0; follow < 2; follow++) {
                SpGlobIter it = (globs[i].rglob ? sp_rglob_begin : sp_glob_begin)(&root, globs[i].pattern, SP_CASE_DEFAULT, follow != 0);
                int n = 0;
                for (SpPath m; sp_glob_next(&it, &m);) n++;
                sp_glob_end(&it);
                ASSERT(it.error == SP_OK && n == (follow ? globs[i].follow_count : globs[i].count));
            }
        }
        /* A pattern that fills SP_GLOB_PATTERN_MAX is past the limit; one character shorter, its trailing slash still
         * requires a directory. */
        char full_pattern[SP_GLOB_PATTERN_MAX + 1];
        memset(full_pattern, '*', sizeof(full_pattern) - 2);
        full_pattern[sizeof(full_pattern) - 2] = '/';
        full_pattern[sizeof(full_pattern) - 1] = '\0';
        SpGlobIter bounded = sp_glob_begin(&root, full_pattern, SP_CASE_DEFAULT, true);
        ASSERT(bounded.error == SP_ERR_LIMIT && !sp_glob_next(&bounded, &sub_txt));
        sp_glob_end(&bounded);
        bounded = sp_glob_begin(&root, full_pattern + 1, SP_CASE_DEFAULT, true);
        int bounded_count = 0;
        for (SpPath m; sp_glob_next(&bounded, &m);) {
            ASSERT(sp_path_eq(&m, &sub));
            bounded_count++;
        }
        sp_glob_end(&bounded);
        ASSERT(bounded.error == SP_OK && bounded_count == 1);
        SpGlobIter bad = sp_glob_begin(&root, "", SP_CASE_DEFAULT, false);
        ASSERT(bad.error == SP_ERR_INVALID_ARG);
        bad = sp_glob_begin(&root, "/x", SP_CASE_DEFAULT, false);
        ASSERT(bad.error == SP_ERR_UNSUPPORTED);

        SpPath sub2 = sp_join_one(&root, "sub2"), sub3 = sp_join_one(&root, "sub3"), inside = sp_join_one(&sub, "x");
        ASSERT_PATH(sp_copy(&sub, &sub2, SP_COPY_FOLLOW_SYMLINKS | SP_COPY_PRESERVE_METADATA), sp_str(&sub2));
        SpPath copied = sp_join_one(&sub2, "b.txt");
        ASSERT(sp_is_file(&copied, true));
        SpPath err = sp_copy(&sub, &inside, SP_COPY_FOLLOW_SYMLINKS);
        ASSERT(err.error == SP_ERR_SAME_FILE);
        err = sp_copy(&sub, &sub2, SP_COPY_FOLLOW_SYMLINKS);
        ASSERT(err.error == SP_ERR_EXISTS);
        err = sp_copy(&sub, &sub2, SP_MKDIR_PARENTS);
        ASSERT(err.error == SP_ERR_INVALID_ARG);
        SpPath into = sp_copy_into(&txt, &sub, SP_COPY_FOLLOW_SYMLINKS);
        ASSERT(into.error == SP_OK && sp_is_file(&into, true));
        ASSERT_PATH(sp_move(&sub2, &sub3), sp_str(&sub3));
        ASSERT(!sp_exists(&sub2, false));
        err = sp_move(&txt, &txt);
        ASSERT(err.error == SP_ERR_SAME_FILE);
        SpPath moved = sp_move_into(&sub3, &sub), moved_txt = sp_join_one(&moved, "b.txt");
        ASSERT(sp_is_file(&moved_txt, true) && !sp_exists(&sub3, false));
        SpPath nameless = sp_path_f("", SP_FLAVOR_NATIVE);
        err = sp_move_into(&nameless, &sub);
        ASSERT(err.error == SP_ERR_NO_NAME);

        ASSERT(sp_unlink(&moved_txt, false) == SP_OK);
        ASSERT(sp_rmdir(&moved) == SP_OK);
        ASSERT(sp_unlink(&into, false) == SP_OK);
        ASSERT(sp_unlink(&sub_txt, false) == SP_OK);
        ASSERT(sp_rmdir(&sub) == SP_OK);
        ASSERT(sp_unlink(&txt, false) == SP_OK);
        ASSERT(sp_unlink(&hidden, false) == SP_OK);
        ASSERT(sp_rmdir(&root) == SP_OK);
    }
    printf("  pathlib 3.15 tests OK\n");

    /* read_file / write_file tests */
    printf("\nread_file/write_file Tests:\n");

    char rw_path[128];
    snprintf(rw_path, sizeof(rw_path), "./test_rw_%ld.tmp", pid);
    SpPath rw_file = sp_path_f(rw_path, SP_FLAVOR_NATIVE);

    /* Write then read back */
    {
        SpIOResult wr = sp_write_file(&rw_file, "hello world", 11);
        ASSERT(wr.error == SP_OK);
        ASSERT(wr.bytes == 11);

        char rbuf[64];
        SpIOResult rd = sp_read_file(&rw_file, rbuf, sizeof(rbuf));
        ASSERT(rd.error == SP_OK);
        ASSERT(rd.bytes == 11);
        ASSERT(memcmp(rbuf, "hello world", 11) == 0);
    }

    /* Overwrite (truncate) */
    {
        SpIOResult wr = sp_write_file(&rw_file, "hi", 2);
        ASSERT(wr.error == SP_OK);
        ASSERT(wr.bytes == 2);

        char rbuf[64];
        SpIOResult rd = sp_read_file(&rw_file, rbuf, sizeof(rbuf));
        ASSERT(rd.error == SP_OK);
        ASSERT(rd.bytes == 2);
        ASSERT(memcmp(rbuf, "hi", 2) == 0);
    }

    /* Buffer too small */
    {
        char tiny[1];
        SpIOResult rd = sp_read_file(&rw_file, tiny, sizeof(tiny));
        ASSERT(rd.error == SP_ERR_TOO_LONG);
        ASSERT(rd.bytes == 2);  /* reports actual file size */
    }

    /* Read nonexistent file */
    {
        SpPath nofile = sp_path_f("/nonexistent/file.txt", P);
        char rbuf[64];
        SpIOResult rd = sp_read_file(&nofile, rbuf, sizeof(rbuf));
        ASSERT(rd.error == SP_ERR_NOT_FOUND);
    }

    /* Write to nonexistent directory */
    {
        SpPath nodir = sp_path_f("/nonexistent/dir/file.txt", P);
        SpIOResult wr = sp_write_file(&nodir, "x", 1);
        ASSERT(wr.error == SP_ERR_NOT_FOUND);
    }

    /* Read directory (should fail with OPEN error) */
    {
        SpPath rw_dir = sp_path_f(".", P);
        char rbuf[64];
        SpIOResult rd = sp_read_file(&rw_dir, rbuf, sizeof(rbuf));
        /* Reading a directory may fail at open or read depending on OS, but should not succeed */
        ASSERT(rd.error != SP_OK || rd.bytes == 0);
    }

    /* Empty file */
    {
        char empty_path[128];
        snprintf(empty_path, sizeof(empty_path), "./test_rw_empty_%ld.tmp", pid);
        SpPath empty_file = sp_path_f(empty_path, SP_FLAVOR_NATIVE);
        SpIOResult wr = sp_write_file(&empty_file, "", 0);
        ASSERT(wr.error == SP_OK);
        ASSERT(wr.bytes == 0);

        char rbuf[64];
        SpIOResult rd = sp_read_file(&empty_file, rbuf, sizeof(rbuf));
        ASSERT(rd.error == SP_OK);
        ASSERT(rd.bytes == 0);
        ASSERT(sp_unlink(&empty_file, false) == SP_OK);
    }

    /* Embedded null in path -> NUL error */
    {
        SpPath null_path = sp_path_from_n("file\0hidden", 11, SP_FLAVOR_NATIVE);
        char rbuf[64];
        SpIOResult rd = sp_read_file(&null_path, rbuf, sizeof(rbuf));
        ASSERT(rd.error == SP_ERR_NUL);
        SpIOResult wr = sp_write_file(&null_path, "x", 1);
        ASSERT(wr.error == SP_ERR_NUL);
    }

    /* Cleanup */
    ASSERT(sp_unlink(&rw_file, true) == SP_OK);

    printf("  read_file/write_file tests OK\n");

    /* expanduser/home tests */
    printf("\nexpanduser/home Tests:\n");

    /* Test home - should return a valid path */
    SpPath home = sp_home(SP_FLAVOR_NATIVE);
    ASSERT(home.error == SP_OK);
    ASSERT(home.len > 0);
    ASSERT(sp_is_absolute(&home));
    ASSERT(sp_is_dir(&home, true));

    /* Test expanduser with ~ */
    SpPath tilde = sp_path_f("~", SP_FLAVOR_NATIVE);
    SpPath expanded = sp_expanduser(&tilde);
    ASSERT(expanded.error == SP_OK);
    ASSERT(sp_path_eq(&expanded, &home));

    /* Test expanduser with ~/subdir */
    SpPath tilde_sub = sp_path_f("~/subdir", SP_FLAVOR_NATIVE);
    SpPath expanded_sub = sp_expanduser(&tilde_sub);
    ASSERT(expanded_sub.error == SP_OK);
    ASSERT(sp_is_absolute(&expanded_sub));
    ASSERT(sp_is_relative_to(&expanded_sub, &home));

    /* Test expanduser with non-tilde path (should return unchanged) */
    SpPath no_tilde = sp_path_f("/usr/bin", P);
    SpPath not_expanded = sp_expanduser(&no_tilde);
    ASSERT(sp_path_eq(&no_tilde, &not_expanded));

    printf("  expanduser/home tests OK\n");

    /* owner/group tests */
    printf("\nowner/group Tests:\n");

#ifndef SP_WINDOWS
    /* Test owner - should return non-empty string for existing file (POSIX only) */
    SpPath owner_file = sp_path_f(__FILE__, SP_FLAVOR_NATIVE);
    SpTerm owner_str = sp_owner(&owner_file, true);
    ASSERT(owner_str.error == SP_OK && owner_str.len > 0);
    ASSERT(owner_str.buf[0] != '\0');

    /* Test group - should return non-empty string for existing file (POSIX only) */
    SpTerm group_str = sp_group(&owner_file, false);
    ASSERT(group_str.error == SP_OK && group_str.len > 0);
    ASSERT(group_str.buf[0] != '\0');

    /* Test owner on nonexistent file - the stat error */
    SpPath owner_nonexist = sp_path_f("/nonexistent/file", P);
    SpTerm owner_none = sp_owner(&owner_nonexist, true);
    ASSERT(owner_none.error == SP_ERR_NOT_FOUND && owner_none.len == 0);
#else
    /* On Windows, owner/group are unsupported */
    SpPath owner_file = sp_path_f(__FILE__, SP_FLAVOR_NATIVE);
    SpTerm owner_str = sp_owner(&owner_file, true);
    ASSERT(owner_str.error == SP_ERR_UNSUPPORTED && owner_str.len == 0);

    SpTerm group_str = sp_group(&owner_file, true);
    ASSERT(group_str.error == SP_ERR_UNSUPPORTED && group_str.len == 0);
#endif

    printf("  owner/group tests OK\n");

    /* iterdir tests */
    printf("\niterdir Tests:\n");

    /* Create test directory structure */
    char iterdir_path[128], iterdir_f1[256], iterdir_f2[256], iterdir_sub[256];
    snprintf(iterdir_path, sizeof(iterdir_path), "./test_iterdir_%ld", pid);
    snprintf(iterdir_f1, sizeof(iterdir_f1), "%s/file1.txt", iterdir_path);
    snprintf(iterdir_f2, sizeof(iterdir_f2), "%s/file2.txt", iterdir_path);
    snprintf(iterdir_sub, sizeof(iterdir_sub), "%s/subdir", iterdir_path);

    SpPath iterdir_base = sp_path_f(iterdir_path, SP_FLAVOR_NATIVE);
    ASSERT(sp_mkdir(&iterdir_base, 0755, SP_MKDIR_PARENTS | SP_MKDIR_EXIST_OK, SP_MODE_DIR) == SP_OK);

    SpPath iterdir_subdir = sp_path_f(iterdir_sub, SP_FLAVOR_NATIVE);
    ASSERT(sp_mkdir(&iterdir_subdir, 0755, SP_MKDIR_PARENTS | SP_MKDIR_EXIST_OK, SP_MODE_DIR) == SP_OK);

    /* Create test files */
    FILE *if1 = fopen(iterdir_f1, "w"); if (if1) fclose(if1);
    FILE *if2 = fopen(iterdir_f2, "w"); if (if2) fclose(if2);

    /* Test iterdir */
    int iterdir_count = 0;
    SpIterdirIter idit = sp_iterdir_begin(&iterdir_base);
    SpPath entry;
    while (sp_iterdir_next(&idit, &entry)) {
        iterdir_count++;
    }
    sp_iterdir_end(&idit);
    ASSERT(iterdir_count == 3);  /* file1.txt, file2.txt, subdir */

    /* Test SP_ITERDIR_FOREACH macro */
    iterdir_count = 0;
    SP_ITERDIR_FOREACH(&iterdir_base, e) {
        iterdir_count++;
    }
    ASSERT(iterdir_count == 3);

    /* Cleanup */
    remove(iterdir_f1);
    remove(iterdir_f2);
    test_rmdir(iterdir_sub);
    test_rmdir(iterdir_path);

    printf("  iterdir tests OK\n");

    /* walk tests */
    printf("\nwalk Tests:\n");

    /* Create test directory structure */
    char walk_path[128], walk_f1[256], walk_f2[256], walk_sub[256], walk_f3[256];
    snprintf(walk_path, sizeof(walk_path), "./test_walk_%ld", pid);
    snprintf(walk_f1, sizeof(walk_f1), "%s/file1.txt", walk_path);
    snprintf(walk_f2, sizeof(walk_f2), "%s/file2.txt", walk_path);
    snprintf(walk_sub, sizeof(walk_sub), "%s/subdir", walk_path);
    snprintf(walk_f3, sizeof(walk_f3), "%s/subdir/file3.txt", walk_path);

    SpPath walk_base = sp_path_f(walk_path, SP_FLAVOR_NATIVE);
    ASSERT(sp_mkdir(&walk_base, 0755, SP_MKDIR_PARENTS | SP_MKDIR_EXIST_OK, SP_MODE_DIR) == SP_OK);

    SpPath walk_subdir = sp_path_f(walk_sub, SP_FLAVOR_NATIVE);
    ASSERT(sp_mkdir(&walk_subdir, 0755, SP_MKDIR_PARENTS | SP_MKDIR_EXIST_OK, SP_MODE_DIR) == SP_OK);

    /* Create test files */
    FILE *wf1 = fopen(walk_f1, "w"); if (wf1) fclose(wf1);
    FILE *wf2 = fopen(walk_f2, "w"); if (wf2) fclose(wf2);
    FILE *wf3 = fopen(walk_f3, "w"); if (wf3) fclose(wf3);

    /* Top-down: each directory, its sorted listing, before its subdirectories */
    {
        static char names[4096];
        SpWalkIter it = sp_walk_begin(&walk_base, SP_WALK_TOP_DOWN, names, sizeof(names));
        SpWalkEntry *e = sp_walk_next(&it);
        ASSERT(e && e->error == SP_OK && sp_path_eq(&e->dirpath, &walk_base));
        ASSERT(e->dirname_count == 1 && strcmp(e->dirnames[0], "subdir") == 0);
        ASSERT(e->filename_count == 2 && strcmp(e->filenames[0], "file1.txt") == 0);
        ASSERT(strcmp(e->filenames[1], "file2.txt") == 0);
        e = sp_walk_next(&it);
        ASSERT(e && e->error == SP_OK && sp_path_eq(&e->dirpath, &walk_subdir));
        ASSERT(e->dirname_count == 0 && e->filename_count == 1 && strcmp(e->filenames[0], "file3.txt") == 0);
        ASSERT(sp_walk_next(&it) == NULL && it.error == SP_OK);

        /* Pruning: the walk goes into no subdirectory left out of dirnames */
        it = sp_walk_begin(&walk_base, SP_WALK_TOP_DOWN, names, sizeof(names));
        e = sp_walk_next(&it);
        e->dirname_count = 0;
        ASSERT(sp_walk_next(&it) == NULL && it.error == SP_OK);

        /* Bottom-up: subdirectories first */
        it = sp_walk_begin(&walk_base, 0, names, sizeof(names));
        e = sp_walk_next(&it);
        ASSERT(e && sp_path_eq(&e->dirpath, &walk_subdir));
        e = sp_walk_next(&it);
        ASSERT(e && sp_path_eq(&e->dirpath, &walk_base) && e->dirname_count == 1 && e->filename_count == 2);
        ASSERT(sp_walk_next(&it) == NULL && it.error == SP_OK);

        /* A buffer too small for the names, a flag walk doesn't take, and a directory that can't be listed */
        it = sp_walk_begin(&walk_base, SP_WALK_TOP_DOWN, names, 40);
        ASSERT(sp_walk_next(&it) == NULL && it.error == SP_ERR_TOO_LONG);
        it = sp_walk_begin(&walk_base, SP_MKDIR_PARENTS, names, sizeof(names));
        ASSERT(sp_walk_next(&it) == NULL && it.error == SP_ERR_INVALID_ARG);
        SpPath missing = sp_join_one(&walk_base, "missing");
        it = sp_walk_begin(&missing, SP_WALK_TOP_DOWN, names, sizeof(names));
        e = sp_walk_next(&it);
        ASSERT(e && e->error == SP_ERR_NOT_FOUND && e->dirname_count == 0 && sp_path_eq(&e->dirpath, &missing));
        ASSERT(sp_walk_next(&it) == NULL && it.error == SP_OK);
    }

    /* Cleanup */
    remove(walk_f1);
    remove(walk_f2);
    remove(walk_f3);
    test_rmdir(walk_sub);
    test_rmdir(walk_path);

    printf("  walk tests OK\n");

    /* ============ Error Tests ============ */
    printf("\nerror Tests:\n");
    {
        /* Past SP_MAX_SUFFIXES suffixes */
        char many[2 * (SP_MAX_SUFFIXES + 1) + 2] = "a";
        for (size_t i = 0; i <= SP_MAX_SUFFIXES; i++)
            strcat(many, ".x");
        SpPath many_path = sp_path_f(many, P);
        SpSuffixes limited = sp_suffixes(&many_path);
        ASSERT(limited.error == SP_ERR_LIMIT && limited.count == 0);

        /* Errors from the OS, specific */
        char err_dir[64];
        snprintf(err_dir, sizeof(err_dir), "./test_errors_%ld", pid);
        SpPath root_dir = sp_path(err_dir), file = sp_join_one(&root_dir, "f"), below = sp_join_one(&file, "x");
        ASSERT(sp_mkdir(&root_dir, 0755, 0, SP_MODE_DIR) == SP_OK);
        ASSERT(sp_touch(&file, 0644, false) == SP_OK);
        ASSERT(sp_mkdir(&root_dir, 0755, SP_MKDIR_EXIST_OK | SP_COPY_FOLLOW_SYMLINKS, SP_MODE_DIR) == SP_ERR_INVALID_ARG);
        ASSERT(sp_mkdir(&file, 0755, SP_MKDIR_EXIST_OK, SP_MODE_DIR) == SP_ERR_EXISTS);
        ASSERT(sp_mkdir(&below, 0755, 0, SP_MODE_DIR) == SP_ERR_NOT_DIR);
        ASSERT(sp_touch(&file, 0644, false) == SP_ERR_EXISTS);
        ASSERT(sp_rmdir(&root_dir) == SP_ERR_NOT_EMPTY);
        SpIterdirIter bad_dir = sp_iterdir_begin(&file);
        SpPath listed;
        ASSERT(bad_dir.error == SP_ERR_NOT_DIR && !sp_iterdir_next(&bad_dir, &listed));
        sp_iterdir_end(&bad_dir);
        SpIterdirIter listing = sp_iterdir_begin(&root_dir);
        ASSERT(sp_iterdir_next(&listing, &listed) && sp_path_eq(&listed, &file));
        ASSERT(!sp_iterdir_next(&listing, &listed) && listing.error == SP_OK);
        sp_iterdir_end(&listing);

#ifndef SP_WINDOWS
        /* Mode 0 is a mode */
        SpPath closed = sp_join_one(&root_dir, "closed");
        ASSERT(sp_mkdir(&closed, 0, 0, SP_MODE_DIR) == SP_OK);
        ASSERT((sp_stat(&closed).sp_mode & 0777) == 0);
        ASSERT(sp_chmod(&closed, 0755, true) == SP_OK);
        ASSERT(sp_rmdir(&closed) == SP_OK);

        /* Symlinks: chmod through one, and copying one onto another */
        SpPath link = sp_join_one(&root_dir, "link"), link2 = sp_join_one(&root_dir, "link2");
        SpPath to_file = sp_path("f"), to_dir = sp_path(".");
        ASSERT(sp_symlink_to(&link, &to_file, false) == SP_OK);
        ASSERT(sp_symlink_to(&link2, &to_dir, true) == SP_OK);
        ASSERT(sp_chmod(&link, 0600, true) == SP_OK && (sp_stat(&file).sp_mode & 0777) == 0600);
        SpError no_follow = sp_chmod(&link, 0644, false);
        ASSERT(no_follow == SP_OK || no_follow == SP_ERR_UNSUPPORTED);
        ASSERT(sp_copy(&link, &link2, 0).error == SP_ERR_EXISTS);
        ASSERT(sp_copy(&link, &link2, SP_COPY_FOLLOW_SYMLINKS).error == SP_ERR_IS_DIR);
        ASSERT(sp_unlink(&link, false) == SP_OK && sp_unlink(&link2, false) == SP_OK);
#endif

        /* A glob deeper than SP_GLOB_MAX_DEPTH */
        SpPath deepest = root_dir;
        for (int i = 0; i < SP_GLOB_MAX_DEPTH; i++)
            deepest = sp_join_one(&deepest, "d");
        ASSERT(sp_mkdir(&deepest, 0755, SP_MKDIR_PARENTS, SP_MODE_DIR) == SP_OK);
        SpGlobIter deep = sp_glob_begin(&root_dir, "**", SP_CASE_DEFAULT, false);
        int found = 0;
        while (sp_glob_next(&deep, &listed))
            found++;
        sp_glob_end(&deep);
        ASSERT(deep.error == SP_ERR_LIMIT && found < SP_GLOB_MAX_DEPTH + 2);
        for (; !sp_path_eq(&deepest, &root_dir); deepest = sp_parent(&deepest))
            ASSERT(sp_rmdir(&deepest) == SP_OK);

        /* An embedded NUL can't reach the OS */
        SpPath nul = sp_path_from_n("a\0b", 3, P);
        ASSERT(sp_mkdir(&nul, 0755, 0, SP_MODE_DIR) == SP_ERR_NUL && sp_stat(&nul).error == SP_ERR_NUL);
        ASSERT(sp_resolve(&nul, false).error == SP_ERR_NUL);

        ASSERT(sp_unlink(&file, false) == SP_OK && sp_rmdir(&root_dir) == SP_OK);
    }
    printf("  error tests OK\n");

    /* ============ Struct Size Tests ============ */
    printf("\nStruct Size Tests:\n");

    /* Print sizes for informational purposes */
    printf("  SpPath:          %4zu bytes\n", sizeof(SpPath));
    printf("  SpTerm:          %4zu bytes\n", sizeof(SpTerm));
    printf("  SpGlobIter:      %4zu bytes\n", sizeof(SpGlobIter));
    printf("  SpParentsIter:   %4zu bytes\n", sizeof(SpParentsIter));
    printf("  SpIterdirIter:   %4zu bytes\n", sizeof(SpIterdirIter));
    printf("  SpPartsIter:     %4zu bytes\n", sizeof(SpPartsIter));
    printf("  SpSuffixes:      %4zu bytes\n", sizeof(SpSuffixes));
    printf("  SpStatResult:    %4zu bytes\n", sizeof(SpStatResult));
    printf("  SpWalkEntry:     %4zu bytes\n", sizeof(SpWalkEntry));
    printf("  SpWalkIter:      %4zu bytes\n", sizeof(SpWalkIter));
    printf("  SpStr:           %4zu bytes\n", sizeof(SpStr));

    /* Struct sizes expressed relative to SP_PATH_MAX / sizeof(SpPath) so they
     * work across configurations (SP_PATH_MAX=1024 for Windows, 4096 for Linux).
     * Must be updated explicitly when struct layouts change. */
#if defined(__LP64__) || defined(__x86_64__) || defined(__aarch64__) || (defined(_WIN32) && defined(_WIN64))
    /* 64-bit (POSIX and Windows) */
    ASSERT_SIZE(sizeof(SpPath), SP_PATH_MAX + 16);
    ASSERT_SIZE(sizeof(SpTerm), SP_PATH_MAX + 16);
    ASSERT_SIZE(sizeof(SpStr), 16);
    ASSERT_SIZE(sizeof(SpPartsIter), 32);
    ASSERT_SIZE(sizeof(SpParentsIter), 16);
    ASSERT_SIZE(sizeof(SpSuffixes), 272);
    ASSERT_SIZE(sizeof(SpStatResult), 104);
    ASSERT_SIZE(sizeof(SpIterdirIter), sizeof(SpPath) + 24);
    ASSERT_SIZE(sizeof(SpWalkEntry), sizeof(SpPath) + 40);
    ASSERT_SIZE(sizeof(SpWalkIter), sizeof(SpWalkEntry) + 48);
    ASSERT_SIZE(sizeof(SpGlobIter), sizeof(SpPath) + 2840);
#endif

    printf("  struct size tests OK\n");

#ifdef SNAKEPATH_FLUENT
    test_fluent_api();
#endif

    printf("\n%d assertions passed\n", tests_run);
    return 0;
}

#endif /* SP_FFI */
