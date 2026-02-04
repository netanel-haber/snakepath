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

static int tests_run = 0;

/* Inline string view comparison */
static int sv_eq(SpStr a, const char *b) {
    size_t blen = strlen(b);
    return a.len == blen && (a.len == 0 || memcmp(a.data, b, a.len) == 0);
}

#define ASSERT(cond) do { tests_run++; if (!(cond)) { fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); exit(1); } } while(0)
#define ASSERT_SV(sv, exp) ASSERT(sv_eq(sv, exp))
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
    { SpPath p = SPF_W("c:/Program Files/")->path(); ASSERT_SV(sp_drive(&p), "c:"); }

    /* >>> PurePosixPath('/etc').drive -> '' */
    { SpPath p = SPF_P("/etc")->path(); ASSERT_SV(sp_drive(&p), ""); }

    /* >>> PureWindowsPath('//host/share/foo.txt').drive -> '\\\\host\\share' */
    { SpPath p = SPF_W("//host/share/foo.txt")->path(); ASSERT_SV(sp_drive(&p), "\\\\host\\share"); }

    /* ============ Root ============ */

    /* >>> PureWindowsPath('c:/Program Files/').root -> '\\' */
    { SpPath p = SPF_W("c:/Program Files/")->path(); ASSERT_SV(sp_root(&p), "\\"); }

    /* >>> PurePosixPath('/etc').root -> '/' */
    { SpPath p = SPF_P("/etc")->path(); ASSERT_SV(sp_root(&p), "/"); }

    /* >>> PureWindowsPath('c:Program Files/').root -> '' */
    { SpPath p = SPF_W("c:Program Files/")->path(); ASSERT_SV(sp_root(&p), ""); }

    /* ============ Anchor ============ */

    /* >>> PureWindowsPath('c:/Program Files/').anchor -> 'c:\\' */
    { SpPath p = SPF_W("c:/Program Files/")->path(); ASSERT_SV(sp_anchor(&p), "c:\\"); }

    /* >>> PurePosixPath('/etc').anchor -> '/' */
    { SpPath p = SPF_P("/etc")->path(); ASSERT_SV(sp_anchor(&p), "/"); }

    /* ============ Parent ============ */

    /* >>> PurePosixPath('/a/b/c/d').parent -> PurePosixPath('/a/b/c') */
    ASSERT_FLUENT(SPF_P("/a/b/c/d")->parent(), "/a/b/c");

    /* >>> PurePosixPath('/').parent -> PurePosixPath('/') */
    ASSERT_FLUENT(SPF_P("/")->parent(), "/");

    /* >>> PurePosixPath('.').parent -> PurePosixPath('.') */
    ASSERT_FLUENT(SPF_P(".")->parent(), ".");

    /* ============ Name ============ */

    /* >>> PurePosixPath('my/library/setup.py').name -> 'setup.py' */
    { SpPath p = SPF_P("my/library/setup.py")->path(); ASSERT_SV(sp_name(&p), "setup.py"); }

    /* >>> PureWindowsPath('//some/share/setup.py').name -> 'setup.py' */
    { SpPath p = SPF_W("//some/share/setup.py")->path(); ASSERT_SV(sp_name(&p), "setup.py"); }

    /* >>> PureWindowsPath('//some/share').name -> '' */
    { SpPath p = SPF_W("//some/share")->path(); ASSERT_SV(sp_name(&p), ""); }

    /* ============ Suffix ============ */

    /* >>> PurePosixPath('my/library/setup.py').suffix -> '.py' */
    { SpPath p = SPF_P("my/library/setup.py")->path(); ASSERT_SV(sp_suffix(&p), ".py"); }

    /* >>> PurePosixPath('my/library.tar.gz').suffix -> '.gz' */
    { SpPath p = SPF_P("my/library.tar.gz")->path(); ASSERT_SV(sp_suffix(&p), ".gz"); }

    /* >>> PurePosixPath('my/library').suffix -> '' */
    { SpPath p = SPF_P("my/library")->path(); ASSERT_SV(sp_suffix(&p), ""); }

    /* ============ Suffixes ============ */

    /* >>> PurePosixPath('my/library.tar.gz').suffixes -> ['.tar', '.gz'] */
    { SpPath p = SPF_P("my/library.tar.gz")->path();
      SpSuffixes s = sp_suffixes(&p);
      ASSERT(s.count == 2); ASSERT_SV(s.items[0], ".tar"); ASSERT_SV(s.items[1], ".gz"); }

    /* >>> PurePosixPath('my/library').suffixes -> [] */
    { SpPath p = SPF_P("my/library")->path(); ASSERT(sp_suffixes(&p).count == 0); }

    /* ============ Stem ============ */

    /* >>> PurePosixPath('my/library.tar.gz').stem -> 'library.tar' */
    { SpPath p = SPF_P("my/library.tar.gz")->path(); ASSERT_SV(sp_stem(&p), "library.tar"); }

    /* >>> PurePosixPath('my/library.tar').stem -> 'library' */
    { SpPath p = SPF_P("my/library.tar")->path(); ASSERT_SV(sp_stem(&p), "library"); }

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
      ASSERT_SV(sp_name(&p), "subdir"); }

    /* ============ owner/group (fluent terminators) ============ */

    /* Path(__FILE__).owner() returns non-empty string */
    { SpStr o = SPF(__FILE__)->owner();
      ASSERT(o.len > 0); }

    /* Path(__FILE__).group() returns non-empty string */
    { SpStr g = SPF(__FILE__)->group();
      ASSERT(g.len > 0); }

    /* ============ Chaining ============ */

    /* Path('/a/b/c.txt').parent.name -> 'b' */
    { SpPath p = SPF_P("/a/b/c.txt")->parent()->path(); ASSERT_SV(sp_name(&p), "b"); }

    /* Path('/a/b/c').parent.parent -> '/a' */
    ASSERT_FLUENT(SPF_P("/a/b/c")->parent()->parent(), "/a");

    /* (Path('/a') / 'b' / 'c').parent -> '/a/b' */
    ASSERT_FLUENT(SPF_P("/a")->join("b")->join("c")->parent(), "/a/b");

    /* Path('a/b.txt').with_name('c.py').suffix -> '.py' */
    { SpPath p = SPF_P("a/b.txt")->with_name("c.py")->path(); ASSERT_SV(sp_suffix(&p), ".py"); }

    /* Path('a/b.txt').with_suffix('.md').stem -> 'b' */
    { SpPath p = SPF_P("a/b.txt")->with_suffix(".md")->path(); ASSERT_SV(sp_stem(&p), "b"); }

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

    printf("  All fluent API tests OK\n");
    printf("\n%d assertions passed\n", tests_run);
    return 0;
}
