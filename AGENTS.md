# Agent Guidelines for snakepath

C99 STB-style header-only pathlib port. No malloc, POSIX + Windows. All code in `snakepath.h`.

Record every wish, rule or learning the maintainer states in this file (in the same change), not only in private notes.

## Code Philosophy
- All logic in `snakepath.h`. Code outside it is glue or Python-specific only (argument coercion, exception classes and messages, Python protocols like `NotImplemented`, harness stubs); path semantics always go in C first.
- Minimize API surface, share `sp_priv_*` internals
- No one uses the library yet, so backward compatibility is not a constraint: judge API and behavior changes on their merits (correctness, simplicity, CPython fidelity)
- No special-casing in wrappers
- Fluent API has near-parity with boring API — only iterators missing; a fluent method returns what the function it forwards to returns
- Port only what is meaningful in C: Python-only pathlib machinery becomes an expected failure, never binding code.
- Macros are for constants, names for hacks (C/C++ compat casts, platform functions) and the simplest iterator sugar (`SP_*_FOREACH`). Never use a macro to share code.

## Call Depth
`snakepath.py` checks real stack depth on the preprocessed library (`cc -E -P`, or `cl /EP` on Windows, with `SNAKEPATH_IMPLEMENTATION` and `SNAKEPATH_FLUENT`):
- Every function the library defines is a frame: public, `sp_priv_*` and `static inline` alike, plus functions passed as callbacks (qsort comparators). A function calling itself is exempt.
- At most 4 snakepath frames on the stack from a public function down: the function itself and 3 below it. A fluent method is a real trampoline frame on top of the public function it forwards to, so fluent chains may reach 5. (The maintainer raised the limit from 3 in exchange for a compression of more than 200 lines.)
- Never pass the check by hiding a call behind a private wrapper or a macro. Structure code as entry → helper → leaf, with room for one more level (a public function may call another public function): leaves take what they need precomputed (anchor lengths, C strings), and orchestration that would need a fifth frame lives in the public function, even if two public functions then repeat a few lines.
- A path's flavor is never `SP_FLAVOR_NATIVE`: making a path resolves it, so code tests `flavor == SP_FLAVOR_WINDOWS` and `c == '/' || c == sep` inline instead of calling predicate helpers.

## Code Layout
Code should breathe, balancing readability and brevity:
- Divide functions into paragraphs (setup, guards, main loop, cleanup, result) with blank lines where they separate ideas. Whitespace should mean something: no ceremonial blank lines or braces (no C#-style layout).
- One statement per line. `if`/`for`/`while` bodies never share the condition's line: no `if (x) return y;`, `while (...) i++;` or `for (...) {}`. A single-statement body goes on the next line without mandatory braces.
- No statement compression to save lines: no comma-chained declarations of unrelated variables, no packed ternary chains where plain statements read better.
- The layout is enforced with clang-format 21. The style lives in `nob.h` (`SNAKEPATH_LAYOUT`, passed as `--style=`), because tool configuration goes on nob's command lines rather than into extra repo files. `./nob format` applies it, and the default `./nob` run fails if `snakepath.h` isn't formatted. CI pins clang-format with `pip install clang-format==21.1.8`, so every environment formats identically.
- **Net LOC is always reported post format.** Run `./nob format` first, then measure. A line delta taken before formatting, or earned by packing statements or deleting blank lines, is not a result.

## Refactoring Rules
Good: extract helpers, delegate to primitives, early return, remove dead code.
Bad: shorten names, define convenience macros, remove whitespace/braces, add macro layers that only reduce LOC before preprocessing.
Rule: semantic compression must compress the expanded code too. If `cc -E -P` turns the "cleanup" back into the same amount of code, more code, or harder-to-follow code, it is not a cleanup.

## Pseudo Commands

### `/sem-compress [path] [optional goal]`

Meaning: do a cleanup-only pass focused on semantic compression for the target file or section.

Scope: `snakepath.h` only unless the maintainer names other files. Touch `snakepath.py`, `test.c` or other files only where a header change forces it, and list other ideas in the report instead of doing them.

Required behavior:
- Do not do whitespace-only cleanup, naming cleanup, brace cleanup, or cosmetic churn.
- Remove repetition, dead wrappers, duplicated control flow, and low-value helper layers.
- Treat source LOC reduction as a side effect, not the goal.
- Judge macro refactors by the expanded code. If `cc -E -P` shows the same boilerplate or more indirection, reject the refactor.
- Keep public API sections concrete when possible. Prefer small implementation-local helpers over public macro inventories.
- Stop when the next change would save lines only by hiding code structure, adding slot-order coupling, or making the expanded code harder to follow.
- Never make the library even slightly worse to save lines: a change that needs a trick whose correctness takes a proof, an argument or flag that only one caller needs, coupling to another function's incidental behavior, more stack or frames, a C-runtime side effect, or a macro that does more than name something stays out, whatever it saves (examples under Learnings).
- Ship a change only if it alone is net −60 lines or more (post format). When the maintainer asks for the small things, bundle real simplifications (each a removed layer or dead branch, never cosmetic) into one PR, and say plainly that no single item clears −60.

Required workflow:
1. Read `README.md` and `snakepath.h` in full before editing, then read the target in full.
2. Identify the largest repeated or wrapper-heavy regions.
3. If macros are involved, inspect the preprocessed view of the touched region.
4. Make only the changes that are simpler in both source and expanded form.
5. When a change rewrites pure path logic (parsing, normalizing, joining, matching), prove it behaves identically with an old-vs-new differential driver. The driver includes `snakepath.h` from `main` and from the working tree and prints every public result per input. Run it over exhaustive short strings (for example over `/\ac:.?U`), token sequences (including `\\?\UNC\` and `\\.\` prefixes, which random generation never hits) and random strings, in both flavors and with `SP_PATH_MAX` 64 and 16. Then compare the outputs.
6. Run `./nob format`, then verify with `nob`.
7. If a failure may be local-environment noise, baseline against clean `main` with `./nob clean` before calling it a regression.

Required final report:
- Net LOC delta for the target file, post format.
- Which changes survived and why they are simpler.
- Which tempting changes were rejected because they only compressed source text, not expanded code.

## Testing

Snakepath is developed from any environment: Linux, macOS, Windows, Android and so on. CI (Linux and Windows) is authoritative.

```bash
cc -x c -o nob nob.h && ./nob   # everything: builds, C/C++/fluent tests, Python checks, layout check, demo
./nob format                    # rewrite snakepath.h in its clang-format layout
./nob python                    # only the Python bindings, their checks and CPython's pathlib tests
./nob clean                     # remove build artifacts (use before baseline comparisons)
```

MSVC builds nob with `cl /Tcnob.h`, and `-DSNAKEPATH_QUIET` quiets nob's command echo. Requirements are a C compiler, Python 3 (CI uses 3.15, matching the vendored `test_pathlib.py`), and clang-format 21. Environment knobs:
- `SNAKEPATH_SKIP_GCC=1`: clang builds only (where `gcc` is missing or is really clang).
- `SNAKEPATH_NO_NRVO=1`: silence clang's `-Wnrvo` on versions that have it.
- `SNAKEPATH_SANITIZE=1`: add sanitizer builds.
- `VERBOSE=0`: fewer logs.

**Direct commands for focused debugging:**

```bash
gcc -std=c99 -I. -Wall -Wextra -Werror -o test_snakepath test.c && ./test_snakepath
g++ -std=c++11 -x c++ -I. -Wall -Wextra -Werror -Wmissing-field-initializers -o test_cpp test.c && ./test_cpp
gcc -shared -fPIC -DSP_FFI -o libsnakepath.so test.c && python snakepath.py
```

**g++ pitfalls:** `{0}` → `memset`, `void*` casts → `SP_PRIV_CAST`, C casts → `SP_PRIV_CAST`

**Environment quirks:** Android forbids hard links, so there the fluent `hardlink_to` test fails even on clean `main`.

## EXPECTED_FAILURES

Dict in `snakepath.py` mapping error substrings → `(class_name, test_name)` tuples. Runner verifies failure reasons, reports wrong-reason and unexpected-success. When adding methods, tests cascade between reason groups — run locally and update.

## Git & CI

- Feature branches, CI requires PR
- `gh pr checks --watch`, `gh run view <id> --log-failed`, grep with `-C10`
- Expected failures = unimplemented functionality only, never bugs

## Known Issues

- Windows CI: race condition with parallel MSVC builds
- Clang `-Wnrvo` (not eliding copies on multi-return paths such as `sp_path_convert`); disabled via SNAKEPATH_NO_NRVO
- Windows console: Turkish İ (U+0130) needs UTF-8 wrapper

## Learnings

- Keep wrapper layers thin: one-to-one calls only, no logic that replaces core behavior.
- Prefer `nob` as the canonical local test entrypoint; use the raw compile commands only when narrowing a specific failure.
- For cleanup work, optimize for semantic compression, not line-count compression. The real measure is the concrete C the compiler sees, not the source LOC count.
- Semantic compression must survive preprocessing. A refactor only counts if the `cc -E -P` output is also smaller, flatter, or clearer in substance.
- If a macro refactor only hides repetition in the source file but expands back to the same wrapper boilerplate, it is not simplification; it is indirection.
- Use the preprocessed view (`cc -E -P`) to judge macro refactors. Keep a macro only if the expanded code is still obviously simpler than the handwritten alternative.
- Public API sections should stay concrete. If repetition remains, prefer tiny implementation-local helper macros over public X-macro inventories.
- Removing blank lines and packing statements is not compression; it made the library hard to read, and the layout check now rejects it.
- Before treating a local filesystem/fluent failure as a regression, stash the patch, run `./nob clean`, verify clean `main`, then compare against that baseline.
- Python bindings should use `os.fspath()` directly (no `str()` fallback) so non-pathlike types raise `TypeError`.
- Use `_decode(..., errors="surrogatepass")` and copy `SpPath` structs in `_from_sp` to preserve embedded nulls.
- Windows builds should not compile `sp_owner_wrap`/`sp_group_wrap`; gate the wrappers in C.
- `sp_with_segments` now takes a `parts_count` (no NULL-terminated arrays); use `SP_ARRAY_LEN`.
- New functionality goes in `snakepath.h` first; then mirror wrappers in the `SP_FFI` section of `test.c` and in `snakepath.py`, plus tests in `test.c` (fluent API tests under `#ifdef SNAKEPATH_FLUENT`).
- When API examples change, update `api_demo.c` first, then its copy in `README.md` (GitHub Pages renders that file as the website through `_layouts/default.html`), and record any new learnings here. `nob` fails (`snakepath.py`) if the copy drifts.
- The `SP_FFI` section of `test.c` exports exactly what `snakepath.py` calls; when a Python caller goes away, drop its C wrapper and `_sig` line too.
- `nob.h` is the build script plus upstream nob trimmed to what it uses (see its header). If it needs more of nob, re-vendor upstream and re-trim instead of hand-copying pieces.
- Don't add config or support files to the repo; tool settings go on nob's command lines (like the clang-format style), documented here.
- For `"."` behavior, keep `SpPath` canonical as empty (`len == 0`) and let string conversion render `"."`; storing literal `"."` breaks equality/parents semantics.
- The Python harness runs CPython 3.15's `test_pathlib.py`. Python-only machinery (`info`, `pathlib.types`, pickling, private hooks, mocks of Python functions) goes in `EXPECTED_FAILURES`, never into C.
- For semantics work, differentially fuzz the bindings against the real `pathlib` (random paths/patterns, compare results); the CPython suite alone misses many edge cases.
- The maintainer reverted every trade-off that made the library slightly worse from the 4-frame compression (PR #88), keeping only changes that are better on the merits. Off limits: `sp_path_cmp` comparing whole strings with the separator sorting first (plus a `low` byte its other callers pass as `'\0'`); one fluent table whose chainable methods clear and re-set the active flag; an owner/group helper switched by a flag; glob relying on the normalizer keeping a `.` before a drive-like part; reuse that adds a stack buffer or a frame (`readlink` and `path_convert` through a temporary buffer, `resolve` through `absolute`); Windows mkdir/unlink/rmdir through the C runtime, whose error codes rely on `GetLastError()` surviving the call; a macro that drops an argument.
