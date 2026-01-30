#define SNAKEPATH_FLUENT
#define SNAKEPATH_IMPLEMENTATION
#include "snakepath.h"
#include <stdio.h>

static void print_sv(const char *label, SpStr sv) {
    printf("  %-12s: %.*s\n", label, (int)sv.len, sv.data);
}

int main(void) {
    const char *repo_files[] = {
        "snakepath.h",
        "test.c",
        "test_fluent_api.c",
        "nob.c",
        "nob.h",
        ".github/workflows/ci.yml",
        NULL
    };

    printf("=== DISSECTING REPO FILES ===\n\n");
    
    for (int i = 0; repo_files[i]; i++) {
        SpPath p = sp_path(repo_files[i]);
        printf("%s\n", sp_str(&p));
        print_sv("name", sp_name(&p));
        print_sv("stem", sp_stem(&p));
        print_sv("suffix", sp_suffix(&p));
        printf("\n");
    }

    printf("=== BUILD OUTPUT PATHS (boring API) ===\n\n");

    SpPath build_dir = sp_path("build");
    for (int i = 0; repo_files[i]; i++) {
        SpPath src = sp_path(repo_files[i]);
        if (!sp_sv_eq_cstr(sp_suffix(&src), ".c")) continue;
        
        SpPath obj = sp_with_suffix(&src, ".o");
        SpPath output = sp_join_one(&build_dir, sp_name(&obj).data);
        printf("  %s -> %s\n", repo_files[i], sp_str(&output));
    }

    printf("\n=== RENAME EXTENSIONS (fluent API) ===\n\n");

    for (int i = 0; repo_files[i]; i++) {
        SpPath src = sp_path(repo_files[i]);
        if (!sp_sv_eq_cstr(sp_suffix(&src), ".h")) continue;

        SpStr new_name = SPF(repo_files[i])->with_suffix(".hpp")->name();
        printf("  %s -> %.*s\n", repo_files[i], (int)new_name.len, new_name.data);
    }

    printf("\n=== ITERATE PARENT DIRECTORIES ===\n\n");

    SpPath deep = sp_path(".github/workflows/ci.yml");
    printf("Parents of %s:\n", sp_str(&deep));
    
    SpParentsIter pit = sp_parents_begin(&deep);
    SpPath parent;
    while (sp_parents_next(&pit, &parent)) {
        printf("  %s\n", sp_str(&parent));
    }

    printf("\n=== CROSS-PLATFORM PATH HANDLING ===\n\n");

    SpPath win = sp_path_f("C:\\Users\\dev\\snakepath\\snakepath.h", SP_FLAVOR_WINDOWS);
    SpPath posix = sp_path_f("/home/dev/snakepath/snakepath.h", SP_FLAVOR_POSIX);

    printf("Windows path:\n");
    print_sv("drive", sp_drive(&win));
    print_sv("root", sp_root(&win));
    print_sv("name", sp_name(&win));
    
    printf("\nPOSIX path:\n");
    print_sv("drive", sp_drive(&posix));
    print_sv("root", sp_root(&posix));
    print_sv("name", sp_name(&posix));

    printf("\n=== RELATIVE PATH COMPUTATION ===\n\n");

    SpPath base = sp_path("/home/nhaber/snakepath");
    SpPath file = sp_path("/home/nhaber/snakepath/test.c");
    SpPath relative = sp_relative_to(&file, &base);
    
    printf("  base: %s\n", sp_str(&base));
    printf("  file: %s\n", sp_str(&file));
    printf("  relative: %s\n", sp_str(&relative));

    printf("\n=== CHAINED TRANSFORMATIONS ===\n\n");

    const char *result = SPF_P("/home/nhaber/snakepath/test.c")
        ->parent()
        ->join("build")
        ->join("release")
        ->with_name("libsnakepath.so")
        ->str();

    printf("  %s\n", result);

    printf("\n=== ITERATE PATH PARTS ===\n\n");

    SpPath workflow = sp_path(".github/workflows/ci.yml");
    printf("Parts of %s:\n", sp_str(&workflow));
    
    SpPartsIter it = sp_parts_begin(&workflow);
    SpStr part;
    int n = 0;
    while (sp_parts_next(&it, &part)) {
        printf("  [%d] %.*s\n", n++, (int)part.len, part.data);
    }

    printf("\n=== MULTIPLE EXTENSIONS ===\n\n");

    SpPath tarball = sp_path("snakepath-1.0.0.tar.gz");
    SpSuffixes suffixes = sp_suffixes(&tarball);
    
    printf("%s has %zu suffixes:\n", sp_str(&tarball), suffixes.count);
    for (size_t j = 0; j < suffixes.count; j++) {
        printf("  %.*s\n", (int)suffixes.items[j].len, suffixes.items[j].data);
    }

    return 0;
}
