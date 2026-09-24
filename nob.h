/* nob.h - snakepath's build script in one file: the build/test logic at the end, on top of
   the parts of nob v3.10.0 it uses (https://github.com/tsoding/nob.h; license at the end).

   Build: cc -x c -o nob nob.h && ./nob     (quiet: add -DSNAKEPATH_QUIET; MSVC: cl /Tcnob.h)
   Builds run in parallel across the available CPU cores.

   The nob part was trimmed from upstream: its docs, prefix-stripped aliases, deprecated and unused
   APIs, C++ support, output redirection, nob_cmd_run options other than .async and extra
   rebuild-self sources. To upgrade, vendor upstream nob.h again and re-trim it to what this uses.
*/

#ifdef SNAKEPATH_QUIET
#define NOB_NO_ECHO
#endif
#ifdef _WIN32
#    ifndef _CRT_SECURE_NO_WARNINGS
#        define _CRT_SECURE_NO_WARNINGS (1)
#    endif // _CRT_SECURE_NO_WARNINGS
#endif //  _WIN32

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

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>

#ifdef _WIN32
#    define WIN32_LEAN_AND_MEAN
#    define _WINUSER_
#    define _WINGDI_
#    define _IMM_
#    define _WINCON_
#    include <windows.h>
#else
#    include <sys/types.h>
#    include <sys/wait.h>
#    include <sys/stat.h>
#    include <unistd.h>
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

#define NOB_UNREACHABLE(message) do { fprintf(stderr, "%s:%d: UNREACHABLE: %s\n", __FILE__, __LINE__, message); abort(); } while(0)

typedef enum {
    NOB_INFO,
    NOB_WARNING,
    NOB_ERROR,
    NOB_NO_LOGS,
} Nob_Log_Level;

// Any messages with the level below nob_minimal_log_level are going to be suppressed by the nob_default_log_handler.
extern Nob_Log_Level nob_minimal_log_level;

typedef void (Nob_Log_Handler)(Nob_Log_Level level, const char *fmt, va_list args);

Nob_Log_Handler nob_default_log_handler;

void nob_log(Nob_Log_Level level, const char *fmt, ...) NOB_PRINTF_FORMAT(2, 3);

// It is an equivalent of shift command from bash (do `help shift` in bash). It basically
// pops an element from the beginning of a sized array.
#define nob_shift(xs, xs_sz) (NOB_ASSERT((xs_sz) > 0), (xs_sz)--, *(xs)++)

typedef enum {
    NOB_FILE_REGULAR = 0,
    NOB_FILE_DIRECTORY,
    NOB_FILE_SYMLINK,
    NOB_FILE_OTHER,
} Nob_File_Type;

Nob_File_Type nob_get_file_type(const char *path);
bool nob_delete_file(const char *path);

#define nob_return_defer(value) do { result = (value); goto defer; } while(0)

// Initial capacity of a dynamic array
#ifndef NOB_DA_INIT_CAP
#define NOB_DA_INIT_CAP 256
#endif

#define nob_da_reserve(da, expected_capacity)                                              \
    do {                                                                                   \
        if ((expected_capacity) > (da)->capacity) {                                        \
            if ((da)->capacity == 0) {                                                     \
                (da)->capacity = NOB_DA_INIT_CAP;                                          \
            }                                                                              \
            while ((expected_capacity) > (da)->capacity) {                                 \
                (da)->capacity *= 2;                                                       \
            }                                                                              \
            (da)->items = NOB_REALLOC((da)->items, (da)->capacity * sizeof(*(da)->items));      \
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

#define nob_da_remove_unordered(da, i)               \
    do {                                             \
        size_t j = (i);                              \
        NOB_ASSERT(j < (da)->count);                 \
        (da)->items[j] = (da)->items[--(da)->count]; \
    } while(0)

typedef struct {
    char *items;
    size_t count;
    size_t capacity;
} Nob_String_Builder;

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

// Free the memory allocated by a string builder
#define nob_sb_free(sb) NOB_FREE((sb).items)

// Process handle
#ifdef _WIN32
typedef HANDLE Nob_Proc;
#define NOB_INVALID_PROC INVALID_HANDLE_VALUE
#else
typedef int Nob_Proc;
#define NOB_INVALID_PROC (-1)
#endif // _WIN32

typedef struct {
    Nob_Proc *items;
    size_t count;
    size_t capacity;
} Nob_Procs;

// Wait until the process has finished
bool nob_proc_wait(Nob_Proc proc);

// Wait until all the processes have finished
bool nob_procs_wait(Nob_Procs procs);

// Wait until all the processes have finished and empty the procs array.
bool nob_procs_flush(Nob_Procs *procs);

// A command - the main workhorse of Nob. Nob is all about building commands and running them
typedef struct {
    const char **items;
    size_t count;
    size_t capacity;
} Nob_Cmd;

// Options for nob_cmd_run_opt() function.
typedef struct {
    // Run the command asynchronously appending its Nob_Proc to the provided Nob_Procs array
    // (at most nob_nprocs() + 1 at a time)
    Nob_Procs *async;
} Nob_Cmd_Opt;

// Run the command with options.
bool nob_cmd_run_opt(Nob_Cmd *cmd, Nob_Cmd_Opt opt);

// Get amount of processors on the machine.
int nob_nprocs(void);

// Same as nob_cmd_run_opt but using cool variadic macro to set the default options.
// See https://x.com/vkrajacic/status/1749816169736073295 for more info on how to use such macros.
#define nob_cmd_run(cmd, ...) nob_cmd_run_opt((cmd), (Nob_Cmd_Opt){ __VA_ARGS__ })

// Render a string representation of a command into a string builder. Keep in mind the the
// string builder is not NULL-terminated by default. Use nob_sb_append_null if you plan to
// use it as a C string.
void nob_cmd_render(Nob_Cmd cmd, Nob_String_Builder *render);

void nob__cmd_append(Nob_Cmd *cmd, size_t n, const char **args);
#define nob_cmd_append(cmd, ...) \
    nob__cmd_append(cmd, sizeof((const char*[]){__VA_ARGS__})/sizeof(const char*), (const char*[]){__VA_ARGS__})

#ifndef NOB_TEMP_CAPACITY
#define NOB_TEMP_CAPACITY (8*1024*1024)
#endif // NOB_TEMP_CAPACITY
void *nob_temp_alloc(size_t size);
char *nob_temp_sprintf(const char *format, ...) NOB_PRINTF_FORMAT(1, 2);
char *nob_temp_vsprintf(const char *format, va_list ap);
size_t nob_temp_save(void);
void nob_temp_rewind(size_t checkpoint);

bool nob_rename(const char *old_path, const char *new_path);
int nob_needs_rebuild(const char *output_path, const char **input_paths, size_t input_paths_count);
int nob_file_exists(const char *file_path);
bool nob_set_current_dir(const char *path);

#ifndef NOB_REBUILD_URSELF
#  if defined(_WIN32)
#    if defined(__clang__)
#      define NOB_REBUILD_URSELF(binary_path, source_path) "clang", "-x", "c", "-o", binary_path, source_path
#    elif defined(__GNUC__)
#      define NOB_REBUILD_URSELF(binary_path, source_path) "gcc", "-x", "c", "-o", binary_path, source_path
#    elif defined(_MSC_VER)
#      define NOB_REBUILD_URSELF(binary_path, source_path) "cl.exe", nob_temp_sprintf("/Fe:%s", (binary_path)), nob_temp_sprintf("/Tc%s", (source_path))
#    elif defined(__TINYC__)
#      define NOB_REBUILD_URSELF(binary_path, source_path) "tcc", "-o", binary_path, source_path
#    endif
#  else
#    define NOB_REBUILD_URSELF(binary_path, source_path) "cc", "-x", "c", "-o", binary_path, source_path
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
void nob__go_rebuild_urself(int argc, char **argv, const char *source_path);
#define NOB_GO_REBUILD_URSELF(argc, argv) nob__go_rebuild_urself(argc, argv, __FILE__)

#ifdef _WIN32

char *nob_win32_error_message(DWORD err);

#endif // _WIN32

// This is like nob_proc_wait() but waits asynchronously. Depending on the platform ms means different thing.
// On Windows it means timeout. On POSIX it means for how long to sleep after checking if the process exited,
// so to not peg the core too much. Since this API is kinda of weird, the function is private for now.
static int nob__proc_wait_async(Nob_Proc proc, int ms);

// Starts the process for the command. Its main purpose is to be the base for nob_cmd_run() and nob_cmd_run_opt().
static Nob_Proc nob__cmd_start_process(Nob_Cmd cmd);

// Any messages with the level below nob_minimal_log_level are going to be suppressed.
Nob_Log_Level nob_minimal_log_level = NOB_INFO;

void nob__cmd_append(Nob_Cmd *cmd, size_t n, const char **args)
{
    for (size_t i = 0; i < n; ++i) {
        nob_da_append(cmd, args[i]);
    }
}

#ifdef _WIN32

// Base on https://stackoverflow.com/a/75644008
// > .NET Core uses 4096 * sizeof(WCHAR) buffer on stack for FormatMessageW call. And...thats it.
// >
// > https://github.com/dotnet/runtime/blob/3b63eb1346f1ddbc921374a5108d025662fb5ffd/src/coreclr/utilcode/posterror.cpp#L264-L265
#ifndef NOB_WIN32_ERR_MSG_SIZE
#define NOB_WIN32_ERR_MSG_SIZE (4 * 1024)
#endif // NOB_WIN32_ERR_MSG_SIZE

char *nob_win32_error_message(DWORD err) {
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

// The implementation idea is stolen from https://github.com/zhiayang/nabs
void nob__go_rebuild_urself(int argc, char **argv, const char *source_path)
{
    const char *binary_path = nob_shift(argv, argc);
#ifdef _WIN32
    // On Windows executables almost always invoked without extension, so
    // it's ./nob, not ./nob.exe. For renaming the extension is a must.
    size_t binary_path_len = strlen(binary_path);
    if (binary_path_len < 4 || strcmp(binary_path + binary_path_len - 4, ".exe") != 0) {
        binary_path = nob_temp_sprintf("%s.exe", binary_path);
    }
#endif

    int rebuild_is_needed = nob_needs_rebuild(binary_path, &source_path, 1);
    if (rebuild_is_needed < 0) exit(1); // error
    if (!rebuild_is_needed) return;     // no rebuild is needed

    Nob_Cmd cmd = {0};

    const char *old_binary_path = nob_temp_sprintf("%s.old", binary_path);

    if (!nob_rename(binary_path, old_binary_path)) exit(1);
    nob_cmd_append(&cmd, NOB_REBUILD_URSELF(binary_path, source_path));
    Nob_Cmd_Opt opt = {0};
    if (!nob_cmd_run_opt(&cmd, opt)) {
        nob_rename(old_binary_path, binary_path);
        exit(1);
    }
    nob_cmd_append(&cmd, binary_path);
    nob_da_append_many(&cmd, argv, argc);
    if (!nob_cmd_run_opt(&cmd, opt)) exit(1);
    exit(0);
}

static size_t nob_temp_size = 0;
static char nob_temp[NOB_TEMP_CAPACITY] = {0};

void nob_cmd_render(Nob_Cmd cmd, Nob_String_Builder *render)
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

int nob_nprocs(void)
{
#ifdef _WIN32
    SYSTEM_INFO siSysInfo;
    GetSystemInfo(&siSysInfo);
    return siSysInfo.dwNumberOfProcessors;
#else
    return sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

bool nob_cmd_run_opt(Nob_Cmd *cmd, Nob_Cmd_Opt opt)
{
    bool result = true;
    Nob_Proc proc = NOB_INVALID_PROC;

    size_t max_procs = (size_t) nob_nprocs() + 1;

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

    proc = nob__cmd_start_process(*cmd);

    if (opt.async) {
        if (proc == NOB_INVALID_PROC) nob_return_defer(false);
        nob_da_append(opt.async, proc);
    } else {
        if (!nob_proc_wait(proc)) nob_return_defer(false);
    }

defer:
    cmd->count = 0;
    return result;
}

static Nob_Proc nob__cmd_start_process(Nob_Cmd cmd)
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
    siStartInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    siStartInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    siStartInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
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

bool nob_procs_wait(Nob_Procs procs)
{
    bool success = true;
    for (size_t i = 0; i < procs.count; ++i) {
        success = nob_proc_wait(procs.items[i]) && success;
    }
    return success;
}

bool nob_procs_flush(Nob_Procs *procs)
{
    bool success = nob_procs_wait(*procs);
    procs->count = 0;
    return success;
}

bool nob_proc_wait(Nob_Proc proc)
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

static Nob_Log_Handler *nob__log_handler = &nob_default_log_handler;

void nob_default_log_handler(Nob_Log_Level level, const char *fmt, va_list args)
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

void nob_log(Nob_Log_Level level, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    nob__log_handler(level, fmt, args);
    va_end(args);
}

Nob_File_Type nob_get_file_type(const char *path)
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

bool nob_delete_file(const char *path)
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

void *nob_temp_alloc(size_t requested_size)
{
    size_t word_size = sizeof(uintptr_t);
    size_t size = (requested_size + word_size - 1)/word_size*word_size;
    if (nob_temp_size + size > NOB_TEMP_CAPACITY) return NULL;
    void *result = &nob_temp[nob_temp_size];
    nob_temp_size += size;
    return result;
}

char *nob_temp_vsprintf(const char *format, va_list ap)
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

char *nob_temp_sprintf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    char *result = nob_temp_vsprintf(format, args);
    va_end(args);
    return result;
}

size_t nob_temp_save(void)
{
    return nob_temp_size;
}

void nob_temp_rewind(size_t checkpoint)
{
    nob_temp_size = checkpoint;
}

int nob_needs_rebuild(const char *output_path, const char **input_paths, size_t input_paths_count)
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

bool nob_rename(const char *old_path, const char *new_path)
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

// RETURNS:
//  0 - file does not exists
//  1 - file exists
int nob_file_exists(const char *file_path)
{
#if _WIN32
    return GetFileAttributesA(file_path) != INVALID_FILE_ATTRIBUTES;
#else
    return access(file_path, F_OK) == 0;
#endif
}

bool nob_set_current_dir(const char *path)
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
    "-Wno-unsafe-buffer-usage"

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
    bool no_nrvo = getenv("SNAKEPATH_NO_NRVO") != NULL;
    switch (compiler) {
    case COMPILER_GCC:
        nob_cmd_append(cmd, BASE_WARNINGS, C_ONLY_WARNINGS, GCC_WARNINGS, GCC_C_WARNINGS);
        break;
    case COMPILER_CLANG:
        nob_cmd_append(cmd, BASE_WARNINGS, C_ONLY_WARNINGS);
        nob_cmd_append(cmd, CLANG_EVERYTHING, CLANG_C_EXCLUSIONS);
        if (no_nrvo) nob_cmd_append(cmd, "-Wno-nrvo");
        break;
    case COMPILER_GPP:
        nob_cmd_append(cmd, BASE_WARNINGS, GCC_WARNINGS);
        break;
    case COMPILER_CLANGPP:
        nob_cmd_append(cmd, BASE_WARNINGS);
        nob_cmd_append(cmd, CLANG_EVERYTHING, CLANG_CPP_EXCLUSIONS);
        if (no_nrvo) nob_cmd_append(cmd, "-Wno-nrvo");
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
    } else
#endif
    {
        if (cfg.sanitizers) {
            nob_cmd_append(&cmd, "-fsanitize=address,undefined,leak");
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

/* SNAKEPATH_SKIP_GCC (Termux, where gcc is clang) keeps only the clang builds, without sanitizers;
 * otherwise sanitizer builds need SNAKEPATH_SANITIZE */
static bool skip_gcc, use_sanitizers;

static bool config_enabled(BuildConfig cfg) {
    if (skip_gcc && (cfg.compiler == COMPILER_GCC || cfg.compiler == COMPILER_GPP)) return false;
    return !cfg.sanitizers || (use_sanitizers && !skip_gcc);
}

static const char *all_artifacts[] = {
#ifdef _WIN32
    "test_msvc.exe", "test_msvc_cpp.exe", "test_fluent_msvc.exe", "api_demo.exe",
    /* PDB and obj files from MSVC */
    "test_msvc.pdb", "test_msvc_cpp.pdb", "test_fluent_msvc.pdb", "api_demo.pdb",
    "test_msvc.obj", "test_msvc_cpp.obj", "test_fluent_msvc.obj", "api_demo.obj",
    "snakepath.dll",
#else
    "test_gcc", "test_clang", "test_gcc_san", "test_clang_san",
    "test_gpp", "test_clangpp", "test_fluent_gcc", "test_fluent_clang",
    "api_demo",
    "libsnakepath.so",
#endif
    NULL
};

/* Build the library the Python bindings load: test.c with -DSP_FFI */
static bool build_python_lib(Compiler compiler, Nob_Procs *procs) {
    Nob_Cmd cmd = {0};

#ifdef _WIN32
    if (compiler == COMPILER_MSVC) {
        nob_cmd_append(&cmd, "cl.exe", "/std:c11", "/LD", "/O2");
        nob_cmd_append(&cmd, "/W4", "/DSP_FFI");
        nob_cmd_append(&cmd, "/Fe:snakepath.dll");
        nob_cmd_append(&cmd, "test.c");
    } else {
        nob_log(NOB_WARNING, "Python lib: Using clang on Windows");
        nob_cmd_append(&cmd, "clang", "-shared", "-fPIC", "-O2");
        nob_cmd_append(&cmd, "-fvisibility=hidden", "-DSP_FFI");
        nob_cmd_append(&cmd, "-o", "snakepath.dll");
        nob_cmd_append(&cmd, "test.c");
    }
#else
    const char *cc = (compiler == COMPILER_CLANG || compiler == COMPILER_CLANGPP) ? "clang" : "gcc";
    nob_cmd_append(&cmd, cc, "-shared", "-fPIC", "-O2");
    nob_cmd_append(&cmd, "-Wall", "-Wextra");
    nob_cmd_append(&cmd, "-fvisibility=hidden", "-DSP_FFI");
    nob_cmd_append(&cmd, "-o", "libsnakepath.so");
    nob_cmd_append(&cmd, "test.c");
#endif

    return nob_cmd_run(&cmd, .async = procs);
}

/* Run Python tests */
static const char *find_python(void) {
#ifdef _WIN32
    return "python";
#else
    /* python3 unless only /usr/bin/python exists */
    return !nob_file_exists("/usr/bin/python3") && nob_file_exists("/usr/bin/python") ? "python" : "python3";
#endif
}

/* Public call depth, README embedding api_demo.c, and CPython's pathlib tests on the bindings */
static bool run_python(void) {
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, find_python(), "snakepath.py");
    return nob_cmd_run(&cmd);
}

static bool clean_artifacts(void) {
    bool all_ok = true;
    LOG_INFO( "Cleaning build artifacts...");
    for (size_t i = 0; all_artifacts[i] != NULL; i++) {
        if (nob_file_exists(all_artifacts[i])) {
            if (!nob_delete_file(all_artifacts[i])) {
                all_ok = false;
            }
        }
    }
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
            if (!build_python_lib(COMPILER_CLANG, NULL)) {
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
        } else {
            nob_log(NOB_ERROR, "Unknown subcommand: %s", subcmd);
            LOG_INFO( "Usage: ./nob [clean|python]");
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
    skip_gcc = getenv("SNAKEPATH_SKIP_GCC") != NULL;
    use_sanitizers = getenv("SNAKEPATH_SANITIZE") != NULL;
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

    /* Build Python shared library */
    LOG_INFO( "  Starting build: Python bindings");
#ifdef _WIN32
    build_python_lib(COMPILER_MSVC, &procs);
#else
    build_python_lib(COMPILER_CLANG, &procs);
#endif

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

#ifndef _WIN32
    /* Phase 4: Valgrind (must be sequential, slow) */
    if (all_ok) {
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
