snakepath

vibe slopcoding experiment: C99 STB style ~port of Python's standard pathlib module. Use at your own peril, assume a human hasn't reviewed any of this. Built with Claude Code and Cursor, with opus 4.5

C99 header-only pathlib library. POSIX + Windows. No malloc.

A pure-computation path manipulation library inspired by Python's `pathlib`. Provides all the path parsing, joining, and manipulation features without any filesystem I/O.

## Features

- **Header-only**: Single file, STB-style library
- **No allocations**: Fixed-size buffers, no malloc/free
- **Cross-platform**: POSIX and Windows path semantics
- **Two APIs**: 
  - **Boring API**: Traditional C functions (`sp_parent(&p)`)
  - **Fluent API**: Python-like chaining (`SPF("a/b").parent().name()`)

## Quick Start

```c
#define SNAKEPATH_IMPLEMENTATION
#include "snakepath.h"

// Boring API
SpPath p = sp_path("a/b/c.txt");
SpStr name = sp_name(&p);      // "c.txt"
SpPath parent = sp_parent(&p); // "a/b"

// Fluent API (define SNAKEPATH_FLUENT)
#define SNAKEPATH_FLUENT
const char *n = SPF_P("/a/b/c.txt").parent().name().data; // "b"
```

## Build

```bash
cc -o nob nob.c && ./nob
```

Runs all compiler/test combinations automatically.

## File Structure

```
snakepath.h              # Library (header-only, define SNAKEPATH_IMPLEMENTATION)
nob.c                    # Build script
nob.h                    # Build system (tsoding/nob.h)
demo.c                   # Usage examples
README.md                # This file

tests/                   # All tests
  test.c                 # Core API tests
  test_fluent_api.c      # Fluent API tests
  python_harness/        # Python bindings and test harness
    snakepath/           # Python package
      __init__.py        # Thin ctypes wrapper over C library
    snakepath_lib.c      # FFI wrapper for Python bindings
    run_cpython_tests.py # Runs CPython's pathlib test suite against snakepath
    skip.txt             # Exact list of tests to skip (unimplemented features)
```

## Boring API

### Path Creation

```c
SpPath p = sp_path("a/b/c");                    // Native flavor
SpPath p = sp_path_f("a/b/c", SP_FLAVOR_POSIX); // Explicit POSIX
SpPath p = sp_path_f("C:/x", SP_FLAVOR_WINDOWS); // Explicit Windows
```

### Path Components

```c
SpStr name   = sp_name(&p);     // Final component: "file.txt"
SpStr stem   = sp_stem(&p);     // Name without suffix: "file"
SpStr suffix = sp_suffix(&p);   // Extension: ".txt"
SpStr drive  = sp_drive(&p);    // Drive letter: "C:" (Windows)
SpStr root   = sp_root(&p);     // Root: "/" or "\"
SpStr anchor = sp_anchor(&p);   // Drive + root: "C:\"
```

### Navigation

```c
SpPath parent = sp_parent(&p);  // Parent directory

// Iterate parts
SpPartsIter it = sp_parts_begin(&p);
SpStr part;
while (sp_parts_next(&it, &part)) { /* use part */ }

// Iterate parents  
SpParentsIter pit = sp_parents_begin(&p);
SpPath par;
while (sp_parents_next(&pit, &par)) { /* use par */ }
```

### Joining

```c
SpPath joined = sp_join_one(&p, "subdir");
SpPath joined = sp_join(&p, "a", "b", "c");  // C only (variadic)
SpPath joined = sp_joinpath(&p, &other);
```

### Modification

```c
SpPath new_p = sp_with_name(&p, "newfile.txt");
SpPath new_p = sp_with_stem(&p, "newname");
SpPath new_p = sp_with_suffix(&p, ".md");
```

### Queries

```c
bool abs = sp_is_absolute(&p);
bool rel = sp_is_relative_to(&p, &base);
SpPath relative = sp_relative_to(&p, &base);
bool eq = sp_path_eq(&a, &b);
```

## Fluent API

Enable with `#define SNAKEPATH_FLUENT` before including.

```c
#define SNAKEPATH_FLUENT
#define SNAKEPATH_IMPLEMENTATION
#include "snakepath.h"

// Python: Path('a/b/c').parent.name
SpPath p = SPF("a/b/c").parent().path();
SpStr name = sp_name(&p);  // "b"

// Python: PurePosixPath('/etc').joinpath('init.d', 'apache2')
SpPath etc = SPF_P("/etc").join("init.d").join("apache2").path();
printf("%s\n", sp_str(&etc));  // "/etc/init.d/apache2"

// Branching from a common base - use non-fluent API
SpPath base = SPF_P("/home/user").path();
SpPath docs = sp_join_one(&base, "Documents");
SpPath pics = sp_join_one(&base, "Pictures");
```

### Path Creation Macros

| Macro | Python Equivalent | Description |
|-------|-------------------|-------------|
| `SPF("path")` | `Path('path')` | Native platform |
| `SPF_P("path")` | `PurePosixPath('path')` | POSIX semantics |
| `SPF_W("path")` | `PureWindowsPath('path')` | Windows semantics |

### Chainable Methods

| Method | Description |
|--------|-------------|
| `.parent()` | Parent directory |
| `.join("x")` | Join with component |
| `.with_name("x")` | Replace name |
| `.with_stem("x")` | Replace stem |
| `.with_suffix(".x")` | Replace suffix |
| `.absolute()` | Make path absolute |
| `.relative_to(&base)` | Relative path from base |
| `.relative_to_walk_up(&base)` | Relative path with `..` |
| `.path()` | **Finish chain → returns `SpPath`** |

### Accessing Results

Call `.path()` to finish the chain and get an `SpPath`:

```c
SpPath p = SPF_P("/home/user").join("docs").with_suffix(".txt").path();
printf("%s\n", sp_str(&p));        // Path as string
printf("%.*s\n", (int)sp_name(&p).len, sp_name(&p).data);  // Name
bool abs = sp_is_absolute(&p);     // Check if absolute
```

## Core Types

```c
SpStr       { const char *data; size_t len; }        // String view
SpPath      { char buf[4096]; size_t len; SpFlavor flavor; }
SpFlavor    { SP_FLAVOR_NATIVE, SP_FLAVOR_POSIX, SP_FLAVOR_WINDOWS }
```

## Configuration

Define before including:

```c
#define SP_PATH_MAX 4096      // Max path length
#define SP_MAX_PARTS 256      // Max path components
#define SP_MAX_SUFFIXES 16    // Max file extensions
```

## Platform Behavior

| | POSIX | Windows |
|---|-------|---------|
| Separator | `/` | `\` (normalizes `/`) |
| Drive | None | `C:` or UNC `\\server\share` |
| Absolute | Has root `/` | Has drive + root `C:\` |

## Python Bindings

Thin ctypes wrapper providing `PurePath`, `PurePosixPath`, and `PureWindowsPath` classes compatible with Python's pathlib interface.

```python
from snakepath import PurePosixPath, PureWindowsPath

p = PurePosixPath('/usr/local/bin')
print(p.name)      # 'bin'
print(p.parent)    # PurePosixPath('/usr/local')
print(p.suffix)    # ''

w = PureWindowsPath('C:/Users/test.txt')
print(w.drive)     # 'C:'
print(w.stem)      # 'test'
```

### Building

```bash
cd tests/python_harness
gcc -shared -fPIC -fvisibility=hidden -O2 -I../.. -o libsnakepath.so snakepath_lib.c
```

### Testing

Tests run CPython's official pathlib test suite against snakepath:

```bash
python tests/python_harness/run_cpython_tests.py
```

- Test runner: [`tests/python_harness/run_cpython_tests.py`](tests/python_harness/run_cpython_tests.py)
- Skip list: [`tests/python_harness/skip.txt`](tests/python_harness/skip.txt) - exact tests to skip for unimplemented features

## Gotchas

### Empty paths normalize to `"."`

Like Python's pathlib, an empty path string normalizes to `"."` (current directory):

```c
SpPath p = sp_path("");
printf("%s\n", sp_str(&p));  // prints "."
```

Python behaves identically ([docs](https://docs.python.org/3/library/pathlib.html#pathlib.PurePath)):

```python
>>> from pathlib import PurePath
>>> PurePath('')
PurePosixPath('.')
```

This means paths can never be empty—the minimum valid path is `"."`.

### `sp_suffixes()` returns all dot-separated segments

Like Python's `pathlib.Path.suffixes`, this function returns **all** segments after dots in the filename—not just "real" file extensions. This means version numbers get included:

```c
SpPath p = sp_path("snakepath-1.0.0.tar.gz");
SpSuffixes s = sp_suffixes(&p);
// s.count == 4
// s.items: ".0", ".0", ".tar", ".gz"
```

Python behaves identically:

```python
>>> from pathlib import Path
>>> Path("snakepath-1.0.0.tar.gz").suffixes
['.0', '.0', '.tar', '.gz']
```

If you only want the final extension, use `sp_suffix()` which returns just `.gz`.

## Code Style

- `sp_` prefix for public API
- `sp_priv_` prefix for internal helpers
- `SpPath`, `SpStr` structs (CamelCase)
- `SP_FLAVOR_*`, `SP_PATH_MAX` constants (SCREAMING_CASE)
- C99 compatible, no C11/C++ features in core library
- `-Wall -Wextra -Wpedantic -Werror`

---

# Pathlib Functionality Checklist

Complete mapping of Python's pathlib to snakepath. Use status markers to track implementation progress.

## Status Key

| Status | Meaning |
|--------|---------|
| `BORING_DONE` | Implemented in boring API (`sp_*` functions) |
| `FLUENT_DONE` | Implemented in fluent API |
| `FLUENT_TODO` | Needs fluent API wrapper |
| `NOT_PLANNED` | Out of scope (I/O, platform-specific) |
| `TODO` | Not yet implemented anywhere |

---

## Pure Path Properties (No I/O)

| Python | Boring API | Fluent API | Status |
|--------|-----------|------------|--------|
| `PurePath.parts` | `sp_parts_begin()` / `sp_parts_next()` | - | BORING_DONE, FLUENT_TODO |
| `PurePath.drive` | `sp_drive()` | `.drive()` | BORING_DONE, FLUENT_DONE |
| `PurePath.root` | `sp_root()` | `.root()` | BORING_DONE, FLUENT_DONE |
| `PurePath.anchor` | `sp_anchor()` | `.anchor()` | BORING_DONE, FLUENT_DONE |
| `PurePath.parents` | `sp_parents_begin()` / `sp_parents_next()` | - | BORING_DONE, FLUENT_TODO |
| `PurePath.parent` | `sp_parent()` | `.parent()` | BORING_DONE, FLUENT_DONE |
| `PurePath.name` | `sp_name()` | `.name()` | BORING_DONE, FLUENT_DONE |
| `PurePath.suffix` | `sp_suffix()` | `.suffix()` | BORING_DONE, FLUENT_DONE |
| `PurePath.suffixes` | `sp_suffixes()` | `.suffixes()` | BORING_DONE, FLUENT_DONE |
| `PurePath.stem` | `sp_stem()` | `.stem()` | BORING_DONE, FLUENT_DONE |

---

## Pure Path Methods (No I/O)

| Python | Boring API | Fluent API | Status |
|--------|-----------|------------|--------|
| `PurePath.as_posix()` | `sp_as_posix()` | `.as_posix()` | BORING_DONE, FLUENT_DONE |
| `PurePath.is_absolute()` | `sp_is_absolute()` | `.is_absolute()` | BORING_DONE, FLUENT_DONE |
| `PurePath.is_relative_to()` | `sp_is_relative_to()` | `.is_relative_to()` | BORING_DONE, FLUENT_DONE |
| `PurePath.joinpath()` | `sp_joinpath()` / `sp_join_one()` | `.join()` | BORING_DONE, FLUENT_DONE |
| `PurePath.match()` | - | - | NOT_PLANNED |
| `PurePath.full_match()` | - | - | NOT_PLANNED |
| `PurePath.relative_to()` | `sp_relative_to()` | `.relative_to()` | BORING_DONE, FLUENT_DONE |
| `PurePath.with_name()` | `sp_with_name()` | `.with_name()` | BORING_DONE, FLUENT_DONE |
| `PurePath.with_stem()` | `sp_with_stem()` | `.with_stem()` | BORING_DONE, FLUENT_DONE |
| `PurePath.with_suffix()` | `sp_with_suffix()` | `.with_suffix()` | BORING_DONE, FLUENT_DONE |
| `PurePath.with_segments()` | - | - | NOT_PLANNED |
| `/` operator | `sp_join_one()` | `.join()` | BORING_DONE, FLUENT_DONE |
| `str(p)` | `sp_str()` | `.str()` | BORING_DONE, FLUENT_DONE |
| `==` comparison | `sp_path_eq()` | - | BORING_DONE, FLUENT_TODO |

---

## Concrete Path Methods (I/O) - NOT IMPLEMENTED

These require filesystem access and are out of scope for this pure-computation library.

| Python | Status | Notes |
|--------|--------|-------|
| `Path.cwd()` | NOT_PLANNED | Requires `getcwd()` |
| `Path.home()` | NOT_PLANNED | Requires `getenv()`/`getpwuid()` |
| `Path.stat()` | NOT_PLANNED | Requires `stat()` |
| `Path.chmod()` | NOT_PLANNED | Requires `chmod()` |
| `Path.exists()` | NOT_PLANNED | Requires `stat()` |
| `Path.expanduser()` | NOT_PLANNED | Requires `getenv()`/`getpwuid()` |
| `Path.glob()` | NOT_PLANNED | Requires `opendir()`/`readdir()` |
| `Path.rglob()` | NOT_PLANNED | Requires recursive dir scan |
| `Path.group()` | NOT_PLANNED | Requires `getgrgid()` |
| `Path.is_dir()` | NOT_PLANNED | Requires `stat()` |
| `Path.is_file()` | NOT_PLANNED | Requires `stat()` |
| `Path.is_mount()` | NOT_PLANNED | Requires `statvfs()` |
| `Path.is_symlink()` | NOT_PLANNED | Requires `lstat()` |
| `Path.is_socket()` | NOT_PLANNED | Requires `stat()` |
| `Path.is_fifo()` | NOT_PLANNED | Requires `stat()` |
| `Path.is_block_device()` | NOT_PLANNED | Requires `stat()` |
| `Path.is_char_device()` | NOT_PLANNED | Requires `stat()` |
| `Path.is_junction()` | NOT_PLANNED | Windows-specific |
| `Path.iterdir()` | NOT_PLANNED | Requires `opendir()`/`readdir()` |
| `Path.walk()` | NOT_PLANNED | Requires recursive dir scan |
| `Path.lchmod()` | NOT_PLANNED | Requires `lchmod()` |
| `Path.lstat()` | NOT_PLANNED | Requires `lstat()` |
| `Path.mkdir()` | NOT_PLANNED | Requires `mkdir()` |
| `Path.open()` | NOT_PLANNED | Requires `fopen()` |
| `Path.owner()` | NOT_PLANNED | Requires `getpwuid()` |
| `Path.read_bytes()` | NOT_PLANNED | Requires `fread()` |
| `Path.read_text()` | NOT_PLANNED | Requires `fread()` |
| `Path.readlink()` | NOT_PLANNED | Requires `readlink()` |
| `Path.rename()` | NOT_PLANNED | Requires `rename()` |
| `Path.replace()` | NOT_PLANNED | Requires `rename()` |
| `Path.absolute()` | NOT_PLANNED | Requires `getcwd()` |
| `Path.resolve()` | NOT_PLANNED | Requires `realpath()` |
| `Path.rmdir()` | NOT_PLANNED | Requires `rmdir()` |
| `Path.samefile()` | NOT_PLANNED | Requires `stat()` |
| `Path.symlink_to()` | NOT_PLANNED | Requires `symlink()` |
| `Path.hardlink_to()` | NOT_PLANNED | Requires `link()` |
| `Path.touch()` | NOT_PLANNED | Requires `utime()`/`creat()` |
| `Path.unlink()` | NOT_PLANNED | Requires `unlink()` |
| `Path.write_bytes()` | NOT_PLANNED | Requires `fwrite()` |
| `Path.write_text()` | NOT_PLANNED | Requires `fwrite()` |
| `Path.copy()` | NOT_PLANNED | Requires file copy |
| `Path.copy_into()` | NOT_PLANNED | Requires file copy |
| `Path.move()` | NOT_PLANNED | Requires `rename()`/copy |
| `Path.move_into()` | NOT_PLANNED | Requires `rename()`/copy |
| `Path.as_uri()` | NOT_PLANNED | URI encoding |
| `Path.from_uri()` | NOT_PLANNED | URI parsing |

---

## Test Targets

**Linux**: `test_gcc`, `test_clang`, `test_gcc_san`, `test_clang_san`, `test_gpp`, `test_clangpp` + valgrind
**Windows**: `test_msvc.exe`, `test_msvc_cpp.exe`

## License

Public domain / MIT (choose whichever works for your project).
