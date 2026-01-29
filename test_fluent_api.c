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
#define ASSERT_PATH(p, exp) do { SpPath _p = (p); ASSERT(strcmp(sp_str(&_p), exp) == 0); } while(0)

int main(void) {
    printf("Fluent API Tests:\n");
    
    /* ============ Pure Path Creation ============ */
    
    /* >>> PurePath('setup.py') -> PurePosixPath('setup.py') */
    ASSERT_STR(SPF("setup.py").str(), "setup.py");
    
    /* >>> PurePath('foo', 'some/path', 'bar') -> PurePosixPath('foo/some/path/bar') */
    ASSERT_STR(SPF_P("foo").join("some/path").join("bar").str(), "foo/some/path/bar");
    
    /* >>> PurePosixPath('/etc/hosts') -> PurePosixPath('/etc/hosts') */
    ASSERT_STR(SPF_P("/etc/hosts").str(), "/etc/hosts");
    
    /* >>> PureWindowsPath('c:/', 'Users', 'Ximénez') -> PureWindowsPath('c:/Users/Ximénez') */
    ASSERT_STR(SPF_W("c:/").join("Users").join("Ximenez").str(), "c:\\Users\\Ximenez");
    
    /* ============ Drive ============ */
    
    /* >>> PureWindowsPath('c:/Program Files/').drive -> 'c:' */
    ASSERT_SV(SPF_W("c:/Program Files/").drive(), "c:");
    
    /* >>> PurePosixPath('/etc').drive -> '' */
    ASSERT_SV(SPF_P("/etc").drive(), "");
    
    /* >>> PureWindowsPath('//host/share/foo.txt').drive -> '\\\\host\\share' */
    ASSERT_SV(SPF_W("//host/share/foo.txt").drive(), "\\\\host\\share");
    
    /* ============ Root ============ */
    
    /* >>> PureWindowsPath('c:/Program Files/').root -> '\\' */
    ASSERT_SV(SPF_W("c:/Program Files/").root(), "\\");
    
    /* >>> PurePosixPath('/etc').root -> '/' */
    ASSERT_SV(SPF_P("/etc").root(), "/");
    
    /* >>> PureWindowsPath('c:Program Files/').root -> '' */
    ASSERT_SV(SPF_W("c:Program Files/").root(), "");
    
    /* ============ Anchor ============ */
    
    /* >>> PureWindowsPath('c:/Program Files/').anchor -> 'c:\\' */
    ASSERT_SV(SPF_W("c:/Program Files/").anchor(), "c:\\");
    
    /* >>> PurePosixPath('/etc').anchor -> '/' */
    ASSERT_SV(SPF_P("/etc").anchor(), "/");
    
    /* ============ Parent ============ */
    
    /* >>> PurePosixPath('/a/b/c/d').parent -> PurePosixPath('/a/b/c') */
    ASSERT_STR(SPF_P("/a/b/c/d").parent().str(), "/a/b/c");
    
    /* >>> PurePosixPath('/').parent -> PurePosixPath('/') */
    ASSERT_STR(SPF_P("/").parent().str(), "/");
    
    /* >>> PurePosixPath('.').parent -> PurePosixPath('.') */
    ASSERT_STR(SPF_P(".").parent().str(), ".");
    
    /* ============ Name ============ */
    
    /* >>> PurePosixPath('my/library/setup.py').name -> 'setup.py' */
    ASSERT_SV(SPF_P("my/library/setup.py").name(), "setup.py");
    
    /* >>> PureWindowsPath('//some/share/setup.py').name -> 'setup.py' */
    ASSERT_SV(SPF_W("//some/share/setup.py").name(), "setup.py");
    
    /* >>> PureWindowsPath('//some/share').name -> '' */
    ASSERT_SV(SPF_W("//some/share").name(), "");
    
    /* ============ Suffix ============ */
    
    /* >>> PurePosixPath('my/library/setup.py').suffix -> '.py' */
    ASSERT_SV(SPF_P("my/library/setup.py").suffix(), ".py");
    
    /* >>> PurePosixPath('my/library.tar.gz').suffix -> '.gz' */
    ASSERT_SV(SPF_P("my/library.tar.gz").suffix(), ".gz");
    
    /* >>> PurePosixPath('my/library').suffix -> '' */
    ASSERT_SV(SPF_P("my/library").suffix(), "");
    
    /* ============ Suffixes ============ */
    
    /* >>> PurePosixPath('my/library.tar.gz').suffixes -> ['.tar', '.gz'] */
    SpSuffixes s1 = SPF_P("my/library.tar.gz").suffixes();
    ASSERT(s1.count == 2); ASSERT_SV(s1.items[0], ".tar"); ASSERT_SV(s1.items[1], ".gz");
    
    /* >>> PurePosixPath('my/library').suffixes -> [] */
    ASSERT(SPF_P("my/library").suffixes().count == 0);
    
    /* ============ Stem ============ */
    
    /* >>> PurePosixPath('my/library.tar.gz').stem -> 'library.tar' */
    ASSERT_SV(SPF_P("my/library.tar.gz").stem(), "library.tar");
    
    /* >>> PurePosixPath('my/library.tar').stem -> 'library' */
    ASSERT_SV(SPF_P("my/library.tar").stem(), "library");
    
    /* ============ as_posix ============ */
    
    /* >>> PureWindowsPath('c:\\windows').as_posix() -> 'c:/windows' */
    ASSERT_STR(SPF_W("c:\\windows").as_posix(), "c:/windows");
    
    /* ============ is_absolute ============ */
    
    /* >>> PurePosixPath('/a/b').is_absolute() -> True */
    ASSERT(SPF_P("/a/b").is_absolute() == true);
    
    /* >>> PurePosixPath('a/b').is_absolute() -> False */
    ASSERT(SPF_P("a/b").is_absolute() == false);
    
    /* >>> PureWindowsPath('c:/a/b').is_absolute() -> True */
    ASSERT(SPF_W("c:/a/b").is_absolute() == true);
    
    /* >>> PureWindowsPath('/a/b').is_absolute() -> False */
    ASSERT(SPF_W("/a/b").is_absolute() == false);
    
    /* >>> PureWindowsPath('c:').is_absolute() -> False */
    ASSERT(SPF_W("c:").is_absolute() == false);
    
    /* >>> PureWindowsPath('//some/share').is_absolute() -> True */
    ASSERT(SPF_W("//server/share/a").is_absolute() == true);
    
    /* ============ joinpath ============ */
    
    /* >>> PurePosixPath('/etc').joinpath('passwd') -> PurePosixPath('/etc/passwd') */
    ASSERT_STR(SPF_P("/etc").join("passwd").str(), "/etc/passwd");
    
    /* >>> PurePosixPath('/etc').joinpath('init.d', 'apache2') -> PurePosixPath('/etc/init.d/apache2') */
    ASSERT_STR(SPF_P("/etc").join("init.d").join("apache2").str(), "/etc/init.d/apache2");
    
    /* >>> PureWindowsPath('c:').joinpath('/Program Files') -> PureWindowsPath('c:/Program Files') */
    ASSERT_STR(SPF_W("c:").join("/Program Files").str(), "c:\\Program Files");
    
    /* ============ with_name ============ */
    
    /* >>> PureWindowsPath('c:/Downloads/pathlib.tar.gz').with_name('setup.py') -> 'c:/Downloads/setup.py' */
    ASSERT_STR(SPF_W("c:/Downloads/pathlib.tar.gz").with_name("setup.py").str(), "c:\\Downloads\\setup.py");
    
    /* ============ with_stem ============ */
    
    /* >>> PureWindowsPath('c:/Downloads/draft.txt').with_stem('final') -> 'c:/Downloads/final.txt' */
    ASSERT_STR(SPF_W("c:/Downloads/draft.txt").with_stem("final").str(), "c:\\Downloads\\final.txt");
    
    /* >>> PureWindowsPath('c:/Downloads/pathlib.tar.gz').with_stem('lib') -> 'c:/Downloads/lib.gz' */
    ASSERT_STR(SPF_W("c:/Downloads/pathlib.tar.gz").with_stem("lib").str(), "c:\\Downloads\\lib.gz");
    
    /* ============ with_suffix ============ */
    
    /* >>> PureWindowsPath('c:/Downloads/pathlib.tar.gz').with_suffix('.bz2') -> 'c:/Downloads/pathlib.tar.bz2' */
    ASSERT_STR(SPF_W("c:/Downloads/pathlib.tar.gz").with_suffix(".bz2").str(), "c:\\Downloads\\pathlib.tar.bz2");
    
    /* >>> PureWindowsPath('README').with_suffix('.txt') -> PureWindowsPath('README.txt') */
    ASSERT_STR(SPF_P("a/README").with_suffix(".txt").str(), "a/README.txt");
    
    /* >>> PureWindowsPath('README.txt').with_suffix('') -> PureWindowsPath('README') */
    ASSERT_STR(SPF_P("a/README.txt").with_suffix("").str(), "a/README");
    
    /* ============ is_relative_to ============ */
    
    /* >>> PurePath('/etc/passwd').is_relative_to('/etc') -> True */
    SpPath base1 = sp_path_f("/etc", SP_FLAVOR_POSIX);
    ASSERT(SPF_P("/etc/passwd").is_relative_to(&base1) == true);
    
    /* >>> PurePath('/etc/passwd').is_relative_to('/usr') -> False */
    SpPath base2 = sp_path_f("/usr", SP_FLAVOR_POSIX);
    ASSERT(SPF_P("/etc/passwd").is_relative_to(&base2) == false);
    
    /* ============ relative_to ============ */
    
    /* >>> PurePosixPath('/etc/passwd').relative_to('/') -> PurePosixPath('etc/passwd') */
    SpPath base3 = sp_path_f("/", SP_FLAVOR_POSIX);
    ASSERT_PATH(SPF_P("/etc/passwd").relative_to(&base3), "etc/passwd");
    
    /* >>> PurePosixPath('/etc/passwd').relative_to('/etc') -> PurePosixPath('passwd') */
    SpPath base4 = sp_path_f("/etc", SP_FLAVOR_POSIX);
    ASSERT_PATH(SPF_P("/etc/passwd").relative_to(&base4), "passwd");
    
    /* ============ Chaining ============ */
    
    /* Path('/a/b/c.txt').parent.name -> 'b' */
    ASSERT_SV(SPF_P("/a/b/c.txt").parent().name(), "b");
    
    /* Path('/a/b/c').parent.parent -> '/a' */
    ASSERT_STR(SPF_P("/a/b/c").parent().parent().str(), "/a");
    
    /* (Path('/a') / 'b' / 'c').parent -> '/a/b' */
    ASSERT_STR(SPF_P("/a").join("b").join("c").parent().str(), "/a/b");
    
    /* Path('a/b.txt').with_name('c.py').suffix -> '.py' */
    ASSERT_SV(SPF_P("a/b.txt").with_name("c.py").suffix(), ".py");
    
    /* Path('a/b.txt').with_suffix('.md').stem -> 'b' */
    ASSERT_SV(SPF_P("a/b.txt").with_suffix(".md").stem(), "b");
    
    /* ============ get() / as_sv() ============ */
    
    ASSERT_STR(sp_str(SPF_P("/etc/passwd").parent().get()), "/etc");
    ASSERT_SV(SPF_P("/etc/passwd").as_sv(), "/etc/passwd");
    
    printf("  All fluent API tests OK\n");
    printf("\n%d assertions passed\n", tests_run);
    return 0;
}
