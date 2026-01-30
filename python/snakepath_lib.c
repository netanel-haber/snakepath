/* snakepath_lib.c - Shared library wrapper for Python bindings */

#define SNAKEPATH_IMPLEMENTATION
#include "../snakepath.h"

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

/* Path + Path -> Path */
WRAP_PATH_PATH(joinpath)
WRAP_PATH_PATH(relative_to)

/* Path -> bool */
WRAP_BOOL_UNARY(is_absolute)

/* Path + Path -> bool */
WRAP_BOOL_BINARY(is_relative_to)
WRAP_BOOL_BINARY(path_eq)

/* Special cases */

SP_EXPORT void sp_path_new_wrap(const char *s, int flavor, SpPath *out) {
    *out = sp_path_new(s, (SpPathOpts){(SpFlavor)flavor});
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
