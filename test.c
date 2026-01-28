/* test.c - Rigorous pathlib tests for snakepath.h
 * Ported from Python's test_pathlib tests (POSIX and Windows)
 */

#define SNAKEPATH_IMPLEMENTATION
#include "snakepath.h"
#include <stdio.h>
#include <stdlib.h>

static int tests_run = 0;
static int tests_passed = 0;

/* C/C++ compatible cast helper */
#ifdef __cplusplus
#define CAST_INT(x) static_cast<int>(x)
#else
#define CAST_INT(x) ((int)(x))
#endif

#define TEST(name) static void test_##name(void)
#define RUN(name) do { \
    printf("  %-50s", #name); \
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

#define ASSERT_PATH_EQ(p, expected) do { \
    SpPath _p = (p); \
    const char *_got = sp_str(&_p); \
    const char *_exp = (expected); \
    tests_run++; \
    if (strcmp(_got, _exp) != 0) { \
        printf("\n    FAIL: %s:%d: expected \"%s\", got \"%s\"\n", \
               __FILE__, __LINE__, _exp, _got); \
        exit(1); \
    } \
    tests_passed++; \
} while(0)

/* ============ POSIX Tests ============ */

TEST(posix_anchor_empty) {
    SpPath p = sp_path_f("", SP_FLAVOR_POSIX);
    ASSERT_SV_EQ(sp_anchor(&p), "");
}

TEST(posix_anchor_relative) {
    SpPath p = sp_path_f("a/b", SP_FLAVOR_POSIX);
    ASSERT_SV_EQ(sp_anchor(&p), "");
}

TEST(posix_anchor_root) {
    SpPath p = sp_path_f("/", SP_FLAVOR_POSIX);
    ASSERT_SV_EQ(sp_anchor(&p), "/");
}

TEST(posix_anchor_absolute) {
    SpPath p = sp_path_f("/a/b", SP_FLAVOR_POSIX);
    ASSERT_SV_EQ(sp_anchor(&p), "/");
}

TEST(posix_drive) {
    SpPath p = sp_path_f("/a/b", SP_FLAVOR_POSIX);
    ASSERT_SV_EQ(sp_drive(&p), "");
}

TEST(posix_root_empty) {
    SpPath p = sp_path_f("a/b", SP_FLAVOR_POSIX);
    ASSERT_SV_EQ(sp_root(&p), "");
}

TEST(posix_root_slash) {
    SpPath p = sp_path_f("/a/b", SP_FLAVOR_POSIX);
    ASSERT_SV_EQ(sp_root(&p), "/");
}

TEST(posix_name_empty) {
    SpPath p = sp_path_f("", SP_FLAVOR_POSIX);
    ASSERT_SV_EQ(sp_name(&p), "");
}

TEST(posix_name_root) {
    SpPath p = sp_path_f("/", SP_FLAVOR_POSIX);
    ASSERT_SV_EQ(sp_name(&p), "");
}

TEST(posix_name_simple) {
    SpPath p = sp_path_f("a/b", SP_FLAVOR_POSIX);
    ASSERT_SV_EQ(sp_name(&p), "b");
}

TEST(posix_name_with_ext) {
    SpPath p = sp_path_f("a/b.py", SP_FLAVOR_POSIX);
    ASSERT_SV_EQ(sp_name(&p), "b.py");
}

TEST(posix_stem_simple) {
    SpPath p = sp_path_f("a/b", SP_FLAVOR_POSIX);
    ASSERT_SV_EQ(sp_stem(&p), "b");
}

TEST(posix_stem_with_ext) {
    SpPath p = sp_path_f("a/b.py", SP_FLAVOR_POSIX);
    ASSERT_SV_EQ(sp_stem(&p), "b");
}

TEST(posix_stem_hidden) {
    SpPath p = sp_path_f("a/.hgrc", SP_FLAVOR_POSIX);
    ASSERT_SV_EQ(sp_stem(&p), ".hgrc");
}

TEST(posix_stem_multi_ext) {
    SpPath p = sp_path_f("a/b.tar.gz", SP_FLAVOR_POSIX);
    ASSERT_SV_EQ(sp_stem(&p), "b.tar");
}

TEST(posix_suffix_py) {
    SpPath p = sp_path_f("a/b.py", SP_FLAVOR_POSIX);
    ASSERT_SV_EQ(sp_suffix(&p), ".py");
}

TEST(posix_suffix_hidden_none) {
    SpPath p = sp_path_f("a/.hgrc", SP_FLAVOR_POSIX);
    ASSERT_SV_EQ(sp_suffix(&p), "");
}

TEST(posix_suffix_hidden_with_ext) {
    SpPath p = sp_path_f("a/.hg.rc", SP_FLAVOR_POSIX);
    ASSERT_SV_EQ(sp_suffix(&p), ".rc");
}

TEST(posix_suffix_multi) {
    SpPath p = sp_path_f("a/b.tar.gz", SP_FLAVOR_POSIX);
    ASSERT_SV_EQ(sp_suffix(&p), ".gz");
}

TEST(posix_suffixes_single) {
    SpPath p = sp_path_f("a/b.py", SP_FLAVOR_POSIX);
    SpSuffixes s = sp_suffixes(&p);
    ASSERT(s.count == 1);
    ASSERT_SV_EQ(s.items[0], ".py");
}

TEST(posix_suffixes_multi) {
    SpPath p = sp_path_f("a/b.tar.gz", SP_FLAVOR_POSIX);
    SpSuffixes s = sp_suffixes(&p);
    ASSERT(s.count == 2);
    ASSERT_SV_EQ(s.items[0], ".tar");
    ASSERT_SV_EQ(s.items[1], ".gz");
}

TEST(posix_suffixes_hidden) {
    SpPath p = sp_path_f("a/.hgrc", SP_FLAVOR_POSIX);
    SpSuffixes s = sp_suffixes(&p);
    ASSERT(s.count == 0);
}

TEST(posix_parts_relative) {
    SpPath p = sp_path_f("a/b", SP_FLAVOR_POSIX);
    SpPartsIter it = sp_parts_begin(&p);
    SpStr part;
    ASSERT(sp_parts_next(&it, &part));
    ASSERT_SV_EQ(part, "a");
    ASSERT(sp_parts_next(&it, &part));
    ASSERT_SV_EQ(part, "b");
    ASSERT(!sp_parts_next(&it, &part));
}

TEST(posix_parts_absolute) {
    SpPath p = sp_path_f("/a/b", SP_FLAVOR_POSIX);
    SpPartsIter it = sp_parts_begin(&p);
    SpStr part;
    ASSERT(sp_parts_next(&it, &part));
    ASSERT_SV_EQ(part, "/");
    ASSERT(sp_parts_next(&it, &part));
    ASSERT_SV_EQ(part, "a");
    ASSERT(sp_parts_next(&it, &part));
    ASSERT_SV_EQ(part, "b");
    ASSERT(!sp_parts_next(&it, &part));
}

TEST(posix_parts_count) {
    SpPath p = sp_path_f("/a/b/c", SP_FLAVOR_POSIX);
    ASSERT(sp_parts_count(&p) == 4);
}

TEST(posix_parent_simple) {
    SpPath p = sp_path_f("a/b/c", SP_FLAVOR_POSIX);
    ASSERT_PATH_EQ(sp_parent(&p), "a/b");
}

TEST(posix_parent_absolute) {
    SpPath p = sp_path_f("/a/b/c", SP_FLAVOR_POSIX);
    ASSERT_PATH_EQ(sp_parent(&p), "/a/b");
}

TEST(posix_parent_root) {
    SpPath p = sp_path_f("/", SP_FLAVOR_POSIX);
    ASSERT_PATH_EQ(sp_parent(&p), "/");
}

TEST(posix_parent_single) {
    SpPath p = sp_path_f("a", SP_FLAVOR_POSIX);
    ASSERT_PATH_EQ(sp_parent(&p), ".");
}

TEST(posix_parents_iter) {
    SpPath p = sp_path_f("a/b/c", SP_FLAVOR_POSIX);
    SpParentsIter it = sp_parents_begin(&p);
    SpPath parent;
    ASSERT(sp_parents_next(&it, &parent));
    ASSERT_PATH_EQ(parent, "a/b");
    ASSERT(sp_parents_next(&it, &parent));
    ASSERT_PATH_EQ(parent, "a");
    ASSERT(sp_parents_next(&it, &parent));
    ASSERT_PATH_EQ(parent, ".");
    ASSERT(!sp_parents_next(&it, &parent));
}

TEST(posix_join_simple) {
    SpPath p = sp_path_f("a/b", SP_FLAVOR_POSIX);
    SpPath r = sp_join_one(&p, "c");
    ASSERT_PATH_EQ(r, "a/b/c");
}

/* sp_join variadic macro only available in C */
#ifndef __cplusplus
TEST(posix_join_multi) {
    SpPath p = sp_path_f("a", SP_FLAVOR_POSIX);
    SpPath r = sp_join(&p, "b", "c");
    ASSERT_PATH_EQ(r, "a/b/c");
}
#endif

TEST(posix_join_absolute_override) {
    SpPath p = sp_path_f("a/b", SP_FLAVOR_POSIX);
    SpPath r = sp_join_one(&p, "/c");
    ASSERT_PATH_EQ(r, "/c");
}

TEST(posix_with_name) {
    SpPath p = sp_path_f("a/b", SP_FLAVOR_POSIX);
    SpPath r = sp_with_name(&p, "d.xml");
    ASSERT_PATH_EQ(r, "a/d.xml");
}

TEST(posix_with_stem) {
    SpPath p = sp_path_f("a/b.py", SP_FLAVOR_POSIX);
    SpPath r = sp_with_stem(&p, "d");
    ASSERT_PATH_EQ(r, "a/d.py");
}

TEST(posix_with_suffix) {
    SpPath p = sp_path_f("a/b.py", SP_FLAVOR_POSIX);
    SpPath r = sp_with_suffix(&p, ".gz");
    ASSERT_PATH_EQ(r, "a/b.gz");
}

TEST(posix_with_suffix_add) {
    SpPath p = sp_path_f("a/b", SP_FLAVOR_POSIX);
    SpPath r = sp_with_suffix(&p, ".gz");
    ASSERT_PATH_EQ(r, "a/b.gz");
}

TEST(posix_with_suffix_remove) {
    SpPath p = sp_path_f("a/b.py", SP_FLAVOR_POSIX);
    SpPath r = sp_with_suffix(&p, "");
    ASSERT_PATH_EQ(r, "a/b");
}

TEST(posix_is_absolute_yes) {
    SpPath p = sp_path_f("/a/b", SP_FLAVOR_POSIX);
    ASSERT(sp_is_absolute(&p));
}

TEST(posix_is_absolute_no) {
    SpPath p = sp_path_f("a/b", SP_FLAVOR_POSIX);
    ASSERT(!sp_is_absolute(&p));
}

TEST(posix_is_relative_to_yes) {
    SpPath p = sp_path_f("a/b", SP_FLAVOR_POSIX);
    SpPath o = sp_path_f("a", SP_FLAVOR_POSIX);
    ASSERT(sp_is_relative_to(&p, &o));
}

TEST(posix_is_relative_to_no) {
    SpPath p = sp_path_f("a/b", SP_FLAVOR_POSIX);
    SpPath o = sp_path_f("c", SP_FLAVOR_POSIX);
    ASSERT(!sp_is_relative_to(&p, &o));
}

TEST(posix_relative_to) {
    SpPath p = sp_path_f("a/b/c", SP_FLAVOR_POSIX);
    SpPath o = sp_path_f("a", SP_FLAVOR_POSIX);
    SpPath r = sp_relative_to(&p, &o);
    ASSERT_PATH_EQ(r, "b/c");
}

TEST(posix_relative_to_self) {
    SpPath p = sp_path_f("a/b", SP_FLAVOR_POSIX);
    SpPath o = sp_path_f("a/b", SP_FLAVOR_POSIX);
    SpPath r = sp_relative_to(&p, &o);
    ASSERT_PATH_EQ(r, ".");
}

TEST(posix_normalize_double_slash) {
    SpPath p = sp_path_f("a//b", SP_FLAVOR_POSIX);
    ASSERT_PATH_EQ(p, "a/b");
}

TEST(posix_normalize_trailing_slash) {
    SpPath p = sp_path_f("a/b/", SP_FLAVOR_POSIX);
    ASSERT_PATH_EQ(p, "a/b");
}

TEST(posix_eq) {
    SpPath a = sp_path_f("a/b", SP_FLAVOR_POSIX);
    SpPath b = sp_path_f("a/b", SP_FLAVOR_POSIX);
    ASSERT(sp_eq(a, b));
}

TEST(posix_as_posix) {
    SpPath p = sp_path_f("a/b/c", SP_FLAVOR_POSIX);
    char buf[SP_PATH_MAX];
    sp_as_posix(&p, buf, sizeof(buf));
    ASSERT(strcmp(buf, "a/b/c") == 0);
}

/* ============ Windows Tests ============ */

TEST(win_drive_letter) {
    SpPath p = sp_path_f("C:/a/b", SP_FLAVOR_WINDOWS);
    ASSERT_SV_EQ(sp_drive(&p), "C:");
}

TEST(win_drive_empty) {
    SpPath p = sp_path_f("/a/b", SP_FLAVOR_WINDOWS);
    ASSERT_SV_EQ(sp_drive(&p), "");
}

TEST(win_root_slash) {
    SpPath p = sp_path_f("C:/a/b", SP_FLAVOR_WINDOWS);
    ASSERT_SV_EQ(sp_root(&p), "\\");
}

TEST(win_root_empty) {
    SpPath p = sp_path_f("C:a/b", SP_FLAVOR_WINDOWS);
    ASSERT_SV_EQ(sp_root(&p), "");
}

TEST(win_anchor_drive_root) {
    SpPath p = sp_path_f("C:/a/b", SP_FLAVOR_WINDOWS);
    ASSERT_SV_EQ(sp_anchor(&p), "C:\\");
}

TEST(win_anchor_drive_only) {
    SpPath p = sp_path_f("C:a/b", SP_FLAVOR_WINDOWS);
    ASSERT_SV_EQ(sp_anchor(&p), "C:");
}

TEST(win_parts_drive_relative) {
    SpPath p = sp_path_f("c:a/b", SP_FLAVOR_WINDOWS);
    SpPartsIter it = sp_parts_begin(&p);
    SpStr part;
    ASSERT(sp_parts_next(&it, &part));
    ASSERT_SV_EQ(part, "c:");
    ASSERT(sp_parts_next(&it, &part));
    ASSERT_SV_EQ(part, "a");
    ASSERT(sp_parts_next(&it, &part));
    ASSERT_SV_EQ(part, "b");
    ASSERT(!sp_parts_next(&it, &part));
}

TEST(win_parts_absolute) {
    SpPath p = sp_path_f("c:/a/b", SP_FLAVOR_WINDOWS);
    SpPartsIter it = sp_parts_begin(&p);
    SpStr part;
    ASSERT(sp_parts_next(&it, &part));
    ASSERT_SV_EQ(part, "c:\\");
    ASSERT(sp_parts_next(&it, &part));
    ASSERT_SV_EQ(part, "a");
    ASSERT(sp_parts_next(&it, &part));
    ASSERT_SV_EQ(part, "b");
    ASSERT(!sp_parts_next(&it, &part));
}

TEST(win_unc_parts) {
    SpPath p = sp_path_f("//server/share/a/b", SP_FLAVOR_WINDOWS);
    SpPartsIter it = sp_parts_begin(&p);
    SpStr part;
    ASSERT(sp_parts_next(&it, &part));
    ASSERT_SV_EQ(part, "\\\\server\\share\\");
    ASSERT(sp_parts_next(&it, &part));
    ASSERT_SV_EQ(part, "a");
    ASSERT(sp_parts_next(&it, &part));
    ASSERT_SV_EQ(part, "b");
    ASSERT(!sp_parts_next(&it, &part));
}

TEST(win_join_simple) {
    SpPath p = sp_path_f("C:/a/b", SP_FLAVOR_WINDOWS);
    SpPath r = sp_join_one(&p, "x/y");
    ASSERT_PATH_EQ(r, "C:\\a\\b\\x\\y");
}

TEST(win_join_root_override) {
    SpPath p = sp_path_f("C:/a/b", SP_FLAVOR_WINDOWS);
    SpPath r = sp_join_one(&p, "/x/y");
    ASSERT_PATH_EQ(r, "C:\\x\\y");
}

TEST(win_join_drive_override) {
    SpPath p = sp_path_f("C:/a/b", SP_FLAVOR_WINDOWS);
    SpPath r = sp_join_one(&p, "D:/x/y");
    ASSERT_PATH_EQ(r, "D:\\x\\y");
}

TEST(win_join_same_drive_relative) {
    SpPath p = sp_path_f("C:/a/b", SP_FLAVOR_WINDOWS);
    SpPath r = sp_join_one(&p, "c:x/y");
    ASSERT_PATH_EQ(r, "C:\\a\\b\\x\\y");
}

TEST(win_name) {
    SpPath p = sp_path_f("C:/a/b.py", SP_FLAVOR_WINDOWS);
    ASSERT_SV_EQ(sp_name(&p), "b.py");
}

TEST(win_suffix) {
    SpPath p = sp_path_f("C:/a/b.py", SP_FLAVOR_WINDOWS);
    ASSERT_SV_EQ(sp_suffix(&p), ".py");
}

TEST(win_suffixes_multi) {
    SpPath p = sp_path_f("c:a/b.tar.gz", SP_FLAVOR_WINDOWS);
    SpSuffixes s = sp_suffixes(&p);
    ASSERT(s.count == 2);
    ASSERT_SV_EQ(s.items[0], ".tar");
    ASSERT_SV_EQ(s.items[1], ".gz");
}

TEST(win_parent) {
    SpPath p = sp_path_f("C:/a/b/c", SP_FLAVOR_WINDOWS);
    ASSERT_PATH_EQ(sp_parent(&p), "C:\\a\\b");
}

TEST(win_is_absolute_yes) {
    SpPath p = sp_path_f("C:/a/b", SP_FLAVOR_WINDOWS);
    ASSERT(sp_is_absolute(&p));
}

TEST(win_is_absolute_no_drive_only) {
    SpPath p = sp_path_f("C:a/b", SP_FLAVOR_WINDOWS);
    ASSERT(!sp_is_absolute(&p));
}

TEST(win_is_absolute_no_root_only) {
    SpPath p = sp_path_f("/a/b", SP_FLAVOR_WINDOWS);
    ASSERT(!sp_is_absolute(&p));
}

TEST(win_is_absolute_unc) {
    SpPath p = sp_path_f("//server/share/a", SP_FLAVOR_WINDOWS);
    ASSERT(sp_is_absolute(&p));
}

TEST(win_normalize_mixed_sep) {
    SpPath p = sp_path_f("C:/a\\b/c", SP_FLAVOR_WINDOWS);
    ASSERT_PATH_EQ(p, "C:\\a\\b\\c");
}

TEST(win_as_posix) {
    SpPath p = sp_path_f("C:\\a\\b\\c", SP_FLAVOR_WINDOWS);
    char buf[SP_PATH_MAX];
    sp_as_posix(&p, buf, sizeof(buf));
    ASSERT(strcmp(buf, "C:/a/b/c") == 0);
}

TEST(win_with_name) {
    SpPath p = sp_path_f("C:/a/b", SP_FLAVOR_WINDOWS);
    SpPath r = sp_with_name(&p, "d.xml");
    ASSERT_PATH_EQ(r, "C:\\a\\d.xml");
}

TEST(win_with_suffix) {
    SpPath p = sp_path_f("C:/a/b.py", SP_FLAVOR_WINDOWS);
    SpPath r = sp_with_suffix(&p, ".gz");
    ASSERT_PATH_EQ(r, "C:\\a\\b.gz");
}

TEST(win_relative_to) {
    SpPath p = sp_path_f("C:/a/b/c", SP_FLAVOR_WINDOWS);
    SpPath o = sp_path_f("C:/a", SP_FLAVOR_WINDOWS);
    SpPath r = sp_relative_to(&p, &o);
    ASSERT_PATH_EQ(r, "b\\c");
}

TEST(win_is_relative_to_diff_drive) {
    SpPath p = sp_path_f("C:/a/b", SP_FLAVOR_WINDOWS);
    SpPath o = sp_path_f("D:/a", SP_FLAVOR_WINDOWS);
    ASSERT(!sp_is_relative_to(&p, &o));
}

/* ============ Edge Cases ============ */

TEST(edge_empty_path) {
    SpPath p = sp_path_f("", SP_FLAVOR_POSIX);
    ASSERT(sp_is_empty(&p));
    ASSERT_SV_EQ(sp_name(&p), "");
    ASSERT_SV_EQ(sp_suffix(&p), "");
}

TEST(edge_dot_path) {
    SpPath p = sp_path_f(".", SP_FLAVOR_POSIX);
    ASSERT_PATH_EQ(p, ".");
    ASSERT_SV_EQ(sp_name(&p), ".");
}

TEST(edge_dotdot_path) {
    SpPath p = sp_path_f("..", SP_FLAVOR_POSIX);
    ASSERT_PATH_EQ(p, "..");
    ASSERT_SV_EQ(sp_name(&p), "..");
}

TEST(edge_many_dots) {
    SpPath p = sp_path_f("...", SP_FLAVOR_POSIX);
    ASSERT_SV_EQ(sp_suffix(&p), "");
}

TEST(edge_complex_suffixes) {
    SpPath p = sp_path_f("a/b.c.d.e", SP_FLAVOR_POSIX);
    SpSuffixes s = sp_suffixes(&p);
    ASSERT(s.count == 3);
    ASSERT_SV_EQ(s.items[0], ".c");
    ASSERT_SV_EQ(s.items[1], ".d");
    ASSERT_SV_EQ(s.items[2], ".e");
}

TEST(edge_hidden_file_multi_ext) {
    SpPath p = sp_path_f(".tar.gz", SP_FLAVOR_POSIX);
    ASSERT_SV_EQ(sp_stem(&p), ".tar");
    ASSERT_SV_EQ(sp_suffix(&p), ".gz");
}

TEST(edge_single_component) {
    SpPath p = sp_path_f("file.txt", SP_FLAVOR_POSIX);
    ASSERT_SV_EQ(sp_name(&p), "file.txt");
    ASSERT_SV_EQ(sp_stem(&p), "file");
    ASSERT_SV_EQ(sp_suffix(&p), ".txt");
    ASSERT_PATH_EQ(sp_parent(&p), ".");
}

TEST(edge_deeply_nested) {
    SpPath p = sp_path_f("/a/b/c/d/e/f/g", SP_FLAVOR_POSIX);
    ASSERT(sp_parts_count(&p) == 8);
    SpPath parent = sp_parent(&p);
    ASSERT_PATH_EQ(parent, "/a/b/c/d/e/f");
}

TEST(edge_join_empty) {
    SpPath p = sp_path_f("a/b", SP_FLAVOR_POSIX);
    SpPath r = sp_join_one(&p, "");
    ASSERT_PATH_EQ(r, "a/b");
}

TEST(edge_join_to_empty) {
    SpPath p = sp_path_f("", SP_FLAVOR_POSIX);
    SpPath r = sp_join_one(&p, "a");
    ASSERT_PATH_EQ(r, "a");
}

int main(void) {
    printf("POSIX Tests:\n");
    RUN(posix_anchor_empty);
    RUN(posix_anchor_relative);
    RUN(posix_anchor_root);
    RUN(posix_anchor_absolute);
    RUN(posix_drive);
    RUN(posix_root_empty);
    RUN(posix_root_slash);
    RUN(posix_name_empty);
    RUN(posix_name_root);
    RUN(posix_name_simple);
    RUN(posix_name_with_ext);
    RUN(posix_stem_simple);
    RUN(posix_stem_with_ext);
    RUN(posix_stem_hidden);
    RUN(posix_stem_multi_ext);
    RUN(posix_suffix_py);
    RUN(posix_suffix_hidden_none);
    RUN(posix_suffix_hidden_with_ext);
    RUN(posix_suffix_multi);
    RUN(posix_suffixes_single);
    RUN(posix_suffixes_multi);
    RUN(posix_suffixes_hidden);
    RUN(posix_parts_relative);
    RUN(posix_parts_absolute);
    RUN(posix_parts_count);
    RUN(posix_parent_simple);
    RUN(posix_parent_absolute);
    RUN(posix_parent_root);
    RUN(posix_parent_single);
    RUN(posix_parents_iter);
    RUN(posix_join_simple);
#ifndef __cplusplus
    RUN(posix_join_multi);
#endif
    RUN(posix_join_absolute_override);
    RUN(posix_with_name);
    RUN(posix_with_stem);
    RUN(posix_with_suffix);
    RUN(posix_with_suffix_add);
    RUN(posix_with_suffix_remove);
    RUN(posix_is_absolute_yes);
    RUN(posix_is_absolute_no);
    RUN(posix_is_relative_to_yes);
    RUN(posix_is_relative_to_no);
    RUN(posix_relative_to);
    RUN(posix_relative_to_self);
    RUN(posix_normalize_double_slash);
    RUN(posix_normalize_trailing_slash);
    RUN(posix_eq);
    RUN(posix_as_posix);

    printf("\nWindows Tests:\n");
    RUN(win_drive_letter);
    RUN(win_drive_empty);
    RUN(win_root_slash);
    RUN(win_root_empty);
    RUN(win_anchor_drive_root);
    RUN(win_anchor_drive_only);
    RUN(win_parts_drive_relative);
    RUN(win_parts_absolute);
    RUN(win_unc_parts);
    RUN(win_join_simple);
    RUN(win_join_root_override);
    RUN(win_join_drive_override);
    RUN(win_join_same_drive_relative);
    RUN(win_name);
    RUN(win_suffix);
    RUN(win_suffixes_multi);
    RUN(win_parent);
    RUN(win_is_absolute_yes);
    RUN(win_is_absolute_no_drive_only);
    RUN(win_is_absolute_no_root_only);
    RUN(win_is_absolute_unc);
    RUN(win_normalize_mixed_sep);
    RUN(win_as_posix);
    RUN(win_with_name);
    RUN(win_with_suffix);
    RUN(win_relative_to);
    RUN(win_is_relative_to_diff_drive);

    printf("\nEdge Cases:\n");
    RUN(edge_empty_path);
    RUN(edge_dot_path);
    RUN(edge_dotdot_path);
    RUN(edge_many_dots);
    RUN(edge_complex_suffixes);
    RUN(edge_hidden_file_multi_ext);
    RUN(edge_single_component);
    RUN(edge_deeply_nested);
    RUN(edge_join_empty);
    RUN(edge_join_to_empty);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return 0;
}
