Snakepath:
C99 STB-style header-only based on [python's pathlib library](https://docs.python.org/3/library/pathlib.html), because I love pathlib.
POSIX + Windows. No malloc.
Vibe-coded with Claude Code + Cursor.

The original API was created by [Antoine Pitrou](https://peps.python.org/pep-0428/).

```c
u=https://raw.githubusercontent.com/netanel-haber/snakepath/main/snakepath.h &&
curl -sSLo snakepath.h "$u" &&
cat <<'EOF' | cc -xc - -o demo &&
#define SP_PATH_MAX 4096
#define SNAKEPATH_FLUENT
#define SNAKEPATH_IMPLEMENTATION
#include "snakepath.h"
#include <stdio.h>

int main(void) {
    SpPath boring = sp_path("/foo/bar.txt");
    printf("BORING API: %s\n", sp_name(&boring).data);
    printf("BORING API: %s\n", sp_stem(&boring).data);

    const char* fluent =
        SPF("/etc")->join("nginx")->join("nginx.conf")->str();
    printf("FLUENT API: %s\n", fluent);
}
EOF
./demo
rm -f demo snakepath.h
```

## Build & Test

```bash
cc -o nob nob.c && ./nob
```


<details markdown="1">
<summary>Snakepath API Reference</summary>

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
SpPath joined = sp_join_sv(&p, sv);          // Join with SpStr (preserves embedded nulls)
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
#define SP_PATH_MAX 4096
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
| `->is_file()` `->is_dir()` `->exists()` | `bool` |

## Core Types

```c
SpStr       { const char *data; size_t len; }        // String view
SpPath      { char buf[4096]; size_t len; SpFlavor flavor; }
SpFlavor    { SP_FLAVOR_NATIVE, SP_FLAVOR_POSIX, SP_FLAVOR_WINDOWS }
```

## Configuration

Define before including:

```c
#define SP_PATH_MAX 4096      // Max path length (REQUIRED)
#define SP_MAX_SUFFIXES 16    // Max file extensions (optional, default 16)
```

**Note:** `SP_PATH_MAX` must be defined before including the header. Use `SP_PATH_MAX_LINUX` (4096) or `SP_PATH_MAX_WINDOWS` (1024) as guidance.

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

## Gotchas

### Empty paths normalize to `"."`

Like Python's pathlib, an empty path string normalizes to `"."` (current directory):

```c
SpPath p = sp_path("");
printf("%s\n", sp_str(&p));  // prints "."
```

### `sp_suffixes()` returns all dot-separated segments

Like Python's `pathlib.Path.suffixes`, this function returns **all** segments after dots in the filename—not just "real" file extensions:

```c
SpPath p = sp_path("snakepath-1.0.0.tar.gz");
SpSuffixes s = sp_suffixes(&p);
// s.count == 4: ".0", ".0", ".tar", ".gz"
```

Use `sp_suffix()` if you only want the final extension (`.gz`).

### Parent iteration terminates at anchors

`sp_parents_count()` and parent iteration stop at the path's anchor—`/` for absolute paths, `.` for relative paths:

```c
sp_parents_count(sp_path("/a/b/c/d")) == 4  // /a/b/c, /a/b, /a, /
sp_parents_count(sp_path("a/b/c"))    == 3  // a/b, a, .
sp_parents_count(sp_path("/"))        == 0  // root has no parents
sp_parents_count(sp_path("."))        == 0  // current dir has no parents
```

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
| `.is_file()` | `sp_is_file()` | `.is_file()` |
| `.is_dir()` | `sp_is_dir()` | `.is_dir()` |
| `.exists()` | `sp_exists()` | `.exists()` |
| `.stat()` | `sp_stat()` | - |
| `len(p.parents)` | `sp_parents_count()` | - |

**Not implemented:** Most I/O methods (`mkdir`, `read_*`, `write_*`, `glob`, etc.)

## Adding New Methods to the Library

When adding a new method, you need to update multiple files. The library provides wrapper macros in `tests/python_harness/snakepath_lib.c` to simplify Python bindings.

### Wrapper Macros

| Macro | Signature | Use For |
|-------|-----------|---------|
| `WRAP_STR(fn)` | `Path → SpStr` | String view getters (name, stem, suffix, etc.) |
| `WRAP_PATH_UNARY(fn)` | `Path → Path` | Unary path transforms (parent, absolute) |
| `WRAP_PATH_CSTR(fn)` | `Path + cstr → Path` | Methods taking a string arg (with_name, join_one) |
| `WRAP_PATH_PATH(fn)` | `Path + Path → Path` | Methods taking another path (joinpath, relative_to) |
| `WRAP_BOOL_UNARY(fn)` | `Path → bool` | Boolean queries (is_absolute, is_file) |
| `WRAP_BOOL_BINARY(fn)` | `Path + Path → bool` | Binary predicates (path_eq, is_relative_to) |

### Checklist for New Methods

1. **`snakepath.h`**: Add function declaration and implementation
2. **`snakepath.h` (fluent)**: Add to struct and X-macro lists if fluent API needed
3. **`tests/test.c`**: Add boring API tests
4. **`tests/test_fluent_api.c`**: Add fluent API tests (if applicable)
5. **`snakepath_lib.c`**: Add wrapper using appropriate macro
6. **`snakepath/__init__.py`**: Add ctypes signature and Python method
7. **`run_cpython_tests.py`**: Remove method from `EXPECTED_FAILURES` if previously listed
8. **`README.md`**: Update Pathlib Mapping table
</details>

<details>
<summary>MIT License</summary>
Copyright (c) 2026 Netanel Haber

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
</details>


<img height="200px" src="./assets/snakepath.png" alt="snake ascii art that also looks like a path"/>
