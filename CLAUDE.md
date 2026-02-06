# Claude Code Guidelines for snakepath

C99 STB-style header-only pathlib port. No malloc, POSIX + Windows. All code in `snakepath.h`.

## Code Philosophy
- All logic in `snakepath.h`, Python bindings are thin FFI only
- Minimize API surface, share `sp_priv_*` internals
- No special-casing in wrappers

## Refactoring Rules
Good: extract helpers, delegate to primitives, early return, remove dead code.
Bad: shorten names, define convenience macros, remove whitespace/braces.

## Testing

**Run all three before committing:**

```bash
gcc -std=c99 -I. -Wall -Wextra -Werror -o test_snakepath tests/test.c && ./test_snakepath
g++ -std=c++11 -x c++ -I. -Wall -Wextra -Werror -Wmissing-field-initializers -o test_cpp tests/test.c && ./test_cpp
cd tests/python_harness && gcc -shared -fPIC -o snakepath/libsnakepath.so snakepath_lib.c -I../.. && python run_cpython_tests.py
```

**g++ pitfalls:** `{0}` → `memset`, `void*` casts → `SP_PRIV_CAST`, C casts → `SP_PRIV_CAST`

**Termux:** `cc -DSNAKEPATH_QUIET -o nob nob.c && SNAKEPATH_SKIP_GCC=1 SNAKEPATH_NO_NRVO=1 ./nob`
Termux `/tmp` symlink failures are expected locally. CI is authoritative.

## EXPECTED_FAILURES

Dict mapping error substrings → `(class_name, test_name)` tuples. Runner verifies failure reasons, reports wrong-reason and unexpected-success. When adding methods, tests cascade between reason groups — run locally and update.

## Git & CI

- Feature branches, CI requires PR
- `gh pr checks --watch`, `gh run view <id> --log-failed`, grep with `-C10`
- Expected failures = unimplemented functionality only, never bugs

## Known Issues

- Windows CI: race condition with parallel MSVC builds
- Clang `-Wnrvo` in `sp_path_convert` (disabled via SNAKEPATH_NO_NRVO)
- Windows console: Turkish İ (U+0130) needs UTF-8 wrapper
