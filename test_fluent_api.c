/* test_fluent_api.c - Fluent API tests for snakepath.h
 * Each test references Python pathlib documentation snippets.
 * https://docs.python.org/3/library/pathlib.html
 */

#define SNAKEPATH_FLUENT
#define SNAKEPATH_IMPLEMENTATION
#include "snakepath.h"
#include <stdio.h>
#include <stdlib.h>

static int tests_run = 0;
static int tests_passed = 0;

#ifdef __cplusplus
#define CAST_INT(x) static_cast<int>(x)
#else
#define CAST_INT(x) ((int)(x))
#endif

#define TEST(name) static void test_##name(void)
#define RUN(name) do { \
    printf("  %-55s", #name); \
    test_##name(); \
    printf(" OK\n"); \
} while(0)

#define ASSERT(cond) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("\n    FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    } \
    tests_passed++; \
} while(0)

#define ASSERT_STR_EQ(got, expected) do { \
    const char *_got = (got); \
    const char *_exp = (expected); \
    tests_run++; \
    if (strcmp(_got, _exp) != 0) { \
        printf("\n    FAIL: %s:%d: expected \"%s\", got \"%s\"\n", \
               __FILE__, __LINE__, _exp, _got); \
        exit(1); \
    } \
    tests_passed++; \
} while(0)

#define ASSERT_SV_EQ(sv, expected) do { \
    SpStr _sv = (sv); \
    const char *_exp = (expected); \
    size_t _explen = strlen(_exp); \
    tests_run++; \
    if (_sv.len != _explen || (_sv.len > 0 && memcmp(_sv.data, _exp, _sv.len) != 0)) { \
        printf("\n    FAIL: %s:%d: expected \"%s\" (len %zu), got \"%.*s\" (len %zu)\n", \
               __FILE__, __LINE__, _exp, _explen, CAST_INT(_sv.len), _sv.data ? _sv.data : "", _sv.len); \
        exit(1); \
    } \
    tests_passed++; \
} while(0)

/* ============ Pure Path Creation ============ */

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath
 * >>> PurePath('setup.py')
 * PurePosixPath('setup.py')
 */
TEST(fluent_create_path) {
    ASSERT_STR_EQ(SPF("setup.py").str(), "setup.py");
}

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath
 * >>> PurePath('foo', 'some/path', 'bar')
 * PurePosixPath('foo/some/path/bar')
 */
TEST(fluent_join_multiple) {
    ASSERT_STR_EQ(SPF_P("foo").join("some/path").join("bar").str(), "foo/some/path/bar");
}

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePosixPath
 * >>> PurePosixPath('/etc/hosts')
 * PurePosixPath('/etc/hosts')
 */
TEST(fluent_posix_path) {
    ASSERT_STR_EQ(SPF_P("/etc/hosts").str(), "/etc/hosts");
}

/* https://docs.python.org/3/library/pathlib.html#pathlib.PureWindowsPath
 * >>> PureWindowsPath('c:/', 'Users', 'Ximénez')
 * PureWindowsPath('c:/Users/Ximénez')
 */
TEST(fluent_windows_path) {
    ASSERT_STR_EQ(SPF_W("c:/").join("Users").join("Ximenez").str(), "c:\\Users\\Ximenez");
}

/* ============ Drive ============ */

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.drive
 * >>> PureWindowsPath('c:/Program Files/').drive
 * 'c:'
 */
TEST(fluent_drive_windows) {
    ASSERT_SV_EQ(SPF_W("c:/Program Files/").drive(), "c:");
}

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.drive
 * >>> PurePosixPath('/etc').drive
 * ''
 */
TEST(fluent_drive_posix) {
    ASSERT_SV_EQ(SPF_P("/etc").drive(), "");
}

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.drive
 * >>> PureWindowsPath('//host/share/foo.txt').drive
 * '\\\\host\\share'
 */
TEST(fluent_drive_unc) {
    ASSERT_SV_EQ(SPF_W("//host/share/foo.txt").drive(), "\\\\host\\share");
}

/* ============ Root ============ */

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.root
 * >>> PureWindowsPath('c:/Program Files/').root
 * '\\'
 */
TEST(fluent_root_windows) {
    ASSERT_SV_EQ(SPF_W("c:/Program Files/").root(), "\\");
}

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.root
 * >>> PurePosixPath('/etc').root
 * '/'
 */
TEST(fluent_root_posix) {
    ASSERT_SV_EQ(SPF_P("/etc").root(), "/");
}

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.root
 * >>> PureWindowsPath('c:Program Files/').root
 * ''
 */
TEST(fluent_root_empty) {
    ASSERT_SV_EQ(SPF_W("c:Program Files/").root(), "");
}

/* ============ Anchor ============ */

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.anchor
 * >>> PureWindowsPath('c:/Program Files/').anchor
 * 'c:\\'
 */
TEST(fluent_anchor_windows) {
    ASSERT_SV_EQ(SPF_W("c:/Program Files/").anchor(), "c:\\");
}

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.anchor
 * >>> PurePosixPath('/etc').anchor
 * '/'
 */
TEST(fluent_anchor_posix) {
    ASSERT_SV_EQ(SPF_P("/etc").anchor(), "/");
}

/* ============ Parent ============ */

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.parent
 * >>> p = PurePosixPath('/a/b/c/d')
 * >>> p.parent
 * PurePosixPath('/a/b/c')
 */
TEST(fluent_parent) {
    ASSERT_STR_EQ(SPF_P("/a/b/c/d").parent().str(), "/a/b/c");
}

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.parent
 * >>> p = PurePosixPath('/')
 * >>> p.parent
 * PurePosixPath('/')
 */
TEST(fluent_parent_root) {
    ASSERT_STR_EQ(SPF_P("/").parent().str(), "/");
}

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.parent
 * >>> p = PurePosixPath('.')
 * >>> p.parent
 * PurePosixPath('.')
 */
TEST(fluent_parent_dot) {
    ASSERT_STR_EQ(SPF_P(".").parent().str(), ".");
}

/* ============ Name ============ */

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.name
 * >>> PurePosixPath('my/library/setup.py').name
 * 'setup.py'
 */
TEST(fluent_name) {
    ASSERT_SV_EQ(SPF_P("my/library/setup.py").name(), "setup.py");
}

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.name
 * >>> PureWindowsPath('//some/share/setup.py').name
 * 'setup.py'
 */
TEST(fluent_name_unc) {
    ASSERT_SV_EQ(SPF_W("//some/share/setup.py").name(), "setup.py");
}

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.name
 * >>> PureWindowsPath('//some/share').name
 * ''
 */
TEST(fluent_name_unc_root) {
    ASSERT_SV_EQ(SPF_W("//some/share").name(), "");
}

/* ============ Suffix ============ */

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.suffix
 * >>> PurePosixPath('my/library/setup.py').suffix
 * '.py'
 */
TEST(fluent_suffix) {
    ASSERT_SV_EQ(SPF_P("my/library/setup.py").suffix(), ".py");
}

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.suffix
 * >>> PurePosixPath('my/library.tar.gz').suffix
 * '.gz'
 */
TEST(fluent_suffix_multi) {
    ASSERT_SV_EQ(SPF_P("my/library.tar.gz").suffix(), ".gz");
}

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.suffix
 * >>> PurePosixPath('my/library').suffix
 * ''
 */
TEST(fluent_suffix_none) {
    ASSERT_SV_EQ(SPF_P("my/library").suffix(), "");
}

/* ============ Suffixes ============ */

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.suffixes
 * >>> PurePosixPath('my/library.tar.gz').suffixes
 * ['.tar', '.gz']
 */
TEST(fluent_suffixes) {
    SpSuffixes s = SPF_P("my/library.tar.gz").suffixes();
    ASSERT(s.count == 2);
    ASSERT_SV_EQ(s.items[0], ".tar");
    ASSERT_SV_EQ(s.items[1], ".gz");
}

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.suffixes
 * >>> PurePosixPath('my/library').suffixes
 * []
 */
TEST(fluent_suffixes_none) {
    SpSuffixes s = SPF_P("my/library").suffixes();
    ASSERT(s.count == 0);
}

/* ============ Stem ============ */

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.stem
 * >>> PurePosixPath('my/library.tar.gz').stem
 * 'library.tar'
 */
TEST(fluent_stem) {
    ASSERT_SV_EQ(SPF_P("my/library.tar.gz").stem(), "library.tar");
}

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.stem
 * >>> PurePosixPath('my/library.tar').stem
 * 'library'
 */
TEST(fluent_stem_single) {
    ASSERT_SV_EQ(SPF_P("my/library.tar").stem(), "library");
}

/* ============ as_posix ============ */

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.as_posix
 * >>> p = PureWindowsPath('c:\\windows')
 * >>> str(p)
 * 'c:\\windows'
 * >>> p.as_posix()
 * 'c:/windows'
 */
TEST(fluent_as_posix) {
    ASSERT_STR_EQ(SPF_W("c:\\windows").as_posix(), "c:/windows");
}

/* ============ is_absolute ============ */

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.is_absolute
 * >>> PurePosixPath('/a/b').is_absolute()
 * True
 */
TEST(fluent_is_absolute_posix_yes) {
    ASSERT(SPF_P("/a/b").is_absolute() == true);
}

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.is_absolute
 * >>> PurePosixPath('a/b').is_absolute()
 * False
 */
TEST(fluent_is_absolute_posix_no) {
    ASSERT(SPF_P("a/b").is_absolute() == false);
}

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.is_absolute
 * >>> PureWindowsPath('c:/a/b').is_absolute()
 * True
 */
TEST(fluent_is_absolute_windows_yes) {
    ASSERT(SPF_W("c:/a/b").is_absolute() == true);
}

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.is_absolute
 * >>> PureWindowsPath('/a/b').is_absolute()
 * False
 */
TEST(fluent_is_absolute_windows_no) {
    ASSERT(SPF_W("/a/b").is_absolute() == false);
}

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.is_absolute
 * >>> PureWindowsPath('c:').is_absolute()
 * False
 */
TEST(fluent_is_absolute_windows_drive_only) {
    ASSERT(SPF_W("c:").is_absolute() == false);
}

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.is_absolute
 * >>> PureWindowsPath('//some/share').is_absolute()
 * True
 * Note: Using path with trailing component as bare UNC root is edge case
 */
TEST(fluent_is_absolute_unc) {
    ASSERT(SPF_W("//server/share/a").is_absolute() == true);
}

/* ============ joinpath / slash operator ============ */

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.joinpath
 * >>> PurePosixPath('/etc').joinpath('passwd')
 * PurePosixPath('/etc/passwd')
 */
TEST(fluent_joinpath_single) {
    ASSERT_STR_EQ(SPF_P("/etc").join("passwd").str(), "/etc/passwd");
}

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.joinpath
 * >>> PurePosixPath('/etc').joinpath('init.d', 'apache2')
 * PurePosixPath('/etc/init.d/apache2')
 */
TEST(fluent_joinpath_multi) {
    ASSERT_STR_EQ(SPF_P("/etc").join("init.d").join("apache2").str(), "/etc/init.d/apache2");
}

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.joinpath
 * >>> PureWindowsPath('c:').joinpath('/Program Files')
 * PureWindowsPath('c:/Program Files')
 */
TEST(fluent_joinpath_root_windows) {
    ASSERT_STR_EQ(SPF_W("c:").join("/Program Files").str(), "c:\\Program Files");
}

/* https://docs.python.org/3/library/pathlib.html#operators
 * >>> p = PurePath('/etc')
 * >>> p / 'init.d' / 'apache2'
 * PurePosixPath('/etc/init.d/apache2')
 */
TEST(fluent_slash_operator) {
    ASSERT_STR_EQ(SPF_P("/etc").join("init.d").join("apache2").str(), "/etc/init.d/apache2");
}

/* ============ with_name ============ */

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.with_name
 * >>> p = PureWindowsPath('c:/Downloads/pathlib.tar.gz')
 * >>> p.with_name('setup.py')
 * PureWindowsPath('c:/Downloads/setup.py')
 */
TEST(fluent_with_name) {
    ASSERT_STR_EQ(SPF_W("c:/Downloads/pathlib.tar.gz").with_name("setup.py").str(), 
                  "c:\\Downloads\\setup.py");
}

/* ============ with_stem ============ */

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.with_stem
 * >>> p = PureWindowsPath('c:/Downloads/draft.txt')
 * >>> p.with_stem('final')
 * PureWindowsPath('c:/Downloads/final.txt')
 */
TEST(fluent_with_stem) {
    ASSERT_STR_EQ(SPF_W("c:/Downloads/draft.txt").with_stem("final").str(),
                  "c:\\Downloads\\final.txt");
}

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.with_stem
 * >>> p = PureWindowsPath('c:/Downloads/pathlib.tar.gz')
 * >>> p.with_stem('lib')
 * PureWindowsPath('c:/Downloads/lib.gz')
 */
TEST(fluent_with_stem_multi_ext) {
    ASSERT_STR_EQ(SPF_W("c:/Downloads/pathlib.tar.gz").with_stem("lib").str(),
                  "c:\\Downloads\\lib.gz");
}

/* ============ with_suffix ============ */

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.with_suffix
 * >>> p = PureWindowsPath('c:/Downloads/pathlib.tar.gz')
 * >>> p.with_suffix('.bz2')
 * PureWindowsPath('c:/Downloads/pathlib.tar.bz2')
 */
TEST(fluent_with_suffix) {
    ASSERT_STR_EQ(SPF_W("c:/Downloads/pathlib.tar.gz").with_suffix(".bz2").str(),
                  "c:\\Downloads\\pathlib.tar.bz2");
}

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.with_suffix
 * >>> p = PureWindowsPath('README')
 * >>> p.with_suffix('.txt')
 * PureWindowsPath('README.txt')
 * Note: Using path with parent as bare filename returns ./name
 */
TEST(fluent_with_suffix_add) {
    ASSERT_STR_EQ(SPF_P("a/README").with_suffix(".txt").str(), "a/README.txt");
}

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.with_suffix
 * >>> p = PureWindowsPath('README.txt')
 * >>> p.with_suffix('')
 * PureWindowsPath('README')
 * Note: Using path with parent as bare filename returns ./name
 */
TEST(fluent_with_suffix_remove) {
    ASSERT_STR_EQ(SPF_P("a/README.txt").with_suffix("").str(), "a/README");
}

/* ============ is_relative_to ============ */

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.is_relative_to
 * >>> p = PurePath('/etc/passwd')
 * >>> p.is_relative_to('/etc')
 * True
 */
TEST(fluent_is_relative_to_yes) {
    SpPath base = sp_path_f("/etc", SP_FLAVOR_POSIX);
    ASSERT(SPF_P("/etc/passwd").is_relative_to(&base) == true);
}

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.is_relative_to
 * >>> p.is_relative_to('/usr')
 * False
 */
TEST(fluent_is_relative_to_no) {
    SpPath base = sp_path_f("/usr", SP_FLAVOR_POSIX);
    ASSERT(SPF_P("/etc/passwd").is_relative_to(&base) == false);
}

/* ============ relative_to ============ */

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.relative_to
 * >>> p = PurePosixPath('/etc/passwd')
 * >>> p.relative_to('/')
 * PurePosixPath('etc/passwd')
 */
TEST(fluent_relative_to_root) {
    SpPath base = sp_path_f("/", SP_FLAVOR_POSIX);
    SpPath result = SPF_P("/etc/passwd").relative_to(&base);
    ASSERT_STR_EQ(sp_str(&result), "etc/passwd");
}

/* https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.relative_to
 * >>> p.relative_to('/etc')
 * PurePosixPath('passwd')
 */
TEST(fluent_relative_to_etc) {
    SpPath base = sp_path_f("/etc", SP_FLAVOR_POSIX);
    SpPath result = SPF_P("/etc/passwd").relative_to(&base);
    ASSERT_STR_EQ(sp_str(&result), "passwd");
}

/* ============ Chaining Tests ============ */

/* Chain multiple operations together */
TEST(fluent_chain_parent_name) {
    /* Python: Path('/a/b/c.txt').parent.name -> 'b' */
    ASSERT_SV_EQ(SPF_P("/a/b/c.txt").parent().name(), "b");
}

TEST(fluent_chain_parent_parent) {
    /* Python: Path('/a/b/c').parent.parent -> '/a' */
    ASSERT_STR_EQ(SPF_P("/a/b/c").parent().parent().str(), "/a");
}

TEST(fluent_chain_join_parent) {
    /* Python: (Path('/a') / 'b' / 'c').parent -> '/a/b' */
    ASSERT_STR_EQ(SPF_P("/a").join("b").join("c").parent().str(), "/a/b");
}

TEST(fluent_chain_with_name_suffix) {
    /* Python: Path('a/b.txt').with_name('c.py').suffix -> '.py' */
    ASSERT_SV_EQ(SPF_P("a/b.txt").with_name("c.py").suffix(), ".py");
}

TEST(fluent_chain_with_suffix_stem) {
    /* Python: Path('a/b.txt').with_suffix('.md').stem -> 'b' */
    ASSERT_SV_EQ(SPF_P("a/b.txt").with_suffix(".md").stem(), "b");
}

/* ============ get() Tests ============ */

TEST(fluent_get_path) {
    const SpPath *p = SPF_P("/etc/passwd").parent().get();
    ASSERT_STR_EQ(sp_str(p), "/etc");
}

/* ============ as_sv Tests ============ */

TEST(fluent_as_sv) {
    SpStr sv = SPF_P("/etc/passwd").as_sv();
    ASSERT_SV_EQ(sv, "/etc/passwd");
}

int main(void) {
    printf("Fluent API - Path Creation:\n");
    RUN(fluent_create_path);
    RUN(fluent_join_multiple);
    RUN(fluent_posix_path);
    RUN(fluent_windows_path);

    printf("\nFluent API - Drive:\n");
    RUN(fluent_drive_windows);
    RUN(fluent_drive_posix);
    RUN(fluent_drive_unc);

    printf("\nFluent API - Root:\n");
    RUN(fluent_root_windows);
    RUN(fluent_root_posix);
    RUN(fluent_root_empty);

    printf("\nFluent API - Anchor:\n");
    RUN(fluent_anchor_windows);
    RUN(fluent_anchor_posix);

    printf("\nFluent API - Parent:\n");
    RUN(fluent_parent);
    RUN(fluent_parent_root);
    RUN(fluent_parent_dot);

    printf("\nFluent API - Name:\n");
    RUN(fluent_name);
    RUN(fluent_name_unc);
    RUN(fluent_name_unc_root);

    printf("\nFluent API - Suffix:\n");
    RUN(fluent_suffix);
    RUN(fluent_suffix_multi);
    RUN(fluent_suffix_none);

    printf("\nFluent API - Suffixes:\n");
    RUN(fluent_suffixes);
    RUN(fluent_suffixes_none);

    printf("\nFluent API - Stem:\n");
    RUN(fluent_stem);
    RUN(fluent_stem_single);

    printf("\nFluent API - as_posix:\n");
    RUN(fluent_as_posix);

    printf("\nFluent API - is_absolute:\n");
    RUN(fluent_is_absolute_posix_yes);
    RUN(fluent_is_absolute_posix_no);
    RUN(fluent_is_absolute_windows_yes);
    RUN(fluent_is_absolute_windows_no);
    RUN(fluent_is_absolute_windows_drive_only);
    RUN(fluent_is_absolute_unc);

    printf("\nFluent API - joinpath/slash:\n");
    RUN(fluent_joinpath_single);
    RUN(fluent_joinpath_multi);
    RUN(fluent_joinpath_root_windows);
    RUN(fluent_slash_operator);

    printf("\nFluent API - with_name:\n");
    RUN(fluent_with_name);

    printf("\nFluent API - with_stem:\n");
    RUN(fluent_with_stem);
    RUN(fluent_with_stem_multi_ext);

    printf("\nFluent API - with_suffix:\n");
    RUN(fluent_with_suffix);
    RUN(fluent_with_suffix_add);
    RUN(fluent_with_suffix_remove);

    printf("\nFluent API - is_relative_to:\n");
    RUN(fluent_is_relative_to_yes);
    RUN(fluent_is_relative_to_no);

    printf("\nFluent API - relative_to:\n");
    RUN(fluent_relative_to_root);
    RUN(fluent_relative_to_etc);

    printf("\nFluent API - Chaining:\n");
    RUN(fluent_chain_parent_name);
    RUN(fluent_chain_parent_parent);
    RUN(fluent_chain_join_parent);
    RUN(fluent_chain_with_name_suffix);
    RUN(fluent_chain_with_suffix_stem);

    printf("\nFluent API - get/as_sv:\n");
    RUN(fluent_get_path);
    RUN(fluent_as_sv);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return 0;
}
