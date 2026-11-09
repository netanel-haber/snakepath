# Agent Guidelines for snakepath

C99 STB-style header-only port of Python's pathlib. No malloc, POSIX + Windows. All logic is in `snakepath.h`.

## Rules

- Path semantics live in `snakepath.h` only. `snakepath.py` and the `SP_PYTHON` module in `test.c` are one-to-one glue.
- Full parity with pathlib 3.15, test-driven: the failing test (`test.c`, `check_fs`, or a CPython test taken out of
  `EXPECTED_FAILURES`) comes first. `EXPECTED_FAILURES` holds only tests that mock Python functions inside CPython.
- Safety from CVEs matters as much as parity: what CPython checks, snakepath checks.
- One `SpError` for everything. A result that can fail carries `.error`, and a path with an error passes it on. Every
  public function returning a value is `SP_NODISCARD`. Nothing is truncated silently (`SP_ERR_TOO_LONG`,
  `SP_ERR_LIMIT`).
- Options are structs named after pathlib's keywords; zero is pathlib's default.
- A function returning an `SpPath` returns one named result, so it isn't copied (clang's `-Wnrvo` checks this).
- At most 6 snakepath frames below a public function, and the stack budgets in `snakepath.py`. Never pass these
  checks by hiding a call behind a wrapper or a macro.
- Macros are for constants and platform hacks, never for sharing code.
- No backward compatibility to keep: judge changes on correctness, simplicity and CPython fidelity.
- Speed is general: judge a change on a benchmark of every function family, never one program.

## Layout

- clang-format 21 enforces the layout (style in `nob.h`); `./nob format` applies it, and `./nob` fails on
  `snakepath.h`, `site/trim.h` or `site/logo.c` unformatted. One statement per line, paragraphs separated by blank
  lines. Report line counts only after formatting.
- A refactor counts only if `cc -E -P` of the result is smaller or clearer too.
- The root holds the core only. Everything else (the site, the logo toy, `trim.h`) lives in `site/`.
- No config or support files; tool settings go on nob's command lines.

## Testing

```bash
cc -x c -o nob nob.h && ./nob   # everything: builds, tests, Python checks, layout check, demo
./nob format                    # apply the layout
./nob python                    # the bindings and CPython's pathlib tests
./nob diff [ref]                # pure path logic against another ref, for rewrites of parsing, joining or matching
./nob snake                     # site/snake.c, the site's export
./nob clean                     # before comparing against a clean main
```

Requirements: a C compiler, CPython 3.15+, clang-format 21. `SNAKEPATH_SKIP_GCC=1` builds with clang only, as nob
does on macOS, FreeBSD and OpenBSD. CI (Linux, macOS, Windows, and FreeBSD in a VM) is authoritative.

## Git

- Feature branches and PRs; never push to `main`.
- Close a gap a change exposes in the same change; never skip or expect-fail a test to get CI green.
- `api_demo.c` and its copy in `README.md` must match.

## The site

- `README.md` is the site's page, rendered through `site/_layouts/default.html`. CI copies it into `site/`, builds with
  GitHub Pages' builder, adds `logo.wasm` and `snake.c`, and deploys from `main`.
- The logo is a toy drawn by `site/logo.c`, running as WebAssembly in `site/toy.js`. Judge every visual change in
  screenshots at the size it is shown, at 1x and at the maintainer's 1.67 device pixels per CSS pixel, across a
  whole sway, in every knob state. Regenerate `site/snakepath.png` and `site/snakepath.gif` when the drawing changes.

## Skills

Use the maintainer's skills from https://github.com/netanel-haber/skills: `coding-comms` for all text about code,
`sem-compress` for `/sem-compress`, and `peek` and `visual-loop` for visual work.
