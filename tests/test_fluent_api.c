/* test_fluent_api.c - Fluent API tests for snakepath.h
 * Each test references Python pathlib documentation snippets.
 * https://docs.python.org/3/library/pathlib.html
 */
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
#define ASSERT_FLUENT(f, exp) do { SpFluentPath _f = (f); ASSERT(strcmp(sp_str(&_f.path), exp) == 0); } while(0)

int main(void) {
    printf("Fluent API Tests:\n");

    /* ============ Pure Path Creation ============ */

    /* >>> PurePath('setup.py') -> PurePosixPath('setup.py') */
    ASSERT_FLUENT(SPF("setup.py"), "setup.py");

    /* >>> PurePath('foo', 'some/path', 'bar') -> PurePosixPath('foo/some/path/bar') */
    ASSERT_FLUENT(SPF_P("foo").join("some/path").join("bar"), "foo/some/path/bar");

    /* >>> PurePosixPath('/etc/hosts') -> PurePosixPath('/etc/hosts') */
    ASSERT_FLUENT(SPF_P("/etc/hosts"), "/etc/hosts");

    /* >>> PureWindowsPath('c:/', 'Users', 'Ximénez') -> PureWindowsPath('c:/Users/Ximénez') */
    ASSERT_FLUENT(SPF_W("c:/").join("Users").join("Ximenez"), "c:\\Users\\Ximenez");

    /* ============ Drive ============ */

    /* >>> PureWindowsPath('c:/Program Files/').drive -> 'c:' */
    { SpFluentPath f = SPF_W("c:/Program Files/"); ASSERT_SV(sp_drive(&f.path), "c:"); }

    /* >>> PurePosixPath('/etc').drive -> '' */
    { SpFluentPath f = SPF_P("/etc"); ASSERT_SV(sp_drive(&f.path), ""); }

    /* >>> PureWindowsPath('//host/share/foo.txt').drive -> '\\\\host\\share' */
    { SpFluentPath f = SPF_W("//host/share/foo.txt"); ASSERT_SV(sp_drive(&f.path), "\\\\host\\share"); }

    /* ============ Root ============ */

    /* >>> PureWindowsPath('c:/Program Files/').root -> '\\' */
    { SpFluentPath f = SPF_W("c:/Program Files/"); ASSERT_SV(sp_root(&f.path), "\\"); }

    /* >>> PurePosixPath('/etc').root -> '/' */
    { SpFluentPath f = SPF_P("/etc"); ASSERT_SV(sp_root(&f.path), "/"); }

    /* >>> PureWindowsPath('c:Program Files/').root -> '' */
    { SpFluentPath f = SPF_W("c:Program Files/"); ASSERT_SV(sp_root(&f.path), ""); }

    /* ============ Anchor ============ */

    /* >>> PureWindowsPath('c:/Program Files/').anchor -> 'c:\\' */
    { SpFluentPath f = SPF_W("c:/Program Files/"); ASSERT_SV(sp_anchor(&f.path), "c:\\"); }

    /* >>> PurePosixPath('/etc').anchor -> '/' */
    { SpFluentPath f = SPF_P("/etc"); ASSERT_SV(sp_anchor(&f.path), "/"); }

    /* ============ Parent ============ */

    /* >>> PurePosixPath('/a/b/c/d').parent -> PurePosixPath('/a/b/c') */
    ASSERT_FLUENT(SPF_P("/a/b/c/d").parent(), "/a/b/c");

    /* >>> PurePosixPath('/').parent -> PurePosixPath('/') */
    ASSERT_FLUENT(SPF_P("/").parent(), "/");

    /* >>> PurePosixPath('.').parent -> PurePosixPath('.') */
    ASSERT_FLUENT(SPF_P(".").parent(), ".");

    /* ============ Name ============ */

    /* >>> PurePosixPath('my/library/setup.py').name -> 'setup.py' */
    { SpFluentPath f = SPF_P("my/library/setup.py"); ASSERT_SV(sp_name(&f.path), "setup.py"); }

    /* >>> PureWindowsPath('//some/share/setup.py').name -> 'setup.py' */
    { SpFluentPath f = SPF_W("//some/share/setup.py"); ASSERT_SV(sp_name(&f.path), "setup.py"); }

    /* >>> PureWindowsPath('//some/share').name -> '' */
    { SpFluentPath f = SPF_W("//some/share"); ASSERT_SV(sp_name(&f.path), ""); }

    /* ============ Suffix ============ */

    /* >>> PurePosixPath('my/library/setup.py').suffix -> '.py' */
    { SpFluentPath f = SPF_P("my/library/setup.py"); ASSERT_SV(sp_suffix(&f.path), ".py"); }

    /* >>> PurePosixPath('my/library.tar.gz').suffix -> '.gz' */
    { SpFluentPath f = SPF_P("my/library.tar.gz"); ASSERT_SV(sp_suffix(&f.path), ".gz"); }

    /* >>> PurePosixPath('my/library').suffix -> '' */
    { SpFluentPath f = SPF_P("my/library"); ASSERT_SV(sp_suffix(&f.path), ""); }

    /* ============ Suffixes ============ */

    /* >>> PurePosixPath('my/library.tar.gz').suffixes -> ['.tar', '.gz'] */
    { SpFluentPath f = SPF_P("my/library.tar.gz"); SpSuffixes s = sp_suffixes(&f.path);
      ASSERT(s.count == 2); ASSERT_SV(s.items[0], ".tar"); ASSERT_SV(s.items[1], ".gz"); }

    /* >>> PurePosixPath('my/library').suffixes -> [] */
    { SpFluentPath f = SPF_P("my/library"); ASSERT(sp_suffixes(&f.path).count == 0); }

    /* ============ Stem ============ */

    /* >>> PurePosixPath('my/library.tar.gz').stem -> 'library.tar' */
    { SpFluentPath f = SPF_P("my/library.tar.gz"); ASSERT_SV(sp_stem(&f.path), "library.tar"); }

    /* >>> PurePosixPath('my/library.tar').stem -> 'library' */
    { SpFluentPath f = SPF_P("my/library.tar"); ASSERT_SV(sp_stem(&f.path), "library"); }

    /* ============ as_posix ============ */

    /* >>> PureWindowsPath('c:\\windows').as_posix() -> 'c:/windows' */
    { SpFluentPath f = SPF_W("c:\\windows"); char buf[SP_PATH_MAX]; sp_as_posix(&f.path, buf, sizeof(buf)); ASSERT_STR(buf, "c:/windows"); }

    /* ============ is_absolute ============ */

    /* >>> PurePosixPath('/a/b').is_absolute() -> True */
    { SpFluentPath f = SPF_P("/a/b"); ASSERT(sp_is_absolute(&f.path) == true); }

    /* >>> PurePosixPath('a/b').is_absolute() -> False */
    { SpFluentPath f = SPF_P("a/b"); ASSERT(sp_is_absolute(&f.path) == false); }

    /* >>> PureWindowsPath('c:/a/b').is_absolute() -> True */
    { SpFluentPath f = SPF_W("c:/a/b"); ASSERT(sp_is_absolute(&f.path) == true); }

    /* >>> PureWindowsPath('/a/b').is_absolute() -> False */
    { SpFluentPath f = SPF_W("/a/b"); ASSERT(sp_is_absolute(&f.path) == false); }

    /* >>> PureWindowsPath('c:').is_absolute() -> False */
    { SpFluentPath f = SPF_W("c:"); ASSERT(sp_is_absolute(&f.path) == false); }

    /* >>> PureWindowsPath('//some/share').is_absolute() -> True */
    { SpFluentPath f = SPF_W("//server/share/a"); ASSERT(sp_is_absolute(&f.path) == true); }

    /* ============ joinpath ============ */

    /* >>> PurePosixPath('/etc').joinpath('passwd') -> PurePosixPath('/etc/passwd') */
    ASSERT_FLUENT(SPF_P("/etc").join("passwd"), "/etc/passwd");

    /* >>> PurePosixPath('/etc').joinpath('init.d', 'apache2') -> PurePosixPath('/etc/init.d/apache2') */
    ASSERT_FLUENT(SPF_P("/etc").join("init.d").join("apache2"), "/etc/init.d/apache2");

    /* >>> PureWindowsPath('c:').joinpath('/Program Files') -> PureWindowsPath('c:/Program Files') */
    ASSERT_FLUENT(SPF_W("c:").join("/Program Files"), "c:\\Program Files");

    /* ============ with_name ============ */

    /* >>> PureWindowsPath('c:/Downloads/pathlib.tar.gz').with_name('setup.py') -> 'c:/Downloads/setup.py' */
    ASSERT_FLUENT(SPF_W("c:/Downloads/pathlib.tar.gz").with_name("setup.py"), "c:\\Downloads\\setup.py");

    /* ============ with_stem ============ */

    /* >>> PureWindowsPath('c:/Downloads/draft.txt').with_stem('final') -> 'c:/Downloads/final.txt' */
    ASSERT_FLUENT(SPF_W("c:/Downloads/draft.txt").with_stem("final"), "c:\\Downloads\\final.txt");

    /* >>> PureWindowsPath('c:/Downloads/pathlib.tar.gz').with_stem('lib') -> 'c:/Downloads/lib.gz' */
    ASSERT_FLUENT(SPF_W("c:/Downloads/pathlib.tar.gz").with_stem("lib"), "c:\\Downloads\\lib.gz");

    /* ============ with_suffix ============ */

    /* >>> PureWindowsPath('c:/Downloads/pathlib.tar.gz').with_suffix('.bz2') -> 'c:/Downloads/pathlib.tar.bz2' */
    ASSERT_FLUENT(SPF_W("c:/Downloads/pathlib.tar.gz").with_suffix(".bz2"), "c:\\Downloads\\pathlib.tar.bz2");

    /* >>> PureWindowsPath('README').with_suffix('.txt') -> PureWindowsPath('README.txt') */
    ASSERT_FLUENT(SPF_P("a/README").with_suffix(".txt"), "a/README.txt");

    /* >>> PureWindowsPath('README.txt').with_suffix('') -> PureWindowsPath('README') */
    ASSERT_FLUENT(SPF_P("a/README.txt").with_suffix(""), "a/README");

    /* ============ is_relative_to ============ */

    /* >>> PurePath('/etc/passwd').is_relative_to('/etc') -> True */
    { SpFluentPath f = SPF_P("/etc/passwd"); SpPath base = sp_path_f("/etc", SP_FLAVOR_POSIX);
      ASSERT(sp_is_relative_to(&f.path, &base) == true); }

    /* >>> PurePath('/etc/passwd').is_relative_to('/usr') -> False */
    { SpFluentPath f = SPF_P("/etc/passwd"); SpPath base = sp_path_f("/usr", SP_FLAVOR_POSIX);
      ASSERT(sp_is_relative_to(&f.path, &base) == false); }

    /* ============ relative_to (fluent chainable) ============ */

    /* >>> PurePosixPath('/etc/passwd').relative_to('/') -> PurePosixPath('etc/passwd') */
    { SpPath base = sp_path_f("/", SP_FLAVOR_POSIX);
      ASSERT_FLUENT(SPF_P("/etc/passwd").relative_to(&base), "etc/passwd"); }

    /* >>> PurePosixPath('/etc/passwd').relative_to('/etc') -> PurePosixPath('passwd') */
    { SpPath base = sp_path_f("/etc", SP_FLAVOR_POSIX);
      ASSERT_FLUENT(SPF_P("/etc/passwd").relative_to(&base), "passwd"); }

    /* ============ relative_to_walk_up (fluent chainable) ============ */

    /* >>> PurePosixPath('/etc/passwd').relative_to('/usr', walk_up=True) -> '../etc/passwd' */
    { SpPath base = sp_path_f("/usr", SP_FLAVOR_POSIX);
      ASSERT_FLUENT(SPF_P("/etc/passwd").relative_to_walk_up(&base), "../etc/passwd"); }

    /* ============ absolute (fluent chainable) ============ */

    /* Path('foo/bar').absolute() returns absolute path */
    { SpFluentPath f = SPF_P("foo/bar").absolute();
      ASSERT(sp_is_absolute(&f.path) == true); }

    /* ============ Chaining ============ */

    /* Path('/a/b/c.txt').parent.name -> 'b' */
    { SpFluentPath f = SPF_P("/a/b/c.txt").parent(); ASSERT_SV(sp_name(&f.path), "b"); }

    /* Path('/a/b/c').parent.parent -> '/a' */
    ASSERT_FLUENT(SPF_P("/a/b/c").parent().parent(), "/a");

    /* (Path('/a') / 'b' / 'c').parent -> '/a/b' */
    ASSERT_FLUENT(SPF_P("/a").join("b").join("c").parent(), "/a/b");

    /* Path('a/b.txt').with_name('c.py').suffix -> '.py' */
    { SpFluentPath f = SPF_P("a/b.txt").with_name("c.py"); ASSERT_SV(sp_suffix(&f.path), ".py"); }

    /* Path('a/b.txt').with_suffix('.md').stem -> 'b' */
    { SpFluentPath f = SPF_P("a/b.txt").with_suffix(".md"); ASSERT_SV(sp_stem(&f.path), "b"); }

    /* ============ Storing and resuming ============ */

    /* Store intermediate, resume with SP() */
    SpFluentPath base = SPF_P("/home/user");
    SpFluentPath docs = SP(base).join("Documents");
    SpFluentPath pics = SP(base).join("Pictures");
    ASSERT_STR(sp_str(&docs.path), "/home/user/Documents");
    ASSERT_STR(sp_str(&pics.path), "/home/user/Pictures");

    /* Safe parallel access - each has its own buffer */
    printf("  docs=%s pics=%s\n", sp_str(&docs.path), sp_str(&pics.path));

    printf("  All fluent API tests OK\n");
    printf("\n%d assertions passed\n", tests_run);
    return 0;
}
