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

### Testing Commands
```bash
# C tests
gcc -std=c99 -I. -Wall -Wextra -o test_snakepath tests/test.c && ./test_snakepath

# Fluent API tests
gcc -std=c99 -I. -Wall -Wextra -o test_fluent tests/test_fluent_api.c && ./test_fluent

# Python tests (rebuild library first)
cd tests/python_harness && gcc -shared -fPIC -o libsnakepath.so snakepath_lib.c -I../..
python run_cpython_tests.py
```

### Python Tests Gotcha: Cascading EXPECTED_FAILURES Updates
When implementing a new method (e.g., `exists()`), you must update EXPECTED_FAILURES in two ways:
1. **Remove** direct test entries (e.g., `test_exists`) since they now pass
2. **Update error messages** for tests that were failing due to the missing method but actually test something else

Example: `test_mkdir` was expected to fail with `"has no attribute 'exists'"` because `exists()` is called first in the test. After implementing `exists()`, it now fails with `"has no attribute 'mkdir'"` - update the error message accordingly.

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
- Use `gh` CLI to monitor CI: `gh run list`, `gh run view <id>`

### Known Issues
- Windows CI has race condition with parallel MSVC builds (pre-existing)
- Clang `-Wnrvo` warning in `sp_path_convert` (pre-existing)

## Technical Details

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

### API Patterns
| Pattern | C string | String view | SpPath |
|---------|----------|-------------|--------|
| Create | `sp_path_new()` | `sp_path_from_sv()` | `sp_path_copy()` |
| Join | `sp_join_one()` | `sp_join_sv()` | `sp_joinpath()` |
