# snakepath - Agent Reference

C99 header-only pathlib library. POSIX + Windows. No malloc.

## Build

```bash
cc -o nob nob.c && ./nob
```

Runs all compiler/test combinations automatically.

## Structure

```
snakepath.h   # Library (header-only, define SNAKEPATH_IMPLEMENTATION)
test.c        # 54 tests
nob.c         # Build script
nob.h         # Build system (tsoding/nob.h)
```

## Usage

```c
#define SNAKEPATH_IMPLEMENTATION
#include "snakepath.h"

SpPath p = sp_path("a/b/c.txt", SP_FLAVOR_POSIX);
SpStr name = sp_name(&p);  // "c.txt"
SpPath parent = sp_parent(&p);  // "a/b"
```

## Test Targets

**Linux**: `test_gcc`, `test_clang`, `test_gcc_san`, `test_clang_san`, `test_gpp`, `test_clangpp` + valgrind
**Windows**: `test_msvc.exe`, `test_msvc_cpp.exe`

## Code Style

- `sp_` prefix for public API
- `sp_priv_` prefix for internal helpers
- `SpPath`, `SpStr` structs (CamelCase)
- `SP_FLAVOR_*`, `SP_PATH_MAX` constants (SCREAMING_CASE)
- C99 only, no C11/C++ features in library
- Fixed buffers, no allocation
- `-Wall -Wextra -Wpedantic -Werror`

## Core Types

```c
SpStr       { const char *data; size_t len; }        // String view
SpPath      { char buf[4096]; size_t len; SpFlavor flavor; }
SpFlavor    { SP_FLAVOR_NATIVE, SP_FLAVOR_POSIX, SP_FLAVOR_WINDOWS }
```

## API

**Create**: `sp_path()`, `sp_path_f()`, `sp_path_from_sv()`, `sp_path_copy()`
**Parts**: `sp_drive()`, `sp_root()`, `sp_anchor()`, `sp_name()`, `sp_stem()`, `sp_suffix()`, `sp_suffixes()`
**Navigate**: `sp_parent()`, `sp_parts_begin()`, `sp_parts_next()`, `sp_parents_begin()`, `sp_parents_next()`
**Modify**: `sp_join_one()`, `sp_joinpath()`, `sp_with_name()`, `sp_with_stem()`, `sp_with_suffix()`
**Query**: `sp_is_absolute()`, `sp_is_relative_to()`, `sp_relative_to()`, `sp_path_eq()`, `sp_is_empty()`
**Convert**: `sp_str()`, `sp_as_sv()`, `sp_as_posix()`

## Test Framework

```c
TEST(test_name) {
    SpPath p = sp_path_f("path", SP_FLAVOR_POSIX);
    ASSERT_SV_EQ(sp_name(&p), "expected");
    ASSERT_PATH_EQ(&p, "expected");
}

RUN(test_name);
```

## Config Macros

Define before including:
- `SP_PATH_MAX` (default 4096)
- `SP_MAX_PARTS` (default 256)
- `SP_MAX_SUFFIXES` (default 16)

## Platform Behavior

**POSIX**: `/` separator, no drive letters, anchor = `/`
**Windows**: `\` or `/` (normalized to `\`), drive letters (`C:`), UNC (`\\server\share`), anchor = drive + root
