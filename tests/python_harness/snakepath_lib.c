/* snakepath_lib.c - Shared library wrapper for Python bindings */

#define SP_PATH_MAX 1024  /* Use SP_PATH_MAX_WINDOWS for cross-platform compatibility */
#define SNAKEPATH_IMPLEMENTATION
#include "snakepath.h"

#ifdef _WIN32
#define SP_EXPORT __declspec(dllexport)
#else
#define SP_EXPORT __attribute__((visibility("default")))
#endif

/* Wrapper macros for common patterns */

#define WRAP_STR(fn) \
    SP_EXPORT void sp_##fn##_wrap(const SpPath *p, const char **data, size_t *len) { \
        SpStr sv = sp_##fn(p); *data = sv.data; *len = sv.len; \
    }

#define WRAP_PATH_UNARY(fn) \
    SP_EXPORT void sp_##fn##_wrap(const SpPath *p, SpPath *out) { *out = sp_##fn(p); }

#define WRAP_PATH_CSTR(fn) \
    SP_EXPORT void sp_##fn##_wrap(const SpPath *p, const char *s, SpPath *out) { *out = sp_##fn(p, s); }

#define WRAP_PATH_PATH(fn) \
    SP_EXPORT void sp_##fn##_wrap(const SpPath *a, const SpPath *b, SpPath *out) { *out = sp_##fn(a, b); }

#define WRAP_BOOL_UNARY(fn) \
    SP_EXPORT int sp_##fn##_wrap(const SpPath *p) { return sp_##fn(p) ? 1 : 0; }

#define WRAP_BOOL_BINARY(fn) \
    SP_EXPORT int sp_##fn##_wrap(const SpPath *a, const SpPath *b) { return sp_##fn(a, b) ? 1 : 0; }

/* Path -> SpStr */
WRAP_STR(drive)
WRAP_STR(root)
WRAP_STR(anchor)
WRAP_STR(name)
WRAP_STR(stem)
WRAP_STR(suffix)

/* Path -> Path */
WRAP_PATH_UNARY(parent)

/* Path + cstr -> Path */
WRAP_PATH_CSTR(with_name)
WRAP_PATH_CSTR(with_stem)
WRAP_PATH_CSTR(with_suffix)
WRAP_PATH_CSTR(join_one)

SP_EXPORT void sp_join_one_len_wrap(const SpPath *p, const char *s, size_t len, SpPath *out) {
    SpStr sv = {s, len};
    *out = sp_join_sv(p, sv);
}

/* Path + Path -> Path */
WRAP_PATH_PATH(joinpath)
WRAP_PATH_PATH(relative_to)
WRAP_PATH_PATH(relative_to_walk_up)

SP_EXPORT int sp_relative_to_is_error_wrap(const SpPath *p) {
    return sp_relative_to_is_error(p) ? 1 : 0;
}

/* Multi-segment variants */
SP_EXPORT int sp_is_relative_to_parts_wrap(const SpPath *p, const char **parts) {
    return sp_is_relative_to_parts(p, parts) ? 1 : 0;
}

SP_EXPORT void sp_relative_to_parts_wrap(const SpPath *p, const char **parts, int walk_up, SpPath *out) {
    *out = sp_relative_to_parts(p, parts, walk_up != 0);
}

SP_EXPORT size_t sp_as_uri_wrap(const SpPath *p, char *buf, size_t buf_size) {
    return sp_as_uri(p, buf, buf_size);
}

/* Path -> bool */
WRAP_BOOL_UNARY(is_absolute)

/* Path -> Path (cwd/absolute) */
WRAP_PATH_UNARY(absolute)
SP_EXPORT void sp_cwd_wrap(int flavor, SpPath *out) { *out = sp_cwd((SpFlavor)flavor); }

/* Path + Path -> bool */
WRAP_BOOL_BINARY(is_relative_to)
WRAP_BOOL_BINARY(path_eq)

/* Additional functions */
SP_EXPORT int sp_path_cmp_wrap(const SpPath *a, const SpPath *b) { return sp_path_cmp(a, b); }
SP_EXPORT unsigned long sp_path_hash_wrap(const SpPath *p) { return sp_path_hash(p); }
SP_EXPORT int sp_match_wrap(const SpPath *p, const char *pattern) { return sp_match(p, pattern); }
SP_EXPORT int sp_match_ex_wrap(const SpPath *p, const char *pattern, int case_sensitive) { return sp_match_ex(p, pattern, case_sensitive); }
WRAP_BOOL_UNARY(is_reserved)
WRAP_BOOL_UNARY(is_file)
WRAP_BOOL_UNARY(is_dir)
WRAP_BOOL_UNARY(exists)
SP_EXPORT void sp_stat_wrap(const SpPath *p, SpStatResult *out) { *out = sp_stat(p); }
SP_EXPORT int sp_stat_eq_wrap(const SpStatResult *a, const SpStatResult *b) { return sp_stat_eq(a, b) ? 1 : 0; }
SP_EXPORT size_t sp_parents_count_wrap(const SpPath *p) { return sp_parents_count(p); }
SP_EXPORT size_t sp_sizeof_stat_result(void) { return sizeof(SpStatResult); }
SP_EXPORT int sp_path_is_error_wrap(const SpPath *p) { return sp_path_is_error(p) ? 1 : 0; }
SP_EXPORT int sp_path_error_code_wrap(const SpPath *p) { return sp_path_error_code(p); }

/* Special cases */

SP_EXPORT void sp_path_new_wrap(const char *s, int flavor, SpPath *out) {
    *out = sp_path_new(s, (SpPathOpts){(SpFlavor)flavor});
}

SP_EXPORT void sp_path_new_len_wrap(const char *s, size_t len, int flavor, SpPath *out) {
    SpStr sv = {s, len};
    *out = sp_path_from_sv(sv, (SpFlavor)flavor);
}

SP_EXPORT void sp_path_convert_wrap(const char *s, int src_flavor, int dest_flavor, SpPath *out) {
    *out = sp_path_convert(s, (SpFlavor)src_flavor, (SpFlavor)dest_flavor);
}

SP_EXPORT void sp_path_copy_wrap(const SpPath *p, SpPath *out) { *out = sp_path_copy(p); }
SP_EXPORT const char *sp_str_wrap(const SpPath *p) { return sp_str(p); }
SP_EXPORT void sp_as_posix_wrap(const SpPath *p, char *out, size_t out_size) { sp_as_posix(p, out, out_size); }

SP_EXPORT size_t sp_suffixes_wrap(const SpPath *p, const char **data_arr, size_t *len_arr, size_t max_items) {
    SpSuffixes s = sp_suffixes(p);
    size_t count = s.count < max_items ? s.count : max_items;
    for (size_t i = 0; i < count; i++) { data_arr[i] = s.items[i].data; len_arr[i] = s.items[i].len; }
    return count;
}

SP_EXPORT size_t sp_parts_count_wrap(const SpPath *p) { return sp_parts_count(p); }
SP_EXPORT void sp_parts_iter_begin_wrap(const SpPath *p, SpPartsIter *out) { *out = sp_parts_begin(p); }
SP_EXPORT int sp_parts_iter_next_wrap(SpPartsIter *it, const char **data, size_t *len) {
    SpStr part;
    if (sp_parts_next(it, &part)) { *data = part.data; *len = part.len; return 1; }
    return 0;
}

SP_EXPORT void sp_parents_iter_begin_wrap(const SpPath *p, SpParentsIter *out) { *out = sp_parents_begin(p); }
SP_EXPORT int sp_parents_iter_next_wrap(SpParentsIter *it, SpPath *out) { return sp_parents_next(it, out) ? 1 : 0; }

/* Struct sizes and constants */
SP_EXPORT size_t sp_sizeof_path(void) { return sizeof(SpPath); }
SP_EXPORT size_t sp_sizeof_parts_iter(void) { return sizeof(SpPartsIter); }
SP_EXPORT size_t sp_sizeof_parents_iter(void) { return sizeof(SpParentsIter); }
SP_EXPORT size_t sp_path_max(void) { return SP_PATH_MAX; }
SP_EXPORT size_t sp_max_suffixes(void) { return SP_MAX_SUFFIXES; }

/* Error codes */
SP_EXPORT int sp_err_none(void) { return SP_ERR_NONE; }
SP_EXPORT int sp_err_not_relative(void) { return SP_ERR_NOT_RELATIVE; }
SP_EXPORT int sp_err_no_name(void) { return SP_ERR_NO_NAME; }
SP_EXPORT int sp_err_invalid_arg(void) { return SP_ERR_INVALID_ARG; }
SP_EXPORT int sp_match_yes(void) { return SP_MATCH_YES; }
SP_EXPORT int sp_match_no(void) { return SP_MATCH_NO; }
SP_EXPORT int sp_match_err_empty(void) { return SP_MATCH_ERR_EMPTY; }
SP_EXPORT int sp_match_err_invalid(void) { return SP_MATCH_ERR_INVALID; }

/* mkdir */
SP_EXPORT int sp_mkdir_wrap(const SpPath *p, unsigned int mode, int parents, int exist_ok) {
    return sp_mkdir(p, mode, parents != 0, exist_ok != 0);
}
SP_EXPORT int sp_mkdir_ok(void) { return SP_MKDIR_OK; }
SP_EXPORT int sp_mkdir_err_exists(void) { return SP_MKDIR_ERR_EXISTS; }
SP_EXPORT int sp_mkdir_err_not_found(void) { return SP_MKDIR_ERR_NOT_FOUND; }
SP_EXPORT int sp_mkdir_err_not_dir(void) { return SP_MKDIR_ERR_NOT_DIR; }
SP_EXPORT int sp_mkdir_err_permission(void) { return SP_MKDIR_ERR_PERMISSION; }
SP_EXPORT int sp_mkdir_err_other(void) { return SP_MKDIR_ERR_OTHER; }
SP_EXPORT int sp_mkdir_err_exists_not_dir(void) { return SP_MKDIR_ERR_EXISTS_NOT_DIR; }

/* glob */
SP_EXPORT SpGlobIter *sp_glob_begin_wrap(const SpPath *p, const char *pattern, int cs) {
    return sp_glob_begin_ex(p, pattern, (SpCaseSensitivity)cs);
}
SP_EXPORT SpGlobIter *sp_rglob_begin_wrap(const SpPath *p, const char *pattern, int cs) {
    return sp_rglob_begin_ex(p, pattern, (SpCaseSensitivity)cs);
}

/* Case sensitivity enum values */
SP_EXPORT int sp_case_platform_default(void) { return SP_CASE_PLATFORM_DEFAULT; }
SP_EXPORT int sp_case_sensitive(void) { return SP_CASE_SENSITIVE; }
SP_EXPORT int sp_case_insensitive(void) { return SP_CASE_INSENSITIVE; }
SP_EXPORT int sp_glob_next_wrap(SpGlobIter *it, SpPath *out) {
    return sp_glob_next(it, out) ? 1 : 0;
}
SP_EXPORT void sp_glob_end_wrap(SpGlobIter *it) {
    sp_glob_end(it);
}
