Snakepath 
C99 header-only pathlib library. POSIX + Windows. No malloc. Vibe-coded with Claude Code + Cursor.

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
const char *n = SPF_P("/a/b/c.txt")->parent()->name().data; // "b"
```

## Test/CI

```bash
cc -o nob nob.c && ./nob
```
Runs all compiler/test combinations automatically.

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

// Path('a/b/c').parent.name -> "b"
SpStr name = SPF("a/b/c")->parent()->name();

// PurePosixPath('/etc').joinpath('init.d').name -> "init.d"
const char *s = SPF_P("/etc")->join("init.d")->join("apache2")->str();

SpPath p = SPF_W("C:/Users")->join("docs")->path();
SpPath child = sp_join_one(&p, "file.txt");
```

### Macros

| Macro | Python |
|-------|--------|
| `SPF("path")` | `Path('path')` |
| `SPF_P("path")` | `PurePosixPath('path')` |
| `SPF_W("path")` | `PureWindowsPath('path')` |

### Chainable

`->parent()` `->join("x")` `->with_name("x")` `->with_stem("x")` `->with_suffix(".x")` `->absolute()` `->relative_to(&p)` `->relative_to_walk_up(&p)`

### Terminators

| Method | Returns |
|--------|---------|
| `->path()` | `SpPath` |
| `->str()` | `const char*` |
| `->name()` `->stem()` `->suffix()` | `SpStr` |
| `->drive()` `->root()` `->anchor()` | `SpStr` |
| `->suffixes()` | `SpSuffixes` |
| `->is_absolute()` | `bool` |
| `->is_relative_to(&p)` | `bool` |

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

### Testing

Tests run CPython's official pathlib test suite against snakepath:

```bash
python tests/python_harness/run_cpython_tests.py
```

- Test runner: [`tests/python_harness/run_cpython_tests.py`](tests/python_harness/run_cpython_tests.py)
- Expected failures: Defined as `EXPECTED_FAILURES` dict in the test runner for unimplemented features

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

## Pathlib Mapping

| Python | Boring API | Fluent |
|--------|-----------|--------|
| `.parts` | `sp_parts_begin/next()` | - |
| `.drive` | `sp_drive()` | `.drive()` |
| `.root` | `sp_root()` | `.root()` |
| `.anchor` | `sp_anchor()` | `.anchor()` |
| `.parents` | `sp_parents_begin/next()` | - |
| `.parent` | `sp_parent()` | `.parent()` |
| `.name` | `sp_name()` | `.name()` |
| `.suffix` | `sp_suffix()` | `.suffix()` |
| `.suffixes` | `sp_suffixes()` | `.suffixes()` |
| `.stem` | `sp_stem()` | `.stem()` |
| `.as_posix()` | `sp_as_posix()` | - |
| `.is_absolute()` | `sp_is_absolute()` | `.is_absolute()` |
| `.is_relative_to()` | `sp_is_relative_to()` | `.is_relative_to()` |
| `.joinpath()` | `sp_join_one()` | `.join()` |
| `.match()` | `sp_match()` | - |
| `.relative_to()` | `sp_relative_to()` | `.relative_to()` |
| `.with_name()` | `sp_with_name()` | `.with_name()` |
| `.with_stem()` | `sp_with_stem()` | `.with_stem()` |
| `.with_suffix()` | `sp_with_suffix()` | `.with_suffix()` |
| `/` operator | `sp_join_one()` | `.join()` |
| `str(p)` | `sp_str()` | `.str()` |
| `==` | `sp_path_eq()` | - |
| `.absolute()` | `sp_absolute()` | `.absolute()` |
| `.as_uri()` | `sp_as_uri()` | - |
| `Path.cwd()` | `sp_cwd()` | - |

**Not implemented:** Most I/O methods (`stat`, `exists`, `mkdir`, `read_*`, `write_*`, `glob`, etc.)

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
```

```c
÷++++++++++++++++++++÷+÷÷÷÷÷÷÷÷÷÷÷÷÷÷≈≈ ≈÷
÷÷+++++++++++++++++++÷++÷+÷÷+÷÷÷÷÷÷÷÷≈ ≈≈≈
÷++++++++++++++++++++++÷+++÷+÷÷÷÷÷÷÷≈ ≈≈≈≈
+++++++++++≈      ÷+++÷÷÷+÷÷+÷÷÷÷÷÷  ≈≈≈≈≈
++++++++++   ≈        ++++÷+÷+÷÷÷  ≈≈≈≈≈≈≈
÷+++++++÷  ÷++++++++÷    ÷+÷÷÷÷  ÷÷≈≈≈≈≈≈≈
+++++++   ++++++++++++÷+÷     ≈÷÷÷≈≈ ≈≈≈≈≈
÷++++    +++++÷+++++++÷+÷+÷÷÷÷÷÷÷÷≈≈≈≈≈≈≈≈
÷++   ÷++++++++÷  ++++÷+÷÷÷÷÷÷÷÷÷÷÷≈≈≈≈≈≈≈
++   ++++÷++++      ++++÷÷+÷÷÷+÷+÷÷≈÷÷÷≈÷÷
+   +++++++++        ++++++      +÷÷÷÷÷÷÷÷
+  +++++++++    ++    +++÷       ÷÷÷÷÷÷÷÷÷
+  +++++++÷    ++++   +++   +++   +÷÷÷÷÷ ÷
+  ++++++++   +++++   ++   ++++   ++++÷  ÷
+  ≈+++++++   +++++   +   +++++   ++++   ÷
++   ++++++  ÷++++   ≈+  ≈+++++  ++++   ÷÷
++÷   ≈++++  +++++   ++  +++++   ++++  +÷÷
++++≈  ÷+++  ++++   ++≈  +++++  +++++ +÷÷÷
++++++  +++  ÷+++   +++  ++++  +++++  ++÷÷
++++++  +++  ÷++÷   +++   +++  +++++  ++÷÷
++++++  +++   ++   ++++   +++  +++   ÷+÷÷≈
+++++   +++   ++   +++++   ++  ≈÷   ++÷+÷÷
+++++  ++++   ++   ÷++++   +++     +++÷÷÷÷
+++++  ++++   ++÷   +++++   ++++++++++++÷÷
+++++  ++++   +++    ++++   ++++++++++++÷÷
+++++  ++++   ++++    ≈+   +++++++++++++÷÷
+++++   ++   +++++≈        +++++++++++++÷÷
++++++      ++++++++      +++++++++++++++÷
+++++++÷   ++++++++++++++++++++++++++÷÷+++
```
Source: https://pdimagearchive.org/images/6bccf45f-787d-488c-a37a-db49d858add9/
