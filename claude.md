# Claude Code Guidelines for snakepath

## Project Overview
snakepath is a C99 STB-style header-only library implementing Python's pathlib API. No malloc, POSIX + Windows support.

## User Preferences

### Code Philosophy
- **First-class implementations**: All logic belongs in `snakepath.h`, not in wrappers. The Python bindings should be thin FFI adapters only.
- **Minimize API surface**: Consolidate related functions, share internal implementations (e.g., `sp_priv_join_len` used by `sp_join_one`, `sp_join_sv`, `sp_joinpath`).
- **No special-casing in wrappers**: If a wrapper needs logic, that logic should be upstreamed to the core library.

### Development Process
- **Implement in C first**: Add function to `snakepath.h`, then expose via wrapper
- **Test thoroughly**: Run C tests, fluent API tests, and Python CPython test suite
- **Python tests are authoritative**: The CPython pathlib test suite is the gold standard for correctness

### Code Style
- Follow existing patterns in the codebase
- Use wrapper macros (`WRAP_STR`, `WRAP_BOOL_UNARY`, etc.) for consistent FFI bindings
- Keep implementations DRY - extract common code into `sp_priv_*` functions

### Refactoring Guidelines
**Semantic compression** means simplifying logic, NOT:
- Shortening variable names (`segment` → `s`, `depth` → `d`) - BAD
- Defining convenience macros (`#define G_ it->priv_`) - BAD
- Removing whitespace or braces - BAD

Good semantic compression:
- Extract repeated patterns into helper functions (`sp_priv_glob_push`, `sp_priv_glob_pop`)
- Use early `continue`/`return` to flatten nesting
- Consolidate duplicated logic branches
- Remove dead code paths

### Testing Commands

**ALWAYS run all three test suites before any git commit:**

```bash
# 1. C tests (gcc)
gcc -std=c99 -I. -Wall -Wextra -Werror -o test_snakepath tests/test.c && ./test_snakepath

# 2. C++ tests (g++) - CI runs both gcc and g++, catches different issues
g++ -std=c++11 -x c++ -I. -Wall -Wextra -Werror -Wmissing-field-initializers -o test_cpp tests/test.c && ./test_cpp

# 3. Python/CPython tests
cd tests/python_harness && gcc -shared -fPIC -o snakepath/libsnakepath.so snakepath_lib.c -I../.. && python run_cpython_tests.py
```

**Why g++ matters:** CI runs C++ builds with strict warnings. Issues that compile fine in C99 will fail in C++:
- `{0}` struct initializer → use `memset(&it, 0, sizeof(it))` instead
- `void*` implicit conversion → use `SP_PRIV_CAST(DIR *, ptr)`
- C-style casts like `(size_t)x` → use `SP_PRIV_CAST(size_t, x)`

### Python Tests: EXPECTED_FAILURES
`EXPECTED_FAILURES` is a dict mapping expected error substrings to lists of `(class_name, test_name)` tuples. The test runner:
- Runs test classes in parallel (each class in its own process with unique temp dir)
- Verifies each failure contains the expected error message (reason verification)
- Reports "unexpected success" if an expected failure passes
- Reports "wrong reason" if a test fails but with a different error message

**When implementing new I/O methods** (like `stat()`), cascading updates are needed:
- Tests that previously failed with `"has no attribute 'stat'"` will now fail for different reasons
- Example: `test_group` changes from `"has no attribute 'stat'"` to `"has no attribute 'group'"`
- Run tests locally and move tests between reason groups accordingly

### Adding New Methods Checklist
1. `snakepath.h`: Add declaration and implementation
2. `snakepath.h` (fluent): Add to struct and X-macro lists if needed
3. `tests/test.c`: Add boring API tests
4. `tests/test_fluent_api.c`: Add fluent API tests
5. `snakepath_lib.c`: Add wrapper using appropriate macro
6. `snakepath/__init__.py`: Add ctypes signature and Python method
7. `run_cpython_tests.py`: Remove from `EXPECTED_FAILURES` if applicable
8. `README.md`: Update Pathlib Mapping table

### Git Workflow
- Create feature branches for changes
- CI requires a PR to run (doesn't trigger on branch push alone)
- Use `gh` CLI to monitor CI: `gh pr checks --watch`, `gh run view <id> --log-failed`
- When grepping CI logs, always use `-C10` or more for context (e.g., `grep -C10 "error:"`)
- Expected failures must only be for **unimplemented functionality**, never for bugs

### CI Output Preferences
- Keep "Starting build/test" log messages in nob - they help track parallel job progress
- Python test runner uses `verbosity=1` (dots) for compact output
- Expected test failures are grouped by reason with counts, not listed individually
- The Windows MSVC environment dump in CI is collapsed by default (inside `##[group]`)

### Known Issues
- Windows CI has race condition with parallel MSVC builds (pre-existing)
- Clang `-Wnrvo` warning in `sp_path_convert` (pre-existing, disabled via SNAKEPATH_NO_NRVO)
- Windows console encoding: Turkish İ (U+0130) can't print on cp1252, fixed with UTF-8 wrapper in run_cpython_tests.py

### Running nob on Termux
On Termux, `gcc` and `g++` are actually clang symlinks, so GCC-specific warnings fail. Use these env vars:

```bash
# Full local build on Termux
SNAKEPATH_SKIP_GCC=1 SNAKEPATH_NO_NRVO=1 ./nob
```

**Environment variables for nob:**
| Variable | Effect | When to use |
|----------|--------|-------------|
| `SNAKEPATH_SANITIZE=1` | Enable sanitizers (ASan, UBSan, leak, etc.) | CI only (set in ci.yml) |
| `SNAKEPATH_SKIP_GCC=1` | Skip GCC/G++ builds, use clang only | Termux (gcc is clang) |
| `SNAKEPATH_NO_NRVO=1` | Disable `-Wnrvo` clang warning | Termux clang has strict NRVO |

**Why Termux is different:**
- `gcc --version` returns "clang version X.X.X" - it's a symlink
- GCC-specific warnings (`-Wformat-overflow=2`, `-Wlogical-op`, etc.) don't exist in clang
- Some sanitizers (leak, pointer-compare) may not be available on Android
- `/tmp` is a symlink to `/data/data/com.termux/files/usr/tmp`, causing some resolve() tests to fail

## Technical Details

### Parent Iteration
Parent iteration (`sp_parents_begin/next`, `sp_parents_count`) terminates at the path's anchor:
- Absolute paths terminate at `/` (root has 0 parents)
- Relative paths terminate at `.` (current dir has 0 parents)
- `/a/b/c/d` has 4 parents: `/a/b/c`, `/a/b`, `/a`, `/`
- `a/b/c` has 3 parents: `a/b`, `a`, `.`

### Embedded Null Handling
Paths can contain embedded null bytes (e.g., `fileA\x00suffix`). The library:
- Uses `SpStr` (data + length) to preserve full content
- `sp_join_sv()` joins with length-aware string views
- `sp_is_file()`/`sp_is_dir()` scan `p->buf[0..p->len]` for nulls and return false

### Python Bindings Architecture
- `snakepath_lib.c`: Thin C wrappers for FFI (output params, bool→int)
- `snakepath/__init__.py`: ctypes bindings, handles Unicode encoding
- Use `create_string_buffer()` + explicit length for embedded nulls
- Use `surrogatepass` encoding for invalid Unicode in paths

### stat() Implementation
- `sp_stat()` follows symlinks (like Python's `Path.stat()` with default `follow_symlinks=True`)
- `sp_lstat()` (no symlink following) is not yet implemented
- Python binding asserts `follow_symlinks=True` and raises AssertionError otherwise
- `sp_stat_eq()` compares two stat results (mode, ino, dev, nlink, uid, gid, size)

### API Patterns
| Pattern | C string | String view | SpPath |
|---------|----------|-------------|--------|
| Create | `sp_path_new()` | `sp_path_from_sv()` | `sp_path_copy()` |
| Join | `sp_join_one()` | `sp_join_sv()` | `sp_joinpath()` |

### Glob Implementation
- `sp_glob_begin()` / `sp_glob_next()` / `sp_glob_end()` - iterator-based API with internal stack
- `sp_rglob_begin()` just prepends `**/` and delegates to glob - good code reuse
- Uses `SpCaseSensitivity` enum: `SP_CASE_PLATFORM_DEFAULT`, `SP_CASE_SENSITIVE`, `SP_CASE_INSENSITIVE`
- `SpGlobIter` contains an internal stack (no malloc, no thread-local storage)
- Configurable limits: `SP_GLOB_MAX_DEPTH` (32), `SP_GLOB_MAX_SEGMENTS` (32), `SP_GLOB_PATTERN_MAX` (256)
- Foreach macros: `SP_GLOB_FOREACH(base, pattern, match)` and `SP_RGLOB_FOREACH(base, pattern, match)`
- Iterator exposes `depth` field for current recursion level

**DOUBLESTAR (`**`) handling order is critical:**
1. First try matching the NEXT segment (if exists)
2. If match: maybe yield, maybe push with seg_idx+2
3. Special case: if pushing to a trailing `**`, yield immediately (`next_ds` check)
4. THEN push for recursion with current seg_idx
5. Finally, if `**` is last segment and entry is dir, yield

The order matters for patterns like `**/dir*/**` - must yield `dirC/dirD`, not just `dirC`.

### Iterator Patterns
The library has three iterators:
- `SpPartsIter` - string cursor over path components
- `SpParentsIter` - generates derived parent paths
- `SpGlobIter` - directory traversal for glob matching

All follow the same API pattern (`begin`/`next`/`end`).

## Implementation Gameplan

Remaining pathlib methods to implement (excluding read/write/text/bytes/open), organized into 4 PRs:

### Group 1: File type predicates (7 methods) ✅ COMPLETE
All are stat-based file type checks:
- `is_symlink` - check if path is a symbolic link (uses lstat)
- `is_block_device` - check if path is a block device
- `is_char_device` - check if path is a character device
- `is_fifo` - check if path is a FIFO/named pipe
- `is_socket` - check if path is a socket
- `is_mount` - check if path is a mount point (POSIX)
- `is_junction` - check if path is a junction (Windows)

### Group 2: Symlink & link operations (6 methods)
- `lstat` - stat without following symlinks
- `readlink` - read symlink target
- `resolve` - resolve to canonical absolute path
- `symlink_to` - create symbolic link
- `hardlink_to` - create hard link
- `samefile` - check if two paths point to same file

### Group 3: File/directory modification (6 methods)
- `touch` - create file or update timestamps
- `unlink` - delete file
- `rmdir` - delete empty directory
- `rename` - rename file/directory
- `replace` - replace target with this file
- `chmod` - change file permissions

### Group 4: Directory traversal & user info (6 methods)
- `iterdir` - iterate directory contents
- `walk` - recursive directory traversal
- `owner` - get file owner name
- `group` - get file group name
- `expanduser` - expand `~` to home directory
- `home` - get user's home directory
