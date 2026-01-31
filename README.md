Snakepath:
C99 STB-style header-only based on [python's pathlib library](https://docs.python.org/3/library/pathlib.html), because I love pathlib.
POSIX + Windows. No malloc. Filesystem I/O.
Vibe-coded with Claude Code + Cursor.

```c
(curl -so snakepath.h https://raw.githubusercontent.com/netanel-haber/snakepath/main/snakepath.h && cc -xc - -o demo <<'EOF'                   
#define SNAKEPATH_FLUENT
#define SNAKEPATH_IMPLEMENTATION
#include "snakepath.h"
#include <stdio.h>
int main() {
    SpPath boring = sp_path("/foo/bar.txt");
    printf("BORING API: %s\n", sp_name(&boring).data); // bar.txt
    printf("BORING API: %s\n", sp_stem(&boring).data); // bar

    const char* fluent = SPF("/etc")->join("nginx")->join("nginx.conf")->str();
    printf("FLUENT API: %s\n", fluent);                // /etc/nginx/nginx.conf

    SpPath home = sp_home(SP_FLAVOR_NATIVE);
    printf("HOME: %s (exists: %d)\n", sp_str(&home), sp_exists(&home));
}  
EOF                               
) && ./demo && rm demo snakepath.h
```

## Features

**Path Manipulation:**
- Parse, join, normalize paths (POSIX + Windows)
- Extract components: name, stem, suffix, drive, root, anchor
- Relative path operations: `relative_to()`, `is_relative_to()`
- Pattern matching with glob syntax

**Filesystem I/O:**
- `exists()` - Check if path exists
- `is_file()`, `is_dir()`, `is_symlink()` - Check path type
- `home()` - Get home directory
- `expanduser()` - Expand `~` in paths
- `cwd()`, `absolute()` - Working directory operations

**Two APIs:**
- **Boring API** - Traditional C functions: `sp_name()`, `sp_join_one()`
- **Fluent API** - Chainable: `SPF("/etc")->join("nginx")->parent()->str()`

**Tested:**
- Runs CPython's pathlib test suite
- Comprehensive C test coverage
- Works on Linux, macOS, Windows

## Build & Test

```bash
cc -o nob nob.c && ./nob
```

## Docs

[API.md](API.md) | [demo.c](demo.c) | [snakepath.h](snakepath.h)

---------------------------------------------------------------------

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
