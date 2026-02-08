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

#define WRAP_TERM(fn) \
    SP_EXPORT void sp_##fn##_wrap(const SpPath *p, char *buf, size_t buf_size, size_t *len) { \
        SpTerm t = sp_##fn(p); \
        size_t n = t.len < buf_size - 1 ? t.len : buf_size - 1; \
        memcpy(buf, t.buf, n); \
        buf[n] = '\0'; \
        *len = t.len; \
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

/* Path -> SpTerm (null-terminated component strings) */
WRAP_TERM(drive)
WRAP_TERM(root)
WRAP_TERM(anchor)
WRAP_TERM(name)
WRAP_TERM(stem)
WRAP_TERM(suffix)

/* Path -> Path */
WRAP_PATH_UNARY(parent)

/* Path + cstr -> Path */
WRAP_PATH_CSTR(with_name)
WRAP_PATH_CSTR(with_stem)
WRAP_PATH_CSTR(with_suffix)
WRAP_PATH_CSTR(join_one)

SP_EXPORT void sp_join_one_len_wrap(const SpPath *p, const char *s, size_t len, SpPath *out) {
    *out = sp_join_n(p, s, len);
}

/* Path + Path -> Path */
WRAP_PATH_PATH(joinpath)
WRAP_PATH_PATH(relative_to)
WRAP_PATH_PATH(relative_to_walk_up)

SP_EXPORT int sp_relative_to_is_error_wrap(const SpPath *p) {
    return (p->len == 0 && p->buf[0] == SP_ERR_NOT_RELATIVE) ? 1 : 0;
}

/* Multi-segment variants */
SP_EXPORT void sp_with_segments_wrap(const SpPath *p, const char **parts, size_t parts_count, SpPath *out) {
    *out = sp_with_segments(p, parts, parts_count);
}

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
SP_EXPORT int sp_match_wrap(const SpPath *p, const char *pattern) { return SP_MATCH(p, pattern); }
SP_EXPORT int sp_match_ex_wrap(const SpPath *p, const char *pattern, int case_sensitive) { return sp_match_ex(p, pattern, case_sensitive); }
WRAP_BOOL_UNARY(is_reserved)
WRAP_BOOL_UNARY(is_file)
WRAP_BOOL_UNARY(is_dir)
WRAP_BOOL_UNARY(exists)
WRAP_BOOL_UNARY(is_symlink)
WRAP_BOOL_UNARY(is_block_device)
WRAP_BOOL_UNARY(is_char_device)
WRAP_BOOL_UNARY(is_fifo)
WRAP_BOOL_UNARY(is_socket)
WRAP_BOOL_UNARY(is_mount)
WRAP_BOOL_UNARY(is_junction)
SP_EXPORT void sp_stat_wrap(const SpPath *p, SpStatResult *out) { *out = sp_stat(p); }
SP_EXPORT void sp_lstat_wrap(const SpPath *p, SpStatResult *out) { *out = sp_lstat(p); }
SP_EXPORT int sp_stat_eq_wrap(const SpStatResult *a, const SpStatResult *b) { return sp_stat_eq(a, b) ? 1 : 0; }

/* Symlink & link operations */
SP_EXPORT void sp_readlink_wrap(const SpPath *p, SpPath *out) { *out = sp_readlink(p); }
SP_EXPORT void sp_resolve_wrap(const SpPath *p, int strict, SpPath *out) { *out = sp_resolve(p, strict != 0); }
SP_EXPORT int sp_symlink_to_wrap(const SpPath *p, const SpPath *target, int target_is_directory) {
    return sp_symlink_to(p, target, target_is_directory != 0) ? 1 : 0;
}
SP_EXPORT int sp_hardlink_to_wrap(const SpPath *p, const SpPath *target) {
    return sp_hardlink_to(p, target) ? 1 : 0;
}
WRAP_BOOL_BINARY(samefile)
SP_EXPORT size_t sp_parents_count_wrap(const SpPath *p) { return sp_parents_count(p); }
SP_EXPORT size_t sp_sizeof_stat_result(void) { return sizeof(SpStatResult); }
SP_EXPORT int sp_path_is_error_wrap(const SpPath *p) { return sp_path_is_error(p) ? 1 : 0; }
SP_EXPORT int sp_path_error_code_wrap(const SpPath *p) { return sp_path_error_code(p); }

/* Special cases */

SP_EXPORT void sp_path_new_wrap(const char *s, int flavor, SpPath *out) {
    *out = sp_path_new(s, (SpPathOpts){(SpFlavor)flavor});
}

SP_EXPORT void sp_path_new_len_wrap(const char *s, size_t len, int flavor, SpPath *out) {
    *out = sp_path_from_n(s, len, (SpFlavor)flavor);
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

/* glob iterator */
SP_EXPORT size_t sp_sizeof_glob_iter(void) { return sizeof(SpGlobIter); }
SP_EXPORT void sp_glob_begin_wrap(const SpPath *p, const char *pattern, int cs, SpGlobIter *out) {
    *out = sp_glob_begin(p, pattern, (SpCaseSensitivity)cs);
}
SP_EXPORT void sp_rglob_begin_wrap(const SpPath *p, const char *pattern, int cs, SpGlobIter *out) {
    *out = sp_rglob_begin(p, pattern, (SpCaseSensitivity)cs);
}
SP_EXPORT int sp_glob_next_wrap(SpGlobIter *it, SpPath *out) { return sp_glob_next(it, out) ? 1 : 0; }
SP_EXPORT void sp_glob_end_wrap(SpGlobIter *it) { sp_glob_end(it); }
SP_EXPORT int sp_glob_depth_wrap(SpGlobIter *it) { return it->depth; }

/* Case sensitivity enum values */
SP_EXPORT int sp_case_platform_default(void) { return SP_CASE_PLATFORM_DEFAULT; }
SP_EXPORT int sp_case_sensitive(void) { return SP_CASE_SENSITIVE; }
SP_EXPORT int sp_case_insensitive(void) { return SP_CASE_INSENSITIVE; }

/* File/directory modification operations */
SP_EXPORT int sp_touch_wrap(const SpPath *p, unsigned int mode, int exist_ok) {
    return sp_touch(p, mode, exist_ok != 0) ? 1 : 0;
}
SP_EXPORT int sp_unlink_wrap(const SpPath *p, int missing_ok) {
    return sp_unlink(p, missing_ok != 0) ? 1 : 0;
}
SP_EXPORT int sp_rmdir_wrap(const SpPath *p) {
    return sp_rmdir(p) ? 1 : 0;
}
SP_EXPORT void sp_rename_wrap(const SpPath *p, const SpPath *target, SpPath *out) {
    *out = sp_rename(p, target);
}
SP_EXPORT void sp_replace_wrap(const SpPath *p, const SpPath *target, SpPath *out) {
    *out = sp_replace(p, target);
}
SP_EXPORT int sp_chmod_wrap(const SpPath *p, unsigned int mode) {
    return sp_chmod(p, mode) ? 1 : 0;
}

/* File I/O */
SP_EXPORT void sp_read_file_wrap(const SpPath *p, char *buf, size_t buf_size, size_t *bytes_out, int *error_out) {
    SpIOResult r = sp_read_file(p, buf, buf_size);
    *bytes_out = r.bytes;
    *error_out = r.error;
}
SP_EXPORT void sp_write_file_wrap(const SpPath *p, const char *data, size_t data_len, size_t *bytes_out, int *error_out) {
    SpIOResult r = sp_write_file(p, data, data_len);
    *bytes_out = r.bytes;
    *error_out = r.error;
}
SP_EXPORT int sp_io_ok(void) { return SP_IO_OK; }
SP_EXPORT int sp_io_err_open(void) { return SP_IO_ERR_OPEN; }
SP_EXPORT int sp_io_err_read(void) { return SP_IO_ERR_READ; }
SP_EXPORT int sp_io_err_write(void) { return SP_IO_ERR_WRITE; }
SP_EXPORT int sp_io_err_too_large(void) { return SP_IO_ERR_TOO_LARGE; }

/* Home directory and user expansion */
SP_EXPORT void sp_home_wrap(int flavor, SpPath *out) {
    *out = sp_home((SpFlavor)flavor);
}

SP_EXPORT void sp_expanduser_wrap(const SpPath *p, SpPath *out) {
    *out = sp_expanduser(p);
}

/* User/group info - returns: 0 = success, 1 = error/not found */
#ifndef SP_WINDOWS
SP_EXPORT int sp_owner_wrap(const SpPath *p, char *buf, size_t buf_size, size_t *len) {
    SpTerm t = sp_owner(p);
    size_t n = t.len < buf_size - 1 ? t.len : buf_size - 1;
    memcpy(buf, t.buf, n);
    buf[n] = '\0';
    *len = t.len;
    return t.len > 0 ? 0 : 1;
}

SP_EXPORT int sp_group_wrap(const SpPath *p, char *buf, size_t buf_size, size_t *len) {
    SpTerm t = sp_group(p);
    size_t n = t.len < buf_size - 1 ? t.len : buf_size - 1;
    memcpy(buf, t.buf, n);
    buf[n] = '\0';
    *len = t.len;
    return t.len > 0 ? 0 : 1;
}
#endif

/* iterdir iterator */
SP_EXPORT size_t sp_sizeof_iterdir_iter(void) { return sizeof(SpIterdirIter); }

SP_EXPORT void sp_iterdir_begin_wrap(const SpPath *p, SpIterdirIter *out) {
    *out = sp_iterdir_begin(p);
}

SP_EXPORT int sp_iterdir_next_wrap(SpIterdirIter *it, SpPath *out) {
    return sp_iterdir_next(it, out) ? 1 : 0;
}

SP_EXPORT void sp_iterdir_end_wrap(SpIterdirIter *it) {
    sp_iterdir_end(it);
}

SP_EXPORT int sp_iterdir_done_wrap(SpIterdirIter *it) {
    return it->done;
}

/* walk - Python walk() composes iterdir + is_dir rather than wrapping sp_walk,
 * because Python's walk API requires generator semantics with in-place dirnames
 * pruning between yields, which can't be expressed through a C callback. */
