# Agent Guidelines for snakepath

C99 STB-style header-only pathlib port. No malloc, POSIX + Windows. All code in `snakepath.h`.

Record every wish, rule or learning the maintainer states in this file (in the same change), not only in private notes.

## Code Philosophy
- All logic in `snakepath.h`; no logic in `snakepath.py` or the C harness (the `SP_FFI` shim in `test.c`), which stay one-to-one glue. Code outside it is glue or Python-specific only (argument coercion, exception classes and messages, Python protocols like `NotImplemented`, harness stubs); path semantics always go in C first.
- Minimize API surface, share `sp_priv_*` internals
- No one uses the library yet, so backward compatibility is not a constraint: judge API and behavior changes on their merits (correctness, simplicity, CPython fidelity)
- No special-casing in wrappers
- Fluent API has near-parity with boring API — only iterators missing; a fluent method returns what the function it forwards to returns
- One C function stands for a family of pathlib methods when the API still reads well, differing from Python's names as long as its comment lists them: `sp_is(p, type, follow_symlinks)` is exists and the ten `is_*` predicates, `sp_stat(p, follow_symlinks)` is stat and lstat, `sp_copy(.into)`/`sp_move(into)` are the `_into` variants, `sp_rename(replace)`, `sp_link_to` (symlink_to, hardlink_to), `sp_remove` (unlink, rmdir), `sp_match(.full)` (match, full_match), `sp_glob_begin(.recursive)` (glob, rglob). Merges favor families of three or more; `with_name`/`with_stem`/`with_suffix` and the six components stay separate because `sp_stem(&p).buf` reads better than a tagged call, and owner/group stay separate by the maintainer's earlier decision.
- Port only what is meaningful in C: Python-only pathlib machinery becomes an expected failure, never binding code.
- Macros are for constants, names for hacks (C/C++ compat casts, platform functions, compiler attributes like `SP_NODISCARD`) and the simplest iterator sugar (`SP_*_FOREACH`). Never use a macro to share code.
- Two X-macro lists are the exceptions the maintainer accepted, because one list keeps parallel tables from drifting: the fluent method tables, and `SP_ERRORS(X)`, which defines `SpError`, `sp_error_str`'s messages and the harness's error constants from one row per code. Nothing else in the public API is generated.

## Errors
Shift every error as far left as it goes: what can be a compile error is one, what can be an explicit runtime error is one.
- One `SpError` for everything, path logic and OS alike, with specific codes (`sp_priv_last_error` maps errno and `GetLastError`). A result that can fail carries it (`.error` on paths, text, suffixes, stat results and iterators); actions return it.
- A path with an error passes it on: every function returning a path, text or struct returns the input's error, and actions return it, so a chain needs one check at the end. Functions returning a bool or a number, and `sp_str`, assert that their paths carry no error. An error path is empty.
- Every public function that returns a value is `SP_NODISCARD`; bad configuration macros are `#error`s.
- Nothing is silently truncated or skipped: a result that doesn't fit its buffer (`SP_PATH_MAX` or the caller's) is `SP_ERR_TOO_LONG`, going past a configured limit (`SP_MAX_SUFFIXES`, `SP_GLOB_MAX_DEPTH`, `SP_GLOB_PATTERN_MAX`) is `SP_ERR_LIMIT`.
- Options: a function with one option takes a named bool; one with two or more takes a struct of them (`SpCopyOptions`, `SpMkdirOptions`, `SpWalkOptions`, `SpLinkOptions`, `SpRemoveOptions`, `SpMatchOptions`, `SpGlobOptions`), fields named after pathlib's keywords, so a wrong option is a compile error rather than a runtime `SP_ERR_INVALID_ARG` (the maintainer: "comptime beats all"). Zero-initialized options are pathlib's defaults unless the field says otherwise (`bottom_up`, not `top_down`, so that zero is os.walk's default; `follow_symlinks` on copy is the documented exception). Callers write `(SpCopyOptions){.into = true}` in C; before C++20 they set the fields of a zero-initialized variable, which is what `test.c`'s `*_opts` constructors do because the tests build as C++ too. The header itself never uses compound literals. Modes are always explicit (0 is a mode; `SP_MODE_DIR`/`SP_MODE_FILE` are pathlib's defaults).
- A fluent chain started while another runs carries `SP_ERR_NESTED_CHAIN` (both chains do, until the next terminator).

## Call Depth and Stack
`snakepath.py` measures two things on the preprocessed library (`cc -E -P`, or `cl /EP` on Windows, with `SNAKEPATH_IMPLEMENTATION` and `SNAKEPATH_FLUENT`) and bounds both with hardcoded limits (`FRAME_LIMIT`, `STACK_BUDGETS`):
- **Frames**: every function the library defines is a frame (public, `sp_priv_*` and `static inline` alike, plus qsort comparators); a function calling itself is exempt. At most 6 snakepath frames from a public function down; a fluent method is one more on top, so 7. Indirection has a price of its own, which is why this bound stays even though it is not the stack risk.
- **Stack bytes**: each function's own frame from `-fstack-usage` at `-O0` (gcc or clang; skipped with a message where neither exists), summed along the heaviest call chain below every public function, at `SP_PATH_MAX` 4096 and 1024. Budgets: 128 KB and 64 KB, 1/16 of Windows' 1 MB default stack at its `SP_PATH_MAX`. Frames depend on the compiler (clang for the MSVC ABI at `-O0` measures about 4× gcc's), so the budgets hold for the worst measuring environment and the printed chains are the numbers to compare. This is the real constraint the frame count used to stand in for.
- The check prints leads, not just a verdict: the functions at the frame limit, the heaviest chain per configuration and the heaviest frames. Read them before a compression pass; a frame-forced duplicate or a large stack buffer shows up there first.
- Never pass either check by hiding a call behind a private wrapper or a macro. Prefer entry → helper → leaf, leaves taking what they need precomputed. With 6 frames a public function may call another public function and still have room (`sp_resolve` → `sp_absolute` → `sp_priv_join_len` → `sp_priv_normalize` → `sp_priv_split_anchor` → `sp_priv_drive_len` is the limit).
- History: the limit was 3, then 4 (PR #88), then 6 with the stack budget (2026-09-27), when the maintainer judged that the frame count alone had been forcing duplicates (`copy_into`, `move_into`, the cwd sites, the Windows readlink step) that a byte bound doesn't.
- `SpPath` carries its `anchor` and `drive` lengths, set by `sp_priv_normalize` (every constructor ends there) and kept by truncation (`sp_priv_prefix`); `SP_ASSERT_PATH_INVARIANT` recomputes and checks them. Code reads the fields instead of calling `sp_priv_split_anchor`, which is for raw text (a glob pattern).
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
5. When a change rewrites pure path logic (parsing, normalizing, joining, matching), prove it behaves identically with `./nob diff` (default base `origin/main`). It builds `test.c` as the `SP_DIFF` driver against both headers, which prints every public result per input over exhaustive short strings over `/\ac:.?U`, token sequences (`\\?\UNC\` and `\\.\` prefixes, drives, case-folding pairs, which random generation never hits) and random strings, in both flavors and at `SP_PATH_MAX` 64 and 16. nob splits each set into 16 parts, runs one driver per core, compares the outputs while the drivers write them and prints each set's first difference as soon as both sides have it (the input and the labeled results that differ), then how many lines differ; the base side's drivers and outputs are cached in `diff_base/` while the base header, `test.c` and the build commands stay the same, so a repeat run only builds and runs the working tree's side. CI runs the same check against the PR's base on Linux and Windows (`diff-linux`, `diff-windows`), the only differential coverage of Windows-only pure logic such as `collapse_dots`: read its report. An intentional behavior change differs by design, so the job is informational and never blocks a merge. The check must stay fast enough to run locally whenever it helps: the maintainer asked for about 30 seconds on their phone and accepted the speed of 2026-09-27 (a repeat run about 10 s with Termux in the foreground, where it gets all 8 cores, and about 40 s in the background, where Android gives it 3), and it reports the first differences as soon as it finds them, so a broken change is fixed while the rest still runs. When it gets slower, profile the driver and make the library (or the driver's glue) faster rather than running it less.
6. Run `./nob format`, then verify with `nob`.
7. If a failure may be local-environment noise, baseline against clean `main` with `./nob clean` before calling it a regression.

Required final report:
- Net LOC delta for the target file, post format.
- The `./nob diff` result, locally and from CI's `diff-windows` job.
- Which changes survived and why they are simpler.
- Which tempting changes were rejected because they only compressed source text, not expanded code.

## Testing

Snakepath is developed from any environment: Linux, macOS, Windows, Android and so on. CI (Linux and Windows) is authoritative.

```bash
cc -x c -o nob nob.h && ./nob   # everything: builds, C/C++/fluent tests, Python checks, layout check, demo
./nob format                    # rewrite snakepath.h in its clang-format layout
./nob python                    # only the Python bindings, their checks and CPython's pathlib tests
./nob clean                     # remove build artifacts (use before baseline comparisons)
./nob diff [ref]                # the pure path API against the header at a git ref (default origin/main), old vs new
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

**g++ pitfalls:** `{0}` → `memset`, `void*` casts → `SP_PRIV_CAST`, C casts → `SP_PRIV_CAST`, pointer to integer → `SP_PRIV_PTR_BITS`. A struct's first member must not be an enum (clang warns that `{0}` converts an int to it).

**`SP_NODISCARD` pitfall:** GCC's `warn_unused_result` ignores `(void)` casts, so store or check every result (the tests assert them, cleanup included).

**Environment quirks:** Android forbids hard links, and CPython there has no `os.link`: `sp_hardlink_to` returns `SP_ERR_UNSUPPORTED` on Android (`pathlib.UnsupportedOperation`), and the tests expect that.

## EXPECTED_FAILURES

Dict in `snakepath.py` mapping error substrings → `(class_name, test_name)` tuples. Runner verifies failure reasons, reports wrong-reason and unexpected-success. When adding methods, tests cascade between reason groups — run locally and update.

## Git & CI

- Feature branches, CI requires PR
- `gh pr checks --watch`, `gh run view <id> --log-failed`, grep with `-C10`
- Expected failures = unimplemented functionality only, never bugs
- Never postpone a known gap to a later stage or PR, and never skip or expect-fail a test to get CI green: when a change exposes a gap (say, CPython's symlink tests failing on Windows), close it in that change.

## Known Issues

- Windows CI: race condition with parallel MSVC builds
- The test binaries' `main` holds hundreds of paths in one frame, past Windows' 1 MB default stack at `/Od`: nob links them with `/STACK:8388608`
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
- `owner`/`group` return `SP_ERR_UNSUPPORTED` on Windows (the bindings raise `pathlib.UnsupportedOperation`), so their wrappers are built everywhere.
- Windows goes through the wide (W) API only: `sp_priv_native` turns a path into what the OS takes (UTF-16 from WTF-8 in a `SP_PRIV_NATIVE_MAX` buffer on Windows, the path's own bytes on POSIX), and `sp_priv_from_wide` brings text back. Files are read and written through native handles (`sp_priv_open`/`read`/`write`/`close`), never the C runtime, so error codes come straight from the OS.
- A function that talks to the OS (`sp_stat`, `sp_readlink`, `sp_mkdir`, `sp_cwd`, and the private `open`, `remove_impl`, `link_to_impl`) takes `const SpPath *` and converts it itself into an `SpPrivNative` (`.path` is what the OS takes, `.buf` its UTF-16 storage on Windows), so callers carry no conversion prologue. Where a public function is the whole implementation (`sp_stat`, `sp_readlink`, `sp_mkdir`, `sp_cwd`, `sp_join_n`), internal code calls it directly instead of a private twin: a public function may call another public function within the frame budget.
- Caching the anchor and drive in `SpPath` was a wash at 4 frames (every constructor needed its own split line) and pays once `sp_priv_normalize` computes them, which needs the 6-frame limit.
- `sp_stat(p, follow_symlinks)` is both `stat` and `lstat`, like `exists`/`is_dir`/`is_file`; `len(p.parts)` and `len(p.parents)` are counted by the caller through the iterators (the harness and bindings do), not by the library.
- `SpStatResult.sp_reparse_tag` is CPython's `st_reparse_tag` (lstat's tag on Windows, 0 elsewhere): `sp_is_junction` and the recursive delete test it instead of opening the file again.
- Scanning a path for the next separator goes through `sp_priv_part_end` (a leaf, so it fits under any caller); a new inline `while (... != '/' && ... != sep)` loop is a duplicate.
- Windows link behavior follows CPython: `lstat` reports `S_IFLNK` only for symlinks, `symlink_to` makes a directory link to an existing directory, `unlink` removes directory links, `readlink` returns the substitute name, `resolve` is `ntpath.realpath`, and files are copied with `CopyFile2`.
- `walk` is an iterator (`sp_walk_begin`/`sp_walk_next`) keeping names in the caller's buffer, in `os.walk`'s shape: the bindings drive it as a generator and hand pruned `dirnames` back between yields, with no walk logic in Python.
- `sp_with_segments` now takes a `parts_count` (no NULL-terminated arrays); use `SP_ARRAY_LEN`.
- New functionality goes in `snakepath.h` first; then mirror wrappers in the `SP_FFI` section of `test.c` and in `snakepath.py`, plus tests in `test.c` (fluent API tests under `#ifdef SNAKEPATH_FLUENT`). The bindings keep pathlib's method names and map them onto the merged C functions (`is_dir` is `sp_is(SP_DIR, follow)`, `rglob` is `sp_glob_begin` with `recursive`); an FFI wrapper takes ints and builds the option struct, which is glue.
- `SP_GLOB_FOREACH(base, pattern, options, var)` takes an `SpGlobOptions` value (`(SpGlobOptions){0}` in C); there is no separate rglob macro.
- When API examples change, update `api_demo.c` first, then its copy in `README.md` (GitHub Pages renders that file as the website through `_layouts/default.html`), and record any new learnings here. `nob` fails (`snakepath.py`) if the copy drifts.
- The `SP_FFI` section of `test.c` exports exactly what `snakepath.py` calls; when a Python caller goes away, drop its C wrapper and `_sig` line too.
- `test.c` has three modes, all glue without path logic: the tests, `SP_FFI` (the bindings' library) and `SP_DIFF` (`./nob diff`'s driver, which includes `<snakepath.h>` so `-I` picks the base ref's copy or the working tree's, and takes `SP_PATH_MAX` from the command line).
- Proof the maintainer asked for runs in CI, in parallel, not by hand on a developer's machine: the differential check is a non-blocking CI job (`continue-on-error`), because a PR that changes behavior on purpose must differ from its base.
- The library should keep getting faster: the differential driver doubles as a benchmark, and its profile is a lead like the frame check's. The first profiles (2026-09-27) found the driver spending half its time in `snprintf`, then in labeling and hashing every result byte by byte, and the matcher's case-insensitive class test scanning the case-group table once per entry and every lowercase run per character; with labels only on printed lines, a hash taking 8 bytes at a time, the case groups binary-searched like the other Unicode tables and only the runs a class range overlaps scanned, a driver needs about an eighth of its old CPU time. A speed-up still has to be better on the merits, like any change here, and `snakepath.h` is not bent to suit the driver (the maintainer: "dont derange the snakepath.h to accomodate the differ too much"): speed the check up through the driver and nob first, and change the header only for an improvement that stands on its own, such as an algorithmic fix, never a fast path or special case for the driver's workload.
- Profiling on Android: `-pg` doesn't link (bionic has no `_mcount`) and `simpleperf` needs `security.perf_harden` set to 0 over adb, so a `SIGPROF` sampler (PC and frame-pointer chain, symbolized with `nm`) stands in; on Linux or macOS use `perf` or gprof.
- Benchmarking on Android: an app in the background gets a restricted cpuset (3 of the phone's 8 cores, `Cpus_allowed_list` in `/proc/self/status`) and a throttled CPU share, in the foreground all 8, so wall times compare only within one state; compare the CPU time of old and new drivers run back to back. Android also kills an app past about 32 child processes (the phantom process killer): `./nob diff` runs at most one driver per core, after starting all 96 parts at once took Termux down.
- `nob.h` is the build script plus upstream nob trimmed to what it uses (see its header). If it needs more of nob, re-vendor upstream and re-trim instead of hand-copying pieces.
- Don't add config or support files to the repo; tool settings go on nob's command lines (like the clang-format style), documented here.
- For `"."` behavior, keep `SpPath` canonical as empty (`len == 0`) and let string conversion render `"."`; storing literal `"."` breaks equality/parents semantics.
- The Python harness runs CPython 3.15's `test_pathlib.py`. Python-only machinery (`info`, `pathlib.types`, pickling, private hooks, mocks of Python functions) goes in `EXPECTED_FAILURES`, never into C.
- For semantics work, differentially fuzz the bindings against the real `pathlib` (random paths/patterns, compare results); the CPython suite alone misses many edge cases.
- File system results are checked against CPython's `os` and `pathlib` on real files (`check_fs`, run by nob with the fuzz): every stat field, the predicates, links, `resolve`, listings, `walk`, and what actions leave behind as `os.stat` sees it. CPython's pathlib tests compare snakepath with itself (a copy's times with its source's, both read through snakepath, and stat results with `==`, which goes through `sp_stat_eq` and ignores times), so a result that is wrong the same way twice passes them: whole-second stat times on Linux and Android, whole-second times from `copy(preserve_metadata=True)`, and `resolve` ignoring dangling links, `..` after a missing part and links into loops all did until this check (2026-09-27). The maintainer asked that bugs like these not happen again: a new file system function or case gets a comparison in `check_fs`, and the check is shown failing on the old code before a fix counts.
- `sp_stat` fills in nanoseconds on every platform (glibc's strict C modes hide POSIX 2008's `st_atim` and show `st_atimensec`) and computes float times as CPython does (seconds + nanoseconds × 1e-9, which rounds differently from nanoseconds / 1e9). `copy(preserve_metadata=True)` sets times to the nanosecond with `utimensat` where the headers show it (`utime`, to the second, in glibc's strict modes). `sp_resolve` on POSIX is `posixpath.realpath`'s walk from the root (`sp_priv_realpath`, strict included; `realpath()` is gone), one `lstat` per part: it follows links to missing files, applies `..` to what is resolved so far, and knows a link met again while its own target is walked (a loop) by file ID; past 40 links inside each other's targets it is `SP_ERR_LIMIT`. The maintainer chose this exact version (about 56 lines) over a smaller one that would resolve symlink loops to a different link than pathlib.
- The maintainer reverted every trade-off that made the library slightly worse from the 4-frame compression (PR #88), keeping only changes that are better on the merits. Off limits: `sp_path_cmp` comparing whole strings with the separator sorting first (plus a `low` byte its other callers pass as `'\0'`); one fluent table whose chainable methods clear and re-set the active flag; an owner/group helper switched by a flag; glob relying on the normalizer keeping a `.` before a drive-like part; reuse that adds a stack buffer or a frame (`readlink` and `path_convert` through a temporary buffer, `resolve` through `absolute`); Windows mkdir/unlink/rmdir through the C runtime, whose error codes rely on `GetLastError()` surviving the call; a macro that drops an argument.
