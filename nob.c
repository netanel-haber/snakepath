/* nob.c - Build script for snakepath using nob.h
 * Usage: cc -o nob nob.c && ./nob
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

static bool build(BuildConfig cfg) {
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

#ifdef _WIN32
    if (cfg.compiler == COMPILER_MSVC || cfg.compiler == COMPILER_MSVC_CPP) {
        nob_cmd_append(&cmd, "/Od", "/Zi");
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

    nob_cmd_append(&cmd, "test.c");
    bool result = nob_cmd_run_sync(cmd);
    nob_cmd_free(cmd);
    return result;
}

static bool run_test(const char *exe) {
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, exe);
    bool result = nob_cmd_run_sync(cmd);
    nob_cmd_free(cmd);
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
    bool result = nob_cmd_run_sync(cmd);
    nob_cmd_free(cmd);
    return result;
}
#endif

static bool build_and_test(BuildConfig cfg, bool *all_ok, bool optional) {
    nob_log(NOB_INFO, "Building with %s...", cfg.name);
    if (!build(cfg)) {
        if (optional) {
            nob_log(NOB_WARNING, "%s build skipped (missing runtime)", cfg.name);
            return true;
        }
        nob_log(NOB_ERROR, "%s build failed", cfg.name);
        *all_ok = false;
        return false;
    }

    nob_log(NOB_INFO, "Running %s build...", cfg.name);
    if (!run_test(cfg.output)) {
        nob_log(NOB_ERROR, "%s tests failed", cfg.name);
        *all_ok = false;
        return false;
    }
    return true;
}

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    bool all_ok = true;

#ifdef _WIN32
    BuildConfig configs[] = {
        {COMPILER_MSVC,     false, "MSVC (C)",   "test_msvc.exe"},
        {COMPILER_MSVC_CPP, false, "MSVC (C++)", "test_msvc_cpp.exe"},
    };
    size_t config_count = sizeof(configs) / sizeof(configs[0]);

    for (size_t i = 0; i < config_count; i++) {
        build_and_test(configs[i], &all_ok, false);
    }
#else
    BuildConfig configs[] = {
        {COMPILER_GCC,     false, "GCC",                 "./test_gcc"},
        {COMPILER_CLANG,   false, "Clang",               "./test_clang"},
        {COMPILER_GCC,     true,  "GCC + sanitizers",    "./test_gcc_san"},
        {COMPILER_CLANG,   true,  "Clang + sanitizers",  "./test_clang_san"},
        {COMPILER_GPP,     false, "G++ (C++)",           "./test_gpp"},
        {COMPILER_CLANGPP, false, "Clang++ (C++)",       "./test_clangpp"},
    };
    size_t config_count = sizeof(configs) / sizeof(configs[0]);

    for (size_t i = 0; i < config_count; i++) {
        bool optional = configs[i].sanitizers && configs[i].compiler == COMPILER_CLANG;
        build_and_test(configs[i], &all_ok, optional);
    }

    nob_log(NOB_INFO, "Running valgrind...");
    if (!run_valgrind("./test_gcc")) {
        nob_log(NOB_ERROR, "Valgrind check failed");
        all_ok = false;
    }
#endif

    if (all_ok) {
        nob_log(NOB_INFO, "All builds and tests passed!");
        return 0;
    } else {
        nob_log(NOB_ERROR, "Some tests failed!");
        return 1;
    }
}
