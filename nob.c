/* nob.c - Build script for snakepath using nob.h
 * Usage: cc -o nob nob.c && ./nob
 *
 * Builds are parallelized across available CPU cores.
 *
 * For quiet mode (no command echo):
 *   cc -DSNAKEPATH_QUIET -o nob nob.c && ./nob
 */

#ifdef SNAKEPATH_QUIET
#define NOB_NO_ECHO
#endif
#define NOB_IMPLEMENTATION
#include "nob.h"

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
static bool build_source_async(BuildConfig cfg, const char *source, const char *extra_define, Nob_Procs *procs) {
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

    /* Include root directory for header files */
#ifdef _WIN32
    if (cfg.compiler == COMPILER_MSVC || cfg.compiler == COMPILER_MSVC_CPP) {
        nob_cmd_append(&cmd, "/I.");
    } else
#endif
    {
        nob_cmd_append(&cmd, "-I.");
    }

    append_warnings(&cmd, cfg.compiler);

    if (extra_define) {
        nob_cmd_append(&cmd, extra_define);
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

    bool result;
    if (procs) {
        result = nob_cmd_run(&cmd, .async = procs);
    } else {
        result = nob_cmd_run(&cmd);
    }
    return result;
}

static bool run_test_async(const char *exe, Nob_Procs *procs) {
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, exe);
    bool result;
    if (procs) {
        result = nob_cmd_run(&cmd, .async = procs);
    } else {
        result = nob_cmd_run(&cmd);
    }
    return result;
}

#ifndef _WIN32
static bool run_valgrind(const char *exe) {
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "valgrind");
    nob_cmd_append(&cmd, "--leak-check=full");
    nob_cmd_append(&cmd, "--error-exitcode=1");
    nob_cmd_append(&cmd, "--track-origins=yes");
    nob_cmd_append(&cmd, exe);
    bool result = nob_cmd_run(&cmd);
    return result;
}
#endif

static const char *all_artifacts[] = {
#ifdef _WIN32
    "tests/test_msvc.exe", "tests/test_msvc_cpp.exe", "tests/test_fluent_msvc.exe", "demo.exe",
    /* PDB and obj files from MSVC (now in tests/ directory to avoid conflicts) */
    "tests/test_msvc.pdb", "tests/test_msvc_cpp.pdb", "tests/test_fluent_msvc.pdb", "demo.pdb",
    "tests/test_msvc.obj", "tests/test_msvc_cpp.obj", "tests/test_fluent_msvc.obj", "api_demo.obj",
    /* Legacy obj files in root (clean these up too) */
    "test.obj", "test_fluent_api.obj",
    "tests/python_harness/snakepath.dll",
#else
    "tests/test_gcc", "tests/test_clang", "tests/test_gcc_san", "tests/test_clang_san",
    "tests/test_gpp", "tests/test_clangpp", "tests/test_fluent_gcc", "tests/test_fluent_clang",
    "api_demo",
    "tests/python_harness/libsnakepath.so",
#endif
    NULL
};

/* Build Python shared library */
static bool build_python_lib(Compiler compiler, Nob_Procs *procs) {
    Nob_Cmd cmd = {0};

#ifdef _WIN32
    if (compiler == COMPILER_MSVC) {
        nob_cmd_append(&cmd, "cl.exe", "/std:c11", "/LD", "/O2");
        nob_cmd_append(&cmd, "/W4", "/I.");
        nob_cmd_append(&cmd, "/Fe:tests/python_harness/snakepath.dll");
        nob_cmd_append(&cmd, "tests/python_harness/snakepath_lib.c");
    } else {
        nob_log(NOB_WARNING, "Python lib: Using clang on Windows");
        nob_cmd_append(&cmd, "clang", "-shared", "-fPIC", "-O2", "-I.");
        nob_cmd_append(&cmd, "-fvisibility=hidden");
        nob_cmd_append(&cmd, "-o", "tests/python_harness/snakepath.dll");
        nob_cmd_append(&cmd, "tests/python_harness/snakepath_lib.c");
    }
#else
    const char *cc = (compiler == COMPILER_CLANG || compiler == COMPILER_CLANGPP) ? "clang" : "gcc";
    nob_cmd_append(&cmd, cc, "-shared", "-fPIC", "-O2", "-I.");
    nob_cmd_append(&cmd, "-Wall", "-Wextra");
    nob_cmd_append(&cmd, "-fvisibility=hidden");
    nob_cmd_append(&cmd, "-o", "tests/python_harness/libsnakepath.so");
    nob_cmd_append(&cmd, "tests/python_harness/snakepath_lib.c");
#endif

    bool result;
    if (procs) {
        result = nob_cmd_run(&cmd, .async = procs);
    } else {
        result = nob_cmd_run(&cmd);
    }
    return result;
}

/* Run Python tests */
static const char *find_python(void) {
    const char *python = NULL;
#ifdef _WIN32
    python = "python";
#else
    if (nob_file_exists("/usr/bin/python3")) {
        python = "python3";
    } else if (nob_file_exists("/usr/bin/python")) {
        python = "python";
    } else {
        python = "python3";
    }
#endif
    return python;
}

static bool run_call_depth_tests(void) {
    Nob_Cmd cmd = {0};
    const char *python = find_python();
    nob_cmd_append(&cmd, python, "tests/test_call_depth.py", "snakepath.h", "3");
    return nob_cmd_run(&cmd);
}

static bool run_python_tests(void) {
    Nob_Cmd cmd = {0};
    const char *python = find_python();
    nob_cmd_append(&cmd, python, "tests/python_harness/run_cpython_tests.py");
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
            LOG_INFO( "=== Running Python tests ===");
            if (!run_python_tests()) {
                nob_log(NOB_ERROR, "Python tests failed");
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
        {COMPILER_MSVC,     false, "MSVC (C)",   "tests/test_msvc.exe"},
        {COMPILER_MSVC_CPP, false, "MSVC (C++)", "tests/test_msvc_cpp.exe"},
    };
    size_t test_count = sizeof(test_configs) / sizeof(test_configs[0]);
    BuildConfig fluent_configs[] = {
        {COMPILER_MSVC, false, "MSVC Fluent", "tests/test_fluent_msvc.exe"},
    };
    BuildConfig demo_config = {COMPILER_MSVC, false, "Demo", "api_demo.exe"};
    const char *demo_output = "api_demo.exe";
#else
    bool use_sanitizers = getenv("SNAKEPATH_SANITIZE") != NULL;
    bool skip_gcc = getenv("SNAKEPATH_SKIP_GCC") != NULL;  /* For Termux where gcc is clang */

    /* Full configs with sanitizers */
    BuildConfig test_configs_full[] = {
        {COMPILER_GCC,     false, "GCC",                "./tests/test_gcc"},
        {COMPILER_CLANG,   false, "Clang",              "./tests/test_clang"},
        {COMPILER_GCC,     true,  "GCC + sanitizers",   "./tests/test_gcc_san"},
        {COMPILER_CLANG,   true,  "Clang + sanitizers", "./tests/test_clang_san"},
        {COMPILER_GPP,     false, "G++ (C++)",          "./tests/test_gpp"},
        {COMPILER_CLANGPP, false, "Clang++ (C++)",      "./tests/test_clangpp"},
    };
    /* No sanitizers */
    BuildConfig test_configs_no_san[] = {
        {COMPILER_GCC,     false, "GCC",                "./tests/test_gcc"},
        {COMPILER_CLANG,   false, "Clang",              "./tests/test_clang"},
        {COMPILER_GPP,     false, "G++ (C++)",          "./tests/test_gpp"},
        {COMPILER_CLANGPP, false, "Clang++ (C++)",      "./tests/test_clangpp"},
    };
    /* Clang-only (for Termux where gcc is clang) */
    BuildConfig test_configs_clang_only[] = {
        {COMPILER_CLANG,   false, "Clang",              "./tests/test_clang"},
        {COMPILER_CLANGPP, false, "Clang++ (C++)",      "./tests/test_clangpp"},
    };

    BuildConfig *test_configs;
    size_t test_count;
    if (skip_gcc) {
        test_configs = test_configs_clang_only;
        test_count = sizeof(test_configs_clang_only) / sizeof(test_configs_clang_only[0]);
        LOG_INFO( "SNAKEPATH_SKIP_GCC set - using clang only");
    } else if (use_sanitizers) {
        test_configs = test_configs_full;
        test_count = sizeof(test_configs_full) / sizeof(test_configs_full[0]);
        LOG_INFO( "SNAKEPATH_SANITIZE set - including sanitizer builds");
    } else {
        test_configs = test_configs_no_san;
        test_count = sizeof(test_configs_no_san) / sizeof(test_configs_no_san[0]);
    }

    BuildConfig fluent_configs_full[] = {
        {COMPILER_GCC,   false, "GCC Fluent",   "./tests/test_fluent_gcc"},
        {COMPILER_CLANG, false, "Clang Fluent", "./tests/test_fluent_clang"},
    };
    BuildConfig fluent_configs_clang[] = {
        {COMPILER_CLANG, false, "Clang Fluent", "./tests/test_fluent_clang"},
    };
    BuildConfig *fluent_configs = skip_gcc ? fluent_configs_clang : fluent_configs_full;
    BuildConfig demo_config = skip_gcc
        ? (BuildConfig){COMPILER_CLANG, false, "Demo", "./api_demo"}
        : (BuildConfig){COMPILER_GCC, false, "Demo", "./api_demo"};
    const char *demo_output = "./api_demo";
#endif

#ifdef _WIN32
    size_t fluent_count = sizeof(fluent_configs) / sizeof(fluent_configs[0]);
#else
    size_t fluent_count = skip_gcc ? 1 : 2;
#endif

    /* Phase 1: Build everything in parallel */
    LOG_INFO( "=== Building all targets ===");

    for (size_t i = 0; i < test_count; i++) {
        LOG_INFO( "  Starting build: %s", test_configs[i].name);
        build_source_async(test_configs[i], "tests/test.c", NULL, &procs);
    }

    for (size_t i = 0; i < fluent_count; i++) {
        LOG_INFO( "  Starting build: %s", fluent_configs[i].name);
        build_source_async(fluent_configs[i], "tests/test_fluent_api.c", NULL, &procs);
    }

    LOG_INFO( "  Starting build: %s", demo_config.name);
    build_source_async(demo_config, "api_demo.c", NULL, &procs);

    /* Build Python shared library */
    LOG_INFO( "  Starting build: Python bindings");
#ifdef _WIN32
    build_python_lib(COMPILER_MSVC, &procs);
#else
    build_python_lib(COMPILER_CLANG, &procs);
#endif

    /* Wait for all builds to complete */
    if (!nob_procs_wait(procs)) {
        nob_log(NOB_ERROR, "Some builds failed");
        all_ok = false;
    }
    procs.count = 0;

    if (!all_ok) goto end;

    /* Phase 2: Run all tests in parallel */
    LOG_INFO( "=== Running all tests ===");

    for (size_t i = 0; i < test_count; i++) {
        LOG_INFO( "  Starting test: %s", test_configs[i].name);
        run_test_async(test_configs[i].output, &procs);
    }

    for (size_t i = 0; i < fluent_count; i++) {
        LOG_INFO( "  Starting test: %s", fluent_configs[i].name);
        run_test_async(fluent_configs[i].output, &procs);
    }

    if (!nob_procs_wait(procs)) {
        nob_log(NOB_ERROR, "Some tests failed");
        all_ok = false;
    }
    procs.count = 0;

    /* Phase 3: Public call-depth tests */
    LOG_INFO( "=== Running call-depth tests ===");
    if (!run_call_depth_tests()) {
        nob_log(NOB_ERROR, "Call-depth tests failed");
        all_ok = false;
    }

    /* Phase 4: Python tests */
    LOG_INFO( "=== Running Python tests ===");
    if (!run_python_tests()) {
        nob_log(NOB_ERROR, "Python tests failed");
        all_ok = false;
    }

#ifndef _WIN32
    /* Phase 5: Valgrind (must be sequential, slow) */
    if (all_ok) {
        LOG_INFO( "=== Running valgrind ===");
        if (!run_valgrind("./tests/test_gcc")) {
            nob_log(NOB_ERROR, "Valgrind check failed");
            all_ok = false;
        }
    }
#endif

    /* Phase 6: Run demo to show it works */
    LOG_INFO( "=== Running demo ===");
    if (!run_test_async(demo_output, NULL)) {
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
