/* test_fluent_api.c - Fluent API tests for snakepath.h
 * Each test references Python pathlib documentation snippets.
 * https://docs.python.org/3/library/pathlib.html
 */
#define SP_PATH_MAX 1024  /* Use SP_PATH_MAX_WINDOWS for CI compatibility */
#define SNAKEPATH_FLUENT
#define SNAKEPATH_IMPLEMENTATION
#include "snakepath.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <process.h>
#define test_getpid _getpid
#else
#include <unistd.h>
#define test_getpid getpid
#endif

static int tests_run = 0;

/* Inline string view comparison */
static int sv_eq(SpStr a, const char *b) {
    size_t blen = strlen(b);
    return a.len == blen && (a.len == 0 || memcmp(a.data, b, a.len) == 0);
}

/* SpTerm comparison */
static int term_eq(SpTerm a, const char *b) {
    size_t blen = strlen(b);
    return a.len == blen && (a.len == 0 || memcmp(a.buf, b, a.len) == 0);
}

#define ASSERT(cond) do { tests_run++; if (!(cond)) { fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); exit(1); } } while(0)
#define ASSERT_SV(sv, exp) ASSERT(sv_eq(sv, exp))
#define ASSERT_TERM(t, exp) ASSERT(term_eq(t, exp))
#define ASSERT_STR(got, exp) ASSERT(strcmp(got, exp) == 0)
#define ASSERT_FLUENT(f, exp) do { SpPath _p = (f)->path(); ASSERT(strcmp(sp_str(&_p), exp) == 0); } while(0)

int main(void) {
    printf("Fluent API Tests:\n");

    /* ============ Pure Path Creation ============ */

    /* >>> PurePath('setup.py') -> PurePosixPath('setup.py') */
    ASSERT_FLUENT(SPF("setup.py"), "setup.py");

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
    { SpPath p = SPF_W("c:\\windows")->path(); char buf[SP_PATH_MAX]; sp_as_posix(&p, buf, sizeof(buf)); ASSERT_STR(buf, "c:/windows"); }

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
      ASSERT_FLUENT(SPF_P("/etc/passwd")->relative_to(&base), "etc/passwd"); }

    /* >>> PurePosixPath('/etc/passwd').relative_to('/etc') -> PurePosixPath('passwd') */
    { SpPath base = sp_path_f("/etc", SP_FLAVOR_POSIX);
      ASSERT_FLUENT(SPF_P("/etc/passwd")->relative_to(&base), "passwd"); }

    /* ============ relative_to_walk_up (fluent chainable) ============ */

    /* >>> PurePosixPath('/etc/passwd').relative_to('/usr', walk_up=True) -> '../etc/passwd' */
    { SpPath base = sp_path_f("/usr", SP_FLAVOR_POSIX);
      ASSERT_FLUENT(SPF_P("/etc/passwd")->relative_to_walk_up(&base), "../etc/passwd"); }

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
    { SpTerm o = SPF(__FILE__)->owner();
      ASSERT(o.len > 0); }

    /* Path(__FILE__).group() returns non-empty string (POSIX only) */
    { SpTerm g = SPF(__FILE__)->group();
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
    { SpPath p = SPF(__FILE__)->path(); ASSERT(sp_is_file(&p) == true); }

    /* Non-existent file should return false */
    ASSERT(SPF("/nonexistent/file.txt")->is_file() == false);

    /* ============ is_dir (fluent) ============ */

    /* Existing directory should return true */
    { SpPath p = SPF(".")->path(); ASSERT(sp_is_dir(&p) == true); }

    /* Non-existent directory should return false */
    ASSERT(SPF("/nonexistent/dir")->is_dir() == false);

    /* ============ exists (fluent) ============ */

    /* Existing file should return true */
    ASSERT(SPF(__FILE__)->exists() == true);

    /* Existing directory should return true */
    ASSERT(SPF(".")->exists() == true);

    /* Non-existent path should return false */
    ASSERT(SPF("/nonexistent/path")->exists() == false);

    /* ============ read_file / write_file (fluent) ============ */

    {
        long fpid = (long)test_getpid();
        char fluent_rw_path[128];
        snprintf(fluent_rw_path, sizeof(fluent_rw_path), "./test_fluent_rw_%ld.tmp", fpid);

        SpIOResult wr = SPF(fluent_rw_path)->write_file("fluent io", 9);
        ASSERT(wr.error == SP_IO_OK);
        ASSERT(wr.bytes == 9);

        char rbuf[64];
        SpIOResult rd = SPF(fluent_rw_path)->read_file(rbuf, sizeof(rbuf));
        ASSERT(rd.error == SP_IO_OK);
        ASSERT(rd.bytes == 9);
        ASSERT(memcmp(rbuf, "fluent io", 9) == 0);

        SpPath cleanup = sp_path(fluent_rw_path);
        sp_unlink(&cleanup, true);
    }

    /* ============ sp_error_str / sp_error_print ============ */

    ASSERT_STR(sp_error_str(SP_OK), "Success");
    ASSERT_STR(sp_error_str(SP_ERR), "Operation failed");
    ASSERT_STR(sp_error_str(SP_ERR_EXISTS), "File exists");
    ASSERT_STR(sp_error_str(SP_ERR_NOT_FOUND), "No such file or directory");
    ASSERT_STR(sp_error_str(SP_ERR_NOT_DIR), "Not a directory");
    ASSERT_STR(sp_error_str(SP_ERR_PERMISSION), "Permission denied");
    ASSERT_STR(sp_error_str(SP_ERR_EXISTS_NOT_DIR), "Path exists but is not a directory");
    ASSERT_STR(sp_error_str(SP_ERR_OPEN), "Could not open file");
    ASSERT_STR(sp_error_str(SP_ERR_READ), "Read failed");
    ASSERT_STR(sp_error_str(SP_ERR_WRITE), "Write failed");
    ASSERT_STR(sp_error_str(SP_ERR_TOO_LARGE), "File too large for buffer");
    ASSERT_STR(sp_error_str(SP_ERR_OTHER_OP), "Unknown error");
    ASSERT_STR(sp_error_str(9999), "Unknown error");

    /* ============ is_reserved (fluent) ============ */

    /* On POSIX, nothing is reserved */
    ASSERT(SPF_P("CON")->is_reserved() == false);
    /* On Windows flavor, CON is reserved */
    ASSERT(SPF_W("CON")->is_reserved() == true);

    /* ============ stat / lstat (fluent) ============ */

    { SpStatResult st = SPF(__FILE__)->stat();
      ASSERT(st.valid == true);
      ASSERT(st.sp_size > 0); }

    { SpStatResult st = SPF(__FILE__)->lstat();
      ASSERT(st.valid == true);
      ASSERT(st.sp_size > 0); }

    /* Non-existent file stat */
    { SpStatResult st = SPF("/nonexistent_stat_test")->stat();
      ASSERT(st.valid == false); }

    /* ============ as_posix (fluent) ============ */

    { char buf[SP_PATH_MAX];
      SPF_W("c:\\windows\\system32")->as_posix(buf, sizeof(buf));
      ASSERT_STR(buf, "c:/windows/system32"); }

    /* ============ as_uri (fluent) ============ */

    { char buf[SP_PATH_MAX];
      size_t len = SPF_P("/etc/hosts")->as_uri(buf, sizeof(buf));
      ASSERT(len > 0);
      ASSERT_STR(buf, "file:///etc/hosts"); }

    /* ============ match (fluent) ============ */

    ASSERT(SPF_P("/foo/bar.py")->match("*.py") == SP_MATCH_YES);
    ASSERT(SPF_P("/foo/bar.py")->match("*.txt") == SP_MATCH_NO);
    ASSERT(SPF_P("/foo/bar.py")->match("foo/*.py") == SP_MATCH_YES);

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
        sp_touch(&tp, 0644, true);
        SpPath lp = sp_path(link_path);
        sp_symlink_to(&lp, &tp, false);

        /* readlink should return the target */
        { SpPath rl = SPF(link_path)->readlink()->path();
          ASSERT(rl.len > 0); }

        sp_unlink(&lp, true);
        sp_unlink(&tp, true);
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
        sp_touch(&srcp, 0644, true);
        SpPath dp = sp_path(dst_path);

        SpPath result = SPF(src_path)->rename(&dp)->path();
        ASSERT_STR(sp_str(&result), dst_path);

        /* Clean up */
        SpPath cleanup = sp_path(dst_path);
        sp_unlink(&cleanup, true);
    }

    {
        long fpid = (long)test_getpid();
        char src_path[128], dst_path[128];
        snprintf(src_path, sizeof(src_path), "test_replace_src_%ld.tmp", fpid);
        snprintf(dst_path, sizeof(dst_path), "test_replace_dst_%ld.tmp", fpid);

        /* Create both files, replace dst with src */
        SpPath srcp2 = sp_path(src_path);
        sp_touch(&srcp2, 0644, true);
        SpPath dp2 = sp_path(dst_path);
        sp_touch(&dp2, 0644, true);

        SpPath result = SPF(src_path)->replace(&dp2)->path();
        ASSERT_STR(sp_str(&result), dst_path);

        /* Clean up */
        SpPath cleanup = sp_path(dst_path);
        sp_unlink(&cleanup, true);
    }

    /* ============ mkdir (fluent SpPathOp) ============ */

    {
        long fpid = (long)test_getpid();
        char dir_path[128];
        snprintf(dir_path, sizeof(dir_path), "test_fluent_mkdir_%ld", fpid);

        SpPathOp r = SPF(dir_path)->mkdir(0755, false, false);
        ASSERT(r.error == SP_OK);
        ASSERT_STR(sp_str(&r.path), dir_path);

        /* mkdir again without exist_ok should fail */
        SpPathOp r2 = SPF(dir_path)->mkdir(0755, false, false);
        ASSERT(r2.error == SP_ERR_EXISTS);

        /* mkdir with exist_ok should succeed */
        SpPathOp r3 = SPF(dir_path)->mkdir(0755, false, true);
        ASSERT(r3.error == SP_OK);

        SpPath cleanup = sp_path(dir_path);
        sp_rmdir(&cleanup);
    }

    /* ============ touch (fluent SpPathOp) ============ */

    {
        long fpid = (long)test_getpid();
        char touch_path[128];
        snprintf(touch_path, sizeof(touch_path), "test_fluent_touch_%ld.tmp", fpid);

        SpPathOp r = SPF(touch_path)->touch(0644, true);
        ASSERT(r.error == SP_OK);
        ASSERT_STR(sp_str(&r.path), touch_path);

        /* Verify file exists */
        ASSERT(SPF(touch_path)->exists() == true);

        SpPath cleanup = sp_path(touch_path);
        sp_unlink(&cleanup, true);
    }

    /* ============ unlink (fluent SpPathOp) ============ */

    {
        long fpid = (long)test_getpid();
        char unlink_path[128];
        snprintf(unlink_path, sizeof(unlink_path), "test_fluent_unlink_%ld.tmp", fpid);

        SpPath p = sp_path(unlink_path);
        sp_touch(&p, 0644, true);

        SpPathOp r = SPF(unlink_path)->unlink(false);
        ASSERT(r.error == SP_OK);
        ASSERT_STR(sp_str(&r.path), unlink_path);

        /* Unlink non-existent without missing_ok should fail */
        SpPathOp r2 = SPF(unlink_path)->unlink(false);
        ASSERT(r2.error == SP_ERR);

        /* Unlink non-existent with missing_ok should succeed */
        SpPathOp r3 = SPF(unlink_path)->unlink(true);
        ASSERT(r3.error == SP_OK);
    }

    /* ============ rmdir (fluent SpPathOp) ============ */

    {
        long fpid = (long)test_getpid();
        char rmdir_path[128];
        snprintf(rmdir_path, sizeof(rmdir_path), "test_fluent_rmdir_%ld", fpid);

        SpPath p = sp_path(rmdir_path);
        sp_mkdir(&p, 0755, false, false);

        SpPathOp r = SPF(rmdir_path)->rmdir();
        ASSERT(r.error == SP_OK);
        ASSERT_STR(sp_str(&r.path), rmdir_path);

        /* Rmdir non-existent should fail */
        SpPathOp r2 = SPF(rmdir_path)->rmdir();
        ASSERT(r2.error == SP_ERR);
    }

    /* ============ chmod (fluent SpPathOp) ============ */

#ifndef _WIN32
    {
        long fpid = (long)test_getpid();
        char chmod_path[128];
        snprintf(chmod_path, sizeof(chmod_path), "test_fluent_chmod_%ld.tmp", fpid);

        SpPath p = sp_path(chmod_path);
        sp_touch(&p, 0644, true);

        SpPathOp r = SPF(chmod_path)->chmod(0600);
        ASSERT(r.error == SP_OK);
        ASSERT_STR(sp_str(&r.path), chmod_path);

        sp_unlink(&p, true);
    }
#endif

    /* ============ symlink_to / hardlink_to (fluent SpPathOp) ============ */

#ifndef _WIN32
    {
        long fpid = (long)test_getpid();
        char target_path[128], sym_path[128], hard_path[128];
        snprintf(target_path, sizeof(target_path), "test_fluent_symtgt_%ld.tmp", fpid);
        snprintf(sym_path, sizeof(sym_path), "test_fluent_sym_%ld.tmp", fpid);
        snprintf(hard_path, sizeof(hard_path), "test_fluent_hard_%ld.tmp", fpid);

        SpPath tp = sp_path(target_path);
        sp_touch(&tp, 0644, true);

        /* symlink_to */
        SpPathOp r = SPF(sym_path)->symlink_to(&tp, false);
        ASSERT(r.error == SP_OK);
        ASSERT_STR(sp_str(&r.path), sym_path);
        ASSERT(SPF(sym_path)->is_symlink() == true);

        /* hardlink_to */
        SpPathOp r2 = SPF(hard_path)->hardlink_to(&tp);
        ASSERT(r2.error == SP_OK);
        ASSERT_STR(sp_str(&r2.path), hard_path);

        SpPath sp2 = sp_path(sym_path);
        SpPath hp = sp_path(hard_path);
        sp_unlink(&sp2, true);
        sp_unlink(&hp, true);
        sp_unlink(&tp, true);
    }
#endif

    printf("  All fluent API tests OK\n");
    printf("\n%d assertions passed\n", tests_run);
    return 0;
}
