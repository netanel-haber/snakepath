/* test.c - Rigorous pathlib tests for snakepath.h */

/* nob.h needs POSIX extensions - must be defined before any headers */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#define SNAKEPATH_IMPLEMENTATION
#include "snakepath.h"

/* Suppress warnings for nob.h which doesn't compile cleanly with strict flags */
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#define NOB_IMPLEMENTATION
#include "nob.h"

#ifdef __clang__
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static int tests_run = 0;

#define SP_TO_SV(sp) nob_sv_from_parts((sp).data, (sp).len)
#define SV(s) nob_sv_from_cstr(s)

#define ASSERT(cond) do { tests_run++; if (!(cond)) { nob_log(NOB_ERROR, "%s:%d: %s", __FILE__, __LINE__, #cond); exit(1); } } while(0)
#define ASSERT_SV(sv, exp) ASSERT(nob_sv_eq(SP_TO_SV(sv), SV(exp)))
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
    printf("POSIX Tests:\n");
    
    SVTest posix_anchor[] = {{"", ""}, {"a/b", ""}, {"/", "/"}, {"/a/b", "/"}};
    test_sv(P, sp_anchor, posix_anchor, NOB_ARRAY_LEN(posix_anchor));
    
    SVTest posix_drive[] = {{"/a/b", ""}};
    test_sv(P, sp_drive, posix_drive, NOB_ARRAY_LEN(posix_drive));
    
    SVTest posix_root[] = {{"a/b", ""}, {"/a/b", "/"}};
    test_sv(P, sp_root, posix_root, NOB_ARRAY_LEN(posix_root));
    
    SVTest posix_name[] = {{"", ""}, {"/", ""}, {"a/b", "b"}, {"a/b.py", "b.py"}};
    test_sv(P, sp_name, posix_name, NOB_ARRAY_LEN(posix_name));
    
    SVTest posix_stem[] = {{"a/b", "b"}, {"a/b.py", "b"}, {"a/.hgrc", ".hgrc"}, {"a/b.tar.gz", "b.tar"}};
    test_sv(P, sp_stem, posix_stem, NOB_ARRAY_LEN(posix_stem));
    
    SVTest posix_suffix[] = {{"a/b.py", ".py"}, {"a/.hgrc", ""}, {"a/.hg.rc", ".rc"}, {"a/b.tar.gz", ".gz"}};
    test_sv(P, sp_suffix, posix_suffix, NOB_ARRAY_LEN(posix_suffix));
    
    SVTest posix_parent[] = {{"a/b/c", "a/b"}, {"/a/b/c", "/a/b"}, {"/", "/"}, {"a", "."}};
    test_path(P, sp_parent, posix_parent, NOB_ARRAY_LEN(posix_parent));
    
    JoinTest posix_join[] = {{"a/b", "c", "a/b/c"}, {"a/b", "/c", "/c"}};
    test_join(P, posix_join, NOB_ARRAY_LEN(posix_join));
    
    JoinTest posix_with_name[] = {{"a/b", "d.xml", "a/d.xml"}};
    test_with(P, sp_with_name, posix_with_name, NOB_ARRAY_LEN(posix_with_name));
    
    JoinTest posix_with_stem[] = {{"a/b.py", "d", "a/d.py"}};
    test_with(P, sp_with_stem, posix_with_stem, NOB_ARRAY_LEN(posix_with_stem));
    
    JoinTest posix_with_suffix[] = {{"a/b.py", ".gz", "a/b.gz"}, {"a/b", ".gz", "a/b.gz"}, {"a/b.py", "", "a/b"}};
    test_with(P, sp_with_suffix, posix_with_suffix, NOB_ARRAY_LEN(posix_with_suffix));
    
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
    SpPath pr4 = sp_path_f("a/b", P), po4 = sp_path_f("a/b", P); ASSERT_PATH(sp_relative_to(&pr4, &po4), ".");
    
    ASSERT_PATH(sp_path_f("a//b", P), "a/b");
    ASSERT_PATH(sp_path_f("a/b/", P), "a/b");
    
    SpPath ea = sp_path_f("a/b", P), eb = sp_path_f("a/b", P); ASSERT(sp_eq(ea, eb));
    
    SpPath pap = sp_path_f("a/b/c", P); char *bap = nob_temp_alloc(SP_PATH_MAX);
    sp_as_posix(&pap, bap, SP_PATH_MAX); ASSERT(strcmp(bap, "a/b/c") == 0);
    
    printf("  POSIX tests OK\n");
    
    printf("\nWindows Tests:\n");
    
    SVTest win_drive[] = {{"C:/a/b", "C:"}, {"/a/b", ""}};
    test_sv(W, sp_drive, win_drive, NOB_ARRAY_LEN(win_drive));
    
    SVTest win_root[] = {{"C:/a/b", "\\"}, {"C:a/b", ""}};
    test_sv(W, sp_root, win_root, NOB_ARRAY_LEN(win_root));
    
    SVTest win_anchor[] = {{"C:/a/b", "C:\\"}, {"C:a/b", "C:"}};
    test_sv(W, sp_anchor, win_anchor, NOB_ARRAY_LEN(win_anchor));
    
    SVTest win_name[] = {{"C:/a/b.py", "b.py"}};
    test_sv(W, sp_name, win_name, NOB_ARRAY_LEN(win_name));
    
    SVTest win_suffix[] = {{"C:/a/b.py", ".py"}};
    test_sv(W, sp_suffix, win_suffix, NOB_ARRAY_LEN(win_suffix));
    
    SVTest win_parent[] = {{"C:/a/b/c", "C:\\a\\b"}};
    test_path(W, sp_parent, win_parent, NOB_ARRAY_LEN(win_parent));
    
    JoinTest win_join[] = {
        {"C:/a/b", "x/y", "C:\\a\\b\\x\\y"}, {"C:/a/b", "/x/y", "C:\\x\\y"},
        {"C:/a/b", "D:/x/y", "D:\\x\\y"}, {"C:/a/b", "c:x/y", "C:\\a\\b\\x\\y"}
    };
    test_join(W, win_join, NOB_ARRAY_LEN(win_join));
    
    JoinTest win_with_name[] = {{"C:/a/b", "d.xml", "C:\\a\\d.xml"}};
    test_with(W, sp_with_name, win_with_name, NOB_ARRAY_LEN(win_with_name));
    
    JoinTest win_with_suffix[] = {{"C:/a/b.py", ".gz", "C:\\a\\b.gz"}};
    test_with(W, sp_with_suffix, win_with_suffix, NOB_ARRAY_LEN(win_with_suffix));
    
    SpPath wp1 = sp_path_f("c:a/b", W); SpPartsIter wit1 = sp_parts_begin(&wp1);
    ASSERT(sp_parts_next(&wit1, &part)); ASSERT_SV(part, "c:");
    ASSERT(sp_parts_next(&wit1, &part)); ASSERT_SV(part, "a");
    ASSERT(sp_parts_next(&wit1, &part)); ASSERT_SV(part, "b");
    
    SpPath wp2 = sp_path_f("c:/a/b", W); SpPartsIter wit2 = sp_parts_begin(&wp2);
    ASSERT(sp_parts_next(&wit2, &part)); ASSERT_SV(part, "c:\\");
    ASSERT(sp_parts_next(&wit2, &part)); ASSERT_SV(part, "a");
    ASSERT(sp_parts_next(&wit2, &part)); ASSERT_SV(part, "b");
    
    SpPath wp3 = sp_path_f("//server/share/a/b", W); SpPartsIter wit3 = sp_parts_begin(&wp3);
    ASSERT(sp_parts_next(&wit3, &part)); ASSERT_SV(part, "\\\\server\\share\\");
    ASSERT(sp_parts_next(&wit3, &part)); ASSERT_SV(part, "a");
    ASSERT(sp_parts_next(&wit3, &part)); ASSERT_SV(part, "b");
    
    SpPath ws = sp_path_f("c:a/b.tar.gz", W); SpSuffixes wss = sp_suffixes(&ws);
    ASSERT(wss.count == 2); ASSERT_SV(wss.items[0], ".tar"); ASSERT_SV(wss.items[1], ".gz");
    
    ASSERT_ABS("C:/a/b", W, true); ASSERT_ABS("C:a/b", W, false);
    ASSERT_ABS("/a/b", W, false); ASSERT_ABS("//server/share/a", W, true);
    
    ASSERT_PATH(sp_path_f("C:/a\\b/c", W), "C:\\a\\b\\c");
    
    SpPath wap = sp_path_f("C:\\a\\b\\c", W); char *wbuf = nob_temp_alloc(SP_PATH_MAX);
    sp_as_posix(&wap, wbuf, SP_PATH_MAX); ASSERT(strcmp(wbuf, "C:/a/b/c") == 0);
    
    SpPath wr1 = sp_path_f("C:/a/b/c", W), wo1 = sp_path_f("C:/a", W);
    ASSERT_PATH(sp_relative_to(&wr1, &wo1), "b\\c");
    
    SpPath wr2 = sp_path_f("C:/a/b", W), wo2 = sp_path_f("D:/a", W);
    ASSERT(!sp_is_relative_to(&wr2, &wo2));
    
    printf("  Windows tests OK\n");
    
    printf("\nEdge Cases:\n");
    
    SpPath e1 = sp_path_f("", P); ASSERT(sp_is_empty(&e1)); ASSERT_SV(sp_name(&e1), ""); ASSERT_SV(sp_suffix(&e1), "");
    SpPath e2 = sp_path_f(".", P); ASSERT_PATH(e2, "."); ASSERT_SV(sp_name(&e2), ".");
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
    
    printf("\n%d assertions passed\n", tests_run);
    return 0;
}
