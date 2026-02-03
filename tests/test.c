/* test.c - Rigorous pathlib tests for snakepath.h */
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS  /* Disable fopen deprecation warning on MSVC */
#endif
#define SP_PATH_MAX 1024  /* Use SP_PATH_MAX_WINDOWS for CI compatibility */
#define SNAKEPATH_IMPLEMENTATION
#include "snakepath.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Platform-specific rmdir and getpid */
#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define sp_rmdir _rmdir
#define sp_getpid _getpid
#else
#include <unistd.h>
#define sp_rmdir rmdir
#define sp_getpid getpid
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

#define ARRAY_LEN(a) (sizeof(a)/sizeof((a)[0]))

#define ASSERT(cond) do { tests_run++; if (!(cond)) { fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); exit(1); } } while(0)
#define ASSERT_SV(sv, exp) ASSERT(sv_eq(sv, exp))
#define ASSERT_PATH(p, exp) do { SpPath _p = (p); ASSERT(strcmp(sp_str(&_p), exp) == 0); } while(0)
#define ASSERT_ABS(path, flav, expected) do { SpPath _p = sp_path_f(path, flav); ASSERT(sp_is_absolute(&_p) == expected); } while(0)

/* Table-driven tests */
typedef struct { const char *path; const char *expected; } SVTest;
typedef struct { const char *path; const char *arg; const char *expected; } JoinTest;

#define P SP_FLAVOR_POSIX
#define W SP_FLAVOR_WINDOWS

static void test_sv(SpFlavor f, SpStr (*fn)(const SpPath*), SVTest *tests, size_t n) {
    for (size_t i = 0; i < n; i++) {
        SpPath p = sp_path_f(tests[i].path, f);
        ASSERT_SV(fn(&p), tests[i].expected);
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

int main(void) {
    /* Initialize unique test directory names based on PID to avoid race conditions */
#ifdef __cplusplus
    long pid = static_cast<long>(sp_getpid());
#else
    long pid = (long)sp_getpid();
#endif
    snprintf(test_dir_mkdir_temp, sizeof(test_dir_mkdir_temp), "./test_mkdir_temp_%ld", pid);
    snprintf(test_dir_mkdir_nested, sizeof(test_dir_mkdir_nested), "./test_mkdir_nested_%ld", pid);
    snprintf(test_dir_glob, sizeof(test_dir_glob), "./test_glob_dir_%ld", pid);

    printf("POSIX Tests:\n");
    
    SVTest posix_anchor[] = {{"", ""}, {"a/b", ""}, {"/", "/"}, {"/a/b", "/"}};
    test_sv(P, sp_anchor, posix_anchor, ARRAY_LEN(posix_anchor));
    
    SVTest posix_drive[] = {{"/a/b", ""}};
    test_sv(P, sp_drive, posix_drive, ARRAY_LEN(posix_drive));
    
    SVTest posix_root[] = {{"a/b", ""}, {"/a/b", "/"}};
    test_sv(P, sp_root, posix_root, ARRAY_LEN(posix_root));
    
    SVTest posix_name[] = {{"", ""}, {".", ""}, {"..", ".."}, {"/", ""}, {"a/b", "b"}, {"a/b.py", "b.py"}};
    test_sv(P, sp_name, posix_name, ARRAY_LEN(posix_name));
    
    SVTest posix_stem[] = {{"", ""}, {"a/b", "b"}, {"a/b.py", "b"}, {"a/.hgrc", ".hgrc"}, {"a/b.tar.gz", "b.tar"}};
    test_sv(P, sp_stem, posix_stem, ARRAY_LEN(posix_stem));
    
    SVTest posix_suffix[] = {{"a/b.py", ".py"}, {"a/.hgrc", ""}, {"a/.hg.rc", ".rc"}, {"a/b.tar.gz", ".gz"}};
    test_sv(P, sp_suffix, posix_suffix, ARRAY_LEN(posix_suffix));
    
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
    SpPath pr3 = sp_path_f("a/b/c", P), po3 = sp_path_f("a", P); ASSERT_PATH(sp_relative_to(&pr3, &po3), "b/c");
    SpPath pr4 = sp_path_f("a/b", P), po4 = sp_path_f("a/b", P); ASSERT_PATH(sp_relative_to(&pr4, &po4), ".");  /* C returns "." for display, but Python returns "" */
    
    ASSERT_PATH(sp_path_f("a//b", P), "a/b");
    ASSERT_PATH(sp_path_f("a/b/", P), "a/b");
    
    SpPath ea = sp_path_f("a/b", P), eb = sp_path_f("a/b", P); ASSERT(sp_eq(ea, eb));
    
    SpPath pap = sp_path_f("a/b/c", P); char bap[SP_PATH_MAX];
    sp_as_posix(&pap, bap, sizeof(bap)); ASSERT(strcmp(bap, "a/b/c") == 0);
    
    printf("  POSIX tests OK\n");
    
    printf("\nWindows Tests:\n");
    
    SVTest win_drive[] = {{"C:/a/b", "C:"}, {"/a/b", ""}};
    test_sv(W, sp_drive, win_drive, ARRAY_LEN(win_drive));
    
    SVTest win_root[] = {{"C:/a/b", "\\"}, {"C:a/b", ""}};
    test_sv(W, sp_root, win_root, ARRAY_LEN(win_root));
    
    SVTest win_anchor[] = {{"C:/a/b", "C:\\"}, {"C:a/b", "C:"}};
    test_sv(W, sp_anchor, win_anchor, ARRAY_LEN(win_anchor));
    
    SVTest win_name[] = {{"C:/a/b.py", "b.py"}};
    test_sv(W, sp_name, win_name, ARRAY_LEN(win_name));
    
    SVTest win_suffix[] = {{"C:/a/b.py", ".py"}};
    test_sv(W, sp_suffix, win_suffix, ARRAY_LEN(win_suffix));
    
    SVTest win_parent[] = {{"C:/a/b/c", "C:\\a\\b"}};
    test_path(W, sp_parent, win_parent, ARRAY_LEN(win_parent));
    
    JoinTest win_join[] = {
        {"C:/a/b", "x/y", "C:\\a\\b\\x\\y"}, {"C:/a/b", "/x/y", "C:\\x\\y"},
        {"C:/a/b", "D:/x/y", "D:\\x\\y"}, {"C:/a/b", "c:x/y", "c:\\a\\b\\x\\y"}
    };
    test_join(W, win_join, ARRAY_LEN(win_join));
    
    JoinTest win_with_name[] = {{"C:/a/b", "d.xml", "C:\\a\\d.xml"}};
    test_with(W, sp_with_name, win_with_name, ARRAY_LEN(win_with_name));
    
    JoinTest win_with_suffix[] = {{"C:/a/b.py", ".gz", "C:\\a\\b.gz"}};
    test_with(W, sp_with_suffix, win_with_suffix, ARRAY_LEN(win_with_suffix));
    
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
    
    SpPath wap = sp_path_f("C:\\a\\b\\c", W); char wbuf[SP_PATH_MAX];
    sp_as_posix(&wap, wbuf, sizeof(wbuf)); ASSERT(strcmp(wbuf, "C:/a/b/c") == 0);
    
    SpPath wr1 = sp_path_f("C:/a/b/c", W), wo1 = sp_path_f("C:/a", W);
    ASSERT_PATH(sp_relative_to(&wr1, &wo1), "b\\c");
    
    SpPath wr2 = sp_path_f("C:/a/b", W), wo2 = sp_path_f("D:/a", W);
    ASSERT(!sp_is_relative_to(&wr2, &wo2));
    
    printf("  Windows tests OK\n");
    
    printf("\nEdge Cases:\n");
    
    SpPath e1 = sp_path_f("", P); ASSERT_PATH(e1, "."); ASSERT_SV(sp_name(&e1), ""); ASSERT_SV(sp_suffix(&e1), "");
    SpPath e2 = sp_path_f(".", P); ASSERT_PATH(e2, "."); ASSERT_SV(sp_name(&e2), "");
    SpPath e3 = sp_path_f("..", P); ASSERT_PATH(e3, ".."); ASSERT_SV(sp_name(&e3), "..");
    SpPath e4 = sp_path_f("...", P); ASSERT_SV(sp_suffix(&e4), "");
    
    SpPath e5 = sp_path_f("a/b.c.d.e", P); SpSuffixes es5 = sp_suffixes(&e5);
    ASSERT(es5.count == 3); ASSERT_SV(es5.items[0], ".c"); ASSERT_SV(es5.items[1], ".d"); ASSERT_SV(es5.items[2], ".e");
    
    SpPath e6 = sp_path_f(".tar.gz", P); ASSERT_SV(sp_stem(&e6), ".tar"); ASSERT_SV(sp_suffix(&e6), ".gz");
    
    SpPath e7 = sp_path_f("file.txt", P);
    ASSERT_SV(sp_name(&e7), "file.txt"); ASSERT_SV(sp_stem(&e7), "file");
    ASSERT_SV(sp_suffix(&e7), ".txt"); ASSERT_PATH(sp_parent(&e7), ".");
    
    SpPath e8 = sp_path_f("/a/b/c/d/e/f/g", P);
    ASSERT(sp_parts_count(&e8) == 8); ASSERT_PATH(sp_parent(&e8), "/a/b/c/d/e/f");
    
    SpPath ej1 = sp_path_f("a/b", P); ASSERT_PATH(sp_join_one(&ej1, ""), "a/b");
    SpPath ej2 = sp_path_f("", P); ASSERT_PATH(sp_join_one(&ej2, "a"), "a");
    
    printf("  Edge cases OK\n");

    printf("\nis_file Tests:\n");

    /* Test is_file - existing file */
    SpPath existing_file = sp_path_f(__FILE__, SP_FLAVOR_NATIVE);
    ASSERT(sp_is_file(&existing_file) == true);

    /* Test is_file - nonexistent path */
    SpPath nonexistent = sp_path_f("/nonexistent/path/file.txt", P);
    ASSERT(sp_is_file(&nonexistent) == false);

    /* Test is_file - directory (not a file) */
    SpPath dir = sp_path_f(".", P);
    ASSERT(sp_is_file(&dir) == false);

    printf("  is_file tests OK\n");

    printf("\nis_dir Tests:\n");

    /* Test is_dir - existing directory */
    SpPath existing_dir = sp_path_f(".", SP_FLAVOR_NATIVE);
    ASSERT(sp_is_dir(&existing_dir) == true);

    /* Test is_dir - nonexistent path */
    SpPath nonexistent_dir = sp_path_f("/nonexistent/path/dir", P);
    ASSERT(sp_is_dir(&nonexistent_dir) == false);

    /* Test is_dir - file (not a directory) */
    SpPath file_path = sp_path_f(__FILE__, SP_FLAVOR_NATIVE);
    ASSERT(sp_is_dir(&file_path) == false);

    printf("  is_dir tests OK\n");

    printf("\nexists Tests:\n");

    /* Test exists - existing file */
    SpPath exists_file = sp_path_f(__FILE__, SP_FLAVOR_NATIVE);
    ASSERT(sp_exists(&exists_file) == true);

    /* Test exists - existing directory */
    SpPath exists_dir = sp_path_f(".", SP_FLAVOR_NATIVE);
    ASSERT(sp_exists(&exists_dir) == true);

    /* Test exists - nonexistent path */
    SpPath not_exists = sp_path_f("/nonexistent/path/file.txt", P);
    ASSERT(sp_exists(&not_exists) == false);

    printf("  exists tests OK\n");

    printf("\nstat Tests:\n");

    /* Test stat - existing file */
    SpPath stat_file = sp_path_f(__FILE__, SP_FLAVOR_NATIVE);
    SpStatResult stat_result = sp_stat(&stat_file);
    ASSERT(stat_result.valid == true);
    ASSERT(stat_result.sp_size > 0);
    ASSERT((stat_result.sp_mode & 0170000) == 0100000);  /* S_IFREG - regular file */

    /* Test stat - existing directory */
    SpPath stat_dir = sp_path_f(".", SP_FLAVOR_NATIVE);
    SpStatResult dir_result = sp_stat(&stat_dir);
    ASSERT(dir_result.valid == true);
    ASSERT((dir_result.sp_mode & 0170000) == 0040000);  /* S_IFDIR - directory */

    /* Test stat - nonexistent path */
    SpPath stat_nonexistent = sp_path_f("/nonexistent/path/file.txt", P);
    SpStatResult nonexistent_result = sp_stat(&stat_nonexistent);
    ASSERT(nonexistent_result.valid == false);

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
    sp_rmdir(sp_str(&mkdir_test));

    /* Test basic mkdir */
    int mkdir_result = sp_mkdir(&mkdir_test, 0755, false, false);
    ASSERT(mkdir_result == SP_MKDIR_OK);
    ASSERT(sp_is_dir(&mkdir_test) == true);

    /* Test mkdir with exist_ok=false should fail when dir exists */
    mkdir_result = sp_mkdir(&mkdir_test, 0755, false, false);
    ASSERT(mkdir_result == SP_MKDIR_ERR_EXISTS);

    /* Test mkdir with exist_ok=true should succeed when dir exists */
    mkdir_result = sp_mkdir(&mkdir_test, 0755, false, true);
    ASSERT(mkdir_result == SP_MKDIR_OK);

    /* Cleanup */
    sp_rmdir(sp_str(&mkdir_test));

    /* Test mkdir with parents=true */
    char nested_path[128], nested_sub[128];
    snprintf(nested_path, sizeof(nested_path), "%s/subdir/deep", test_dir_mkdir_nested);
    snprintf(nested_sub, sizeof(nested_sub), "%s/subdir", test_dir_mkdir_nested);
    SpPath mkdir_nested = sp_path_f(nested_path, SP_FLAVOR_NATIVE);
    mkdir_result = sp_mkdir(&mkdir_nested, 0755, true, false);
    ASSERT(mkdir_result == SP_MKDIR_OK);
    ASSERT(sp_is_dir(&mkdir_nested) == true);

    /* Cleanup nested dirs */
    sp_rmdir(nested_path);
    sp_rmdir(nested_sub);
    sp_rmdir(test_dir_mkdir_nested);

    /* Test mkdir without parents should fail if parent doesn't exist */
    SpPath mkdir_no_parent = sp_path_f("./nonexistent_parent/subdir", SP_FLAVOR_NATIVE);
    mkdir_result = sp_mkdir(&mkdir_no_parent, 0755, false, false);
    ASSERT(mkdir_result == SP_MKDIR_ERR_NOT_FOUND);

    printf("  mkdir tests OK\n");

    /* lstat tests */
    printf("\nlstat Tests:\n");

    /* Test lstat on regular file */
    SpPath lstat_file = sp_path_f(__FILE__, SP_FLAVOR_NATIVE);
    SpStatResult lstat_res = sp_lstat(&lstat_file);
    ASSERT(lstat_res.valid == true);
    ASSERT(lstat_res.sp_size > 0);

    /* Test lstat on nonexistent file */
    SpPath lstat_nonexist = sp_path_f("/nonexistent/path/file.txt", P);
    SpStatResult lstat_nonexist_res = sp_lstat(&lstat_nonexist);
    ASSERT(lstat_nonexist_res.valid == false);

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
    ASSERT(!sp_path_is_error(&resolved_file));

    /* Test resolve strict mode on nonexistent path */
    SpPath resolve_nonexist = sp_path_f("/nonexistent/path/file.txt", P);
    SpPath resolved_nonexist = sp_resolve(&resolve_nonexist, true);
    ASSERT(sp_path_is_error(&resolved_nonexist));

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
    sp_mkdir(&glob_base, 0755, true, true);

    SpPath glob_sub = sp_path_f(glob_sub_path, SP_FLAVOR_NATIVE);
    sp_mkdir(&glob_sub, 0755, true, true);

    /* Create test files by touching them (just need the glob to find them) */
    FILE *f1 = fopen(glob_f1, "w");
    if (f1) fclose(f1);
    FILE *f2 = fopen(glob_f2, "w");
    if (f2) fclose(f2);
    FILE *f3 = fopen(glob_f3, "w");
    if (f3) fclose(f3);

    /* Test basic glob *.txt using iterator */
    int txt_count = 0;
    SpGlobIter git = sp_glob_begin(&glob_base, "*.txt", SP_CASE_PLATFORM_DEFAULT);
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
    sp_rmdir(glob_sub_path);
    sp_rmdir(test_dir_glob);

    printf("  glob tests OK\n");

    printf("\n%d assertions passed\n", tests_run);
    return 0;
}
