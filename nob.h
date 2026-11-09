/* nob.h - snakepath's build script in one file: upstream nob v3.10.1 (https://github.com/tsoding/nob.h; its license
   is at its end), vendored whole and unchanged, then the build/test logic.

   Build: cc -x c -o nob nob.h && ./nob     (quiet: add -DSNAKEPATH_QUIET; MSVC: cl /Tcnob.h)
   Format: ./nob format                    (snakepath.h's clang-format layout, which ./nob checks)
   Diff: ./nob diff [ref]                  (the pure path API against the header at a git ref, default origin/main)
   Snake: ./nob snake                      (snake.c, the site's export: site/logo.c with the snakepath it uses, by trim.h, into site/snake.c)
   Builds run in parallel across the available CPU cores.

   To upgrade nob, replace the part between the markers with upstream nob.h.
*/

#ifdef SNAKEPATH_QUIET
#define NOB_NO_ECHO
#endif
#define NOB_IMPLEMENTATION

/* ======== upstream nob.h v3.10.1 ======== */
/* nob - v3.10.1 - Public Domain - https://github.com/tsoding/nob.h

   This library is the next generation of the [NoBuild](https://github.com/tsoding/nobuild) idea.

   # Quick Example

      ```c
      // nob.c
      #define NOB_IMPLEMENTATION
      #include "nob.h"

      int main(int argc, char **argv)
      {
          NOB_GO_REBUILD_URSELF(argc, argv);
          Nob_Cmd cmd = {0};
          nob_cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-o", "main", "main.c");
          if (!nob_cmd_run(&cmd)) return 1;
          return 0;
      }
      ```

      ```console
      $ cc -o nob nob.c
      $ ./nob
      ```

      The `nob` automatically rebuilds itself if `nob.c` is modified thanks to
      the `NOB_GO_REBUILD_URSELF` macro (don't forget to check out how it works below)

   # Stripping off `nob_` Prefixes

      Since Pure C does not have any namespaces we prefix each name of the API with the `nob_` to avoid any
      potential conflicts with any other names in your code. But sometimes it is very annoying and makes
      the code noisy. Because of that you can drop the `nob_` prefix.

      ```c
      // nob.c
      #define NOB_IMPLEMENTATION
      #include "nob.h"

      int main(int argc, char **argv)
      {
          GO_REBUILD_URSELF(argc, argv);
          Cmd cmd = {0};
          cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-o", "main", "main.c");
          if (!cmd_run(&cmd)) return 1;
          return 0;
      }
      ```

      If the lack of prefixes causes any problems you can disable the prefix stripping by defining
      `NOB_UNSTRIP_PREFIX` feature macro before including "nob.h".

      Not all the names have strippable prefixes. All the redefinable names like `NOB_REBUILD_URSELF`
      for instance will retain their prefix always. Notable exception is the nob_log() function. Stripping
      away the prefix results in log() which was historically always referring to the natural logarithmic
      function that is already defined in math.h. So there is no reason to strip off the prefix for nob_log().
      Another exception is nob_rename() which collides with the widely known POSIX function rename(2) if you
      strip the prefix off.

      The prefixes are stripped off only on the level of the preprocessor. The names of the functions in the
      compiled object file will still retain the `nob_` prefix. Keep that in mind when you FFI with nob.h
      from other languages (for whatever reason).

      If only few specific names create conflicts for you, you can just #undef those names after the
      `#include <nob.h>` without enabling `NOB_UNSTRIP_PREFIX` since they are macros anyway.

   # Macro Interface

      All these macros are `#define`d by the user before including nob.h

   ## Flags

      Enable or disable certain aspects of nob.h

      - NOB_IMPLEMENTATION - Enable definitions of the functions. By default only declarations are included.
        See https://github.com/nothings/stb/blob/f58f558c120e9b32c217290b80bad1a0729fbb2c/docs/stb_howto.txt
        for more info.
      - NOB_WARN_DEPRECATED - Warn about the usage of deprecated function. We rarely actually remove deprecated functions,
        but if you want to know what is discouraged you may want to enable this flag.
      - NOB_EXPERIMENTAL_DELETE_OLD - Experimental feature that automatically removes `nob.old` files. It's unclear how well
        it works on Windows, so it's experimental for now.
      - NOB_EXPERIMENTAL_TRACE_CMD_RUN_FAIL - Log the locations of fail nob_cmd_run()-s. It's behind an experimental flag
        because I'm not sure how much it alters the UX of the library for other people, but this is something I personally need.
      - NOB_UNSTRIP_PREFIX - do not strip the `nob_` prefixes from non-redefinable names.
      - NOB_NO_ECHO - do not echo the actions various nob functions are doing (like nob_cmd_run(), nob_mkdir_if_not_exists(), etc).

   ## Redefinable Macros

      Redefine default behaviors of nob.h.

      - NOBDEF - Appends additional things to function declarations. You can do something like `#define NOBDEF static inline`.
      - NOB_ASSERT(condition) - Redefine which assert() nob.h shall use.
      - NOB_REALLOC(oldptr, size) - Redefine which realloc() nob.h shall use.
      - NOB_FREE(ptr) - Redefine which free() nob.h shall use.
      - NOB_DEPRECATED(message) - Redefine how nob.h shall mark functions as deprecated.
      - NOB_DA_INIT_CAP - Redefine initial capacity of Dynamic Arrays.
      - NOB_TEMP_CAPACITY - Redefine the capacity of the temporary storage.
      - NOB_REBUILD_URSELF(binary_path, source_path) - redefine how nob.h shall rebuild itself.
      - NOB_WIN32_ERR_MSG_SIZE - Redefine the capacity of the buffer for error message on Windows.
*/

#ifndef NOB_H_
#define NOB_H_
#ifdef _WIN32
#    ifndef _CRT_SECURE_NO_WARNINGS
#        define _CRT_SECURE_NO_WARNINGS (1)
#    endif // _CRT_SECURE_NO_WARNINGS
#endif //  _WIN32

#ifndef NOBDEF
/*
   Goes before declarations and definitions of the nob functions. Useful to `#define NOBDEF static inline`
   if your source code is a single file and you want the compiler to remove unused functions.
*/
#define NOBDEF
#endif /* NOBDEF */

/* snakepath: Linux only, where glibc's strict modes need it; on the BSDs it hides sysconf's _SC_NPROCESSORS_ONLN */
#if defined(__linux__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#ifndef NOB_ASSERT
#include <assert.h>
#define NOB_ASSERT assert
#endif /* NOB_ASSERT */

#ifndef NOB_REALLOC
#include <stdlib.h>
#define NOB_REALLOC realloc
#endif /* NOB_REALLOC */

#ifndef NOB_FREE
#include <stdlib.h>
#define NOB_FREE free
#endif /* NOB_FREE */

#ifdef NOB_WARN_DEPRECATED
#    ifndef NOB_DEPRECATED
#        if defined(__GNUC__) || defined(__clang__)
#            define NOB_DEPRECATED(message) __attribute__((deprecated(message)))
#        elif defined(_MSC_VER)
#            define NOB_DEPRECATED(message) __declspec(deprecated(message))
#        else
#            define NOB_DEPRECATED(...)
#        endif
#    endif /* NOB_DEPRECATED */
#else
#    define NOB_DEPRECATED(...)
#endif /* NOB_WARN_DEPRECATED */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>

#ifdef _WIN32
#    define WIN32_LEAN_AND_MEAN
#    define _WINUSER_
#    define _WINGDI_
#    define _IMM_
#    define _WINCON_
#    include <windows.h>
#    include <direct.h>
#    include <io.h>
#    include <shellapi.h>
#else
#    ifdef __APPLE__
#        include <mach-o/dyld.h>
#    endif
#    ifdef __FreeBSD__
#        include <sys/sysctl.h>
#    endif
#    include <sys/types.h>
#    include <sys/wait.h>
#    include <sys/stat.h>
#    include <unistd.h>
#    include <fcntl.h>
#    include <dirent.h>
#endif

#ifdef __HAIKU__
#   include <image.h>
#endif

#ifdef _WIN32
#    define NOB_LINE_END "\r\n"
#else
#    define NOB_LINE_END "\n"
#endif

#if defined(__GNUC__) || defined(__clang__)
//   https://gcc.gnu.org/onlinedocs/gcc-4.7.2/gcc/Function-Attributes.html
#    ifdef __MINGW_PRINTF_FORMAT
#        define NOB_PRINTF_FORMAT(STRING_INDEX, FIRST_TO_CHECK) __attribute__ ((format (__MINGW_PRINTF_FORMAT, STRING_INDEX, FIRST_TO_CHECK)))
#    else
#        define NOB_PRINTF_FORMAT(STRING_INDEX, FIRST_TO_CHECK) __attribute__ ((format (printf, STRING_INDEX, FIRST_TO_CHECK)))
#    endif // __MINGW_PRINTF_FORMAT
#else
//   TODO: implement NOB_PRINTF_FORMAT for MSVC
#    define NOB_PRINTF_FORMAT(STRING_INDEX, FIRST_TO_CHECK)
#endif

NOBDEF void nob__panicf(const char *file, int line, const char *label, const char *format, ...);

#define NOB_UNUSED(value) (void)(value)
#define NOB_TODO(message) do { fprintf(stderr, "%s:%d: TODO: %s\n", __FILE__, __LINE__, message); abort(); } while(0)
#define NOB_TODOF(...) nob__panicf(__FILE__, __LINE__, "TODO", __VA_ARGS__)
#define NOB_UNREACHABLE(message) do { fprintf(stderr, "%s:%d: UNREACHABLE: %s\n", __FILE__, __LINE__, message); abort(); } while(0)
#define NOB_UNREACHABLEF(...) nob__panicf(__FILE__, __LINE__, "UNREACHABLE", __VA_ARGS__)

#define NOB_ARRAY_LEN(array) (sizeof(array)/sizeof(array[0]))
#define NOB_ARRAY_GET(array, index) \
    (NOB_ASSERT((size_t)index < NOB_ARRAY_LEN(array)), array[(size_t)index])

typedef enum {
    NOB_INFO,
    NOB_WARNING,
    NOB_ERROR,
    NOB_NO_LOGS,
} Nob_Log_Level;

// Any messages with the level below nob_minimal_log_level are going to be suppressed by the nob_default_log_handler.
extern Nob_Log_Level nob_minimal_log_level;

typedef void (Nob_Log_Handler)(Nob_Log_Level level, const char *fmt, va_list args);
NOB_DEPRECATED("Uncapitalized nob_log_handler type is deprecated. Use Nob_Log_Handler instead. It's just when we were releasing the log handler feature we forgot that we had a convention that all the types must be capitalized like that. Sorry about it!")
typedef Nob_Log_Handler nob_log_handler;

NOBDEF void nob_set_log_handler(Nob_Log_Handler *handler);
NOBDEF Nob_Log_Handler *nob_get_log_handler(void);

NOBDEF Nob_Log_Handler nob_default_log_handler;
NOBDEF Nob_Log_Handler nob_cancer_log_handler;
NOBDEF Nob_Log_Handler nob_null_log_handler;

NOBDEF void nob_log(Nob_Log_Level level, const char *fmt, ...) NOB_PRINTF_FORMAT(2, 3);

// It is an equivalent of shift command from bash (do `help shift` in bash). It basically
// pops an element from the beginning of a sized array.
#define nob_shift(xs, xs_sz) (NOB_ASSERT((xs_sz) > 0), (xs_sz)--, *(xs)++)
// NOTE: nob_shift_args() is an alias for an old variant of nob_shift that only worked with
// the command line arguments passed to the main() function. nob_shift() is more generic.
// So nob_shift_args() is semi-deprecated, but I don't see much reason to urgently
// remove it. This alias does not hurt anybody.
#define nob_shift_args(argc, argv) nob_shift(*argv, *argc)

typedef struct {
    const char **items;
    size_t count;
    size_t capacity;
} Nob_File_Paths;

typedef enum {
    NOB_FILE_REGULAR = 0,
    NOB_FILE_DIRECTORY,
    NOB_FILE_SYMLINK,
    NOB_FILE_OTHER,
} Nob_File_Type;

NOBDEF bool nob_mkdir_if_not_exists(const char *path);
NOBDEF bool nob_copy_file(const char *src_path, const char *dst_path);
NOBDEF bool nob_copy_directory_recursively(const char *src_path, const char *dst_path);
NOBDEF bool nob_read_entire_dir(const char *parent, Nob_File_Paths *children);
NOBDEF bool nob_write_entire_file(const char *path, const void *data, size_t size);
NOBDEF Nob_File_Type nob_get_file_type(const char *path);
NOBDEF bool nob_delete_file(const char *path);

typedef enum {
    // If the current file is a directory go inside of it.
    NOB_WALK_CONT,
    // If the current file is a directory do not go inside of it.
    NOB_WALK_SKIP,
    // Stop the recursive traversal process entirely.
    NOB_WALK_STOP,
} Nob_Walk_Action;

typedef struct {
    // The path to the visited file. The lifetime of the path string is very short.
    // As soon as the execution exits the Nob_Walk_Func it's dead. Dup it somewhere
    // if you want to preserve it for longer periods of time.
    const char *path;
    // The type of the visited file.
    Nob_File_Type type;
    // How nested we currently are in the directory tree.
    size_t level;
    // User data supplied in Nob_Walk_Dir_Opt.data.
    void *data;
    // The action nob_walk_dir_opt() must perform after the Nob_Walk_Func has returned.
    // Default is NOB_WALK_CONT.
    Nob_Walk_Action *action;
} Nob_Walk_Entry;

// A function that is called by nob_walk_dir_opt() on each visited file.
// Nob_Walk_Entry provides the details about the visited file and also
// expects you to modify the `action` in case you want to alter the
// usual behavior of the recursive walking algorithm.
//
// If the function returns `false`, an error is assumed which causes the entire
// recursive walking process to exit and nob_walk_dir_opt() return `false`.
typedef bool (*Nob_Walk_Func)(Nob_Walk_Entry entry);

typedef struct {
    // User data passed to Nob_Walk_Entry.data
    void *data;
    // Walk the directory in post-order visiting the leaf files first.
    bool post_order;
} Nob_Walk_Dir_Opt;

NOBDEF bool nob_walk_dir_opt(const char *root, Nob_Walk_Func func, Nob_Walk_Dir_Opt);

#define nob_walk_dir(root, func, ...) nob_walk_dir_opt((root), (func), NOB_CLIT(Nob_Walk_Dir_Opt){__VA_ARGS__})

typedef struct {
    char *name;
    bool error;

    struct {
#ifdef _WIN32
        WIN32_FIND_DATA win32_data;
        HANDLE win32_hFind;
        bool win32_init;
#else
        DIR *posix_dir;
        struct dirent *posix_ent;
#endif // _WIN32
    } nob__private; // TODO: we don't have solid conventions regarding private struct fields
} Nob_Dir_Entry;

// nob_dir_entry_open() - open the directory entry for iteration.
// RETURN:
//   true  - Sucess.
//   false - Error. I will be logged automatically with nob_log().
NOBDEF bool nob_dir_entry_open(const char *dir_path, Nob_Dir_Entry *dir);
// nob_dir_entry_next() - acquire the next file in the directory.
// RETURN:
//   true - Successfully acquired the next file.
//   false - Either failure or no more files to iterate. In case of failure dir->error is set to true.
NOBDEF bool nob_dir_entry_next(Nob_Dir_Entry *dir);
NOBDEF void nob_dir_entry_close(Nob_Dir_Entry dir);

#define nob_return_defer(value) do { result = (value); goto defer; } while(0)

// Initial capacity of a dynamic array
#ifndef NOB_DA_INIT_CAP
#define NOB_DA_INIT_CAP 256
#endif

#ifdef __cplusplus
#define NOB_DECLTYPE_CAST(T) (decltype(T))
#else
#define NOB_DECLTYPE_CAST(T)
#endif // __cplusplus

#define nob_da_reserve(da, expected_capacity)                                              \
    do {                                                                                   \
        if ((expected_capacity) > (da)->capacity) {                                        \
            if ((da)->capacity == 0) {                                                     \
                (da)->capacity = NOB_DA_INIT_CAP;                                          \
            }                                                                              \
            while ((expected_capacity) > (da)->capacity) {                                 \
                (da)->capacity *= 2;                                                       \
            }                                                                              \
            (da)->items = NOB_DECLTYPE_CAST((da)->items)NOB_REALLOC((da)->items, (da)->capacity * sizeof(*(da)->items)); \
            NOB_ASSERT((da)->items != NULL && "Buy more RAM lol");                         \
        }                                                                                  \
    } while (0)

// Append an item to a dynamic array
#define nob_da_append(da, item)                \
    do {                                       \
        nob_da_reserve((da), (da)->count + 1); \
        (da)->items[(da)->count++] = (item);   \
    } while (0)

#define nob_da_free(da) NOB_FREE((da).items)

// Append several items to a dynamic array
#define nob_da_append_many(da, new_items, new_items_count)                                      \
    do {                                                                                        \
        nob_da_reserve((da), (da)->count + (new_items_count));                                  \
        memcpy((da)->items + (da)->count, (new_items), (new_items_count)*sizeof(*(da)->items)); \
        (da)->count += (new_items_count);                                                       \
    } while (0)

#define nob_da_resize(da, new_size)     \
    do {                                \
        nob_da_reserve((da), new_size); \
        (da)->count = (new_size);       \
    } while (0)

#define nob_da_pop(da) (da)->items[(NOB_ASSERT((da)->count > 0), --(da)->count)]
#define nob_da_first(da) (da)->items[(NOB_ASSERT((da)->count > 0), 0)]
#define nob_da_last(da) (da)->items[(NOB_ASSERT((da)->count > 0), (da)->count-1)]
#define nob_da_remove_unordered(da, i)               \
    do {                                             \
        size_t j = (i);                              \
        NOB_ASSERT(j < (da)->count);                 \
        (da)->items[j] = (da)->items[--(da)->count]; \
    } while(0)

// Foreach over Dynamic Arrays. Example:
// ```c
// typedef struct {
//     int *items;
//     size_t count;
//     size_t capacity;
// } Numbers;
//
// Numbers xs = {0};
//
// nob_da_append(&xs, 69);
// nob_da_append(&xs, 420);
// nob_da_append(&xs, 1337);
//
// nob_da_foreach(int, x, &xs) {
//     // `x` here is a pointer to the current element. You can get its index by taking a difference
//     // between `x` and the start of the array which is `x.items`.
//     size_t index = x - xs.items;
//     nob_log(INFO, "%zu: %d", index, *x);
// }
// ```
#define nob_da_foreach(Type, it, da) for (Type *it = (da)->items; it < (da)->items + (da)->count; ++it)

// The Fixed Array append. `items` fields must be a fixed size array. Its size determines the capacity.
#define nob_fa_append(fa, item) \
    (NOB_ASSERT((fa)->count < NOB_ARRAY_LEN((fa)->items)), \
     (fa)->items[(fa)->count++] = (item))

typedef struct {
    char *items;
    size_t count;
    size_t capacity;
} Nob_String_Builder;

#define nob_swap(T, a, b) do { T t = a; a = b; b = t; } while (0)

NOBDEF bool nob_read_entire_file(const char *path, Nob_String_Builder *sb);
NOBDEF int nob_sb_appendf(Nob_String_Builder *sb, const char *fmt, ...) NOB_PRINTF_FORMAT(2, 3);
// Pads the String_Builder (sb) to the desired word size boundary with 0s.
// Imagine we have sb that contains 5 `a`-s:
//
//   aaaa|a
//
// If we pad align it by size 4 it will look like this:
//
//   aaaa|a000| <- padded with 0s to the next size 4 boundary
//
// Useful when you are building some sort of binary format using String_Builder.
NOBDEF void nob_sb_pad_align(Nob_String_Builder *sb, size_t size);

// Append a sized buffer to a string builder
#define nob_sb_append_buf(sb, buf, size) nob_da_append_many(sb, buf, size)

// Append a string view to a string builder
#define nob_sb_append_sv(sb, sv) nob_sb_append_buf((sb), (sv).data, (sv).count)

// Append a NULL-terminated string to a string builder
#define nob_sb_append_cstr(sb, cstr)  \
    do {                              \
        const char *s = (cstr);       \
        size_t n = strlen(s);         \
        nob_da_append_many(sb, s, n); \
    } while (0)

// Append a single NULL character at the end of a string builder. So then you can
// use it a NULL-terminated C string
#define nob_sb_append_null(sb) nob_da_append_many(sb, "", 1)

#define nob_sb_append nob_da_append

// Free the memory allocated by a string builder
#define nob_sb_free(sb) NOB_FREE((sb).items)

// Process handle
#ifdef _WIN32
typedef HANDLE Nob_Proc;
#define NOB_INVALID_PROC INVALID_HANDLE_VALUE
typedef HANDLE Nob_Fd;
#define NOB_INVALID_FD INVALID_HANDLE_VALUE
#else
typedef int Nob_Proc;
#define NOB_INVALID_PROC (-1)
typedef int Nob_Fd;
#define NOB_INVALID_FD (-1)
#endif // _WIN32

NOBDEF Nob_Fd nob_fd_open_for_read(const char *path);
NOBDEF Nob_Fd nob_fd_open_for_write(const char *path);
NOBDEF void nob_fd_close(Nob_Fd fd);

typedef struct {
    Nob_Fd read;
    Nob_Fd write;
} Nob_Pipe;

NOBDEF bool nob_pipe_create(Nob_Pipe *pp);

typedef struct {
    Nob_Proc *items;
    size_t count;
    size_t capacity;
} Nob_Procs;

// Wait until the process has finished
NOBDEF bool nob_proc_wait(Nob_Proc proc);

// Wait until all the processes have finished
NOBDEF bool nob_procs_wait(Nob_Procs procs);

// Wait until all the processes have finished and empty the procs array.
NOBDEF bool nob_procs_flush(Nob_Procs *procs);

// Alias to nob_procs_flush
NOB_DEPRECATED("Use `nob_procs_flush(&procs)` instead.")
NOBDEF bool nob_procs_wait_and_reset(Nob_Procs *procs);

// Append a new process to procs array and if procs.count reaches max_procs_count call nob_procs_wait_and_reset() on it
NOB_DEPRECATED("Use `nob_cmd_run(&cmd, .async = &procs, .max_procs = <integer>)` instead")
NOBDEF bool nob_procs_append_with_flush(Nob_Procs *procs, Nob_Proc proc, size_t max_procs_count);

// A command - the main workhorse of Nob. Nob is all about building commands and running them
typedef struct {
    const char **items;
    size_t count;
    size_t capacity;
} Nob_Cmd;

// Options for nob_cmd_run_opt() function.
typedef struct {
    // Run the command asynchronously appending its Nob_Proc to the provided Nob_Procs array
    Nob_Procs *async;
    // Maximum processes allowed in the .async list. Zero implies nob_nprocs().
    size_t max_procs;
    // Do not reset the command after execution.
    bool dont_reset;
    // Redirect stdin to file
    const char *stdin_path;
    // Redirect stdout to file
    const char *stdout_path;
    // Redirect stderr to file
    const char *stderr_path;
} Nob_Cmd_Opt;

// Run the command with options.
NOBDEF bool nob_cmd_run_opt(Nob_Cmd *cmd, Nob_Cmd_Opt opt);
NOBDEF bool nob__cmd_run_opt_with_location(Nob_Cmd *cmd, const char *file, int line, Nob_Cmd_Opt opt);

// Command Chains (in Shell Scripting they are know as Pipes)
//
// Usage:
// ```c
// Nob_Cmd cmd = {0};
// Nob_Chain chain = {0};
// if (!nob_chain_begin(&chain)) return 1;
// {
//     nob_cmd_append(&cmd, "echo", "Hello, World");
//     if (!nob_chain_cmd(&chain, &cmd)) return 1;
//
//     nob_cmd_append(&cmd, "rev");
//     if (!nob_chain_cmd(&chain, &cmd)) return 1;
//
//     nob_cmd_append(&cmd, "xxd");
//     if (!nob_chain_cmd(&chain, &cmd)) return 1;
// }
// if (!nob_chain_end(&chain)) return 1;
// ```
//
// The above is equivalent to a shell command:
//
// ```sh
// echo "Hello, World" | rev | xxd
// ```
//
// After nob_chain_end() the Nob_Chain struct can be reused again.
//
// The fields of the Nob_Chain struct contain the intermediate state of the Command
// Chain that is being built with the nob_chain_cmd() calls and generally have no
// particular use for the user.
//
// The only memory dynamically allocated within Nob_Chain belongs to the .cmd field.
// So if you want to clean it all up you can just do free(chain.cmd.items).
typedef struct {
    // The file descriptor of the output of the previous command. Will be used as the input for the next command.
    Nob_Fd fdin;
    // The command from the last nob_chain_cmd() call.
    Nob_Cmd cmd;
    // The value of the optional .err2out parameter from the last nob_chain_cmd() call.
    bool err2out;
} Nob_Chain;

typedef struct {
    const char *stdin_path;
} Nob_Chain_Begin_Opt;
#define nob_chain_begin(chain, ...) nob_chain_begin_opt((chain), NOB_CLIT(Nob_Chain_Begin_Opt) { __VA_ARGS__ })
NOBDEF bool nob_chain_begin_opt(Nob_Chain *chain, Nob_Chain_Begin_Opt opt);

typedef struct {
    bool err2out;
    bool dont_reset;
} Nob_Chain_Cmd_Opt;
#define nob_chain_cmd(chain, cmd, ...) nob_chain_cmd_opt((chain), (cmd), NOB_CLIT(Nob_Chain_Cmd_Opt) { __VA_ARGS__ })
NOBDEF bool nob_chain_cmd_opt(Nob_Chain *chain, Nob_Cmd *cmd, Nob_Chain_Cmd_Opt opt);

typedef struct {
    Nob_Procs *async;
    size_t max_procs;
    const char *stdout_path;
    const char *stderr_path;
} Nob_Chain_End_Opt;
#define nob_chain_end(chain, ...) nob_chain_end_opt((chain), NOB_CLIT(Nob_Chain_End_Opt) { __VA_ARGS__ })
NOBDEF bool nob_chain_end_opt(Nob_Chain *chain, Nob_Chain_End_Opt opt);

// Get amount of processors on the machine.
NOBDEF int nob_nprocs(void);

#define NOB_NANOS_PER_SEC (1000*1000*1000)

// The maximum time span representable is 584 years.
NOBDEF uint64_t nob_nanos_since_unspecified_epoch(void);

// Same as nob_cmd_run_opt but using cool variadic macro to set the default options.
// See https://x.com/vkrajacic/status/1749816169736073295 for more info on how to use such macros.
#define nob_cmd_run(cmd, ...) nob__cmd_run_opt_with_location((cmd), __FILE__, __LINE__, NOB_CLIT(Nob_Cmd_Opt){ __VA_ARGS__ })

// DEPRECATED:
//
// You were suppose to use this structure like this:
//
// ```c
// Nob_Fd fdin = nob_fd_open_for_read("input.txt");
// if (fdin == NOB_INVALID_FD) fail();
// Nob_Fd fdout = nob_fd_open_for_write("output.txt");
// if (fdout == NOB_INVALID_FD) fail();
// Nob_Cmd cmd = {0};
// nob_cmd_append(&cmd, "cat");
// if (!nob_cmd_run_sync_redirect_and_reset(&cmd, (Nob_Cmd_Redirect) {
//     .fdin = &fdin,
//     .fdout = &fdout
// })) fail();
// ```
//
// But these days you should do:
//
// ```c
// Nob_Cmd cmd = {0};
// nob_cmd_append(&cmd, "cat");
// if (!nob_cmd_run(&cmd, .stdin_path = "input.txt", .stdout_path = "output.txt")) fail();
// ```
typedef struct {
    Nob_Fd *fdin;
    Nob_Fd *fdout;
    Nob_Fd *fderr;
} Nob_Cmd_Redirect;

// Render a string representation of a command into a string builder. Keep in mind the the
// string builder is not NULL-terminated by default. Use nob_sb_append_null if you plan to
// use it as a C string.
NOBDEF void nob_cmd_render(Nob_Cmd cmd, Nob_String_Builder *render);

// Compound Literal
#if defined(__cplusplus)
    #define NOB_CLIT(type) type
#else
    #define NOB_CLIT(type) (type)
#endif

NOBDEF void nob__cmd_append(Nob_Cmd *cmd, size_t n, const char **args);
#if defined(__cplusplus)
    template <typename... Args>
    static inline void nob__cpp_cmd_append_wrapper(Nob_Cmd *cmd, Args... strs)
    {
        const char* args[] = { strs... };
        nob__cmd_append(cmd, sizeof(args)/sizeof(args[0]), args);
    }
    #define nob_cmd_append(cmd, ...) nob__cpp_cmd_append_wrapper(cmd, __VA_ARGS__)
#else
    #define nob_cmd_append(cmd, ...) \
        nob__cmd_append(cmd, sizeof((const char*[]){__VA_ARGS__})/sizeof(const char*), (const char*[]){__VA_ARGS__})
#endif // __cplusplus

// TODO: nob_cmd_extend() evaluates other_cmd twice
// It can be fixed by turning nob_cmd_extend() call into a statement.
// But that may break backward compatibility of the API.
#define nob_cmd_extend(cmd, other_cmd) \
    nob_da_append_many(cmd, (other_cmd)->items, (other_cmd)->count)

// Free all the memory allocated by command arguments
#define nob_cmd_free(cmd) NOB_FREE(cmd.items)

// Run command asynchronously
NOB_DEPRECATED("Use `nob_cmd_run(&cmd, .async = &procs, .dont_reset = true)`.")
NOBDEF Nob_Proc nob_cmd_run_async(Nob_Cmd cmd);

// nob_cmd_run_async_and_reset() is just like nob_cmd_run_async() except it also resets cmd.count to 0
// so the Nob_Cmd instance can be seamlessly used several times in a row
NOB_DEPRECATED("Use `nob_cmd_run(&cmd, .async = &procs)` intead.")
NOBDEF Nob_Proc nob_cmd_run_async_and_reset(Nob_Cmd *cmd);

// Run redirected command asynchronously
NOB_DEPRECATED("Use `nob_cmd_run(&cmd, "
               ".async = &procs, "
               ".stdin_path = \"path/to/stdin\", "
               ".stdout_path = \"path/to/stdout\", "
               ".stderr_path = \"path/to/stderr\", "
               ".dont_reset = true"
               ")` instead.")
NOBDEF Nob_Proc nob_cmd_run_async_redirect(Nob_Cmd cmd, Nob_Cmd_Redirect redirect);

// Run redirected command asynchronously and set cmd.count to 0 and close all the opened files
NOB_DEPRECATED("Use `nob_cmd_run(&cmd, "
               ".async = &procs, "
               ".stdin_path = \"path/to/stdin\", "
               ".stdout_path = \"path/to/stdout\", "
               ".stderr_path = \"path/to/stderr\")` instead.")
NOBDEF Nob_Proc nob_cmd_run_async_redirect_and_reset(Nob_Cmd *cmd, Nob_Cmd_Redirect redirect);

// Run command synchronously
NOB_DEPRECATED("Use `nob_cmd_run(&cmd, .dont_reset = true)` instead.")
NOBDEF bool nob_cmd_run_sync(Nob_Cmd cmd);

// NOTE: nob_cmd_run_sync_and_reset() is just like nob_cmd_run_sync() except it also resets cmd.count to 0
// so the Nob_Cmd instance can be seamlessly used several times in a row
NOB_DEPRECATED("Use `nob_cmd_run(&cmd)` instead.")
NOBDEF bool nob_cmd_run_sync_and_reset(Nob_Cmd *cmd);

// Run redirected command synchronously
NOB_DEPRECATED("Use `nob_cmd_run(&cmd, "
               ".stdin_path  = \"path/to/stdin\", "
               ".stdout_path = \"path/to/stdout\", "
               ".stderr_path = \"path/to/stderr\", "
               ".dont_reset = true"
               ")` instead.")
NOBDEF bool nob_cmd_run_sync_redirect(Nob_Cmd cmd, Nob_Cmd_Redirect redirect);

// Run redirected command synchronously and set cmd.count to 0 and close all the opened files
NOB_DEPRECATED("Use `nob_cmd_run(&cmd, "
               ".stdin_path = \"path/to/stdin\", "
               ".stdout_path = \"path/to/stdout\", "
               ".stderr_path = \"path/to/stderr\")` instead.")
NOBDEF bool nob_cmd_run_sync_redirect_and_reset(Nob_Cmd *cmd, Nob_Cmd_Redirect redirect);

#ifndef NOB_TEMP_CAPACITY
#define NOB_TEMP_CAPACITY (8*1024*1024)
#endif // NOB_TEMP_CAPACITY
NOBDEF char *nob_temp_strdup(const char *cstr);
NOBDEF char *nob_temp_strndup(const char *cstr, size_t size);
NOBDEF void *nob_temp_alloc(size_t size);
NOBDEF char *nob_temp_sprintf(const char *format, ...) NOB_PRINTF_FORMAT(1, 2);
NOBDEF char *nob_temp_vsprintf(const char *format, va_list ap);
// nob_temp_reset() - Resets the entire temporary storage to 0.
//
// It is generally not recommended to call this function ever. What you usually want to do is let's say you have a loop,
// that allocates some temporary objects and cleans them up at the end of each iteration. You should use
// nob_temp_save() and nob_temp_rewind() to organize such loop like this:
//
// ```c
// char *message = nob_temp_sprintf("This message is still valid after the loop below");
// while (!quit) {
//     size_t mark = nob_temp_save();
//     nob_temp_alloc(69);
//     nob_temp_alloc(420);
//     nob_temp_alloc(1337);
//     nob_temp_rewind(mark);
// }
// printf("%s\n", message);
// ```
//
// That way all the temporary allocations created before the loop are still valid even after the loop.
// Such save/rewind blocks define lifetime boundaries of the temporary objects which also could be nested.
// This turns the temporary storage into kind of a second stack with a more manual management.
NOBDEF void nob_temp_reset(void);
NOBDEF size_t nob_temp_save(void);
NOBDEF void nob_temp_rewind(size_t checkpoint);

// Given any path returns the last part of that path.
// "/path/to/a/file.c" -> "file.c"; "/path/to/a/directory" -> "directory"
NOBDEF const char *nob_path_name(const char *path);
NOBDEF bool nob_rename(const char *old_path, const char *new_path);
NOBDEF int nob_needs_rebuild(const char *output_path, const char **input_paths, size_t input_paths_count);
NOBDEF int nob_needs_rebuild1(const char *output_path, const char *input_path);
NOBDEF int nob_file_exists(const char *file_path);
NOBDEF const char *nob_get_current_dir_temp(void);
NOBDEF bool nob_set_current_dir(const char *path);
// Returns you the directory part of the path allocated on the temporary storage.
NOBDEF char *nob_temp_dir_name(const char *path);
NOBDEF char *nob_temp_file_name(const char *path);
NOBDEF char *nob_temp_file_ext(const char *path);
NOBDEF char *nob_temp_running_executable_path(void);

// TODO: we should probably document somewhere all the compilers we support

// The nob_cc_* macros try to abstract away the specific compiler.
// They are very basic and not particularly flexible, but you can redefine them if you need to
// or not use them at all and create your own abstraction on top of Nob_Cmd.

#ifndef nob_cc
#  if _WIN32
#    if defined(__clang__)
#       define nob_cc(cmd) nob_cmd_append(cmd, "clang")
#    elif defined(__GNUC__)
#       define nob_cc(cmd) nob_cmd_append(cmd, "cc")
#    elif defined(_MSC_VER)
#       define nob_cc(cmd) nob_cmd_append(cmd, "cl.exe")
#    elif defined(__TINYC__)
#       define nob_cc(cmd) nob_cmd_append(cmd, "tcc")
#    endif
#  else
#    define nob_cc(cmd) nob_cmd_append(cmd, "cc")
#  endif
#endif // nob_cc

#ifndef nob_cc_flags
#  if defined(_MSC_VER) && !defined(__clang__)
#    define nob_cc_flags(cmd) nob_cmd_append(cmd, "/W4", "/nologo", "/D_CRT_SECURE_NO_WARNINGS")
#  else
#    define nob_cc_flags(cmd) nob_cmd_append(cmd, "-Wall", "-Wextra")
#  endif
#endif // nob_cc_flags

#ifndef nob_cc_output
#  if defined(_MSC_VER) && !defined(__clang__)
#    define nob_cc_output(cmd, output_path) nob_cmd_append(cmd, nob_temp_sprintf("/Fe:%s", (output_path)), nob_temp_sprintf("/Fo:%s", (output_path)))
#  else
#    define nob_cc_output(cmd, output_path) nob_cmd_append(cmd, "-o", (output_path))
#  endif
#endif // nob_cc_output

#ifndef nob_cc_inputs
#  define nob_cc_inputs(cmd, ...) nob_cmd_append(cmd, __VA_ARGS__)
#endif // nob_cc_inputs

// TODO: add MinGW support for Go Rebuild Urself™ Technology and all the nob_cc_* macros above
//   Musializer contributors came up with a pretty interesting idea of an optional prefix macro which could be useful for
//   MinGW support:
//   https://github.com/tsoding/musializer/blob/b7578cc76b9ecb573d239acc9ccf5a04d3aba2c9/src_build/nob_win64_mingw.c#L3-L9
// TODO: Maybe instead NOB_REBUILD_URSELF macro, the Go Rebuild Urself™ Technology should use the
//   user defined nob_cc_* macros instead?
#ifndef NOB_REBUILD_URSELF
#  if defined(_WIN32)
#    if defined(__clang__)
#      if defined(__cplusplus)
#        define NOB_REBUILD_URSELF(binary_path, source_path) "clang", "-x", "c++", "-o", binary_path, source_path
#      else
#        define NOB_REBUILD_URSELF(binary_path, source_path) "clang", "-x", "c", "-o", binary_path, source_path
#      endif
#    elif defined(__GNUC__)
#      if defined(__cplusplus)
#        define NOB_REBUILD_URSELF(binary_path, source_path) "gcc", "-x", "c++", "-o", binary_path, source_path
#      else
#        define NOB_REBUILD_URSELF(binary_path, source_path) "gcc", "-x", "c", "-o", binary_path, source_path
#      endif
#    elif defined(_MSC_VER)
#       define NOB_REBUILD_URSELF(binary_path, source_path) "cl.exe", nob_temp_sprintf("/Fe:%s", (binary_path)), source_path
#    elif defined(__TINYC__)
#       define NOB_REBUILD_URSELF(binary_path, source_path) "tcc", "-o", binary_path, source_path
#    endif
#  else
#    if defined(__cplusplus)
#      define NOB_REBUILD_URSELF(binary_path, source_path) "cc", "-x", "c++", "-o", binary_path, source_path
#    else
#      define NOB_REBUILD_URSELF(binary_path, source_path) "cc", "-x", "c", "-o", binary_path, source_path
#    endif
#  endif
#endif

// Go Rebuild Urself™ Technology
//
//   How to use it:
//     int main(int argc, char** argv) {
//         NOB_GO_REBUILD_URSELF(argc, argv);
//         // actual work
//         return 0;
//     }
//
//   After you added this macro every time you run ./nob it will detect
//   that you modified its original source code and will try to rebuild itself
//   before doing any actual work. So you only need to bootstrap your build system
//   once.
//
//   The modification is detected by comparing the last modified times of the executable
//   and its source code. The same way the make utility usually does it.
//
//   The rebuilding is done by using the NOB_REBUILD_URSELF macro which you can redefine
//   if you need a special way of bootstraping your build system. (which I personally
//   do not recommend since the whole idea of NoBuild is to keep the process of bootstrapping
//   as simple as possible and doing all of the actual work inside of ./nob)
//
NOBDEF void nob__go_rebuild_urself(int argc, char **argv, const char *source_path, ...);
#define NOB_GO_REBUILD_URSELF(argc, argv) nob__go_rebuild_urself(argc, argv, __FILE__, NULL)
// Sometimes your nob.c includes additional files, so you want the Go Rebuild Urself™ Technology to check
// if they also were modified and rebuild nob.c accordingly. For that we have NOB_GO_REBUILD_URSELF_PLUS():
// ```c
// #define NOB_IMPLEMENTATION
// #include "nob.h"
//
// #include "foo.c"
// #include "bar.c"
//
// int main(int argc, char **argv)
// {
//     NOB_GO_REBUILD_URSELF_PLUS(argc, argv, "foo.c", "bar.c");
//     // ...
//     return 0;
// }
#define NOB_GO_REBUILD_URSELF_PLUS(argc, argv, ...) nob__go_rebuild_urself(argc, argv, __FILE__, __VA_ARGS__, NULL);

typedef struct {
    size_t count;
    union {
        const char *data;
        const char *items;
    };
} Nob_String_View;

NOBDEF const char *nob_temp_sv_to_cstr(Nob_String_View sv);

NOBDEF Nob_String_View nob_sv_chop_while(Nob_String_View *sv, int (*p)(int x));
NOBDEF Nob_String_View nob_sv_chop_by_delim(Nob_String_View *sv, char delim);
NOBDEF Nob_String_View nob_sv_chop_left(Nob_String_View *sv, size_t n);
NOBDEF Nob_String_View nob_sv_chop_right(Nob_String_View *sv, size_t n);
// If `sv` starts with `prefix` chops off the prefix and returns true.
// Otherwise, leaves `sv` unmodified and returns false.
NOBDEF bool nob_sv_chop_prefix(Nob_String_View *sv, Nob_String_View prefix);
// If `sv` ends with `suffix` chops off the suffix and returns true.
// Otherwise, leaves `sv` unmodified and returns false.
NOBDEF bool nob_sv_chop_suffix(Nob_String_View *sv, Nob_String_View suffix);
NOBDEF Nob_String_View nob_sv_trim(Nob_String_View sv);
NOBDEF Nob_String_View nob_sv_trim_left(Nob_String_View sv);
NOBDEF Nob_String_View nob_sv_trim_right(Nob_String_View sv);
NOBDEF bool nob_sv_eq(Nob_String_View a, Nob_String_View b);
NOB_DEPRECATED("Use nob_sv_ends_with_cstr(sv, suffix) instead. "
               "Pay attention to the `s` at the end of the `end`. "
               "The reason this function was deprecated is because "
               "of the typo in the name, of course, but also "
               "because the second argument was a NULL-terminated string "
               "while nob_sv_starts_with() accepted Nob_String_View as the "
               "prefix which created an inconsistency in the API.")
NOBDEF bool nob_sv_end_with(Nob_String_View sv, const char *cstr);
NOBDEF bool nob_sv_ends_with_cstr(Nob_String_View sv, const char *cstr);
NOBDEF bool nob_sv_ends_with(Nob_String_View sv, Nob_String_View suffix);
NOBDEF bool nob_sv_starts_with(Nob_String_View sv, Nob_String_View prefix);
NOBDEF Nob_String_View nob_sv_from_cstr(const char *cstr);
NOBDEF Nob_String_View nob_sv_from_parts(const char *data, size_t count);
// nob_sb_to_sv() enables you to just view Nob_String_Builder as Nob_String_View
#define nob_sb_to_sv(sb) nob_sv_from_parts((sb).items, (sb).count)

#define NOB_SVLIT(lit) (NOB_CLIT(Nob_String_View){.count = sizeof(lit)-1, .data = (lit)})
// Generally majority of the C/C++ compilers will allow you to use NOB_SVLIT to construct global variables.
// But there are some (specifically MSVC with /TC flag enabled) that will refuse.
// For such compilers use NOB_SVLIT_STATIC instead.
#define NOB_SVLIT_STATIC(lit) {.count = sizeof(lit)-1, .data = (lit)}

// printf macros for String_View
#ifndef SV_Fmt
#define SV_Fmt "%.*s"
#endif // SV_Fmt
#ifndef SV_Arg
#define SV_Arg(sv) (int) (sv).count, (sv).data
#endif // SV_Arg
// USAGE:
//   String_View name = ...;
//   printf("Name: "SV_Fmt"\n", SV_Arg(name));

// Stolen from Jai's Unicode module
// Example:
// ```c
// String_View sv = SVLIT("Привет, Мир!");
// while (sv.count > 0) {
//     size_t n = nob_bytes_for_utf8[(uint8_t)*sv.data];
//     String_View c = sv_chop_left(&sv, n);
//     printf(SV_Fmt" => %zu\n", SV_Arg(c), n);
// }
// ```
static const uint8_t nob_bytes_for_utf8[] = {
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3, 4,4,4,4,4,4,4,4,5,5,5,5,6,6,6,6,
};

NOBDEF size_t nob_sv_utf8_len(Nob_String_View sv, size_t *bytes_overrun);

#ifdef _WIN32

NOBDEF char *nob_win32_error_message(DWORD err);

#endif // _WIN32

#endif // NOB_H_

#ifdef NOB_IMPLEMENTATION

// This is like nob_proc_wait() but waits asynchronously. Depending on the platform ms means different thing.
// On Windows it means timeout. On POSIX it means for how long to sleep after checking if the process exited,
// so to not peg the core too much. Since this API is kinda of weird, the function is private for now.
static int nob__proc_wait_async(Nob_Proc proc, int ms);

// Starts the process for the command. Its main purpose is to be the base for nob_cmd_run() and nob_cmd_run_opt().
static Nob_Proc nob__cmd_start_process(Nob_Cmd cmd, Nob_Fd *fdin, Nob_Fd *fdout, Nob_Fd *fderr);

// Any messages with the level below nob_minimal_log_level are going to be suppressed.
Nob_Log_Level nob_minimal_log_level = NOB_INFO;

NOBDEF void nob__cmd_append(Nob_Cmd *cmd, size_t n, const char **args)
{
    for (size_t i = 0; i < n; ++i) {
        nob_da_append(cmd, args[i]);
    }
}

NOBDEF size_t nob_sv_utf8_len(Nob_String_View sv, size_t *bytes_overrun)
{
    size_t i = 0;
    size_t n = 0;
    while (true) {
        if (i >= sv.count) {
            if (bytes_overrun) *bytes_overrun = i - sv.count;
            return n;
        }
        i += nob_bytes_for_utf8[(uint8_t)sv.data[i]];
        n += 1;
    }
    NOB_UNREACHABLE("sv_utf8_len");
    return 0;
}

#ifdef _WIN32

// Base on https://stackoverflow.com/a/75644008
// > .NET Core uses 4096 * sizeof(WCHAR) buffer on stack for FormatMessageW call. And...thats it.
// >
// > https://github.com/dotnet/runtime/blob/3b63eb1346f1ddbc921374a5108d025662fb5ffd/src/coreclr/utilcode/posterror.cpp#L264-L265
#ifndef NOB_WIN32_ERR_MSG_SIZE
#define NOB_WIN32_ERR_MSG_SIZE (4 * 1024)
#endif // NOB_WIN32_ERR_MSG_SIZE

NOBDEF char *nob_win32_error_message(DWORD err) {
    static char win32ErrMsg[NOB_WIN32_ERR_MSG_SIZE] = {0};
    DWORD errMsgSize = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, LANG_USER_DEFAULT, win32ErrMsg,
                                      NOB_WIN32_ERR_MSG_SIZE, NULL);

    if (errMsgSize == 0) {
        if (GetLastError() != ERROR_MR_MID_NOT_FOUND) {
            if (sprintf(win32ErrMsg, "Could not get error message for 0x%lX", err) > 0) {
                return (char *)&win32ErrMsg;
            } else {
                return NULL;
            }
        } else {
            if (sprintf(win32ErrMsg, "Invalid Windows Error code (0x%lX)", err) > 0) {
                return (char *)&win32ErrMsg;
            } else {
                return NULL;
            }
        }
    }

    while (errMsgSize > 1 && isspace(win32ErrMsg[errMsgSize - 1])) {
        win32ErrMsg[--errMsgSize] = '\0';
    }

    return win32ErrMsg;
}

#endif // _WIN32

NOBDEF void nob__panicf(const char *file, int line, const char *label, const char *format, ...)
{
    fprintf(stderr, "%s:%d: %s: ", file, line, label);
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
    abort();
}

// The implementation idea is stolen from https://github.com/zhiayang/nabs
NOBDEF void nob__go_rebuild_urself(int argc, char **argv, const char *source_path, ...)
{
    const char *binary_path = nob_shift(argv, argc);
#ifdef _WIN32
    // On Windows executables almost always invoked without extension, so
    // it's ./nob, not ./nob.exe. For renaming the extension is a must.
    if (!nob_sv_ends_with_cstr(nob_sv_from_cstr(binary_path), ".exe")) {
        binary_path = nob_temp_sprintf("%s.exe", binary_path);
    }
#endif

    Nob_File_Paths source_paths = {0};
    nob_da_append(&source_paths, source_path);
    va_list args;
    va_start(args, source_path);
    for (;;) {
        const char *path = va_arg(args, const char*);
        if (path == NULL) break;
        nob_da_append(&source_paths, path);
    }
    va_end(args);

    int rebuild_is_needed = nob_needs_rebuild(binary_path, source_paths.items, source_paths.count);
    if (rebuild_is_needed < 0) exit(1); // error
    if (!rebuild_is_needed) {           // no rebuild is needed
        NOB_FREE(source_paths.items);
        return;
    }

    Nob_Cmd cmd = {0};

    const char *old_binary_path = nob_temp_sprintf("%s.old", binary_path);

    if (!nob_rename(binary_path, old_binary_path)) exit(1);
    nob_cmd_append(&cmd, NOB_REBUILD_URSELF(binary_path, source_path));
    Nob_Cmd_Opt opt = {0};
    if (!nob_cmd_run_opt(&cmd, opt)) {
        nob_rename(old_binary_path, binary_path);
        exit(1);
    }
#ifdef NOB_EXPERIMENTAL_DELETE_OLD
    // TODO: this is an experimental behavior behind a compilation flag.
    // Once it is confirmed that it does not cause much problems on both POSIX and Windows
    // we may turn it on by default.
    nob_delete_file(old_binary_path);
#endif // NOB_EXPERIMENTAL_DELETE_OLD

    nob_cmd_append(&cmd, binary_path);
    nob_da_append_many(&cmd, argv, argc);
    if (!nob_cmd_run_opt(&cmd, opt)) exit(1);
    exit(0);
}

static size_t nob_temp_size = 0;
static char nob_temp[NOB_TEMP_CAPACITY] = {0};

NOBDEF bool nob_mkdir_if_not_exists(const char *path)
{
#ifdef _WIN32
    int result = _mkdir(path);
#else
    int result = mkdir(path, 0755);
#endif
    if (result < 0) {
        if (errno == EEXIST) {
#ifndef NOB_NO_ECHO
            nob_log(NOB_INFO, "directory `%s` already exists", path);
#endif // NOB_NO_ECHO
            return true;
        }
        nob_log(NOB_ERROR, "could not create directory `%s`: %s", path, strerror(errno));
        return false;
    }

#ifndef NOB_NO_ECHO
    nob_log(NOB_INFO, "created directory `%s`", path);
#endif // NOB_NO_ECHO
    return true;
}

NOBDEF bool nob_copy_file(const char *src_path, const char *dst_path)
{
#ifndef NOB_NO_ECHO
    nob_log(NOB_INFO, "copying %s -> %s", src_path, dst_path);
#endif // NOB_NO_ECHO
#ifdef _WIN32
    if (!CopyFile(src_path, dst_path, FALSE)) {
        nob_log(NOB_ERROR, "Could not copy file: %s", nob_win32_error_message(GetLastError()));
        return false;
    }
    return true;
#else
    int src_fd = -1;
    int dst_fd = -1;
    size_t buf_size = 32*1024;
    char *buf = (char*)NOB_REALLOC(NULL, buf_size);
    NOB_ASSERT(buf != NULL && "Buy more RAM lol!!");
    bool result = true;

    src_fd = open(src_path, O_RDONLY);
    if (src_fd < 0) {
        nob_log(NOB_ERROR, "Could not open file %s: %s", src_path, strerror(errno));
        nob_return_defer(false);
    }

    struct stat src_stat;
    if (fstat(src_fd, &src_stat) < 0) {
        nob_log(NOB_ERROR, "Could not get mode of file %s: %s", src_path, strerror(errno));
        nob_return_defer(false);
    }

    dst_fd = open(dst_path, O_CREAT | O_TRUNC | O_WRONLY, src_stat.st_mode);
    if (dst_fd < 0) {
        nob_log(NOB_ERROR, "Could not create file %s: %s", dst_path, strerror(errno));
        nob_return_defer(false);
    }

    for (;;) {
        ssize_t n = read(src_fd, buf, buf_size);
        if (n == 0) break;
        if (n < 0) {
            nob_log(NOB_ERROR, "Could not read from file %s: %s", src_path, strerror(errno));
            nob_return_defer(false);
        }
        char *buf2 = buf;
        while (n > 0) {
            ssize_t m = write(dst_fd, buf2, n);
            if (m < 0) {
                nob_log(NOB_ERROR, "Could not write to file %s: %s", dst_path, strerror(errno));
                nob_return_defer(false);
            }
            n    -= m;
            buf2 += m;
        }
    }

defer:
    NOB_FREE(buf);
    close(src_fd);
    close(dst_fd);
    return result;
#endif
}

NOBDEF void nob_cmd_render(Nob_Cmd cmd, Nob_String_Builder *render)
{
    for (size_t i = 0; i < cmd.count; ++i) {
        const char *arg = cmd.items[i];
        if (arg == NULL) break;
        if (i > 0) nob_sb_append_cstr(render, " ");
        if (!strchr(arg, ' ')) {
            nob_sb_append_cstr(render, arg);
        } else {
            nob_da_append(render, '\'');
            nob_sb_append_cstr(render, arg);
            nob_da_append(render, '\'');
        }
    }
}

#ifdef _WIN32
// https://learn.microsoft.com/en-gb/archive/blogs/twistylittlepassagesallalike/everyone-quotes-command-line-arguments-the-wrong-way
static void nob__win32_cmd_quote(Nob_Cmd cmd, Nob_String_Builder *quoted)
{
    for (size_t i = 0; i < cmd.count; ++i) {
        const char *arg = cmd.items[i];
        if (arg == NULL) break;
        size_t len = strlen(arg);
        if (i > 0) nob_da_append(quoted, ' ');
        if (len != 0 && NULL == strpbrk(arg, " \t\n\v\"")) {
            // no need to quote
            nob_da_append_many(quoted, arg, len);
        } else {
            // we need to escape:
            // 1. double quotes in the original arg
            // 2. consequent backslashes before a double quote
            size_t backslashes = 0;
            nob_da_append(quoted, '\"');
            for (size_t j = 0; j < len; ++j) {
                char x = arg[j];
                if (x == '\\') {
                    backslashes += 1;
                } else {
                    if (x == '\"') {
                        // escape backslashes (if any) and the double quote
                        for (size_t k = 0; k < 1+backslashes; ++k) {
                            nob_da_append(quoted, '\\');
                        }
                    }
                    backslashes = 0;
                }
                nob_da_append(quoted, x);
            }
            // escape backslashes (if any)
            for (size_t k = 0; k < backslashes; ++k) {
                nob_da_append(quoted, '\\');
            }
            nob_da_append(quoted, '\"');
        }
    }
}
#endif

NOBDEF int nob_nprocs(void)
{
#ifdef _WIN32
    SYSTEM_INFO siSysInfo;
    GetSystemInfo(&siSysInfo);
    return siSysInfo.dwNumberOfProcessors;
#else
    return sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

NOBDEF bool nob__cmd_run_opt_with_location(Nob_Cmd *cmd, const char *file, int line, Nob_Cmd_Opt opt)
{
    bool ok = nob_cmd_run_opt(cmd, opt);
#ifdef NOB_EXPERIMENTAL_TRACE_CMD_RUN_FAIL
    if (!ok) {
        nob_log(NOB_ERROR, "%s:%d: ERROR: cmd_run failed", file, line);
    }
#else
    NOB_UNUSED(file);
    NOB_UNUSED(line);
#endif // NOB_EXPERIMENTAL_TRACE_CMD_RUN_FAIL
    return ok;
}

NOBDEF bool nob_cmd_run_opt(Nob_Cmd *cmd, Nob_Cmd_Opt opt)
{
    bool result = true;
    Nob_Fd fdin  = NOB_INVALID_FD;
    Nob_Fd fdout = NOB_INVALID_FD;
    Nob_Fd fderr = NOB_INVALID_FD;
    Nob_Fd *opt_fdin  = NULL;
    Nob_Fd *opt_fdout = NULL;
    Nob_Fd *opt_fderr = NULL;
    Nob_Proc proc = NOB_INVALID_PROC;

    size_t max_procs = opt.max_procs > 0 ? opt.max_procs : (size_t) nob_nprocs() + 1;

    if (opt.async && max_procs > 0) {
        while (opt.async->count >= max_procs) {
            for (size_t i = 0; i < opt.async->count; ++i) {
                int ret = nob__proc_wait_async(opt.async->items[i], 1);
                if (ret < 0) nob_return_defer(false);
                if (ret) {
                    nob_da_remove_unordered(opt.async, i);
                    break;
                }
            }
        }
    }

    if (opt.stdin_path) {
        fdin = nob_fd_open_for_read(opt.stdin_path);
        if (fdin == NOB_INVALID_FD) nob_return_defer(false);
        opt_fdin = &fdin;
    }
    if (opt.stdout_path) {
        fdout = nob_fd_open_for_write(opt.stdout_path);
        if (fdout == NOB_INVALID_FD) nob_return_defer(false);
        opt_fdout = &fdout;
    }
    if (opt.stderr_path) {
        fderr = nob_fd_open_for_write(opt.stderr_path);
        if (fderr == NOB_INVALID_FD) nob_return_defer(false);
        opt_fderr = &fderr;
    }
    proc = nob__cmd_start_process(*cmd, opt_fdin, opt_fdout, opt_fderr);

    if (opt.async) {
        if (proc == NOB_INVALID_PROC) nob_return_defer(false);
        nob_da_append(opt.async, proc);
    } else {
        if (!nob_proc_wait(proc)) nob_return_defer(false);
    }

defer:
    if (opt_fdin)  nob_fd_close(*opt_fdin);
    if (opt_fdout) nob_fd_close(*opt_fdout);
    if (opt_fderr) nob_fd_close(*opt_fderr);
    if (!opt.dont_reset) cmd->count = 0;
    return result;
}

NOBDEF bool nob_chain_begin_opt(Nob_Chain *chain, Nob_Chain_Begin_Opt opt)
{
    chain->cmd.count = 0;
    chain->err2out = false;
    chain->fdin = NOB_INVALID_FD;
    if (opt.stdin_path) {
        chain->fdin = nob_fd_open_for_read(opt.stdin_path);
        if (chain->fdin == NOB_INVALID_FD) return false;
    }
    return true;
}

NOBDEF bool nob_chain_cmd_opt(Nob_Chain *chain, Nob_Cmd *cmd, Nob_Chain_Cmd_Opt opt)
{
    bool result = true;
    Nob_Pipe pp = {0};
    struct {
        size_t count;
        Nob_Fd items[5]; // should be no more than 3, but we allocate 5 just in case
    } fds = {0};

    NOB_ASSERT(cmd->count > 0);

    if (chain->cmd.count != 0) { // not first cmd in the chain
        Nob_Fd *pfdin = NULL;
        if (chain->fdin != NOB_INVALID_FD) {
            nob_fa_append(&fds, chain->fdin);
            pfdin = &chain->fdin;
        }
        if (!nob_pipe_create(&pp)) nob_return_defer(false);
        nob_fa_append(&fds, pp.write);
        Nob_Fd *pfdout = &pp.write;
        Nob_Fd *pfderr = chain->err2out ? pfdout : NULL;

        Nob_Proc proc = nob__cmd_start_process(chain->cmd, pfdin, pfdout, pfderr);
        chain->cmd.count = 0;
        if (proc == NOB_INVALID_PROC) {
            nob_fa_append(&fds, pp.read);
            nob_return_defer(false);
        }
        chain->fdin = pp.read;
    }

    nob_da_append_many(&chain->cmd, cmd->items, cmd->count);
    chain->err2out = opt.err2out;

defer:
    for (size_t i = 0; i < fds.count; ++i) {
        nob_fd_close(fds.items[i]);
    }
    if (!opt.dont_reset) cmd->count = 0;
    return result;
}

static Nob_Fd nob__fd_stdout(void)
{
#ifdef _WIN32
    return GetStdHandle(STD_OUTPUT_HANDLE);
#else
    return STDOUT_FILENO;
#endif // _WIN32
}

NOBDEF bool nob_chain_end_opt(Nob_Chain *chain, Nob_Chain_End_Opt opt)
{
    bool result = true;

    Nob_Fd *pfdin = NULL;
    struct {
        size_t count;
        Nob_Fd items[5]; // should be no more than 3, but we allocate 5 just in case
    } fds = {0};

    if (chain->fdin != NOB_INVALID_FD) {
        nob_fa_append(&fds, chain->fdin);
        pfdin = &chain->fdin;
    }

    if (chain->cmd.count != 0) { // Non-empty chain case
        size_t max_procs = opt.max_procs > 0 ? opt.max_procs : (size_t) nob_nprocs() + 1;

        if (opt.async && max_procs > 0) {
            while (opt.async->count >= max_procs) {
                for (size_t i = 0; i < opt.async->count; ++i) {
                    int ret = nob__proc_wait_async(opt.async->items[i], 1);
                    if (ret < 0) nob_return_defer(false);
                    if (ret) {
                        nob_da_remove_unordered(opt.async, i);
                        break;
                    }
                }
            }
        }

        Nob_Fd fdout = nob__fd_stdout();
        if (opt.stdout_path) {
            fdout = nob_fd_open_for_write(opt.stdout_path);
            if (fdout == NOB_INVALID_FD) nob_return_defer(false);
            nob_fa_append(&fds, fdout);
        }

        Nob_Fd fderr = 0;
        Nob_Fd *pfderr = NULL;
        if (chain->err2out) pfderr = &fdout;
        if (opt.stderr_path) {
            if (pfderr == NULL) {
                fderr = nob_fd_open_for_write(opt.stderr_path);
                if (fderr == NOB_INVALID_FD) nob_return_defer(false);
                nob_fa_append(&fds, fderr);
                pfderr = &fderr;
            } else {
                // There was err2out set for the last command.
                // All the stderr will go to stdout.
                // So the stderr file is going to be empty.
                NOB_ASSERT(chain->err2out);
                if (!nob_write_entire_file(opt.stderr_path, NULL, 0)) nob_return_defer(false);
            }
        }

        Nob_Proc proc = nob__cmd_start_process(chain->cmd, pfdin, &fdout, pfderr);
        chain->cmd.count = 0;

        if (opt.async) {
            if (proc == NOB_INVALID_PROC) nob_return_defer(false);
            nob_da_append(opt.async, proc);
        } else {
            if (!nob_proc_wait(proc)) nob_return_defer(false);
        }
    }

defer:
    for (size_t i = 0; i < fds.count; ++i) {
        nob_fd_close(fds.items[i]);
    }
    return result;
}

// The maximum time span representable is 584 years.
NOBDEF uint64_t nob_nanos_since_unspecified_epoch(void)
{
#ifdef _WIN32
    LARGE_INTEGER Time;
    QueryPerformanceCounter(&Time);

    static LARGE_INTEGER Frequency = {0};
    if (Frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&Frequency);
    }

    uint64_t Secs  = Time.QuadPart / Frequency.QuadPart;
    uint64_t Nanos = Time.QuadPart % Frequency.QuadPart * NOB_NANOS_PER_SEC / Frequency.QuadPart;
    return NOB_NANOS_PER_SEC * Secs + Nanos;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return NOB_NANOS_PER_SEC * ts.tv_sec + ts.tv_nsec;
#endif // _WIN32
}

NOBDEF Nob_Proc nob_cmd_run_async_redirect(Nob_Cmd cmd, Nob_Cmd_Redirect redirect)
{
    return nob__cmd_start_process(cmd, redirect.fdin, redirect.fdout, redirect.fderr);
}

static Nob_Proc nob__cmd_start_process(Nob_Cmd cmd, Nob_Fd *fdin, Nob_Fd *fdout, Nob_Fd *fderr)
{
    if (cmd.count < 1) {
        nob_log(NOB_ERROR, "Could not run empty command");
        return NOB_INVALID_PROC;
    }

#ifndef NOB_NO_ECHO
    Nob_String_Builder sb = {0};
    nob_cmd_render(cmd, &sb);
    nob_sb_append_null(&sb);
    nob_log(NOB_INFO, "CMD: %s", sb.items);
    nob_sb_free(sb);
    memset(&sb, 0, sizeof(sb));
#endif // NOB_NO_ECHO

#ifdef _WIN32
    // https://docs.microsoft.com/en-us/windows/win32/procthread/creating-a-child-process-with-redirected-input-and-output

    STARTUPINFO siStartInfo;
    ZeroMemory(&siStartInfo, sizeof(siStartInfo));
    siStartInfo.cb = sizeof(STARTUPINFO);
    // NOTE: theoretically setting NULL to std handles should not be a problem
    // https://docs.microsoft.com/en-us/windows/console/getstdhandle?redirectedfrom=MSDN#attachdetach-behavior
    // TODO: check for errors in GetStdHandle
    siStartInfo.hStdError = fderr ? *fderr : GetStdHandle(STD_ERROR_HANDLE);
    siStartInfo.hStdOutput = fdout ? *fdout : GetStdHandle(STD_OUTPUT_HANDLE);
    siStartInfo.hStdInput = fdin ? *fdin : GetStdHandle(STD_INPUT_HANDLE);
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION piProcInfo;
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

    Nob_String_Builder quoted = {0};
    nob__win32_cmd_quote(cmd, &quoted);
    nob_sb_append_null(&quoted);
    BOOL bSuccess = CreateProcessA(NULL, quoted.items, NULL, NULL, TRUE, 0, NULL, NULL, &siStartInfo, &piProcInfo);
    nob_sb_free(quoted);

    if (!bSuccess) {
        nob_log(NOB_ERROR, "Could not create child process for %s: %s", cmd.items[0], nob_win32_error_message(GetLastError()));
        return NOB_INVALID_PROC;
    }

    CloseHandle(piProcInfo.hThread);

    return piProcInfo.hProcess;
#else
    pid_t cpid = fork();
    if (cpid < 0) {
        nob_log(NOB_ERROR, "Could not fork child process: %s", strerror(errno));
        return NOB_INVALID_PROC;
    }

    if (cpid == 0) {
        if (fdin) {
            if (dup2(*fdin, STDIN_FILENO) < 0) {
                nob_log(NOB_ERROR, "Could not setup stdin for child process: %s", strerror(errno));
                exit(1);
            }
        }

        if (fdout) {
            if (dup2(*fdout, STDOUT_FILENO) < 0) {
                nob_log(NOB_ERROR, "Could not setup stdout for child process: %s", strerror(errno));
                exit(1);
            }
        }

        if (fderr) {
            if (dup2(*fderr, STDERR_FILENO) < 0) {
                nob_log(NOB_ERROR, "Could not setup stderr for child process: %s", strerror(errno));
                exit(1);
            }
        }

        // NOTE: This leaks a bit of memory in the child process.
        // But do we actually care? It's a one off leak anyway...
        Nob_Cmd cmd_null = {0};
        nob_da_append_many(&cmd_null, cmd.items, cmd.count);
        nob_cmd_append(&cmd_null, (const char*)NULL);

        if (execvp(cmd.items[0], (char * const*) cmd_null.items) < 0) {
            nob_log(NOB_ERROR, "Could not exec child process for %s: %s", cmd.items[0], strerror(errno));
            exit(1);
        }
        NOB_UNREACHABLE("nob_cmd_run_async_redirect");
    }

    return cpid;
#endif
}

NOBDEF Nob_Proc nob_cmd_run_async(Nob_Cmd cmd)
{
    return nob__cmd_start_process(cmd, NULL, NULL, NULL);
}

NOBDEF Nob_Proc nob_cmd_run_async_and_reset(Nob_Cmd *cmd)
{
    Nob_Proc proc = nob__cmd_start_process(*cmd, NULL, NULL, NULL);
    cmd->count = 0;
    return proc;
}

NOBDEF Nob_Proc nob_cmd_run_async_redirect_and_reset(Nob_Cmd *cmd, Nob_Cmd_Redirect redirect)
{
    Nob_Proc proc = nob__cmd_start_process(*cmd, redirect.fdin, redirect.fdout, redirect.fderr);
    cmd->count = 0;
    if (redirect.fdin) {
        nob_fd_close(*redirect.fdin);
        *redirect.fdin = NOB_INVALID_FD;
    }
    if (redirect.fdout) {
        nob_fd_close(*redirect.fdout);
        *redirect.fdout = NOB_INVALID_FD;
    }
    if (redirect.fderr) {
        nob_fd_close(*redirect.fderr);
        *redirect.fderr = NOB_INVALID_FD;
    }
    return proc;
}

NOBDEF Nob_Fd nob_fd_open_for_read(const char *path)
{
#ifndef _WIN32
    Nob_Fd result = open(path, O_RDONLY);
    if (result < 0) {
        nob_log(NOB_ERROR, "Could not open file %s: %s", path, strerror(errno));
        return NOB_INVALID_FD;
    }
    return result;
#else
    // https://docs.microsoft.com/en-us/windows/win32/fileio/opening-a-file-for-reading-or-writing
    SECURITY_ATTRIBUTES saAttr = {0};
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;

    Nob_Fd result = CreateFile(
                    path,
                    GENERIC_READ,
                    0,
                    &saAttr,
                    OPEN_EXISTING,
                    FILE_ATTRIBUTE_READONLY,
                    NULL);

    if (result == INVALID_HANDLE_VALUE) {
        nob_log(NOB_ERROR, "Could not open file %s: %s", path, nob_win32_error_message(GetLastError()));
        return NOB_INVALID_FD;
    }

    return result;
#endif // _WIN32
}

NOBDEF Nob_Fd nob_fd_open_for_write(const char *path)
{
#ifndef _WIN32
    Nob_Fd result = open(path,
                     O_WRONLY | O_CREAT | O_TRUNC,
                     S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (result < 0) {
        nob_log(NOB_ERROR, "could not open file %s: %s", path, strerror(errno));
        return NOB_INVALID_FD;
    }
    return result;
#else
    SECURITY_ATTRIBUTES saAttr = {0};
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;

    Nob_Fd result = CreateFile(
                    path,                            // name of the write
                    GENERIC_WRITE,                   // open for writing
                    0,                               // do not share
                    &saAttr,                         // default security
                    CREATE_ALWAYS,                   // create always
                    FILE_ATTRIBUTE_NORMAL,           // normal file
                    NULL                             // no attr. template
                );

    if (result == INVALID_HANDLE_VALUE) {
        nob_log(NOB_ERROR, "Could not open file %s: %s", path, nob_win32_error_message(GetLastError()));
        return NOB_INVALID_FD;
    }

    return result;
#endif // _WIN32
}

NOBDEF void nob_fd_close(Nob_Fd fd)
{
#ifdef _WIN32
    CloseHandle(fd);
#else
    close(fd);
#endif // _WIN32
}

NOBDEF bool nob_pipe_create(Nob_Pipe *pp)
{
#ifdef _WIN32
    // https://docs.microsoft.com/en-us/windows/win32/ProcThread/creating-a-child-process-with-redirected-input-and-output

    SECURITY_ATTRIBUTES saAttr = {0};
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;

    if (!CreatePipe(&pp->read, &pp->write, &saAttr, 0)) {
        nob_log(NOB_ERROR, "Could not create pipe: %s", nob_win32_error_message(GetLastError()));
        return false;
    }

    return true;
#else
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        nob_log(NOB_ERROR, "Could not create pipe: %s\n", strerror(errno));
        return false;
    }

    pp->read  = pipefd[0];
    pp->write = pipefd[1];

    return true;
#endif // _WIN32
}

NOBDEF bool nob_procs_wait(Nob_Procs procs)
{
    bool success = true;
    for (size_t i = 0; i < procs.count; ++i) {
        success = nob_proc_wait(procs.items[i]) && success;
    }
    return success;
}

NOBDEF bool nob_procs_flush(Nob_Procs *procs)
{
    bool success = nob_procs_wait(*procs);
    procs->count = 0;
    return success;
}

NOBDEF bool nob_procs_wait_and_reset(Nob_Procs *procs)
{
    return nob_procs_flush(procs);
}

NOBDEF bool nob_proc_wait(Nob_Proc proc)
{
    if (proc == NOB_INVALID_PROC) return false;

#ifdef _WIN32
    DWORD result = WaitForSingleObject(
                       proc,    // HANDLE hHandle,
                       INFINITE // DWORD  dwMilliseconds
                   );

    if (result == WAIT_FAILED) {
        nob_log(NOB_ERROR, "could not wait on child process: %s", nob_win32_error_message(GetLastError()));
        return false;
    }

    DWORD exit_status;
    if (!GetExitCodeProcess(proc, &exit_status)) {
        nob_log(NOB_ERROR, "could not get process exit code: %s", nob_win32_error_message(GetLastError()));
        return false;
    }

    if (exit_status != 0) {
        nob_log(NOB_ERROR, "command exited with exit code %lu", exit_status);
        return false;
    }

    CloseHandle(proc);

    return true;
#else
    for (;;) {
        int wstatus = 0;
        if (waitpid(proc, &wstatus, 0) < 0) {
            nob_log(NOB_ERROR, "could not wait on command (pid %d): %s", proc, strerror(errno));
            return false;
        }

        if (WIFEXITED(wstatus)) {
            int exit_status = WEXITSTATUS(wstatus);
            if (exit_status != 0) {
                nob_log(NOB_ERROR, "command exited with exit code %d", exit_status);
                return false;
            }

            break;
        }

        if (WIFSIGNALED(wstatus)) {
            nob_log(NOB_ERROR, "command process was terminated by signal %d", WTERMSIG(wstatus));
            return false;
        }
    }

    return true;
#endif
}

static int nob__proc_wait_async(Nob_Proc proc, int ms)
{
    if (proc == NOB_INVALID_PROC) return false;

#ifdef _WIN32
    DWORD result = WaitForSingleObject(
                       proc,    // HANDLE hHandle,
                       ms       // DWORD  dwMilliseconds
                   );

    if (result == WAIT_TIMEOUT) {
        return 0;
    }

    if (result == WAIT_FAILED) {
        nob_log(NOB_ERROR, "could not wait on child process: %s", nob_win32_error_message(GetLastError()));
        return -1;
    }

    DWORD exit_status;
    if (!GetExitCodeProcess(proc, &exit_status)) {
        nob_log(NOB_ERROR, "could not get process exit code: %s", nob_win32_error_message(GetLastError()));
        return -1;
    }

    if (exit_status != 0) {
        nob_log(NOB_ERROR, "command exited with exit code %lu", exit_status);
        return -1;
    }

    CloseHandle(proc);

    return 1;
#else
    long ns = ms*1000*1000;
    struct timespec duration = {
        .tv_sec = ns/(1000*1000*1000),
        .tv_nsec = ns%(1000*1000*1000),
    };

    int wstatus = 0;
    pid_t pid = waitpid(proc, &wstatus, WNOHANG);
    if (pid < 0) {
        nob_log(NOB_ERROR, "could not wait on command (pid %d): %s", proc, strerror(errno));
        return -1;
    }

    if (pid == 0) {
        nanosleep(&duration, NULL);
        return 0;
    }

    if (WIFEXITED(wstatus)) {
        int exit_status = WEXITSTATUS(wstatus);
        if (exit_status != 0) {
            nob_log(NOB_ERROR, "command exited with exit code %d", exit_status);
            return -1;
        }

        return 1;
    }

    if (WIFSIGNALED(wstatus)) {
        nob_log(NOB_ERROR, "command process was terminated by signal %d", WTERMSIG(wstatus));
        return -1;
    }

    nanosleep(&duration, NULL);
    return 0;
#endif
}

NOBDEF bool nob_procs_append_with_flush(Nob_Procs *procs, Nob_Proc proc, size_t max_procs_count)
{
    nob_da_append(procs, proc);

    if (procs->count >= max_procs_count) {
        if (!nob_procs_flush(procs)) return false;
    }

    return true;
}

NOBDEF bool nob_cmd_run_sync_redirect(Nob_Cmd cmd, Nob_Cmd_Redirect redirect)
{
    Nob_Proc p = nob__cmd_start_process(cmd, redirect.fdin, redirect.fdout, redirect.fderr);
    return nob_proc_wait(p);
}

NOBDEF bool nob_cmd_run_sync(Nob_Cmd cmd)
{
    Nob_Proc p = nob__cmd_start_process(cmd, NULL, NULL, NULL);
    return nob_proc_wait(p);
}

NOBDEF bool nob_cmd_run_sync_and_reset(Nob_Cmd *cmd)
{
    Nob_Proc p = nob__cmd_start_process(*cmd, NULL, NULL, NULL);
    cmd->count = 0;
    return nob_proc_wait(p);
}

NOBDEF bool nob_cmd_run_sync_redirect_and_reset(Nob_Cmd *cmd, Nob_Cmd_Redirect redirect)
{
    Nob_Proc p = nob__cmd_start_process(*cmd, redirect.fdin, redirect.fdout, redirect.fderr);
    cmd->count = 0;
    if (redirect.fdin) {
        nob_fd_close(*redirect.fdin);
        *redirect.fdin = NOB_INVALID_FD;
    }
    if (redirect.fdout) {
        nob_fd_close(*redirect.fdout);
        *redirect.fdout = NOB_INVALID_FD;
    }
    if (redirect.fderr) {
        nob_fd_close(*redirect.fderr);
        *redirect.fderr = NOB_INVALID_FD;
    }
    return nob_proc_wait(p);
}

static Nob_Log_Handler *nob__log_handler = &nob_default_log_handler;

NOBDEF void nob_set_log_handler(Nob_Log_Handler *handler)
{
    nob__log_handler = handler;
}

NOBDEF Nob_Log_Handler *nob_get_log_handler(void)
{
    return nob__log_handler;
}

NOBDEF void nob_default_log_handler(Nob_Log_Level level, const char *fmt, va_list args)
{
    if (level < nob_minimal_log_level) return;

    const char *prefix = NULL;
    switch (level) {
    case NOB_INFO:
        prefix = "[INFO] ";
        break;
    case NOB_WARNING:
        prefix = "[WARNING] ";
        break;
    case NOB_ERROR:
        prefix = "[ERROR] ";
        break;
    case NOB_NO_LOGS: return;
    default:
        NOB_UNREACHABLE("Nob_Log_Level");
    }

    size_t mark = nob_temp_save();
    const char *msg = nob_temp_vsprintf(fmt, args);
    fprintf(stderr, "%s%s\n", prefix, msg);
    nob_temp_rewind(mark);
}

NOBDEF void nob_null_log_handler(Nob_Log_Level level, const char *fmt, va_list args)
{
    NOB_UNUSED(level);
    NOB_UNUSED(fmt);
    NOB_UNUSED(args);
}

NOBDEF void nob_cancer_log_handler(Nob_Log_Level level, const char *fmt, va_list args)
{
    if (level < nob_minimal_log_level) return;

    const char *prefix = NULL;
    switch (level) {
    case NOB_INFO:
        prefix = "ℹ️ \x1b[36m[INFO]\x1b[0m ";
        break;
    case NOB_WARNING:
        prefix = "⚠️ \x1b[33m[WARNING]\x1b[0m ";
        break;
    case NOB_ERROR:
        prefix = "🚨 \x1b[31m[ERROR]\x1b[0m ";
        break;
    case NOB_NO_LOGS: return;
    default:
        NOB_UNREACHABLE("Nob_Log_Level");
    }

    size_t mark = nob_temp_save();
    const char *msg = nob_temp_vsprintf(fmt, args);
    fprintf(stderr, "%s%s\n", prefix, msg);
    nob_temp_rewind(mark);
}

NOBDEF void nob_log(Nob_Log_Level level, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    nob__log_handler(level, fmt, args);
    va_end(args);
}

NOBDEF bool nob_dir_entry_open(const char *dir_path, Nob_Dir_Entry *dir)
{
    memset(dir, 0, sizeof(*dir));
#ifdef _WIN32
    size_t temp_mark = nob_temp_save();
    char *buffer = nob_temp_sprintf("%s\\*", dir_path);
    dir->nob__private.win32_hFind = FindFirstFile(buffer, &dir->nob__private.win32_data);
    nob_temp_rewind(temp_mark);

    if (dir->nob__private.win32_hFind == INVALID_HANDLE_VALUE) {
        nob_log(NOB_ERROR, "Could not open directory %s: %s", dir_path, nob_win32_error_message(GetLastError()));
        dir->error = true;
        return false;
    }
#else
    dir->nob__private.posix_dir = opendir(dir_path);
    if (dir->nob__private.posix_dir == NULL) {
        nob_log(NOB_ERROR, "Could not open directory %s: %s", dir_path, strerror(errno));
        dir->error = true;
        return false;
    }
#endif // _WIN32
    return true;
}

NOBDEF bool nob_dir_entry_next(Nob_Dir_Entry *dir)
{
#ifdef _WIN32
    if (!dir->nob__private.win32_init) {
        dir->nob__private.win32_init = true;
        dir->name = dir->nob__private.win32_data.cFileName;
        return true;
    }

    if (!FindNextFile(dir->nob__private.win32_hFind, &dir->nob__private.win32_data)) {
        if (GetLastError() == ERROR_NO_MORE_FILES) return false;
        nob_log(NOB_ERROR, "Could not read next directory entry: %s", nob_win32_error_message(GetLastError()));
        dir->error = true;
        return false;
    }
    dir->name = dir->nob__private.win32_data.cFileName;
#else
    errno = 0;
    dir->nob__private.posix_ent = readdir(dir->nob__private.posix_dir);
    if (dir->nob__private.posix_ent == NULL) {
        if (errno == 0) return false;
        nob_log(NOB_ERROR, "Could not read next directory entry: %s", strerror(errno));
        dir->error = true;
        return false;
    }
    dir->name = dir->nob__private.posix_ent->d_name;
#endif // _WIN32
    return true;
}

NOBDEF void nob_dir_entry_close(Nob_Dir_Entry dir)
{
#ifdef _WIN32
    FindClose(dir.nob__private.win32_hFind);
#else
    if (dir.nob__private.posix_dir) closedir(dir.nob__private.posix_dir);
#endif
}

// On the moment of entering `nob__walk_dir_opt_impl()`, the `file_path` Nob_String_Builder is expected to be NULL-terminated.
// So you can freely pass `file_path->items` to functions that expect NULL-terminated file path.
// On existing `nob__walk_dir_opt_impl()` is expected to restore the original content of `file_path`
bool nob__walk_dir_opt_impl(Nob_String_Builder *file_path, Nob_Walk_Func func, size_t level, bool *stop, Nob_Walk_Dir_Opt opt)
{
    NOB_ASSERT(file_path->count > 0 && "file_path was probably not properly NULL-terminated");
    bool result = true;

    Nob_Dir_Entry dir = {0};
    size_t saved_file_path_count = file_path->count;
    Nob_Walk_Action action = NOB_WALK_CONT;

    Nob_File_Type file_type = nob_get_file_type(file_path->items);
    if (file_type < 0) nob_return_defer(false);

    // Pre-order walking
    if (!opt.post_order) {
        if (!func(NOB_CLIT(Nob_Walk_Entry) {
            .path = file_path->items,
            .type = file_type,
            .level = level,
            .data = opt.data,
            .action = &action,
        })) nob_return_defer(false);
        switch (action) {
        case NOB_WALK_CONT: break;
        case NOB_WALK_STOP: *stop = true; // fallthrough
        case NOB_WALK_SKIP: nob_return_defer(true);
        default: NOB_UNREACHABLE("Nob_Walk_Action");
        }
    }

    if (file_type == NOB_FILE_DIRECTORY) {
        if (!nob_dir_entry_open(file_path->items, &dir)) nob_return_defer(false);
        for (;;) {
            // Next entry
            if (!nob_dir_entry_next(&dir)) {
                if (!dir.error) break;
                nob_return_defer(false);
            }

            // Ignore . and ..
            if (strcmp(dir.name, ".")  == 0) continue;
            if (strcmp(dir.name, "..") == 0) continue;

            // Prepare the new file_path
            file_path->count = saved_file_path_count - 1;
#ifdef _WIN32
            nob_sb_appendf(file_path, "\\%s", dir.name);
#else
            nob_sb_appendf(file_path, "/%s", dir.name);
#endif // _WIN32
            nob_sb_append_null(file_path);

            // Recurse
            if (!nob__walk_dir_opt_impl(file_path, func, level+1, stop, opt)) nob_return_defer(false);
            if (*stop) nob_return_defer(true);
        }
        file_path->count = saved_file_path_count;
        nob_da_last(file_path) = '\0';
    }

    // Post-order walking
    if (opt.post_order) {
        if (!func(NOB_CLIT(Nob_Walk_Entry) {
            .path = file_path->items,
            .type = file_type,
            .level = level,
            .data = opt.data,
            .action = &action,
        })) nob_return_defer(false);
        switch (action) {
        case NOB_WALK_CONT: break;
        case NOB_WALK_STOP: *stop = true; // fallthrough
        case NOB_WALK_SKIP: nob_return_defer(true);
        default: NOB_UNREACHABLE("Nob_Walk_Action");
        }
    }

defer:
    // Always reset the file_path back to what it was
    file_path->count = saved_file_path_count;
    nob_da_last(file_path) = '\0';

    nob_dir_entry_close(dir);
    return result;
}

NOBDEF bool nob_walk_dir_opt(const char *root, Nob_Walk_Func func, Nob_Walk_Dir_Opt opt)
{
    Nob_String_Builder file_path = {0};

    nob_sb_appendf(&file_path, "%s", root);
    nob_sb_append_null(&file_path);

    bool stop = false;
    bool ok = nob__walk_dir_opt_impl(&file_path, func, 0, &stop, opt);
    free(file_path.items);
    return ok;
}

NOBDEF bool nob_read_entire_dir(const char *parent, Nob_File_Paths *children)
{
    if (strlen(parent) == 0) {
        nob_log(NOB_ERROR, "Cannot read empty path");
        return false;
    }
    bool result = true;
    Nob_Dir_Entry dir = {0};
    if (!nob_dir_entry_open(parent, &dir)) nob_return_defer(false);
    while (nob_dir_entry_next(&dir)) nob_da_append(children, nob_temp_strdup(dir.name));
    if (dir.error) nob_return_defer(false);
defer:
    nob_dir_entry_close(dir);
    return result;
}

NOBDEF bool nob_write_entire_file(const char *path, const void *data, size_t size)
{
    bool result = true;

    const char *buf = NULL;
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        nob_log(NOB_ERROR, "Could not open file %s for writing: %s\n", path, strerror(errno));
        nob_return_defer(false);
    }

    //           len
    //           v
    // aaaaaaaaaa
    //     ^
    //     data

    buf = (const char*)data;
    while (size > 0) {
        size_t n = fwrite(buf, 1, size, f);
        if (ferror(f)) {
            nob_log(NOB_ERROR, "Could not write into file %s: %s\n", path, strerror(errno));
            nob_return_defer(false);
        }
        size -= n;
        buf  += n;
    }

defer:
    if (f) fclose(f);
    return result;
}

NOBDEF Nob_File_Type nob_get_file_type(const char *path)
{
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        nob_log(NOB_ERROR, "Could not get file attributes of %s: %s", path, nob_win32_error_message(GetLastError()));
        return (Nob_File_Type)-1;
    }

    if (attr & FILE_ATTRIBUTE_DIRECTORY) return NOB_FILE_DIRECTORY;
    // TODO: detect symlinks on Windows (whatever that means on Windows anyway)
    return NOB_FILE_REGULAR;
#else // _WIN32
    struct stat statbuf;
    if (lstat(path, &statbuf) < 0) {
        nob_log(NOB_ERROR, "Could not get stat of %s: %s", path, strerror(errno));
        return (Nob_File_Type)(-1);
    }

    if (S_ISREG(statbuf.st_mode)) return NOB_FILE_REGULAR;
    if (S_ISDIR(statbuf.st_mode)) return NOB_FILE_DIRECTORY;
    if (S_ISLNK(statbuf.st_mode)) return NOB_FILE_SYMLINK;
    return NOB_FILE_OTHER;
#endif // _WIN32
}

NOBDEF bool nob_delete_file(const char *path)
{
#ifndef NOB_NO_ECHO
    nob_log(NOB_INFO, "deleting %s", path);
#endif // NOB_NO_ECHO
#ifdef _WIN32
    Nob_File_Type type = nob_get_file_type(path);
    switch (type) {
    case NOB_FILE_DIRECTORY:
        if (!RemoveDirectoryA(path)) {
            nob_log(NOB_ERROR, "Could not delete directory %s: %s", path, nob_win32_error_message(GetLastError()));
            return false;
        }
        break;
    case NOB_FILE_REGULAR:
    case NOB_FILE_SYMLINK:
    case NOB_FILE_OTHER:
        if (!DeleteFileA(path)) {
            nob_log(NOB_ERROR, "Could not delete file %s: %s", path, nob_win32_error_message(GetLastError()));
            return false;
        }
        break;
    default: NOB_UNREACHABLE("Nob_File_Type");
    }
    return true;
#else
    if (remove(path) < 0) {
        nob_log(NOB_ERROR, "Could not delete file %s: %s", path, strerror(errno));
        return false;
    }
    return true;
#endif // _WIN32
}

NOBDEF bool nob_copy_directory_recursively(const char *src_path, const char *dst_path)
{
    bool result = true;
    Nob_File_Paths children = {0};
    Nob_String_Builder src_sb = {0};
    Nob_String_Builder dst_sb = {0};
    size_t temp_checkpoint = nob_temp_save();

    Nob_File_Type type = nob_get_file_type(src_path);
    if (type < 0) return false;

    switch (type) {
        case NOB_FILE_DIRECTORY: {
            if (!nob_mkdir_if_not_exists(dst_path)) nob_return_defer(false);
            if (!nob_read_entire_dir(src_path, &children)) nob_return_defer(false);

            for (size_t i = 0; i < children.count; ++i) {
                if (strcmp(children.items[i], ".") == 0) continue;
                if (strcmp(children.items[i], "..") == 0) continue;

                src_sb.count = 0;
                nob_sb_append_cstr(&src_sb, src_path);
                nob_sb_append_cstr(&src_sb, "/");
                nob_sb_append_cstr(&src_sb, children.items[i]);
                nob_sb_append_null(&src_sb);

                dst_sb.count = 0;
                nob_sb_append_cstr(&dst_sb, dst_path);
                nob_sb_append_cstr(&dst_sb, "/");
                nob_sb_append_cstr(&dst_sb, children.items[i]);
                nob_sb_append_null(&dst_sb);

                if (!nob_copy_directory_recursively(src_sb.items, dst_sb.items)) {
                    nob_return_defer(false);
                }
            }
        } break;

        case NOB_FILE_REGULAR: {
            if (!nob_copy_file(src_path, dst_path)) {
                nob_return_defer(false);
            }
        } break;

        case NOB_FILE_SYMLINK: {
            nob_log(NOB_WARNING, "TODO: Copying symlinks is not supported yet");
        } break;

        case NOB_FILE_OTHER: {
            nob_log(NOB_ERROR, "Unsupported type of file %s", src_path);
            nob_return_defer(false);
        } break;

        default: NOB_UNREACHABLE("nob_copy_directory_recursively");
    }

defer:
    nob_temp_rewind(temp_checkpoint);
    nob_da_free(src_sb);
    nob_da_free(dst_sb);
    nob_da_free(children);
    return result;
}

NOBDEF char *nob_temp_strdup(const char *cstr)
{
    size_t n = strlen(cstr);
    char *result = (char*)nob_temp_alloc(n + 1);
    NOB_ASSERT(result != NULL && "Increase NOB_TEMP_CAPACITY");
    memcpy(result, cstr, n);
    result[n] = '\0';
    return result;
}

NOBDEF char *nob_temp_strndup(const char *s, size_t n)
{
    char *r = (char*)nob_temp_alloc(n + 1);
    NOB_ASSERT(r != NULL && "Extend the size of the temporary allocator");
    memcpy(r, s, n);
    r[n] = '\0';
    return r;
}

NOBDEF void *nob_temp_alloc(size_t requested_size)
{
    size_t word_size = sizeof(uintptr_t);
    size_t size = (requested_size + word_size - 1)/word_size*word_size;
    if (nob_temp_size + size > NOB_TEMP_CAPACITY) return NULL;
    void *result = &nob_temp[nob_temp_size];
    nob_temp_size += size;
    return result;
}

NOBDEF char *nob_temp_vsprintf(const char *format, va_list ap)
{
    va_list args;
    va_copy(args, ap);
    int n = vsnprintf(NULL, 0, format, args);
    va_end(args);

    NOB_ASSERT(n >= 0);
    char *result = (char*)nob_temp_alloc(n + 1);
    NOB_ASSERT(result != NULL && "Extend the size of the temporary allocator");
    // TODO: use proper arenas for the temporary allocator;
    va_copy(args, ap);
    vsnprintf(result, n + 1, format, args);
    va_end(args);

    return result;
}

NOBDEF char *nob_temp_sprintf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    char *result = nob_temp_vsprintf(format, args);
    va_end(args);
    return result;
}

NOBDEF void nob_temp_reset(void)
{
    nob_temp_size = 0;
}

NOBDEF size_t nob_temp_save(void)
{
    return nob_temp_size;
}

NOBDEF void nob_temp_rewind(size_t checkpoint)
{
    nob_temp_size = checkpoint;
}

NOBDEF const char *nob_temp_sv_to_cstr(Nob_String_View sv)
{
    return nob_temp_strndup(sv.data, sv.count);
}

NOBDEF int nob_needs_rebuild(const char *output_path, const char **input_paths, size_t input_paths_count)
{
#ifdef _WIN32
    BOOL bSuccess;

    HANDLE output_path_fd = CreateFile(output_path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
    if (output_path_fd == INVALID_HANDLE_VALUE) {
        // NOTE: if output does not exist it 100% must be rebuilt
        if (GetLastError() == ERROR_FILE_NOT_FOUND) return 1;
        nob_log(NOB_ERROR, "Could not open file %s: %s", output_path, nob_win32_error_message(GetLastError()));
        return -1;
    }
    FILETIME output_path_time;
    bSuccess = GetFileTime(output_path_fd, NULL, NULL, &output_path_time);
    CloseHandle(output_path_fd);
    if (!bSuccess) {
        nob_log(NOB_ERROR, "Could not get time of %s: %s", output_path, nob_win32_error_message(GetLastError()));
        return -1;
    }

    for (size_t i = 0; i < input_paths_count; ++i) {
        const char *input_path = input_paths[i];
        HANDLE input_path_fd = CreateFile(input_path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
        if (input_path_fd == INVALID_HANDLE_VALUE) {
            // NOTE: non-existing input is an error cause it is needed for building in the first place
            nob_log(NOB_ERROR, "Could not open file %s: %s", input_path, nob_win32_error_message(GetLastError()));
            return -1;
        }
        FILETIME input_path_time;
        bSuccess = GetFileTime(input_path_fd, NULL, NULL, &input_path_time);
        CloseHandle(input_path_fd);
        if (!bSuccess) {
            nob_log(NOB_ERROR, "Could not get time of %s: %s", input_path, nob_win32_error_message(GetLastError()));
            return -1;
        }

        // NOTE: if even a single input_path is fresher than output_path that's 100% rebuild
        if (CompareFileTime(&input_path_time, &output_path_time) == 1) return 1;
    }

    return 0;
#else
    struct stat statbuf = {0};

    if (stat(output_path, &statbuf) < 0) {
        // NOTE: if output does not exist it 100% must be rebuilt
        if (errno == ENOENT) return 1;
        nob_log(NOB_ERROR, "could not stat %s: %s", output_path, strerror(errno));
        return -1;
    }
    time_t output_path_time = statbuf.st_mtime;

    for (size_t i = 0; i < input_paths_count; ++i) {
        const char *input_path = input_paths[i];
        if (stat(input_path, &statbuf) < 0) {
            // NOTE: non-existing input is an error cause it is needed for building in the first place
            nob_log(NOB_ERROR, "could not stat %s: %s", input_path, strerror(errno));
            return -1;
        }
        time_t input_path_time = statbuf.st_mtime;
        // NOTE: if even a single input_path is fresher than output_path that's 100% rebuild
        if (input_path_time > output_path_time) return 1;
    }

    return 0;
#endif
}

NOBDEF int nob_needs_rebuild1(const char *output_path, const char *input_path)
{
    return nob_needs_rebuild(output_path, &input_path, 1);
}

NOBDEF const char *nob_path_name(const char *path)
{
#ifdef _WIN32
    const char *p1 = strrchr(path, '/');
    const char *p2 = strrchr(path, '\\');
    const char *p = (p1 > p2)? p1 : p2;  // NULL is ignored if the other search is successful
    return p ? p + 1 : path;
#else
    const char *p = strrchr(path, '/');
    return p ? p + 1 : path;
#endif // _WIN32
}

NOBDEF bool nob_rename(const char *old_path, const char *new_path)
{
#ifndef NOB_NO_ECHO
    nob_log(NOB_INFO, "renaming %s -> %s", old_path, new_path);
#endif // NOB_NO_ECHO
#ifdef _WIN32
    if (!MoveFileEx(old_path, new_path, MOVEFILE_REPLACE_EXISTING)) {
        nob_log(NOB_ERROR, "could not rename %s to %s: %s", old_path, new_path, nob_win32_error_message(GetLastError()));
        return false;
    }
#else
    if (rename(old_path, new_path) < 0) {
        nob_log(NOB_ERROR, "could not rename %s to %s: %s", old_path, new_path, strerror(errno));
        return false;
    }
#endif // _WIN32
    return true;
}

NOBDEF bool nob_read_entire_file(const char *path, Nob_String_Builder *sb)
{
    bool result = true;

    FILE *f = fopen(path, "rb");
    size_t new_count = 0;
    long long m = 0;
    if (f == NULL)                 nob_return_defer(false);
    if (fseek(f, 0, SEEK_END) < 0) nob_return_defer(false);
#ifndef _WIN32
    m = ftell(f);
#else
    m = _telli64(_fileno(f));
#endif
    if (m < 0)                     nob_return_defer(false);
    if (fseek(f, 0, SEEK_SET) < 0) nob_return_defer(false);

    new_count = sb->count + m;
    if (new_count > sb->capacity) {
        sb->items = NOB_DECLTYPE_CAST(sb->items)NOB_REALLOC(sb->items, new_count);
        NOB_ASSERT(sb->items != NULL && "Buy more RAM lool!!");
        sb->capacity = new_count;
    }

    fread(sb->items + sb->count, m, 1, f);
    if (ferror(f)) {
        // TODO: Afaik, ferror does not set errno. So the error reporting in defer is not correct in this case.
        nob_return_defer(false);
    }
    sb->count = new_count;

defer:
    if (!result) nob_log(NOB_ERROR, "Could not read file %s: %s", path, strerror(errno));
    if (f) fclose(f);
    return result;
}

NOBDEF int nob_sb_appendf(Nob_String_Builder *sb, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    int n = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    // NOTE: the new_capacity needs to be +1 because of the null terminator.
    // However, further below we increase sb->count by n, not n + 1.
    // This is because we don't want the sb to include the null terminator. The user can always sb_append_null() if they want it
    nob_da_reserve(sb, sb->count + n + 1);
    char *dest = sb->items + sb->count;
    va_start(args, fmt);
    vsnprintf(dest, n+1, fmt, args);
    va_end(args);

    sb->count += n;

    return n;
}

NOBDEF void nob_sb_pad_align(Nob_String_Builder *sb, size_t size)
{
    size_t rem = sb->count%size;
    if (rem == 0) return;
    for (size_t i = 0; i < size - rem; ++i) {
        nob_da_append(sb, 0);
    }
}

NOBDEF Nob_String_View nob_sv_chop_while(Nob_String_View *sv, int (*p)(int x))
{
    size_t i = 0;
    while (i < sv->count && p(sv->data[i])) {
        i += 1;
    }

    Nob_String_View result = nob_sv_from_parts(sv->data, i);
    sv->count -= i;
    sv->data  += i;

    return result;
}

NOBDEF Nob_String_View nob_sv_chop_by_delim(Nob_String_View *sv, char delim)
{
    size_t i = 0;
    while (i < sv->count && sv->data[i] != delim) {
        i += 1;
    }

    Nob_String_View result = nob_sv_from_parts(sv->data, i);

    if (i < sv->count) {
        sv->count -= i + 1;
        sv->data  += i + 1;
    } else {
        sv->count -= i;
        sv->data  += i;
    }

    return result;
}

NOBDEF bool nob_sv_chop_prefix(Nob_String_View *sv, Nob_String_View prefix)
{
    if (nob_sv_starts_with(*sv, prefix)) {
        nob_sv_chop_left(sv, prefix.count);
        return true;
    }
    return false;
}

NOBDEF bool nob_sv_chop_suffix(Nob_String_View *sv, Nob_String_View suffix)
{
    if (nob_sv_ends_with(*sv, suffix)) {
        nob_sv_chop_right(sv, suffix.count);
        return true;
    }
    return false;
}

NOBDEF Nob_String_View nob_sv_chop_left(Nob_String_View *sv, size_t n)
{
    if (n > sv->count) {
        n = sv->count;
    }

    Nob_String_View result = nob_sv_from_parts(sv->data, n);

    sv->data  += n;
    sv->count -= n;

    return result;
}

NOBDEF Nob_String_View nob_sv_chop_right(Nob_String_View *sv, size_t n)
{
    if (n > sv->count) {
        n = sv->count;
    }

    Nob_String_View result = nob_sv_from_parts(sv->data + sv->count - n, n);

    sv->count -= n;

    return result;
}

NOBDEF Nob_String_View nob_sv_from_parts(const char *data, size_t count)
{
    Nob_String_View sv;
    sv.count = count;
    sv.data = data;
    return sv;
}

NOBDEF Nob_String_View nob_sv_trim_left(Nob_String_View sv)
{
    size_t i = 0;
    while (i < sv.count && isspace(sv.data[i])) {
        i += 1;
    }

    // Apparently if sv.data == 0 and i == 0 adding them
    // together is an undefined behavior. According to
    // clang's UndefinedBehaviorSanitizer at least.
    if (i == 0) return sv;

    return nob_sv_from_parts(sv.data + i, sv.count - i);
}

NOBDEF Nob_String_View nob_sv_trim_right(Nob_String_View sv)
{
    size_t i = 0;
    while (i < sv.count && isspace(sv.data[sv.count - 1 - i])) {
        i += 1;
    }

    return nob_sv_from_parts(sv.data, sv.count - i);
}

NOBDEF Nob_String_View nob_sv_trim(Nob_String_View sv)
{
    return nob_sv_trim_right(nob_sv_trim_left(sv));
}

NOBDEF Nob_String_View nob_sv_from_cstr(const char *cstr)
{
    return nob_sv_from_parts(cstr, strlen(cstr));
}

NOBDEF bool nob_sv_eq(Nob_String_View a, Nob_String_View b)
{
    if (a.count != b.count) {
        return false;
    } else {
        return memcmp(a.data, b.data, a.count) == 0;
    }
}

NOBDEF bool nob_sv_end_with(Nob_String_View sv, const char *cstr)
{
    return nob_sv_ends_with_cstr(sv, cstr);
}

NOBDEF bool nob_sv_ends_with_cstr(Nob_String_View sv, const char *cstr)
{
    return nob_sv_ends_with(sv, nob_sv_from_cstr(cstr));
}

NOBDEF bool nob_sv_ends_with(Nob_String_View sv, Nob_String_View suffix)
{
    if (sv.count >= suffix.count) {
        Nob_String_View sv_tail = {
            .count = suffix.count,
            .data = sv.data + sv.count - suffix.count,
        };
        return nob_sv_eq(sv_tail, suffix);
    }
    return false;
}

NOBDEF bool nob_sv_starts_with(Nob_String_View sv, Nob_String_View expected_prefix)
{
    if (expected_prefix.count <= sv.count) {
        Nob_String_View actual_prefix = nob_sv_from_parts(sv.data, expected_prefix.count);
        return nob_sv_eq(expected_prefix, actual_prefix);
    }

    return false;
}

// RETURNS:
//  0 - file does not exists
//  1 - file exists
NOBDEF int nob_file_exists(const char *file_path)
{
#if _WIN32
    return GetFileAttributesA(file_path) != INVALID_FILE_ATTRIBUTES;
#else
    return access(file_path, F_OK) == 0;
#endif
}

NOBDEF const char *nob_get_current_dir_temp(void)
{
#ifdef _WIN32
    DWORD nBufferLength = GetCurrentDirectory(0, NULL);
    if (nBufferLength == 0) {
        nob_log(NOB_ERROR, "could not get current directory: %s", nob_win32_error_message(GetLastError()));
        return NULL;
    }

    char *buffer = (char*) nob_temp_alloc(nBufferLength);
    if (GetCurrentDirectory(nBufferLength, buffer) == 0) {
        nob_log(NOB_ERROR, "could not get current directory: %s", nob_win32_error_message(GetLastError()));
        return NULL;
    }

    return buffer;
#else
    char *buffer = (char*) nob_temp_alloc(PATH_MAX);
    if (getcwd(buffer, PATH_MAX) == NULL) {
        nob_log(NOB_ERROR, "could not get current directory: %s", strerror(errno));
        return NULL;
    }

    return buffer;
#endif // _WIN32
}

NOBDEF bool nob_set_current_dir(const char *path)
{
#ifdef _WIN32
    if (!SetCurrentDirectory(path)) {
        nob_log(NOB_ERROR, "could not set current directory to %s: %s", path, nob_win32_error_message(GetLastError()));
        return false;
    }
    return true;
#else
    if (chdir(path) < 0) {
        nob_log(NOB_ERROR, "could not set current directory to %s: %s", path, strerror(errno));
        return false;
    }
    return true;
#endif // _WIN32
}

NOBDEF char *nob_temp_dir_name(const char *path)
{
#ifndef _WIN32
    // Stolen from the musl's implementation of dirname.
    // We are implementing our own one because libc vendors cannot agree on whether dirname(3)
    // modifies the path or not.
    if (!path || !*path) return nob_temp_strdup(".");
    size_t i = strlen(path) - 1;
    for (; path[i] == '/'; i--) if (!i) return nob_temp_strdup("/");
    for (; path[i] != '/'; i--) if (!i) return nob_temp_strdup(".");
    for (; path[i] == '/'; i--) if (!i) return nob_temp_strdup("/");
    return nob_temp_strndup(path, i + 1);
#else
    if (!path) path = ""; // Treating NULL as empty.
    char *drive = (char*) nob_temp_alloc(_MAX_DRIVE);
    char *dir   = (char*) nob_temp_alloc(_MAX_DIR);
    // https://learn.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-2010/8e46eyt7(v=vs.100)
    errno_t ret = _splitpath_s(path, drive, _MAX_DRIVE, dir, _MAX_DIR, NULL, 0, NULL, 0);
    NOB_ASSERT(ret == 0);
    return nob_temp_sprintf("%s%s", drive, dir);
#endif // _WIN32
}

NOBDEF char *nob_temp_file_name(const char *path)
{
#ifndef _WIN32
    // Stolen from the musl's implementation of dirname.
    // We are implementing our own one because libc vendors cannot agree on whether basename(3)
    // modifies the path or not.
    if (!path || !*path) return nob_temp_strdup(".");
    char *s = nob_temp_strdup(path);
    size_t i = strlen(s)-1;
    for (; i&&s[i]=='/'; i--) s[i] = 0;
    for (; i&&s[i-1]!='/'; i--);
    return s+i;
#else
    if (!path) path = ""; // Treating NULL as empty.
    char *fname = (char*)nob_temp_alloc(_MAX_FNAME);
    char *ext   = (char*)nob_temp_alloc(_MAX_EXT);
    // https://learn.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-2010/8e46eyt7(v=vs.100)
    errno_t ret = _splitpath_s(path, NULL, 0, NULL, 0, fname, _MAX_FNAME, ext, _MAX_EXT);
    NOB_ASSERT(ret == 0);
    return nob_temp_sprintf("%s%s", fname, ext);
#endif // _WIN32
}

NOBDEF char *nob_temp_file_ext(const char *path)
{
#ifndef _WIN32
    return strrchr(nob_temp_file_name(path), '.');
#else
    if (!path) path = ""; // Treating NULL as empty.
    char *ext = (char*)nob_temp_alloc(_MAX_EXT);
    // https://learn.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-2010/8e46eyt7(v=vs.100)
    errno_t ret = _splitpath_s(path, NULL, 0, NULL, 0, NULL, 0, ext, _MAX_EXT);
    NOB_ASSERT(ret == 0);
    return ext;
#endif // _WIN32
}

NOBDEF char *nob_temp_running_executable_path(void)
{
#if defined(__linux__)
    char buf[4096];
    int length = readlink("/proc/self/exe", buf, NOB_ARRAY_LEN(buf));
    if (length < 0) return nob_temp_strdup("");
    return nob_temp_strndup(buf, length);
#elif defined(_WIN32)
    char buf[MAX_PATH];
    int length = GetModuleFileNameA(NULL, buf, MAX_PATH);
    return nob_temp_strndup(buf, length);
#elif defined(__APPLE__)
    char buf[4096];
    uint32_t size = NOB_ARRAY_LEN(buf);
    if (_NSGetExecutablePath(buf, &size) != 0) return nob_temp_strdup("");
    int length = strlen(buf);
    return nob_temp_strndup(buf, length);
#elif defined(__FreeBSD__)
    char buf[4096];
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
    size_t length = sizeof(buf);
    if (sysctl(mib, 4, buf, &length, NULL, 0) < 0) return nob_temp_strdup("");
    return nob_temp_strndup(buf, length);
#elif defined(__HAIKU__)
    int cookie = 0;
    image_info info;
    while (get_next_image_info(B_CURRENT_TEAM, &cookie, &info) == B_OK)
        if (info.type == B_APP_IMAGE)
            break;
    return nob_temp_strndup(info.name, strlen(info.name));
#else
    fprintf(stderr, "%s:%d: TODO: nob_temp_running_executable_path is not implemented for this platform\n", __FILE__, __LINE__);
    return nob_temp_strdup("");
#endif
}

#endif // NOB_IMPLEMENTATION

#ifndef NOB_STRIP_PREFIX_GUARD_
#define NOB_STRIP_PREFIX_GUARD_
    // NOTE: The name stripping should be part of the header so it's not accidentally included
    // several times. At the same time, it should be at the end of the file so to not create any
    // potential conflicts in the NOB_IMPLEMENTATION. The header obviously cannot be at the end
    // of the file because NOB_IMPLEMENTATION needs the forward declarations from there. So the
    // solution is to split the header into two parts where the name stripping part is at the
    // end of the file after the NOB_IMPLEMENTATION.
    #ifndef NOB_UNSTRIP_PREFIX
        #define TODO NOB_TODO
        #define TODOF NOB_TODOF
        #define UNREACHABLE NOB_UNREACHABLE
        #define UNREACHABLEF NOB_UNREACHABLEF
        #define SVLIT NOB_SVLIT
        #define SVLIT_STATIC NOB_SVLIT_STATIC
        #define UNUSED NOB_UNUSED
        #define ARRAY_LEN NOB_ARRAY_LEN
        #define ARRAY_GET NOB_ARRAY_GET
        #define INFO NOB_INFO
        #define WARNING NOB_WARNING
        #define ERROR NOB_ERROR
        #define NO_LOGS NOB_NO_LOGS
        #define Log_Level Nob_Log_Level
        #define minimal_log_level nob_minimal_log_level
        #define log_handler nob_log_handler
        #define Log_Handler Nob_Log_Handler
        #define set_log_handler nob_set_log_handler
        #define get_log_handler nob_get_log_handler
        #define null_log_handler nob_null_log_handler
        #define default_log_handler nob_default_log_handler
        #define cancer_log_handler nob_cancer_log_handler
        // NOTE: Name log is already defined in math.h and historically always was the natural logarithmic function.
        // So there should be no reason to strip the `nob_` prefix in this specific case.
        // #define log nob_log
        #define shift nob_shift
        #define shift_args nob_shift_args
        #define GO_REBUILD_URSELF NOB_GO_REBUILD_URSELF
        #define GO_REBUILD_URSELF_PLUS NOB_GO_REBUILD_URSELF_PLUS
        #define File_Paths Nob_File_Paths
        #define FILE_REGULAR NOB_FILE_REGULAR
        #define FILE_DIRECTORY NOB_FILE_DIRECTORY
        #define FILE_SYMLINK NOB_FILE_SYMLINK
        #define FILE_OTHER NOB_FILE_OTHER
        #define File_Type Nob_File_Type
        #define mkdir_if_not_exists nob_mkdir_if_not_exists
        #define copy_file nob_copy_file
        #define copy_directory_recursively nob_copy_directory_recursively
        #define read_entire_dir nob_read_entire_dir
        #define WALK_CONT NOB_WALK_CONT
        #define WALK_SKIP NOB_WALK_SKIP
        #define WALK_STOP NOB_WALK_STOP
        #define Walk_Action Nob_Walk_Action
        #define Walk_Entry Nob_Walk_Entry
        #define Walk_Func Nob_Walk_Func
        #define Walk_Dir_Opt Nob_Walk_Dir_Opt
        #define walk_dir nob_walk_dir
        #define walk_dir_opt nob_walk_dir_opt
        #define write_entire_file nob_write_entire_file
        #define get_file_type nob_get_file_type
        #define delete_file nob_delete_file
        #define Dir_Entry Nob_Dir_Entry
        #define dir_entry_open nob_dir_entry_open
        #define dir_entry_next nob_dir_entry_next
        #define dir_entry_close nob_dir_entry_close
        #define return_defer nob_return_defer
        #define da_append nob_da_append
        #define da_free nob_da_free
        #define da_append_many nob_da_append_many
        #define da_resize nob_da_resize
        #define da_reserve nob_da_reserve
        #define da_last nob_da_last
        #define da_first nob_da_first
        #define da_pop nob_da_pop
        #define da_remove_unordered nob_da_remove_unordered
        #define da_foreach nob_da_foreach
        #define fa_append nob_fa_append
        // C++ has it's own std::swap which may collide with ours
        #ifndef __cplusplus
            #define swap nob_swap
        #endif // __cplusplus
        #define String_Builder Nob_String_Builder
        #define read_entire_file nob_read_entire_file
        #define sb_appendf nob_sb_appendf
        #define sb_append_buf nob_sb_append_buf
        #define sb_append_sv nob_sb_append_sv
        #define sb_append_cstr nob_sb_append_cstr
        #define sb_append_null nob_sb_append_null
        #define sb_append nob_sb_append
        #define sb_pad_align nob_sb_pad_align
        #define sb_free nob_sb_free
        #define Proc Nob_Proc
        #define INVALID_PROC NOB_INVALID_PROC
        #define Fd Nob_Fd
        #define Pipe Nob_Pipe
        #define pipe_create nob_pipe_create
        #define Chain Nob_Chain
        #define Chain_Begin_Opt Nob_Chain_Begin_Opt
        #define chain_begin nob_chain_begin
        #define chain_begin_opt nob_chain_begin_opt
        #define Chain_Cmd_Opt Nob_Chain_Cmd_Opt
        #define chain_cmd nob_chain_cmd
        #define chain_cmd_opt nob_chain_cmd_opt
        #define Chain_End_Opt Nob_Chain_End_Opt
        #define chain_end nob_chain_end
        #define chain_end_opt nob_chain_end_opt
        #define INVALID_FD NOB_INVALID_FD
        #define fd_open_for_read nob_fd_open_for_read
        #define fd_open_for_write nob_fd_open_for_write
        #define fd_close nob_fd_close
        #define Procs Nob_Procs
        #define proc_wait nob_proc_wait
        #define procs_wait nob_procs_wait
        #define procs_wait_and_reset nob_procs_wait_and_reset
        #define procs_append_with_flush nob_procs_append_with_flush
        #define procs_flush nob_procs_flush
        #define CLIT NOB_CLIT
        #define Cmd Nob_Cmd
        #define Cmd_Redirect Nob_Cmd_Redirect
        #define Cmd_Opt Nob_Cmd_Opt
        #define cmd_run_opt nob_cmd_run_opt
        #define cmd_run nob_cmd_run
        #define cmd_render nob_cmd_render
        #define cmd_append nob_cmd_append
        #define cmd_extend nob_cmd_extend
        #define cmd_free nob_cmd_free
        #define cmd_run_async nob_cmd_run_async
        #define cmd_run_async_and_reset nob_cmd_run_async_and_reset
        #define cmd_run_async_redirect nob_cmd_run_async_redirect
        #define cmd_run_async_redirect_and_reset nob_cmd_run_async_redirect_and_reset
        #define cmd_run_sync nob_cmd_run_sync
        #define cmd_run_sync_and_reset nob_cmd_run_sync_and_reset
        #define cmd_run_sync_redirect nob_cmd_run_sync_redirect
        #define cmd_run_sync_redirect_and_reset nob_cmd_run_sync_redirect_and_reset
        #define temp_strdup nob_temp_strdup
        #define temp_strndup nob_temp_strndup
        #define temp_alloc nob_temp_alloc
        #define temp_sprintf nob_temp_sprintf
        #define temp_vsprintf nob_temp_vsprintf
        #define temp_reset nob_temp_reset
        #define temp_save nob_temp_save
        #define temp_rewind nob_temp_rewind
        #define path_name nob_path_name
        // NOTE: rename(2) is widely known POSIX function. We never wanna collide with it.
        // #define rename nob_rename
        #define needs_rebuild nob_needs_rebuild
        #define needs_rebuild1 nob_needs_rebuild1
        #define file_exists nob_file_exists
        #define get_current_dir_temp nob_get_current_dir_temp
        #define set_current_dir nob_set_current_dir
        #define temp_dir_name nob_temp_dir_name
        #define temp_file_name nob_temp_file_name
        #define temp_file_ext nob_temp_file_ext
        #define temp_running_executable_path nob_temp_running_executable_path
        #define String_View Nob_String_View
        #define temp_sv_to_cstr nob_temp_sv_to_cstr
        #define sv_chop_by_delim nob_sv_chop_by_delim
        #define sv_chop_while nob_sv_chop_while
        #define sv_chop_prefix nob_sv_chop_prefix
        #define sv_chop_suffix nob_sv_chop_suffix
        #define sv_chop_left nob_sv_chop_left
        #define sv_chop_right nob_sv_chop_right
        #define sv_trim nob_sv_trim
        #define sv_trim_left nob_sv_trim_left
        #define sv_trim_right nob_sv_trim_right
        #define sv_eq nob_sv_eq
        #define sv_starts_with nob_sv_starts_with
        #define sv_end_with nob_sv_end_with
        #define sv_ends_with nob_sv_ends_with
        #define sv_ends_with_cstr nob_sv_ends_with_cstr
        #define sv_from_cstr nob_sv_from_cstr
        #define sv_from_parts nob_sv_from_parts
        #define sv_utf8_len nob_sv_utf8_len
        #define bytes_for_utf8 nob_bytes_for_utf8
        #define sb_to_sv nob_sb_to_sv
        #define win32_error_message nob_win32_error_message
        #define nprocs nob_nprocs
        #define nanos_since_unspecified_epoch nob_nanos_since_unspecified_epoch
        #define NANOS_PER_SEC NOB_NANOS_PER_SEC
    #endif // NOB_STRIP_PREFIX
#endif // NOB_STRIP_PREFIX_GUARD_

/*
   Revision history:

     3.10.1 (2026-10-28) Set _POSIX_C_SOURCE to 200809L for Unix environments (by @aalmkainzi)
                         Make nob_cc check for clang first on _WIN32 (by @aalmkainzi)
     3.10.0 (2026-07-17) Make NOB_SVLIT a bit more usable at compile-time (by @Arhcout)
                         Add NOB_SVLIT_STATIC for when a compiler simply refuses to accept NOB_SVLIT() at compile-time (looking at you `cl.exe /TC`) (by @rexim)
      3.9.0 (2026-07-15) Add NOB_TODOF() and NOB_UNREACHABLEF()
                         Add NOB_SVLIT()
                         Add nob_bytes_for_utf8[] and nob_sv_utf8_len()
                         Add String_View.items alias to String_View.data
                         Make nob_cmd_run log the location of its failure
                         Make nob_default_log_handler and nob_cancer_log_handler work better in multi-threaded or multi-process environments (by @ChenXinyu-CHD)
                         Do not strip the prefix for nob_swap on C++ (by @LoganjdM)
      3.8.3 (2026-07-14) Fix "applying zero offset to null pointer" error by clang's UndefinedBehaviorSanitizer
      3.8.2 (2026-04-01) Fix the broken type safety of nob_cmd_append() (by @aalmkainzi)
      3.8.1 (2026-04-01) Fix annoying clang warning
      3.8.0 (2026-03-24) Add NOB_CLIT()
                         Fix compliation on MSVC with /TP
      3.7.0 (2026-03-20) Add nob_sb_append_sv()
      3.6.0 (2026-03-16) Add nob_sv_chop_suffix()
                         Deprecate nob_sv_end_with()
                         Add nob_sv_ends_with_cstr() instead of nob_sv_end_with()
                         Add nob_sv_ends_with()
                         Add nob_sv_chop_right()
      3.5.0 (2026-03-13) Add nob_null_log_handler() (by @rexim)
                         Rename nob_log_handler to Nob_Log_Handler (by @rexim)
      3.4.0 (2026-03-12) Add nob_da_first() (by @rexim)
                         Add nob_da_pop() (by @rexim)
                         Add nob_sv_chop_while() (by @rexim)
      3.3.0 (2026-03-07) Add nob_sv_chop_prefix() (by @rexim)
      3.2.2 (2026-02-06) Fix read_entire_dir crash on empty path (by @ysoftware)
      3.2.1 (2026-01-29) Fix the implicit declaration error when nob is included as a header (by @ysoftware)
      3.2.0 (2026-01-28) Introduce Chain API
                           - Nob_Chain
                           - Nob_Chain_Begin_Opt
                           - nob_chain_begin()
                           - nob_chain_begin_opt()
                           - Nob_Chain_Cmd_Opt
                           - nob_chain_cmd()
                           - nob_chain_cmd_opt()
                           - Nob_Chain_End_Opt
                           - nob_chain_end()
                           - nob_chain_end_opt()
                         Introduce some auxiliary things that were used in Chain API implementation, but might be useful outside of it:
                           - Nob_Pipe
                           - nob_pipe_create()
                           - nob_fa_append()
      3.1.0 (2026-01-22) Make nob_delete_file() be able to delete empty dir on Windows (by @rexim)
                         Introduce Directory Entry API - similar to POSIX dirent but with names that don't collide
                           - Nob_Dir_Entry
                           - nob_dir_entry_open()
                           - nob_dir_entry_next()
                           - nob_dir_entry_close()
                         Rewrite Directory Walking API using Directory Entry API
                         Introduce .post_order parameter to Nob_Walk_Dir_Opt which walks the directories in post order starting from leaf files
                         Rewrite nob_read_entire_dir() using Directory Entry API
      3.0.0 (2026-01-13) Improve C++ support (by @rexim)
                           - Fix various C++ compilers warnings and complains throughout the code.
                           - Reimplement nob_cmd_append() without taking a pointer to temporary array (some C++ compilers don't like that)
                           - Make default NOB_REBUILD_URSELF() try to recompile with C++ if __cplusplus macro is defined
                         Strip prefixes by default (by @rexim)
                           - Ignore NOB_STRIP_PREFIX macro
                           - Introduce NOB_UNSTRIP_PREFIX macro
                           BACKWARD INCOMPATIBLE CHANGE!!! If you had code that intentionally didn't enable NOB_STRIP_PREFIX
                           because all the names from nob.h were causing too many collisions for you, upgrading to 3.0.0 may break it.
                           In that case you should go and explicitly enable NOB_UNSTRIP_PREFIX where needed after upgrading.
                         Add nob_sb_append alias to nob_da_append (by @rexim)
      2.0.1 (2026-01-07) Fix Walk_Entry naming (by @Sinha-Ujjawal)
                         Using single String Builder in nob__walk_dir_opt_impl (by @Sinha-Ujjawal)
                         Add tcc to nob_cc_*() and NOB_REBUILD_URSELF() macros (by @vylsaz)
                         Fix building nob_read_entire_file() with tcc on windows  (by @vylsaz)
                         Fix Y2038 in nob_needs_rebuild() (by @lnvitesace)
      2.0.0 (2026-01-06) Remove minirent.h (by @rexim)
                           BACKWARD INCOMPATIBLE CHANGE!!! If you were using minirent.h from this library
                           just use it directly from https://github.com/tsoding/minirent
                           or consider using the New Directory Walking API.
                         Introduce New Directory Walking API (by @rexim)
                           - NOB_WALK_CONT
                           - NOB_WALK_SKIP
                           - NOB_WALK_STOP
                           - Nob_Walk_Action
                           - Nob_Walk_Entry
                           - Nob_Walk_Func
                           - Nob_Walk_Dir_Opt
                           - nob_walk_dir()
                           - nob_walk_dir_opt()
                         Add support for Haiku to nob_temp_running_executable_path() (By @Cephon)
                         Make nob_file_exists() unfailable (By @rexim)
     1.27.0 (2025-12-30) Add .dont_reset option to cmd_run (by @Israel77)
                         Fix support for FreeBSD (by @cqundefine)
                         Strip prefixes from NOB_GO_REBUILD_URSELF and NOB_GO_REBUILD_URSELF_PLUS (by @huwwa)
                         Add /Fo flag to MSVC version of nob_cc_output() (by @ratchetfreak)
     1.26.0 (2025-12-28) Introduce customizable log handlers (by @rexim)
                           - Add nob_log_handler
                           - Add nob_set_log_handler
                           - Add nob_get_log_handler
                           - Add nob_default_log_handler
                           - Add nob_cancer_log_handler
                         Introduce nob_temp_vsprintf (by @rexim)
                         Fix compilation error on Windows when NOB_NO_ECHO is enabled (by @mlorenc227)
                         Do not redefine _CRT_SECURE_NO_WARNINGS if it's already defined (by @vylsaz)
     1.25.1 (2025-11-06) Fix forward declaration of _NSGetExecutablePath on MacOS (by @agss0)
     1.25.0 (2025-10-25)   - Add nob_sb_pad_align()
                           - Add nob_swap()
                           - Add nob_temp_strndup()
                           - Add nob_temp_dir_name()
                           - Add nob_temp_file_name()
                           - Add nob_temp_file_ext()
                           - Add nob_temp_running_executable_path()
     1.24.0 (2025-10-23) Introduce NOB_NO_ECHO macro flag (@rexim)
     1.23.0 (2025-08-22) Introduce new API for running commands (by @rexim, @programmerlexi, @0x152a)
                           - Add nob_cmd_run()
                           - Add nob_cmd_run_opt()
                           - Add struct Nob_Cmd_Opt
                           - Add nob_procs_flush()
                           - Add nob_nprocs()
                         Deprecate old API for running commands. (by @rexim)
                           We do not plan to delete this API any time, but we believe that the new one is more convenient.
                           - Deprecate struct Nob_Cmd_Redirect{} (it's not explicitly marked with NOB_DEPRECATED, but functions that use it are)
                           - Turn nob_cmd_run_async() into a function (otherwise it's not deprecatable with NOB_DEPRECATED)
                           - Deprecate nob_cmd_run_async()
                           - Deprecate nob_cmd_run_async_and_reset()
                           - Deprecate nob_cmd_run_async_redirect()
                           - Deprecate nob_cmd_run_async_redirect_and_reset()
                           - Deprecate nob_cmd_run_sync()
                           - Deprecate nob_cmd_run_sync_and_reset()
                           - Deprecate nob_cmd_run_sync_redirect()
                           - Deprecate nob_cmd_run_sync_redirect_and_reset()
                           - Deprecate nob_procs_append_with_flush()
                           - Deprecate nob_procs_wait_and_reset()
                         Introduce deprecation mechanism (by @yuI4140, @rexim)
                           By default, deprecation warnings are not reported. You have to #define NOB_WARN_DEPRECATED to enable them.
                           - Add NOB_DEPRECATED()
                           - Add NOB_WARN_DEPRECATED
                         Add NOB_DECLTYPE_CAST() for C++-compatible casting of allocation results (by @rexim)
                         Introduce basic performance measuring mechanism (By @mikmart)
                           - Add nob_nanos_since_unspecified_epoch()
                           - Add NOB_NANOS_PER_SEC
     1.22.0 (2025-08-12) Add NOBDEF macro to the beginning of function declarations (by @minefreak19)
                         Add more flags to MSVC nob_cc_flags() (by @PieVieRo)
     1.21.0 (2025-08-11) Add NOB_NO_MINIRENT guard for "minirent.h" (by @fietec)
     1.20.9 (2025-08-11) Fix warnings on Windows: Define _CRT_SECURE_NO_WARNINGS, Rename mkdir to _mkdir (by @OetkenPurveyorOfCode)
     1.20.8 (2025-08-11) Fix the bug with nob_get_file_type() not identifying symlinks correctly on POSIX (By @samuellieberman)
     1.20.7 (2025-07-29) Align nob_temp_alloc() allocations by the word size (By @rexim)
     1.20.6 (2025-05-16) Never strip nob_* suffix from nob_rename (By @rexim)
     1.20.5 (2025-05-16) NOB_PRINTF_FORMAT() support for MinGW (By @KillerxDBr)
     1.20.4 (2025-05-16) More reliable rendering of the Windows command (By @vylsaz)
     1.20.3 (2025-05-16) Add check for __clang__ along with _MSC_VER checks (By @nashiora)
     1.20.2 (2025-04-24) Report the program name that failed to start up in nob_cmd_run_async_redirect() (By @rexim)
     1.20.1 (2025-04-16) Use vsnprintf() in nob_sb_appendf() instead of vsprintf() (By @LainLayer)
     1.20.0 (2025-04-16) Introduce nob_cc(), nob_cc_flags(), nob_cc_inputs(), nob_cc_output() macros (By @rexim)
     1.19.0 (2025-03-25) Add nob_procs_append_with_flush() (By @rexim and @anion155)
     1.18.0 (2025-03-24) Add nob_da_foreach() (By @rexim)
                         Allow file sizes greater than 2GB to be read on windows (By @satchelfrost and @KillerxDBr)
                         Fix nob_fd_open_for_write behaviour on windows so it truncates the opened files (By @twixuss)
     1.17.0 (2025-03-16) Factor out nob_da_reserve() (By @rexim)
                         Add nob_sb_appendf() (By @angelcaru)
     1.16.1 (2025-03-16) Make nob_da_resize() exponentially grow capacity similar to no_da_append_many()
     1.16.0 (2025-03-16) Introduce NOB_PRINTF_FORMAT
     1.15.1 (2025-03-16) Make nob.h compilable in gcc/clang with -std=c99 on POSIX. This includes:
                           not using strsignal()
                           using S_IS* stat macros instead of S_IF* flags
     1.15.0 (2025-03-03) Add nob_sv_chop_left()
     1.14.1 (2025-03-02) Add NOB_EXPERIMENTAL_DELETE_OLD flag that enables deletion of nob.old in Go Rebuild Urself™ Technology
     1.14.0 (2025-02-17) Add nob_da_last()
                         Add nob_da_remove_unordered()
     1.13.1 (2025-02-17) Fix segfault in nob_delete_file() (By @SileNce5k)
     1.13.0 (2025-02-11) Add nob_da_resize() (By @satchelfrost)
     1.12.0 (2025-02-04) Add nob_delete_file()
                         Add nob_sv_start_with()
     1.11.0 (2025-02-04) Add NOB_GO_REBUILD_URSELF_PLUS() (By @rexim)
     1.10.0 (2025-02-04) Make NOB_ASSERT, NOB_REALLOC, and NOB_FREE redefinable (By @OleksiiBulba)
      1.9.1 (2025-02-04) Fix signature of nob_get_current_dir_temp() (By @julianstoerig)
      1.9.0 (2024-11-06) Add Nob_Cmd_Redirect mechanism (By @rexim)
                         Add nob_path_name() (By @0dminnimda)
      1.8.0 (2024-11-03) Add nob_cmd_extend() (By @0dminnimda)
      1.7.0 (2024-11-03) Add nob_win32_error_message and NOB_WIN32_ERR_MSG_SIZE (By @KillerxDBr)
      1.6.0 (2024-10-27) Add nob_cmd_run_sync_and_reset()
                         Add nob_sb_to_sv()
                         Add nob_procs_wait_and_reset()
      1.5.1 (2024-10-25) Include limits.h for Linux musl libc (by @pgalkin)
      1.5.0 (2024-10-23) Add nob_get_current_dir_temp()
                         Add nob_set_current_dir()
      1.4.0 (2024-10-21) Fix UX issues with NOB_GO_REBUILD_URSELF on Windows when you call nob without the .exe extension (By @pgalkin)
                         Add nob_sv_end_with (By @pgalkin)
      1.3.2 (2024-10-21) Fix unreachable error in nob_log on passing NOB_NO_LOGS
      1.3.1 (2024-10-21) Fix redeclaration error for minimal_log_level (By @KillerxDBr)
      1.3.0 (2024-10-17) Add NOB_UNREACHABLE
      1.2.2 (2024-10-16) Fix compilation of nob_cmd_run_sync_and_reset on Windows (By @KillerxDBr)
      1.2.1 (2024-10-16) Add a separate include guard for NOB_STRIP_PREFIX.
      1.2.0 (2024-10-15) Make NOB_DA_INIT_CAP redefinable
                         Add NOB_STRIP_PREFIX which strips off nob_* prefix from all the user facing names
                         Add NOB_UNUSED macro
                         Add NOB_TODO macro
                         Add nob_sv_trim_left and nob_sv_trim_right declarations to the header part
      1.1.1 (2024-10-15) Remove forward declaration for is_path1_modified_after_path2
      1.1.0 (2024-10-15) nob_minimal_log_level
                         nob_cmd_run_sync_and_reset
      1.0.0 (2024-10-15) first release based on https://github.com/tsoding/musializer/blob/4ac7cce9874bc19e02d8c160c8c6229de8919401/nob.h
*/

/*
   Version Conventions:

      We are following https://semver.org/ so the version has a format MAJOR.MINOR.PATCH:
      - Modifying comments does not update the version.
      - PATCH is incremented in case of a bug fix or refactoring without touching the API.
      - MINOR is incremented when new functions and/or types are added in a way that does
        not break any existing user code. We want to do this in the majority of the situation.
        If we want to delete a certain function or type in favor of another one we should
        just add the new function/type and deprecate the old one in a backward compatible way
        and let them co-exist for a while.
      - MAJOR update should be just a periodic cleanup of the DEPRECATED functions and types
        without really modifying any existing functionality.
      - Breaking backward compatibility in a MINOR release should be considered a bug and
        should be promptly fixed in the next PATCH release.

   API conventions:

      - All the user facing names should be prefixed with `nob_`, `NOB_`, or `Nob_` depending on the case.
      - The prefixes of non-redefinable names should be stripped in NOB_STRIP_PREFIX_GUARD_ section,
        unless explicitly stated otherwise like in case of nob_log() or nob_rename().
      - Internal (private) names should be prefixed with `nob__` (double underscore). The user code is discouraged
        from using such names since they are allowed to be broken in a backward incompatible way even in PATCH
        releases. (This is why they are internal)
      - If a public macro uses an private function internally such function must be forward declared in the NOB_H_
        section.
*/

/*
   ------------------------------------------------------------------------------
   This software is available under 2 licenses -- choose whichever you prefer.
   ------------------------------------------------------------------------------
   ALTERNATIVE A - MIT License
   Copyright (c) 2024 Alexey Kutepov
   Permission is hereby granted, free of charge, to any person obtaining a copy of
   this software and associated documentation files (the "Software"), to deal in
   the Software without restriction, including without limitation the rights to
   use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
   of the Software, and to permit persons to whom the Software is furnished to do
   so, subject to the following conditions:
   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
   ------------------------------------------------------------------------------
   ALTERNATIVE B - Public Domain (www.unlicense.org)
   This is free and unencumbered software released into the public domain.
   Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
   software, either in source code form or as a compiled binary, for any purpose,
   commercial or non-commercial, and by any means.
   In jurisdictions that recognize copyright laws, the author or authors of this
   software dedicate any and all copyright interest in the software to the public
   domain. We make this dedication for the benefit of the public at large and to
   the detriment of our heirs and successors. We intend this dedication to be an
   overt act of relinquishment in perpetuity of all present and future rights to
   this software under copyright law.
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
   WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
   ------------------------------------------------------------------------------
*/
/* ======== end of upstream nob.h ======== */

/* The build's own files go through snakepath, and trim.h cuts snakepath.h down for the site's export */
#ifdef _WIN32
#define SP_PATH_MAX SP_PATH_MAX_WINDOWS
#else
#define SP_PATH_MAX SP_PATH_MAX_LINUX
#endif
#define SNAKEPATH_IMPLEMENTATION
#include "snakepath.h"
#define TRIM_IMPLEMENTATION
#include "site/trim.h"

#define TEXT_MAX (1 << 20) /* the largest file the build reads whole */

static bool exists(const char *path) {
    SpPath p = sp_path(path);

    return sp_is(&p, SP_ANY, true);
}

/* path and the directories above it, unless it is there */
static bool make_dir(const char *path) {
    SpPath p = sp_path(path);
    SpMkdirOptions parents = {true, true, SP_MODE_DIR};
    SpError err = sp_mkdir(&p, SP_MODE_DIR, parents);

    if (err != SP_OK)
        nob_log(NOB_ERROR, "could not make %s: %s", path, sp_error_str(err));
    return err == SP_OK;
}

/* path's text in buf, which has room for size bytes, followed by a NUL */
static bool read_text(const char *path, char *buf, size_t size, size_t *len) {
    SpPath p = sp_path(path);
    SpIOResult r = sp_read_file(&p, buf, size - 1);

    if (r.error != SP_OK) {
        nob_log(NOB_ERROR, "could not read %s: %s", path, sp_error_str(r.error));
        return false;
    }
    buf[r.bytes] = '\0';
    *len = r.bytes;
    return true;
}

static bool write_text(const char *path, const char *text, size_t len) {
    SpPath p = sp_path(path);
    SpIOResult r = sp_write_file(&p, text, len);

    if (r.error != SP_OK)
        nob_log(NOB_ERROR, "could not write %s: %s", path, sp_error_str(r.error));
    return r.error == SP_OK;
}

/* Verbose mode - set VERBOSE=0 to suppress INFO logs */
static bool verbose = true;
#define LOG_INFO(...) do { if (verbose) nob_log(NOB_INFO, __VA_ARGS__); } while(0)

#define BASE_WARNINGS \
    "-Wall", "-Wextra", "-Wpedantic", "-Werror", \
    "-Wconversion", "-Wsign-conversion", "-Wshadow", \
    "-Wdouble-promotion", "-Wundef", "-Wwrite-strings", \
    "-Wcast-qual", "-Wcast-align", "-Wpointer-arith", \
    "-Wnull-dereference", "-Wformat=2", "-Wvla"

#define C_ONLY_WARNINGS \
    "-Wstrict-prototypes", "-Wmissing-prototypes", \
    "-Wold-style-definition"

#define GCC_WARNINGS \
    "-Wformat-overflow=2", "-Wformat-truncation=2", \
    "-Wlogical-op", "-Wduplicated-cond", \
    "-Wduplicated-branches", "-Wrestrict"

#define GCC_C_WARNINGS \
    "-Wjump-misses-init"

#define CLANG_EVERYTHING \
    "-Weverything", \
    "-Wno-disabled-macro-expansion", "-Wno-padded", \
    "-Wno-covered-switch-default", "-Wno-unknown-warning-option", \
    "-Wno-unsafe-buffer-usage", "-Wno-poison-system-directories"

#define CLANG_C_EXCLUSIONS \
    "-Wno-declaration-after-statement"

#define CLANG_CPP_EXCLUSIONS \
    "-Wno-c++98-compat", "-Wno-c++98-compat-pedantic"

typedef enum {
    COMPILER_GCC,
    COMPILER_CLANG,
    COMPILER_GPP,
    COMPILER_CLANGPP,
#ifdef _WIN32
    COMPILER_MSVC,
    COMPILER_MSVC_CPP,
#endif
} Compiler;

typedef struct {
    Compiler compiler;
    bool sanitizers;
    bool fluent; /* also build and run the fluent API tests (-DSNAKEPATH_FLUENT) */
    const char *name;
    const char *output;
} BuildConfig;

static void append_warnings(Nob_Cmd *cmd, Compiler compiler) {
    switch (compiler) {
    case COMPILER_GCC:
        nob_cmd_append(cmd, BASE_WARNINGS, C_ONLY_WARNINGS, GCC_WARNINGS, GCC_C_WARNINGS);
        break;
    case COMPILER_CLANG:
        nob_cmd_append(cmd, BASE_WARNINGS, C_ONLY_WARNINGS);
        nob_cmd_append(cmd, CLANG_EVERYTHING, CLANG_C_EXCLUSIONS);
        break;
    case COMPILER_GPP:
        nob_cmd_append(cmd, BASE_WARNINGS, GCC_WARNINGS);
        break;
    case COMPILER_CLANGPP:
        nob_cmd_append(cmd, BASE_WARNINGS);
        nob_cmd_append(cmd, CLANG_EVERYTHING, CLANG_CPP_EXCLUSIONS);
        break;
#ifdef _WIN32
    case COMPILER_MSVC:
        nob_cmd_append(cmd, "/W4", "/WX");
        break;
    case COMPILER_MSVC_CPP:
        nob_cmd_append(cmd, "/W4", "/WX", "/EHsc");
        break;
#endif
    }
}

/* Build source file, optionally async. Returns true if command was started/completed. */
static bool build_source_async(BuildConfig cfg, const char *source, Nob_Procs *procs) {
    Nob_Cmd cmd = {0};

    switch (cfg.compiler) {
    case COMPILER_GCC:
        nob_cmd_append(&cmd, "gcc", "-std=c99");
        break;
    case COMPILER_CLANG:
        nob_cmd_append(&cmd, "clang", "-std=c99");
        break;
    case COMPILER_GPP:
        nob_cmd_append(&cmd, "g++", "-std=c++11", "-x", "c++");
        break;
    case COMPILER_CLANGPP:
        nob_cmd_append(&cmd, "clang++", "-std=c++11", "-x", "c++");
        break;
#ifdef _WIN32
    case COMPILER_MSVC:
        nob_cmd_append(&cmd, "cl.exe", "/std:c11");
        break;
    case COMPILER_MSVC_CPP:
        nob_cmd_append(&cmd, "cl.exe", "/std:c++14", "/TP");
        break;
#endif
    }

    append_warnings(&cmd, cfg.compiler);

    if (cfg.fluent) {
        nob_cmd_append(&cmd, "-DSNAKEPATH_FLUENT");
    }

#ifdef _WIN32
    if (cfg.compiler == COMPILER_MSVC || cfg.compiler == COMPILER_MSVC_CPP) {
        nob_cmd_append(&cmd, "/Od", "/Zi");
        /* Use /Fd and /Fo to give each build its own PDB and OBJ file for parallel compilation */
        nob_cmd_append(&cmd, nob_temp_sprintf("/Fd:%.*s.pdb", (int)(strlen(cfg.output) - 4), cfg.output));
        nob_cmd_append(&cmd, nob_temp_sprintf("/Fo%.*s.obj", (int)(strlen(cfg.output) - 4), cfg.output));
        nob_cmd_append(&cmd, "/Fe:", cfg.output);
        /* The tests' main holds hundreds of paths in one frame, past Windows' 1 MB default stack at /Od */
        nob_cmd_append(&cmd, source, "/link", "/STACK:8388608");
        return nob_cmd_run(&cmd, .async = procs);
    } else
#endif
    {
        if (cfg.sanitizers) {
            nob_cmd_append(&cmd, "-fsanitize=address,undefined");
            nob_cmd_append(&cmd, "-fsanitize=pointer-compare,pointer-subtract");
            nob_cmd_append(&cmd, "-fsanitize=float-divide-by-zero,float-cast-overflow");
            nob_cmd_append(&cmd, "-fsanitize-address-use-after-scope");
            nob_cmd_append(&cmd, "-fno-omit-frame-pointer");
            nob_cmd_append(&cmd, "-fno-common");
        }
        nob_cmd_append(&cmd, "-g", "-O0");
        nob_cmd_append(&cmd, "-o", cfg.output);
    }

    nob_cmd_append(&cmd, source);
    return nob_cmd_run(&cmd, .async = procs);  /* NULL procs runs synchronously */
}

static bool run_test_async(const char *exe, Nob_Procs *procs) {
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, exe);
    return nob_cmd_run(&cmd, .async = procs);
}

#ifndef _WIN32
static bool run_valgrind(const char *exe) {
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "valgrind", "--leak-check=full", "--error-exitcode=1", "--track-origins=yes", exe);
    return nob_cmd_run(&cmd);
}
#endif

/* SNAKEPATH_SKIP_GCC keeps only the clang builds, as on macOS, FreeBSD and OpenBSD, whose compiler is clang (gcc
 * there is clang or missing), and on Termux; sanitizer builds need SNAKEPATH_SANITIZE */
static bool skip_gcc, use_sanitizers;

static bool config_enabled(BuildConfig cfg) {
    if (skip_gcc && (cfg.compiler == COMPILER_GCC || cfg.compiler == COMPILER_GPP)) return false;
    return !cfg.sanitizers || use_sanitizers;
}

static const char *all_artifacts[] = {
#ifdef _WIN32
    "test_msvc.exe", "test_msvc_cpp.exe", "test_fluent_msvc.exe", "api_demo.exe",
    /* PDB and obj files from MSVC */
    "test_msvc.pdb", "test_msvc_cpp.pdb", "test_fluent_msvc.pdb", "api_demo.pdb",
    "test_msvc.obj", "test_msvc_cpp.obj", "test_fluent_msvc.obj", "api_demo.obj",
#else
    "test_gcc", "test_clang", "test_gcc_san", "test_clang_san",
    "test_gpp", "test_clangpp", "test_fluent_gcc", "test_fluent_clang",
    "api_demo",
#endif
    NULL
};

/* Run Python tests */
static const char *find_python(void) {
#ifdef _WIN32
    return "python";
#else
    /* python3 unless only /usr/bin/python exists */
    return !exists("/usr/bin/python3") && exists("/usr/bin/python") ? "python" : "python3";
#endif
}

/* The Python bindings' extension module (_snakepath, test.c with -DSP_PYTHON), which snakepath.py builds for the
 * Python that runs it: that Python's headers, extension suffix and, on Windows, cl.exe */
static bool build_python_lib(Nob_Procs *procs) {
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, find_python(), "snakepath.py", "--build");
    return nob_cmd_run(&cmd, .async = procs);
}

/* Call depth, README embedding api_demo.c, and CPython's pathlib tests on the bindings */
static bool run_python(void) {
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, find_python(), "snakepath.py");
    return nob_cmd_run(&cmd);
}

/* snakepath.h's layout, for clang-format 21 (CI pins it): LLVM style at a 4-space indent and 120 columns, where
   every if/for/while body gets its own line, so line counts can't be shrunk by cramming statements together.
   Blank lines stay the author's call: they go where they separate ideas, not in front of every block. */
#define SNAKEPATH_LAYOUT                                                                                               \
    "--style={BasedOnStyle: LLVM, IndentWidth: 4, ColumnLimit: 120, AllowShortIfStatementsOnASingleLine: Never, "     \
    "AllowShortLoopsOnASingleLine: false, AllowShortBlocksOnASingleLine: Never, SortIncludes: Never, "                \
    "ReflowComments: Never}"

/* Rewrite snakepath.h, trim.h and logo.c in that layout (./nob format), or check that they already are */
static bool run_clang_format(bool rewrite) {
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "clang-format", SNAKEPATH_LAYOUT);
    if (rewrite)
        nob_cmd_append(&cmd, "-i");
    else
        nob_cmd_append(&cmd, "--dry-run", "--Werror");
    nob_cmd_append(&cmd, "snakepath.h", "site/trim.h", "site/logo.c");
    return nob_cmd_run(&cmd);
}


/* path, a file or a directory with what is in it, unless it is gone already */
static bool delete_if_exists(const char *path) {
    SpPath p = sp_path(path);
    SpRemoveOptions tree = {false, true, true};

    return sp_remove(&p, tree) == SP_OK;
}

/* ./nob diff [ref]: the pure path API of the working tree's snakepath.h against the header at a git ref (default
   origin/main), through test.c's SP_DIFF driver, which -I points at one header or the other: every result for
   exhaustive short strings, token sequences and random strings, at SP_PATH_MAX 64 and 16, one hash per line. Each set
   is split across the cores (a driver process takes every nth line), and the outputs are compared while the drivers
   run, so a set's first difference is shown in full as soon as both sides have it. The base side's drivers and outputs
   stay in DIFF_BASE_DIR and are reused while the base header, test.c and the build commands are the same. An
   intentional behavior change differs by design, so CI runs this as an informational job. */
#define DIFF_BASE_DIR "diff_base"
#define DIFF_HEAD_DIR "diff_head"
#define DIFF_RECORD 17     /* one line of a driver's output: a hash in 16 hex digits and '\n' */
#define DIFF_SHARDS 16     /* parts per input set and side: fixed, so the cached base side fits any machine */
static const char *diff_specs[][2] = {{"exhaustive", "5"}, {"tokens", "3"}, {"random", "100000"}};
static const char *diff_sizes[] = {"64", "16"};
static const char *diff_dirs[] = {DIFF_BASE_DIR, DIFF_HEAD_DIR};

static const char *diff_exe(size_t side, size_t s) {
#ifdef _WIN32
    return nob_temp_sprintf("%s\\driver_%s.exe", diff_dirs[side], diff_sizes[s]);
#else
    return nob_temp_sprintf("./%s/driver_%s", diff_dirs[side], diff_sizes[s]);
#endif
}

static const char *diff_out(size_t side, size_t s, size_t spec, int shard) {
    return nob_temp_sprintf("%s/%s_%s_%d.txt", diff_dirs[side], diff_sizes[s], diff_specs[spec][0], shard);
}

/* The header at ref, as DIFF_BASE_DIR/snakepath.h */
static bool write_base_header(const char *ref) {
    Nob_Cmd cmd = {0};
    SpPath header = sp_path(DIFF_BASE_DIR "/snakepath.h");

    if (!make_dir(DIFF_BASE_DIR) || !make_dir(DIFF_HEAD_DIR))
        return false;
    nob_cmd_append(&cmd, "git", "show", nob_temp_sprintf("%s:snakepath.h", ref));
    if (!nob_cmd_run(&cmd, .stdout_path = DIFF_BASE_DIR "/snakepath.h") || sp_stat(&header, true).sp_size <= 0) {
        nob_log(NOB_ERROR, "git show %s:snakepath.h failed (fetch the ref first?)", ref);
        return false;
    }
    return true;
}

/* test.c as the SP_DIFF driver at SP_PATH_MAX size, against the base header (side 0) or the working tree's */
static void diff_driver_cmd(size_t side, size_t s, Nob_Cmd *cmd) {
    const char *include_dir = side == 0 ? DIFF_BASE_DIR : ".";
    const char *output = diff_exe(side, s);
#ifdef _WIN32
    nob_cmd_append(cmd, "cl.exe", "/std:c11", "/W4", "/O2", "/DSP_DIFF");
    nob_cmd_append(cmd, nob_temp_sprintf("/DSP_PATH_MAX=%s", diff_sizes[s]), nob_temp_sprintf("/I%s", include_dir));
    nob_cmd_append(cmd, nob_temp_sprintf("/Fo%.*s.obj", (int)(strlen(output) - 4), output), "/Fe:", output);
#else
    nob_cmd_append(cmd, skip_gcc ? "clang" : "gcc", "-std=c99", "-O2", "-Wall", "-Wextra", "-DSP_DIFF");
    nob_cmd_append(cmd, nob_temp_sprintf("-DSP_PATH_MAX=%s", diff_sizes[s]), "-I", include_dir, "-o", output);
#endif
    nob_cmd_append(cmd, "test.c");
}

/* FNV-1a over bytes, continuing from h */
static unsigned long long diff_hash(unsigned long long h, const char *data, size_t len) {
    for (size_t i = 0; i < len; i++)
        h = (h ^ (unsigned char)data[i]) * 1099511628211ULL;
    return h;
}

/* What the base side's outputs depend on: the base header, test.c, the build commands and the split */
static unsigned long long diff_base_key(int shards) {
    unsigned long long h = 14695981039346656037ULL;
    const char *files[] = {DIFF_BASE_DIR "/snakepath.h", "test.c"};
    for (size_t i = 0; i < 2; i++) {
        static char text[TEXT_MAX];
        size_t len = 0;
        if (read_text(files[i], text, sizeof(text), &len))
            h = diff_hash(h, text, len);
    }
    for (size_t s = 0; s < 2; s++) {
        Nob_Cmd cmd = {0};
        Nob_String_Builder sb = {0};
        diff_driver_cmd(0, s, &cmd);
        nob_cmd_render(cmd, &sb);
        h = diff_hash(h, sb.items, sb.count);
        nob_da_free(cmd);
        nob_sb_free(sb);
    }
    return diff_hash(h, (const char *)&shards, sizeof shards);
}

/* The key the base side's drivers and outputs were made with, as text (empty when there is none) */
static void diff_key_text(unsigned long long key, char text[32]) {
    snprintf(text, 32, "%016llx\n", key);
}

/* Whether the base side's drivers and outputs were made with this key */
static bool diff_base_cached(unsigned long long key, int shards) {
    char want[32];
    char have[32] = {0};
    diff_key_text(key, want);
    SpPath key_path = sp_path(DIFF_BASE_DIR "/key");
    SpIOResult read = sp_read_file(&key_path, have, sizeof have - 1); /* none yet is no error */
    bool cached = read.error == SP_OK && read.bytes == strlen(want) && memcmp(have, want, read.bytes) == 0;
    for (size_t s = 0; cached && s < 2; s++) {
        cached = exists(diff_exe(0, s));
        for (size_t spec = 0; cached && spec < 3; spec++)
            for (int k = 0; cached && k < shards; k++)
                cached = exists(diff_out(0, s, spec, k));
    }
    return cached;
}

static void diff_sleep_ms(int ms) {
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec t = {0, ms * 1000000L};
    nanosleep(&t, NULL);
#endif
}

/* One input set at one SP_PATH_MAX: its shards' processes and outputs as they grow, and what they showed so far */
typedef struct {
    Nob_Proc proc[DIFF_SHARDS][2];
    FILE *out[DIFF_SHARDS][2];
    long compared[DIFF_SHARDS]; /* records of both sides compared so far */
    long first;                     /* the first differing line found (not always the lowest), 0 for none */
    long differing;
    long lines;
    bool failed;
} Diff_Set;

/* Line of set spec at size s in full from one side's driver: its input on the first line, its labeled results on the
 * second (the caller frees the text) */
static char *diff_line_text(size_t side, size_t s, size_t spec, long line) {
    const char *cmd = nob_temp_sprintf("%s %s %s - %ld 0", diff_exe(side, s), diff_specs[spec][0], diff_specs[spec][1], line);
#ifdef _WIN32
    FILE *p = _popen(cmd, "rb");
#else
    FILE *p = popen(cmd, "r");
#endif
    Nob_String_Builder sb = {0};
    char buf[4096];
    for (size_t n; p && (n = fread(buf, 1, sizeof buf, p)) > 0;)
        nob_da_append_many(&sb, buf, n);
    if (p) {
#ifdef _WIN32
        _pclose(p);
#else
        pclose(p);
#endif
    }
    nob_sb_append_null(&sb);
    return sb.items;
}

/* The input of a differing line and the results that differ, "label: base -> head" */
static void diff_show_line(size_t s, size_t spec, long line) {
    char *text[2];
    for (size_t side = 0; side < 2; side++)
        text[side] = diff_line_text(side, s, spec, line);

    char *input_end = strchr(text[1], '\n');
    printf("    input: %.*s\n", input_end ? (int)(input_end - text[1]) : 0, text[1]);
    char *field[2];
    for (size_t side = 0; side < 2; side++) {
        char *nl = strchr(text[side], '\n');
        field[side] = nl ? nl + 1 : text[side] + strlen(text[side]);
    }

    /* Both sides give the same labels in the same order until a difference changes what follows */
    int shown = 0;
    while (*field[0] || *field[1]) {
        size_t len[2];
        for (size_t side = 0; side < 2; side++)
            len[side] = strcspn(field[side], "|\n");
        if (len[0] != len[1] || memcmp(field[0], field[1], len[0]) != 0) {
            const char *eq = memchr(field[0], '=', len[0]);
            size_t label = eq ? (size_t)(eq - field[0]) : 0;
            if (shown++ < 12)
                printf("    %.*s: %.*s -> %.*s\n", (int)label, field[0], (int)(len[0] - label - (eq ? 1 : 0)),
                       field[0] + label + (eq ? 1 : 0), (int)(len[1] - label - (eq ? 1 : 0)), field[1] + label + (eq ? 1 : 0));
        }
        for (size_t side = 0; side < 2; side++)
            field[side] += len[side] + (field[side][len[side]] == '|' ? 1 : 0);
        if (*field[0] == '\n' || *field[1] == '\n')
            break;
    }
    if (shown > 12)
        printf("    ... and %d more results\n", shown - 12);
    fflush(stdout);
    free(text[0]);
    free(text[1]);
}

/* Compares whatever both sides of a shard have written since the last call; a set's first difference is shown at once */
static void diff_compare(Diff_Set *set, time_t started, size_t s, size_t spec, int k, int shards) {
    long avail[2];
    for (size_t side = 0; side < 2; side++) {
        if (!set->out[k][side])
            set->out[k][side] = fopen(diff_out(side, s, spec, k), "rb");
        if (!set->out[k][side] || fseek(set->out[k][side], 0, SEEK_END) != 0)
            return;
        avail[side] = ftell(set->out[k][side]) / DIFF_RECORD;
    }

    static char a[DIFF_RECORD * 4096], b[DIFF_RECORD * 4096];
    for (long n = (avail[0] < avail[1] ? avail[0] : avail[1]) - set->compared[k]; n > 0;) {
        long count = n < 4096 ? n : 4096;
        fseek(set->out[k][0], set->compared[k] * DIFF_RECORD, SEEK_SET);
        fseek(set->out[k][1], set->compared[k] * DIFF_RECORD, SEEK_SET);
        if (fread(a, DIFF_RECORD, (size_t)count, set->out[k][0]) != (size_t)count ||
            fread(b, DIFF_RECORD, (size_t)count, set->out[k][1]) != (size_t)count)
            return;

        for (long i = 0; i < count; i++) {
            if (memcmp(a + i * DIFF_RECORD, b + i * DIFF_RECORD, DIFF_RECORD) == 0)
                continue;
            long line = k + 1 + (set->compared[k] + i) * shards;
            set->differing++;
            if (set->first != 0)
                continue;
            set->first = line;
            nob_log(NOB_ERROR, "  DIFF  SP_PATH_MAX=%s %s %s, line %ld, after %ld s (the first found; more may follow):",
                    diff_sizes[s], diff_specs[spec][0], diff_specs[spec][1], line, (long)(time(NULL) - started));
            diff_show_line(s, spec, line);
        }
        set->compared[k] += count;
        set->lines += count;
        n -= count;
    }
}

static bool run_diff(const char *ref) {
    if (!write_base_header(ref)) return false;
    int shards = DIFF_SHARDS;
    int slots = nob_nprocs() < 1 ? 1 : nob_nprocs(); /* drivers running at once */
    unsigned long long key = diff_base_key(shards);
    bool cached = diff_base_cached(key, shards);
    if (!cached)
        delete_if_exists(DIFF_BASE_DIR "/key");
    time_t started = time(NULL);

    LOG_INFO( "=== Building the differential drivers (base: %s%s) ===", ref, cached ? ", its results cached" : "");
    Nob_Procs procs = {0};
    for (size_t s = 0; s < 2; s++)
        for (size_t side = cached ? 1 : 0; side < 2; side++) {
            Nob_Cmd cmd = {0};
            diff_driver_cmd(side, s, &cmd);
            nob_cmd_run(&cmd, .async = &procs);
        }
    if (!nob_procs_flush(&procs)) {
        nob_log(NOB_ERROR, "Building the differential drivers failed: %s may have a different API than the working tree", ref);
        return false;
    }

    /* Jobs part by part, so the first ones cover every set, each base job next to its head job; at most one driver
       per core runs at a time (Android kills an app past 32 child processes) */
    LOG_INFO( "=== Comparing each input set in %d parts per side, as the drivers write ===", shards);
    static Diff_Set sets[2][3];
    static size_t queue[DIFF_SHARDS * 2 * 3 * 2][4]; /* part, size, set, side */
    memset(sets, 0, sizeof sets);
    size_t jobs = 0;
    Nob_Log_Level quiet = nob_minimal_log_level;
    nob_minimal_log_level = NOB_WARNING; /* not a line per deleted output */
    for (int k = 0; k < shards; k++)
        for (size_t s = 0; s < 2; s++)
            for (size_t spec = 0; spec < 3; spec++)
                for (size_t side = 0; side < 2; side++) {
                    sets[s][spec].proc[k][side] = NOB_INVALID_PROC;
                    if (side == 1 || !cached) {
                        /* An old output would be read before its driver starts over: gone until the driver makes it */
                        size_t job[4] = {(size_t)k, s, spec, side};
                        memcpy(queue[jobs++], job, sizeof job);
                        if (!delete_if_exists(diff_out(side, s, spec, k)))
                            return false;
                    }
                }
    nob_minimal_log_level = quiet;

    int running = 0;
    for (size_t next = 0; next < jobs || running > 0;) {
        for (size_t s = 0; s < 2; s++)
            for (size_t spec = 0; spec < 3; spec++)
                for (int k = 0; k < shards; k++)
                    for (size_t side = 0; side < 2; side++) {
                        Nob_Proc *proc = &sets[s][spec].proc[k][side];
                        if (*proc == NOB_INVALID_PROC)
                            continue;
                        int status = nob__proc_wait_async(*proc, 0);
                        if (status == 0)
                            continue;
                        sets[s][spec].failed |= status < 0;
                        *proc = NOB_INVALID_PROC;
                        running--;
                    }

        size_t mark = nob_temp_save();
        Nob_Log_Level level = nob_minimal_log_level;
        nob_minimal_log_level = NOB_WARNING; /* not a command line per process */
        for (; next < jobs && running < slots; next++) {
            size_t k = queue[next][0];
            size_t s = queue[next][1];
            size_t spec = queue[next][2];
            size_t side = queue[next][3];
            Nob_Cmd cmd = {0};
            nob_cmd_append(&cmd, diff_exe(side, s), diff_specs[spec][0], diff_specs[spec][1], diff_out(side, s, spec, (int)k));
            nob_cmd_append(&cmd, nob_temp_sprintf("%zu", k + 1), nob_temp_sprintf("%d", shards));
            Nob_Proc proc = nob__cmd_start_process(cmd, NULL, NULL, NULL);
            nob_da_free(cmd);
            sets[s][spec].failed |= proc == NOB_INVALID_PROC;
            if (proc == NOB_INVALID_PROC)
                continue;
            sets[s][spec].proc[k][side] = proc;
            running++;
        }
        nob_minimal_log_level = level;

        for (size_t s = 0; s < 2; s++)
            for (size_t spec = 0; spec < 3; spec++)
                for (int k = 0; k < shards; k++)
                    diff_compare(&sets[s][spec], started, s, spec, k, shards);
        nob_temp_rewind(mark);
        if (next < jobs || running > 0)
            diff_sleep_ms(50);
    }

    bool same = true;
    bool complete = true; /* every driver finished, and the sides wrote the same line counts */
    for (size_t s = 0; s < 2; s++)
        for (size_t spec = 0; spec < 3; spec++) {
            Diff_Set *set = &sets[s][spec];
            for (int k = 0; k < shards; k++) {
                long long size[2] = {0, 0};
                for (size_t side = 0; side < 2; side++) {
                    SpPath out = sp_path(diff_out(side, s, spec, k));
                    if (set->out[k][side])
                        fclose(set->out[k][side]);
                    size[side] = sp_stat(&out, true).sp_size;
                }
                set->failed |= size[0] != size[1] || size[0] % DIFF_RECORD != 0;
            }

            const char *label = nob_temp_sprintf("SP_PATH_MAX=%s %s %s", diff_sizes[s], diff_specs[spec][0], diff_specs[spec][1]);
            if (set->failed)
                nob_log(NOB_ERROR, "  FAILED  %s: a driver failed, or the two sides wrote different line counts", label);
            else if (set->differing > 0)
                nob_log(NOB_ERROR, "  DIFF  %s: %ld of %ld lines differ (line %ld shown above)", label, set->differing, set->lines, set->first);
            else
                LOG_INFO( "  SAME  %s (%ld lines)", label, set->lines);
            same = same && !set->failed && set->differing == 0;
            complete = complete && !set->failed;
        }

    /* A complete base side is kept for the next run against the same base */
    if (!cached && complete) {
        char text[32];
        diff_key_text(key, text);
        (void)write_text(DIFF_BASE_DIR "/key", text, strlen(text));
    }
    LOG_INFO( "=== Compared in %ld s ===", (long)(time(NULL) - started));
    return same;
}

/* ======== trim.h: snakepath.h cut down to what a program uses ======== */

#define TRIM_DIR "trim_out" /* the tests against snakepath.h trimmed to what test.c uses, and snake.c's builds */
/* Appends snakepath.h cut down by trim.h to what program uses, without the #ifdef drop block (NULL: none) */
static bool trim_snakepath(const char *program, const char *drop, Nob_String_Builder *out) {
    static char header[TEXT_MAX];
    static char text[TEXT_MAX];
    TrimContext ctx = {header, 0, text, 0, &drop, drop ? 1 : 0};
    size_t len;

    if (!read_text("snakepath.h", header, sizeof(header), &ctx.header_len) ||
        !read_text(program, text, sizeof(text), &ctx.program_len))
        return false;
    nob_da_reserve(out, out->count + ctx.header_len);
    if (trim(&ctx, out->items + out->count, ctx.header_len, &len) != TRIM_OK) {
        nob_log(NOB_ERROR, "could not trim snakepath.h for %s", program);
        return false;
    }
    out->count += len;
    return true;
}

/* snake.c, the site's export: logo.c as a program (LOGO_MAIN), with the snakepath it uses in place of its #include */
static bool make_snake(void) {
    static const char include[] = "#include \"snakepath.h\""; /* the line, ended by \n or, checked out on
                                                                 * Windows, \r\n */
    static char logo[TEXT_MAX];
    Nob_String_Builder snake = {0};
    size_t len;
    const char *at;
    bool ok;

    if (!read_text("site/logo.c", logo, sizeof(logo), &len))
        return false;
    at = strstr(logo, include);
    if (at == NULL) {
        nob_log(NOB_ERROR, "logo.c has no %s", include);
        return false;
    }
    nob_sb_append_cstr(&snake,
                       "/* snake.c - the snakepath logo, swaying in a terminal: https://snakepath.dev\n"
                       " *   curl -sSL https://snakepath.dev/snake.c | cc -std=c99 -xc - -o snake && ./snake\n"
                       " * It is logo.c with the snakepath it uses (snakepath.h cut down by trim.h) in place of its "
                       "#include. */\n#define LOGO_MAIN\n");
    nob_sb_append_buf(&snake, logo, (size_t)(at - logo));
    ok = trim_snakepath("site/logo.c", "SNAKEPATH_FLUENT", &snake);
    at = strchr(at, '\n');
    nob_sb_append_cstr(&snake, at == NULL ? "" : at + 1);
    ok = ok && write_text("site/snake.c", snake.items, snake.count);
    nob_sb_free(snake);
    return ok;
}

/* TRIM_DIR/test.c and TRIM_DIR/snakepath.h, cut down to what test.c uses */
static bool make_trimmed_tests(void) {
    Nob_String_Builder trimmed = {0};
    SpPath test = sp_path("test.c");
    SpPath dir = sp_path(TRIM_DIR);
    SpCopyOptions into = {true, false, true};
    bool ok = make_dir(TRIM_DIR) && sp_copy(&test, &dir, into).error == SP_OK &&
              trim_snakepath("test.c", NULL, &trimmed) &&
              write_text(TRIM_DIR "/snakepath.h", trimmed.items, trimmed.count);

    nob_sb_free(trimmed);
    return ok;
}

static bool clean_artifacts(void) {
    const char *trees[] = {TRIM_DIR, DIFF_BASE_DIR, DIFF_HEAD_DIR, "site/snake.c", "site/README.md"};
    bool all_ok = true;
    LOG_INFO( "Cleaning build artifacts...");
    for (size_t i = 0; all_artifacts[i] != NULL; i++)
        all_ok = delete_if_exists(all_artifacts[i]) && all_ok;
    for (size_t i = 0; i < sizeof(trees) / sizeof(trees[0]); i++)
        all_ok = delete_if_exists(trees[i]) && all_ok;
    return all_ok;
}

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    /* Check VERBOSE env var - default true, set to 0 to suppress INFO logs */
    const char *verbose_env = getenv("VERBOSE");
    if (verbose_env && (strcmp(verbose_env, "0") == 0 || strcmp(verbose_env, "false") == 0)) {
        verbose = false;
    }

    const char *program = nob_shift(argv, argc);
    (void)program;
#ifndef _WIN32
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
    skip_gcc = true;
#else
    skip_gcc = getenv("SNAKEPATH_SKIP_GCC") != NULL;
#endif
    use_sanitizers = getenv("SNAKEPATH_SANITIZE") != NULL;
#endif

    if (argc > 0) {
        const char *subcmd = nob_shift(argv, argc);
        if (strcmp(subcmd, "clean") == 0) {
            if (clean_artifacts()) {
                LOG_INFO( "Clean complete.");
                return 0;
            } else {
                nob_log(NOB_ERROR, "Clean failed.");
                return 1;
            }
        } else if (strcmp(subcmd, "python") == 0) {
            /* Build and test Python bindings only */
            LOG_INFO( "=== Building Python bindings ===");
            if (!build_python_lib(NULL)) {
                nob_log(NOB_ERROR, "Failed to build Python library");
                return 1;
            }
            LOG_INFO( "=== Running Python checks and tests ===");
            if (!run_python()) {
                nob_log(NOB_ERROR, "Python checks or tests failed");
                return 1;
            }
            LOG_INFO( "Python bindings built and tested successfully!");
            return 0;
        } else if (strcmp(subcmd, "snake") == 0) {
            return make_snake() ? 0 : 1;
        } else if (strcmp(subcmd, "format") == 0) {
            return run_clang_format(true) ? 0 : 1;
        } else if (strcmp(subcmd, "diff") == 0) {
            const char *ref = argc > 0 ? nob_shift(argv, argc) : "origin/main";
            if (!run_diff(ref)) {
                nob_log(NOB_ERROR, "snakepath.h's pure path API differs from %s (or the check could not run)", ref);
                return 1;
            }
            LOG_INFO( "snakepath.h's pure path API matches %s.", ref);
            return 0;
        } else {
            nob_log(NOB_ERROR, "Unknown subcommand: %s", subcmd);
            LOG_INFO( "Usage: ./nob [clean|python|format|snake|diff [ref]]");
            return 1;
        }
    }

    bool all_ok = true;
    Nob_Procs procs = {0};

    LOG_INFO( "Building with %d parallel jobs...", nob_nprocs());

#ifdef _WIN32
    BuildConfig test_configs[] = {
        {COMPILER_MSVC,     false, false, "MSVC (C)",    "test_msvc.exe"},
        {COMPILER_MSVC_CPP, false, false, "MSVC (C++)",  "test_msvc_cpp.exe"},
        {COMPILER_MSVC,     false, true,  "MSVC Fluent", "test_fluent_msvc.exe"},
    };
    BuildConfig demo_config = {COMPILER_MSVC, false, false, "Demo", "api_demo.exe"};
#else
    if (skip_gcc) LOG_INFO( "SNAKEPATH_SKIP_GCC set - using clang only");
    else if (use_sanitizers) LOG_INFO( "SNAKEPATH_SANITIZE set - including sanitizer builds");

    BuildConfig test_configs[] = {
        {COMPILER_GCC,     false, false, "GCC",                "./test_gcc"},
        {COMPILER_CLANG,   false, false, "Clang",              "./test_clang"},
        {COMPILER_GCC,     true,  false, "GCC + sanitizers",   "./test_gcc_san"},
        {COMPILER_CLANG,   true,  false, "Clang + sanitizers", "./test_clang_san"},
        {COMPILER_GPP,     false, false, "G++ (C++)",          "./test_gpp"},
        {COMPILER_CLANGPP, false, false, "Clang++ (C++)",      "./test_clangpp"},
        {COMPILER_GCC,     false, true,  "GCC Fluent",         "./test_fluent_gcc"},
        {COMPILER_CLANG,   false, true,  "Clang Fluent",       "./test_fluent_clang"},
    };
    BuildConfig demo_config = {skip_gcc ? COMPILER_CLANG : COMPILER_GCC, false, false, "Demo", "./api_demo"};
#endif
    size_t test_count = sizeof(test_configs) / sizeof(test_configs[0]);

    /* Phase 1: Build everything in parallel */
    LOG_INFO( "=== Building all targets ===");

    for (size_t i = 0; i < test_count; i++) {
        if (!config_enabled(test_configs[i])) continue;
        LOG_INFO( "  Starting build: %s", test_configs[i].name);
        build_source_async(test_configs[i], "test.c", &procs);
    }

    LOG_INFO( "  Starting build: %s", demo_config.name);
    build_source_async(demo_config, "api_demo.c", &procs);
    /* The tests again, against snakepath.h cut down by trim.h to what they use, and snake.c, the site's export */
    BuildConfig trimmed_configs[sizeof(test_configs) / sizeof(test_configs[0])];
#ifdef _WIN32
    BuildConfig snake_configs[] = {{COMPILER_MSVC, false, false, "snake.c (MSVC)", TRIM_DIR "/snake_msvc.exe"}};
#else
    BuildConfig snake_configs[] = {
        {COMPILER_GCC,   false, false, "snake.c (GCC)",   TRIM_DIR "/snake_gcc"},
        {COMPILER_CLANG, false, false, "snake.c (Clang)", TRIM_DIR "/snake_clang"},
    };
#endif
    size_t snake_count = sizeof(snake_configs) / sizeof(snake_configs[0]);
    if (!make_trimmed_tests() || !make_snake()) {
        all_ok = false;
        goto end;
    }
    for (size_t i = 0; i < test_count; i++) {
        const char *base = strrchr(test_configs[i].output, '/');
        trimmed_configs[i] = test_configs[i];
        trimmed_configs[i].name = nob_temp_sprintf("%s, trimmed", test_configs[i].name);
        trimmed_configs[i].output = nob_temp_sprintf(TRIM_DIR "/%s", base ? base + 1 : test_configs[i].output);
        if (!config_enabled(trimmed_configs[i])) continue;
        LOG_INFO( "  Starting build: %s", trimmed_configs[i].name);
        build_source_async(trimmed_configs[i], TRIM_DIR "/test.c", &procs);
    }
    for (size_t i = 0; i < snake_count; i++) {
        if (!config_enabled(snake_configs[i])) continue;
        LOG_INFO( "  Starting build: %s", snake_configs[i].name);
        build_source_async(snake_configs[i], "site/snake.c", &procs);
    }

    /* Build the Python bindings' extension module */
    LOG_INFO( "  Starting build: Python bindings");
    build_python_lib(&procs);

    /* Wait for all builds to complete */
    if (!nob_procs_flush(&procs)) {
        nob_log(NOB_ERROR, "Some builds failed");
        all_ok = false;
    }

    if (!all_ok) goto end;

    /* Phase 2: Run all tests in parallel */
    LOG_INFO( "=== Running all tests ===");

    for (size_t i = 0; i < test_count; i++) {
        if (!config_enabled(test_configs[i])) continue;
        LOG_INFO( "  Starting test: %s", test_configs[i].name);
        run_test_async(test_configs[i].output, &procs);
        LOG_INFO( "  Starting test: %s", trimmed_configs[i].name);
        run_test_async(trimmed_configs[i].output, &procs);
    }
    for (size_t i = 0; i < snake_count; i++) {
        Nob_Cmd cmd = {0};
        if (!config_enabled(snake_configs[i])) continue;
        LOG_INFO( "  Starting test: %s, writing one sway", snake_configs[i].name);
        nob_cmd_append(&cmd, snake_configs[i].output, nob_temp_sprintf("%s_frames", snake_configs[i].output));
        nob_cmd_run(&cmd, .async = &procs);
    }

    if (!nob_procs_flush(&procs)) {
        nob_log(NOB_ERROR, "Some tests failed");
        all_ok = false;
    }

    /* Phase 3: Python checks and CPython's pathlib tests */
    LOG_INFO( "=== Running Python checks and tests ===");
    if (!run_python()) {
        nob_log(NOB_ERROR, "Python checks or tests failed");
        all_ok = false;
    }

    LOG_INFO( "=== Checking snakepath.h's layout ===");
    if (!run_clang_format(false)) {
        nob_log(NOB_ERROR, "snakepath.h needs ./nob format (clang-format 21: pip install clang-format==21.1.8)");
        all_ok = false;
    }

#ifndef _WIN32
    /* Phase 4: Valgrind on the gcc build (must be sequential, slow) */
    if (all_ok && !skip_gcc) {
        LOG_INFO( "=== Running valgrind ===");
        if (!run_valgrind("./test_gcc")) {
            nob_log(NOB_ERROR, "Valgrind check failed");
            all_ok = false;
        }
    }
#endif

    /* Phase 5: Run demo to show it works */
    LOG_INFO( "=== Running demo ===");
    if (!run_test_async(demo_config.output, NULL)) {
        nob_log(NOB_ERROR, "Demo failed");
        all_ok = false;
    }

end:
    nob_da_free(procs);

    if (all_ok) {
        LOG_INFO( "All builds and tests passed!");
        return 0;
    } else {
        nob_log(NOB_ERROR, "Some tests failed!");
        return 1;
    }
}
