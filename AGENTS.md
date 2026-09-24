# Agent Guidelines for snakepath

C99 STB-style header-only pathlib port. No malloc, POSIX + Windows. All code in `snakepath.h`.

## Code Philosophy
- All logic in `snakepath.h`, Python bindings are thin FFI only
- Minimize API surface, share `sp_priv_*` internals
- No special-casing in wrappers
- Fluent API has near-parity with boring API — only iterators/mutators missing

## Refactoring Rules
Good: extract helpers, delegate to primitives, early return, remove dead code.
Bad: shorten names, define convenience macros, remove whitespace/braces, add macro layers that only reduce LOC before preprocessing.
Rule: semantic compression must compress the expanded code too. If `cc -E -P` turns the "cleanup" back into the same amount of code, more code, or harder-to-follow code, it is not a cleanup.

## Pseudo Commands

### `/sem-compress [path] [optional goal]`

Meaning: do a cleanup-only pass focused on semantic compression for the target file or section.

Required behavior:
- Do not do whitespace-only cleanup, naming cleanup, brace cleanup, or cosmetic churn.
- Remove repetition, dead wrappers, duplicated control flow, and low-value helper layers.
- Treat source LOC reduction as a side effect, not the goal.
- Judge macro refactors by the expanded code. If `cc -E -P` shows the same boilerplate or more indirection, reject the refactor.
- Keep public API sections concrete when possible. Prefer small implementation-local helpers over public macro inventories.
- Stop when the next change would save lines only by hiding code structure, adding slot-order coupling, or making the expanded code harder to follow.

Required workflow:
1. Read `README.md` and `snakepath.h` in full before editing, then read the target in full.
2. Identify the largest repeated or wrapper-heavy regions.
3. If macros are involved, inspect the preprocessed view of the touched region.
4. Make only the changes that are simpler in both source and expanded form.
5. Verify with `nob`.
6. If a failure may be local-environment noise, baseline against clean `main` with `./nob clean` before calling it a regression.

Required final report:
- Net LOC delta for the target file.
- Which changes survived and why they are simpler.
- Which tempting changes were rejected because they only compressed source text, not expanded code.

## Testing

**Default local runner:**

```bash
cc -x c -DSNAKEPATH_QUIET -o nob nob.h
env SNAKEPATH_SKIP_GCC=1 SNAKEPATH_NO_NRVO=1 ./nob
```

Use `./nob clean` before baseline comparisons or when local build artifacts may be stale.

**Direct commands for focused debugging:**

```bash
gcc -std=c99 -I. -Wall -Wextra -Werror -o test_snakepath test.c && ./test_snakepath
g++ -std=c++11 -x c++ -I. -Wall -Wextra -Werror -Wmissing-field-initializers -o test_cpp test.c && ./test_cpp
gcc -shared -fPIC -DSP_FFI -o libsnakepath.so test.c && python snakepath.py
```

**g++ pitfalls:** `{0}` → `memset`, `void*` casts → `SP_PRIV_CAST`, C casts → `SP_PRIV_CAST`

**Termux:** prefer `nob`. The local baseline still fails the fluent `hardlink_to` test (Android forbids hard links) even on clean `main`. CI is authoritative.

## EXPECTED_FAILURES

Dict in `snakepath.py` mapping error substrings → `(class_name, test_name)` tuples. Runner verifies failure reasons, reports wrong-reason and unexpected-success. When adding methods, tests cascade between reason groups — run locally and update.

## Git & CI

- Feature branches, CI requires PR
- `gh pr checks --watch`, `gh run view <id> --log-failed`, grep with `-C10`
- Expected failures = unimplemented functionality only, never bugs

## Known Issues

- Windows CI: race condition with parallel MSVC builds
- Clang `-Wnrvo` in `sp_path_convert` (disabled via SNAKEPATH_NO_NRVO)
- Windows console: Turkish İ (U+0130) needs UTF-8 wrapper

## Learnings

- Keep wrapper layers thin: one-to-one calls only, no logic that replaces core behavior.
- Prefer `nob` as the canonical local test entrypoint; use the raw compile commands only when narrowing a specific failure.
- For cleanup work, optimize for semantic compression, not line-count compression. The real measure is the concrete C the compiler sees, not the source LOC count.
- Semantic compression must survive preprocessing. A refactor only counts if the `cc -E -P` output is also smaller, flatter, or clearer in substance.
- If a macro refactor only hides repetition in the source file but expands back to the same wrapper boilerplate, it is not simplification; it is indirection.
- Use the preprocessed view (`cc -E -P`) to judge macro refactors. Keep a macro only if the expanded code is still obviously simpler than the handwritten alternative.
- Public API sections should stay concrete. If repetition remains, prefer tiny implementation-local helper macros over public X-macro inventories.
- Before treating a local filesystem/fluent failure as a regression, stash the patch, run `./nob clean`, verify clean `main`, then compare against that baseline.
- Python bindings should use `os.fspath()` directly (no `str()` fallback) so non-pathlike types raise `TypeError`.
- Use `_decode(..., errors="surrogatepass")` and copy `SpPath` structs in `_from_sp` to preserve embedded nulls.
- Windows builds should not compile `sp_owner_wrap`/`sp_group_wrap`; gate the wrappers in C.
- `sp_with_segments` now takes a `parts_count` (no NULL-terminated arrays); use `SP_ARRAY_LEN`.
- New functionality goes in `snakepath.h` first; then mirror wrappers in the `SP_FFI` section of `test.c` and in `snakepath.py`, plus tests in `test.c` (fluent API tests under `#ifdef SNAKEPATH_FLUENT`).
- When API examples change, update `api_demo.c` first, then its copy in `README.md` (GitHub Pages renders that file as the website through `_layouts/default.html`), and record any new learnings here. `nob` fails (`snakepath.py`) if the copy drifts.
- The `SP_FFI` section of `test.c` exports exactly what `snakepath.py` calls; when a Python caller goes away, drop its C wrapper and `_sig` line too.
- `nob.h` is the build script plus upstream nob trimmed to what it uses (see its header). If it needs more of nob, re-vendor upstream and re-trim instead of hand-copying pieces.
- Public API call depth is now enforced by `snakepath.py` (limit = 3 public frames); keep wrapper chains flat and favor `sp_priv_*` delegation.
- For `"."` behavior, keep `SpPath` canonical as empty (`len == 0`) and let string conversion render `"."`; storing literal `"."` breaks equality/parents semantics.
- The Python harness runs CPython 3.15's `test_pathlib.py`. Python-only machinery (`info`, `pathlib.types`, pickling, private hooks, mocks of Python functions) goes in `EXPECTED_FAILURES`, never into C.
- For semantics work, differentially fuzz the bindings against the real `pathlib` (random paths/patterns, compare results); the CPython suite alone misses many edge cases.
