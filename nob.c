/* nob.c - Build script for snakepath using nob.h
 * Usage: cc -o nob nob.c && ./nob
 *
 * Builds are parallelized across available CPU cores.
 */

#define NOB_IMPLEMENTATION
#include "nob.h"

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

    append_warnings(&cmd, cfg.compiler);

    if (extra_define) {
        nob_cmd_append(&cmd, extra_define);
    }

#ifdef _WIN32
    if (cfg.compiler == COMPILER_MSVC || cfg.compiler == COMPILER_MSVC_CPP) {
        nob_cmd_append(&cmd, "/Od", "/Zi");
        /* Use /Fd to give each build its own PDB file for parallel compilation */
        nob_cmd_append(&cmd, nob_temp_sprintf("/Fd:%.*s.pdb", (int)(strlen(cfg.output) - 4), cfg.output));
        nob_cmd_append(&cmd, "/Fe:", cfg.output);
    } else
#endif
    {
        if (cfg.sanitizers) {
            nob_cmd_append(&cmd, "-fsanitize=address,undefined");
            nob_cmd_append(&cmd, "-fno-omit-frame-pointer");
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
    "test_msvc.exe", "test_msvc_cpp.exe", "test_fluent_msvc.exe", "demo.exe",
    /* PDB and obj files from MSVC */
    "test_msvc.pdb", "test_msvc_cpp.pdb", "test_fluent_msvc.pdb", "demo.pdb",
    "test_msvc.obj", "test_msvc_cpp.obj", "test_fluent_msvc.obj", "demo.obj",
#else
    "test_gcc", "test_clang", "test_gcc_san", "test_clang_san",
    "test_gpp", "test_clangpp", "test_fluent_gcc", "test_fluent_clang",
    "demo",
#endif
    NULL
};

static bool clean_artifacts(void) {
    bool all_ok = true;
    nob_log(NOB_INFO, "Cleaning build artifacts...");
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

    const char *program = nob_shift(argv, argc);
    (void)program;

    if (argc > 0) {
        const char *subcmd = nob_shift(argv, argc);
        if (strcmp(subcmd, "clean") == 0) {
            if (clean_artifacts()) {
                nob_log(NOB_INFO, "Clean complete.");
                return 0;
            } else {
                nob_log(NOB_ERROR, "Clean failed.");
                return 1;
            }
        } else {
            nob_log(NOB_ERROR, "Unknown subcommand: %s", subcmd);
            nob_log(NOB_INFO, "Usage: ./nob [clean]");
            return 1;
        }
    }

    bool all_ok = true;
    Nob_Procs procs = {0};

    nob_log(NOB_INFO, "Building with %d parallel jobs...", nob_nprocs());

#ifdef _WIN32
    BuildConfig test_configs[] = {
        {COMPILER_MSVC,     false, "MSVC (C)",   "test_msvc.exe"},
        {COMPILER_MSVC_CPP, false, "MSVC (C++)", "test_msvc_cpp.exe"},
    };
    BuildConfig fluent_configs[] = {
        {COMPILER_MSVC, false, "MSVC Fluent", "test_fluent_msvc.exe"},
    };
    BuildConfig demo_config = {COMPILER_MSVC, false, "Demo", "demo.exe"};
    const char *demo_output = "demo.exe";
#else
    BuildConfig test_configs[] = {
        {COMPILER_GCC,     false, "GCC",                "./test_gcc"},
        {COMPILER_CLANG,   false, "Clang",              "./test_clang"},
        {COMPILER_GCC,     true,  "GCC + sanitizers",   "./test_gcc_san"},
        {COMPILER_CLANG,   true,  "Clang + sanitizers", "./test_clang_san"},
        {COMPILER_GPP,     false, "G++ (C++)",          "./test_gpp"},
        {COMPILER_CLANGPP, false, "Clang++ (C++)",      "./test_clangpp"},
    };
    BuildConfig fluent_configs[] = {
        {COMPILER_GCC,   false, "GCC Fluent",   "./test_fluent_gcc"},
        {COMPILER_CLANG, false, "Clang Fluent", "./test_fluent_clang"},
    };
    BuildConfig demo_config = {COMPILER_GCC, false, "Demo", "./demo"};
    const char *demo_output = "./demo";
#endif

    size_t test_count = sizeof(test_configs) / sizeof(test_configs[0]);
    size_t fluent_count = sizeof(fluent_configs) / sizeof(fluent_configs[0]);

    /* Phase 1: Build everything in parallel */
    nob_log(NOB_INFO, "=== Building all targets ===");

    for (size_t i = 0; i < test_count; i++) {
        nob_log(NOB_INFO, "  Starting build: %s", test_configs[i].name);
        build_source_async(test_configs[i], "test.c", NULL, &procs);
    }

    for (size_t i = 0; i < fluent_count; i++) {
        nob_log(NOB_INFO, "  Starting build: %s", fluent_configs[i].name);
        build_source_async(fluent_configs[i], "test_fluent_api.c", NULL, &procs);
    }

    nob_log(NOB_INFO, "  Starting build: %s", demo_config.name);
    build_source_async(demo_config, "demo.c", NULL, &procs);

    /* Wait for all builds to complete */
    if (!nob_procs_wait(procs)) {
        nob_log(NOB_ERROR, "Some builds failed");
        all_ok = false;
    }
    procs.count = 0;

    if (!all_ok) goto end;

    /* Phase 2: Run all tests in parallel */
    nob_log(NOB_INFO, "=== Running all tests ===");

    for (size_t i = 0; i < test_count; i++) {
        nob_log(NOB_INFO, "  Starting test: %s", test_configs[i].name);
        run_test_async(test_configs[i].output, &procs);
    }

    for (size_t i = 0; i < fluent_count; i++) {
        nob_log(NOB_INFO, "  Starting test: %s", fluent_configs[i].name);
        run_test_async(fluent_configs[i].output, &procs);
    }

    if (!nob_procs_wait(procs)) {
        nob_log(NOB_ERROR, "Some tests failed");
        all_ok = false;
    }
    procs.count = 0;

#ifndef _WIN32
    /* Phase 3: Valgrind (must be sequential, slow) */
    nob_log(NOB_INFO, "=== Running valgrind ===");
    if (!run_valgrind("./test_gcc")) {
        nob_log(NOB_ERROR, "Valgrind check failed");
        all_ok = false;
    }
#endif

    /* Phase 4: Run demo to show it works */
    nob_log(NOB_INFO, "=== Running demo ===");
    if (!run_test_async(demo_output, NULL)) {
        nob_log(NOB_ERROR, "Demo failed");
        all_ok = false;
    }

end:
    nob_da_free(procs);

    if (all_ok) {
        nob_log(NOB_INFO, "All builds and tests passed!");
        return 0;
    } else {
        nob_log(NOB_ERROR, "Some tests failed!");
        return 1;
    }
}
