/* nob.c - Build script for snakepath using nob.h
 * Usage: cc -o nob nob.c && ./nob
 */

#define NOB_IMPLEMENTATION
#include "nob.h"

#define COMMON_WARNINGS \
    "-Wall", "-Wextra", "-Wpedantic", "-Werror", \
    "-Wconversion", "-Wsign-conversion", "-Wshadow", \
    "-Wstrict-prototypes", "-Wmissing-prototypes", \
    "-Wold-style-definition", "-Wdouble-promotion", \
    "-Wundef", "-Wwrite-strings", "-Wcast-qual", \
    "-Wcast-align", "-Wpointer-arith", "-Wnull-dereference", \
    "-Wformat=2", "-Wformat-overflow=2", "-Wformat-truncation=2", \
    "-Wvla", "-Wlogical-op", "-Wduplicated-cond", \
    "-Wduplicated-branches", "-Wrestrict", "-Wjump-misses-init"

#define CLANG_WARNINGS \
    "-Wall", "-Wextra", "-Wpedantic", "-Werror", \
    "-Wconversion", "-Wsign-conversion", "-Wshadow", \
    "-Wstrict-prototypes", "-Wmissing-prototypes", \
    "-Wold-style-definition", "-Wdouble-promotion", \
    "-Wundef", "-Wwrite-strings", "-Wcast-qual", \
    "-Wcast-align", "-Wpointer-arith", "-Wnull-dereference", \
    "-Wformat=2", "-Wvla", "-Weverything", \
    "-Wno-declaration-after-statement", \
    "-Wno-disabled-macro-expansion", "-Wno-padded", \
    "-Wno-covered-switch-default", "-Wno-unknown-warning-option", \
    "-Wno-unsafe-buffer-usage"

static bool build_with_gcc(bool sanitizers) {
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "gcc");
    nob_cmd_append(&cmd, "-std=c99");
    nob_cmd_append(&cmd, COMMON_WARNINGS);
    if (sanitizers) {
        nob_cmd_append(&cmd, "-fsanitize=address,undefined");
        nob_cmd_append(&cmd, "-fno-omit-frame-pointer");
    }
    nob_cmd_append(&cmd, "-g", "-O0");
    nob_cmd_append(&cmd, "-o", sanitizers ? "test_gcc_san" : "test_gcc");
    nob_cmd_append(&cmd, "test.c");
    bool result = nob_cmd_run_sync(cmd);
    nob_cmd_free(cmd);
    return result;
}

static bool build_with_clang(bool sanitizers) {
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "clang");
    nob_cmd_append(&cmd, "-std=c99");
    nob_cmd_append(&cmd, CLANG_WARNINGS);
    if (sanitizers) {
        nob_cmd_append(&cmd, "-fsanitize=address,undefined");
        nob_cmd_append(&cmd, "-fno-omit-frame-pointer");
    }
    nob_cmd_append(&cmd, "-g", "-O0");
    nob_cmd_append(&cmd, "-o", sanitizers ? "test_clang_san" : "test_clang");
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

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    bool all_ok = true;

    nob_log(NOB_INFO, "Building with GCC...");
    if (!build_with_gcc(false)) {
        nob_log(NOB_ERROR, "GCC build failed");
        all_ok = false;
    } else {
        nob_log(NOB_INFO, "Running GCC build...");
        if (!run_test("./test_gcc")) {
            nob_log(NOB_ERROR, "GCC tests failed");
            all_ok = false;
        }
    }

    nob_log(NOB_INFO, "Building with Clang...");
    if (!build_with_clang(false)) {
        nob_log(NOB_ERROR, "Clang build failed");
        all_ok = false;
    } else {
        nob_log(NOB_INFO, "Running Clang build...");
        if (!run_test("./test_clang")) {
            nob_log(NOB_ERROR, "Clang tests failed");
            all_ok = false;
        }
    }

    nob_log(NOB_INFO, "Building with GCC + sanitizers...");
    if (!build_with_gcc(true)) {
        nob_log(NOB_ERROR, "GCC sanitizer build failed");
        all_ok = false;
    } else {
        nob_log(NOB_INFO, "Running GCC sanitizer build...");
        if (!run_test("./test_gcc_san")) {
            nob_log(NOB_ERROR, "GCC sanitizer tests failed");
            all_ok = false;
        }
    }

    nob_log(NOB_INFO, "Building with Clang + sanitizers (optional)...");
    if (!build_with_clang(true)) {
        nob_log(NOB_WARNING, "Clang sanitizer build skipped (missing runtime)");
    } else {
        nob_log(NOB_INFO, "Running Clang sanitizer build...");
        if (!run_test("./test_clang_san")) {
            nob_log(NOB_ERROR, "Clang sanitizer tests failed");
            all_ok = false;
        }
    }

    nob_log(NOB_INFO, "Running valgrind...");
    if (!run_valgrind("./test_gcc")) {
        nob_log(NOB_ERROR, "Valgrind check failed");
        all_ok = false;
    }

    if (all_ok) {
        nob_log(NOB_INFO, "All builds and tests passed!");
        return 0;
    } else {
        nob_log(NOB_ERROR, "Some tests failed!");
        return 1;
    }
}
