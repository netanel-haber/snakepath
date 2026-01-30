Snakepath:
C99 STB-style header-only based on [python's pathlib library](https://docs.python.org/3/library/pathlib.html), because I love pathlib. 
POSIX + Windows. No malloc. 
Vibe-coded with Claude Code + Cursor.

```c
(curl -so snakepath.h https://raw.githubusercontent.com/netanel-haber/snakepath/main/snakepath.h && cc -xc - -o demo <<'EOF'
#define SNAKEPATH_FLUENT
#define SNAKEPATH_IMPLEMENTATION
#include "snakepath.h"
#include <stdio.h>
int main() {
    SpPath boring = sp_path("/foo/bar.txt");
    printf("BORING API: %s\n", sp_name(&boring).data);
    printf("BORING API: %s\n", sp_stem(&boring).data);
    const char* fluent = SPF("/etc")->join("nginx")->join("nginx.conf")->str();
    printf("FLUENT API: %s\n", fluent);
}
EOF
) && ./demo && rm demo snakepath.h
```

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
