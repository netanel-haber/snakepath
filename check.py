"""
check.py - the checks nob runs on snakepath (python check.py): snakepath.h's real call depth and stack use, that
README.md embeds api_demo.c verbatim, the shipped snakepath module against the running Python's pathlib, and CPython
3.15's own pathlib tests on it.

python check.py bench <runs> <dir> <module> [args] times a program (python -m <module> [args], run in dir) as it is and
with pathlib's classes swapped for snakepath's, the way a user would monkeypatch them in (CI's bench job).
"""

import os
import pathlib
import re
import subprocess
import sys
import unittest
import urllib.request

from snakepath import PurePath, PurePosixPath, PureWindowsPath, Path, PosixPath, WindowsPath, UnsupportedOperation

TOKEN_RE = re.compile(r"[A-Za-z_]\w*|[{}();]")
LITERAL_RE = re.compile(r"\"(?:\\.|[^\"\\\n])*\"|'(?:\\.|[^'\\\n])*'")


def preprocessed_library(header: pathlib.Path) -> str:
    """The header with its implementation and fluent API, as the compiler sees it"""
    defines = ["SNAKEPATH_IMPLEMENTATION", "SNAKEPATH_FLUENT", "SP_PATH_MAX=4096"]
    if os.name == "nt":
        cmd = ["cl", "/nologo", "/EP", "/TC", *[f"/D{d}" for d in defines], str(header)]
    else:
        cmd = [os.environ.get("CC", "cc"), "-E", "-P", "-x", "c", *[f"-D{d}" for d in defines], str(header)]
    return subprocess.run(cmd, capture_output=True, text=True, check=True).stdout


def call_graph(code: str) -> tuple[dict[str, set[str]], set[str]]:
    """The sp_ functions that preprocessed code defines, each mapped to the sp_ functions its body names (calls, or
    callbacks it hands to qsort), and the functions whose address is taken outside any function (the fluent table)"""
    tokens = TOKEN_RE.findall(LITERAL_RE.sub(" ", code))
    graph: dict[str, set[str]] = {}
    escaped: set[str] = set()
    owner = None
    depth = 0

    i = 0
    while i < len(tokens):
        tok = tokens[i]
        if tok == "{":
            depth += 1
        elif tok == "}":
            depth -= 1
            if depth == 0:
                owner = None
        elif owner is not None:
            graph[owner].add(tok)
        elif depth == 0 and tokens[i + 1:i + 2] == ["("]:
            # A name and its parameter list at file scope: a function definition when a body follows
            i += 1
            parens = 0
            while True:
                parens += {"(": 1, ")": -1}.get(tokens[i], 0)
                i += 1
                if parens == 0:
                    break
            if tokens[i:i + 1] == ["{"] and tok.startswith("sp_"):
                owner = tok
                graph.setdefault(tok, set())
            continue
        else:
            escaped.add(tok)
        i += 1

    graph = {name: (calls & graph.keys()) - {name} for name, calls in graph.items()}
    return graph, escaped & graph.keys()


# Worst-case bytes of snakepath frames per public call, by SP_PATH_MAX: 1/16 of Windows' 1 MB default stack at its
# SP_PATH_MAX. Frames depend on the compiler (clang for the MSVC ABI at -O0 measures about 4x gcc's), so the budgets
# hold for the worst measuring environment, and the printed chains are the numbers to compare.
STACK_BUDGETS = {4096: 128 * 1024, 1024: 64 * 1024}
FRAME_LIMIT = 6  # snakepath frames on the stack from a public function down; a fluent method is one more on top


def stack_usage(header: pathlib.Path, path_max: int) -> dict[str, int] | None:
    """Each function's own frame in bytes at -O0 (gcc's or clang's -fstack-usage), or None without such a compiler"""
    import shutil
    import tempfile
    cc = next((c for c in (os.environ.get("CC"), "gcc", "clang", "cc") if c and shutil.which(c)), None)
    if cc is None or (os.name == "nt" and shutil.which("clang") is None):
        return None
    cc = "clang" if os.name == "nt" else cc
    with tempfile.TemporaryDirectory() as tmp:
        src = pathlib.Path(tmp) / "stack.c"
        src.write_text(f'#include "{header.resolve().as_posix()}"\n', encoding="utf-8")
        defines = ["SNAKEPATH_IMPLEMENTATION", "SNAKEPATH_FLUENT", f"SP_PATH_MAX={path_max}"]
        cmd = [cc, "-std=c99", "-O0", "-fstack-usage", "-c", "-o", str(src.with_suffix(".o")), src.name,
               *[f"-D{d}" for d in defines]]
        subprocess.run(cmd, cwd=tmp, capture_output=True, text=True, check=True)
        usage = {}
        for line in src.with_suffix(".su").read_text(encoding="utf-8").splitlines():
            where, size, _qualifier = line.split("\t")
            usage[where.rsplit(":", 1)[1]] = int(size)
    return usage


def check_frames(header_path: pathlib.Path) -> int:
    """Two bounds on every public function's call chain, and the leads behind them: at most FRAME_LIMIT snakepath
    frames deep (indirection has a price of its own), and at most STACK_BUDGETS bytes of their frames (the real stack
    risk, on Windows' 1 MB default in particular). Every function the preprocessed library defines is a frame; a
    fluent method is one more on top; a function calling itself is exempt (its depth depends on the input)."""
    graph, fluent = call_graph(preprocessed_library(header_path))
    chains: dict[str, list[str]] = {}

    def chain(name: str, active: tuple[str, ...] = ()) -> list[str]:
        """The longest chain of frames from name down"""
        if name in active:
            raise ValueError("mutual recursion: " + " -> ".join(active[active.index(name):] + (name,)))
        if name not in chains:
            below = max((chain(callee, active + (name,)) for callee in graph[name]), key=len, default=[])
            chains[name] = [name] + below
        return chains[name]

    public = sorted(name for name in graph if not name.startswith("sp_priv_"))
    limits = {name: FRAME_LIMIT + 1 for name in fluent}
    limits.update({name: FRAME_LIMIT for name in public})
    try:
        offenders = [chain(name) for name, limit in sorted(limits.items()) if len(chain(name)) > limit]
    except ValueError as err:
        print(f"call-depth check: FAILED ({err})")
        return 1
    deepest = max(len(chains[name]) for name in public)
    print(f"call-depth check: functions={len(graph)} deepest={deepest} limit={FRAME_LIMIT} (fluent methods "
          f"{FRAME_LIMIT + 1}): {'OK' if not offenders else 'FAILED'}")
    for path in offenders:
        print(f"  {' -> '.join(path)}")
    at_limit = [name for name in public if len(chains[name]) == FRAME_LIMIT]
    if at_limit:
        print(f"  at the limit: {', '.join(at_limit)}")

    failed = bool(offenders)
    for path_max, budget in STACK_BUDGETS.items():
        usage = stack_usage(header_path, path_max)
        if usage is None:
            print(f"stack check (SP_PATH_MAX {path_max}): skipped, no gcc or clang for -fstack-usage")
            continue
        costs: dict[str, tuple[int, list[str]]] = {}

        def cost(name: str) -> tuple[int, list[str]]:
            """The heaviest chain of frames from name down, in bytes, and the chain"""
            if name not in costs:
                below = max((cost(callee) for callee in graph[name]), default=(0, []))
                costs[name] = (usage.get(name, 0) + below[0], [name] + below[1])
            return costs[name]

        worst = max((cost(name) for name in public), key=lambda c: c[0])
        heavy = sorted(((usage.get(name, 0), name) for name in graph), reverse=True)[:4]
        ok = worst[0] <= budget
        failed |= not ok
        print(f"stack check (SP_PATH_MAX {path_max}): worst chain {worst[0] // 1024} KB of {budget // 1024} KB: "
              f"{'OK' if ok else 'FAILED'}")
        print(f"  {' -> '.join(worst[1])}")
        print("  heaviest frames: " + ", ".join(f"{name} {size // 1024} KB" for size, name in heavy))
    return 1 if failed else 0


def check_docs(root: pathlib.Path) -> int:
    ok = (root / "api_demo.c").read_text(encoding="utf-8") in (root / "README.md").read_text(encoding="utf-8")
    print("docs check: OK" if ok else "docs check: FAILED (README.md does not embed api_demo.c verbatim)")
    return 0 if ok else 1


def _outcome(fn):
    """What fn() returns, as comparable text, or the name of the exception it raises"""
    try:
        value = fn()
    except Exception as exc:  # the exception type is the behavior being compared
        return f"<{type(exc).__name__}>"
    if isinstance(value, (list, tuple)):
        return repr([str(v) for v in value])
    return repr(value) if isinstance(value, (bool, int)) else str(value)


def fuzz_inputs():
    """Deterministic path strings: every short string over an alphabet of anchors, dots and wildcards, token
    sequences for prefixes random generation never hits (UNC and device prefixes, drives, non-ASCII case pairs),
    and seeded random strings"""
    import itertools
    import random
    alphabet = ["/", "\\", "a", "c", ":", ".", "*", "?"]
    inputs = ["".join(t) for n in range(5) for t in itertools.product(alphabet, repeat=n)]
    tokens = ["\\\\?\\UNC\\", "\\\\.\\", "\\\\?\\", "//", "/", "\\", "c:", "C:", "..", ".", "a", "b.tar.gz", "~",
              "x.", ".y", "srv", "sh", "\xe9", "\xc9", "\u0130", "i", "\u03a3", "\u03c3", "\u00df", "[a-c]", "**"]
    inputs += ["".join(t) for n in (2, 3) for t in itertools.product(tokens, repeat=n)][::7]
    rng = random.Random(1729)
    wide = alphabet + ["b", "C", "~", "%", "|", " ", "[", "]", "!", "-", "\xe9", "\u03a3", "\u03c3", "\u0130", "\xdf",
                       "\u212a", "\u017f", "\U0001f600"]
    inputs += ["".join(rng.choice(wide) for _ in range(rng.randrange(1, 24))) for _ in range(1500)]
    return inputs


FUZZ_OTHERS = ["", ".", "a", "a/b", "/", "/a", "c:", "c:/", "c:a", "C:x", "//s/h", "//s/h/x", "\\\\?\\UNC\\s\\h\\x",
               "..", "../a", "~", "b.txt", ".hidden", "a.", "*", "**", "*.txt", "a/*", "[ab]*", "?", "x/./y",
               "\xe9", "\xc9", "\u03c3"]


def check_fuzz(limit=20):
    """Compare the bindings with the running Python's pathlib on generated paths, in both flavors"""
    import warnings
    warnings.simplefilter("ignore", DeprecationWarning)
    inputs = fuzz_inputs()
    mismatches = []
    checks = 0

    def compare(what, ours, real):
        nonlocal checks
        checks += 1
        a, b = _outcome(ours), _outcome(real)
        if a != b:
            mismatches.append(f"  {what}: snakepath {a!r}, pathlib {b!r}")

    for ours_cls, real_cls in [(PurePosixPath, pathlib.PurePosixPath), (PureWindowsPath, pathlib.PureWindowsPath)]:
        name = real_cls.__name__
        for i, s in enumerate(inputs):
            ours, real = ours_cls(s), real_cls(s)
            for attr in ["drive", "root", "anchor", "name", "stem", "suffix", "suffixes", "parts", "parent"]:
                compare(f"{name}({s!r}).{attr}", lambda: getattr(ours, attr), lambda: getattr(real, attr))
            compare(f"str({name}({s!r}))", lambda: str(ours), lambda: str(real))
            compare(f"{name}({s!r}).parents", lambda: list(ours.parents), lambda: list(real.parents))
            for method in ["is_absolute", "as_posix"]:
                compare(f"{name}({s!r}).{method}()", getattr(ours, method), getattr(real, method))
            if i % 20:
                continue  # binary operations on every 20th input keep the run short
            for o in FUZZ_OTHERS:
                oo, ro = ours_cls(o), real_cls(o)
                compare(f"{name}({s!r}) / {o!r}", lambda: ours / o, lambda: real / o)
                compare(f"{name}({o!r}) / {s!r}", lambda: oo / s, lambda: ro / s)
                for method in ["with_name", "with_stem", "with_suffix"]:
                    compare(f"{name}({s!r}).{method}({o!r})",
                            lambda: getattr(ours, method)(o), lambda: getattr(real, method)(o))
                compare(f"{name}({s!r}).relative_to({o!r})", lambda: ours.relative_to(o), lambda: real.relative_to(o))
                compare(f"{name}({s!r}).relative_to({o!r}, walk_up=True)",
                        lambda: ours.relative_to(o, walk_up=True), lambda: real.relative_to(o, walk_up=True))
                compare(f"{name}({s!r}).is_relative_to({o!r})", lambda: ours.is_relative_to(o),
                        lambda: real.is_relative_to(o))
                compare(f"{name}({s!r}) == {o!r}", lambda: ours == oo, lambda: real == ro)
                compare(f"{name}({s!r}) < {o!r}", lambda: ours < oo, lambda: real < ro)
                for cs in [None, True, False]:
                    compare(f"{name}({s!r}).match({o!r}, case_sensitive={cs})",
                            lambda: ours.match(o, case_sensitive=cs), lambda: real.match(o, case_sensitive=cs))
                    compare(f"{name}({o!r}).full_match({s!r}, case_sensitive={cs})",
                            lambda: oo.full_match(s, case_sensitive=cs), lambda: ro.full_match(s, case_sensitive=cs))
            if ours == ours_cls(s.upper()):
                compare(f"hash({name}({s!r})) == hash of its upper case", lambda: hash(ours) == hash(ours_cls(s.upper())),
                        lambda: True)

    print(f"fuzz check: {checks} comparisons with pathlib, {len(mismatches)} mismatches")
    for line in mismatches[:limit]:
        print(line)
    return 1 if mismatches else 0


THIS_DIR = pathlib.Path(__file__).resolve().parent
TEST_DIR = THIS_DIR / "cpython_tests"
# CPython's own pathlib tests: the 3.15 branch's Lib/test/test_pathlib/test_pathlib.py. The other
# files in that package only test Python-only machinery (pathlib.types protocols, zip/local backends).
CPYTHON_BRANCH = "3.15"
TEST_URL = f"https://raw.githubusercontent.com/python/cpython/{CPYTHON_BRANCH}/Lib/test/test_pathlib/test_pathlib.py"
TEST_FILE = "test_pathlib_3_15.py"
CLASS_TIMEOUT = 600  # seconds a test class may run before its worker dumps tracebacks and exits  # cached under a versioned name

# Tests expected to fail with specific error messages
# Format: {expected_error_substring: [(class_name, test_name), ...]}
# These exercise Python-only pathlib machinery with no C counterpart. Both flavors are listed where the host
# decides the flavor (PurePath, Path); the other flavor's tests are skipped, which counts as not passing.
_PURE = ["PurePathTest", "PurePosixPathTest", "PureWindowsPathTest", "PurePathSubclassTest"]
_PATH = ["PathTest", "PathSubclassTest", "PosixPathTest", "WindowsPathTest"]
EXPECTED_FAILURES = {
    # PurePath.as_uri() is deprecated in favor of Path.as_uri() - a warnings.warn() about Python classes
    "DeprecationWarning not triggered": [
        (cls, test) for cls in ["PurePathTest", "PurePosixPathTest", "PurePathSubclassTest"]
        for test in ["test_as_uri_posix", "test_as_uri_non_ascii", "test_as_uri_windows"]
    ] + [("PureWindowsPathTest", "test_as_uri_windows")],

    # Private CPython hooks: _parse_path (parser internals) and _delete (shutil.rmtree wrapper)
    "has no attribute '_parse_path'": [
        (cls, test) for cls in _PURE + _PATH
        for test in ["test_parse_path_common", "test_parse_path_posix", "test_parse_path_windows"]
    ],
    "has no attribute '_delete'": [
        (cls, f"test_delete_{what}") for cls in _PATH
        for what in ["dir", "file", "missing", "on_named_pipe", "does_not_choke_on_failing_lstat",
                     "inner_junction", "outer_junction", "symlink", "inner_symlink", "unwritable"]
    ],

    # Path.info (a caching PathInfo object) and pathlib.types protocols
    "has no attribute 'info'": [
        (cls, test) for cls in _PATH
        for test in ["test_info_exists_caching", "test_info_is_dir_caching", "test_info_is_file_caching",
                     "test_info_is_symlink_caching", "test_glob_posix"]
    ],
    "has no attribute 'types'": [(cls, "test_matches_writablepath_docstrings") for cls in _PATH],

    # Pickles made by 3.13, which name the pathlib._local module
    "pathlib._local": [(cls, "test_unpicking_3_13") for cls in _PURE + _PATH],

    # Mocks of Python functions the C library never calls: parser.isjunction, os.getcwd, fast-copy syscalls
    "MagicMock": [(cls, "test_is_junction_true") for cls in _PATH],
    "!=": [(cls, test) for cls in _PATH for test in ["test_absolute_common", "test_absolute_windows"]],
    # Only POSIX has the patched fast-copy functions; on Windows the test passes
    "FileNotFoundError not raised": [
        (cls, "test_copy_error_handling") for cls in ["PathTest", "PathSubclassTest", "PosixPathTest"] if os.name != 'nt'
    ],

}

# Build reverse lookup: (class_name, test_name) -> expected_error_substring
_EXPECTED_FAILURES_BY_TEST = {}
for _reason, _tests in EXPECTED_FAILURES.items():
    for _test in _tests:
        _EXPECTED_FAILURES_BY_TEST[_test] = _reason


def download_file(url, dest):
    """Download a file."""
    dest.parent.mkdir(parents=True, exist_ok=True)
    with urllib.request.urlopen(url) as r:
        content = r.read()
    dest.write_bytes(content)


def setup_tests():
    """Download test file if needed."""
    TEST_DIR.mkdir(exist_ok=True)

    # Create package __init__.py
    init_path = TEST_DIR / "__init__.py"
    if not init_path.exists():
        init_path.touch()

    # Download test file
    dest = TEST_DIR / TEST_FILE
    if not dest.exists():
        print("Downloading CPython pathlib test...")
        print(f"  {TEST_FILE}")
        download_file(TEST_URL, dest)


def _stub_module(name):
    """Module stand-in with a spec, which Python 3.15's ModuleNotFoundError formatting inspects via importlib.resources"""
    import importlib.machinery
    import types
    module = types.ModuleType(name)
    module.__spec__ = importlib.machinery.ModuleSpec(name, None)
    return module


def setup_pathlib_patch(testfn=None):
    """Patch pathlib module to use snakepath."""
    import types
    import tempfile

    # Create pathlib module with snakepath classes
    pathlib_pkg = _stub_module('pathlib')
    pathlib_pkg.PurePath = PurePath
    pathlib_pkg.PurePosixPath = PurePosixPath
    pathlib_pkg.PureWindowsPath = PureWindowsPath
    pathlib_pkg.Path = Path
    pathlib_pkg.PosixPath = PosixPath
    pathlib_pkg.WindowsPath = WindowsPath
    pathlib_pkg.UnsupportedOperation = UnsupportedOperation
    sys.modules['pathlib'] = pathlib_pkg

    # Stub test.support
    test_pkg = _stub_module('test')
    sys.modules['test'] = test_pkg

    test_support = _stub_module('test.support')
    test_support.is_emscripten = False
    test_support.is_wasi = False
    test_support.verbose = False
    test_support.cpython_only = lambda f: f
    test_support.is_android = False
    test_support.is_wasm32 = False
    is_root = hasattr(os, 'geteuid') and os.geteuid() == 0
    test_support.requires_root_user = unittest.skipUnless(is_root, "requires root")
    test_support.requires_non_root_user = unittest.skipIf(is_root, "requires non-root")

    # Context manager for recursion limit
    import contextlib
    @contextlib.contextmanager
    def set_recursion_limit(limit):
        old = sys.getrecursionlimit()
        try:
            sys.setrecursionlimit(limit)
            yield
        finally:
            sys.setrecursionlimit(old)
    test_support.set_recursion_limit = set_recursion_limit

    @contextlib.contextmanager
    def infinite_recursion(max_depth=None):
        with set_recursion_limit(max_depth or 150):
            yield
    test_support.infinite_recursion = infinite_recursion

    class ImportHelper:
        @staticmethod
        def import_module(name):
            return __import__(name)

        @staticmethod
        def ensure_lazy_imports(*args, **kwargs):
            raise unittest.SkipTest("lazy imports: not meaningful in C")
    test_support.import_helper = ImportHelper()
    sys.modules['test.support'] = test_support

    # Stub test.support.os_helper
    import shutil
    os_helper = _stub_module('test.support.os_helper')
    if testfn is None:
        testfn = str(pathlib.Path(tempfile.gettempdir()) / 'test_pathlib_tmp')
    os_helper.TESTFN = testfn
    os_helper.FS_NONASCII = '\xe9'
    class FakePath:
        def __init__(self, path): self.path = path
        def __fspath__(self): return self.path
    os_helper.FakePath = FakePath
    def probe(make):
        """Whether this host can make the link or chmod that `make` tries in a scratch directory"""
        scratch = tempfile.mkdtemp()
        try:
            make(os.path.join(scratch, 'target'), os.path.join(scratch, 'link'))
            return True
        except (OSError, NotImplementedError, AttributeError):
            return False
        finally:
            shutil.rmtree(scratch, ignore_errors=True)

    def touch_then(action):
        def make(target, link):
            open(target, 'w').close()
            action(target, link)
        return make

    def chmod_sticks(target, link):
        os.chmod(target, 0o400)
        if os.stat(target).st_mode & 0o777 != 0o400:
            raise OSError('chmod does not stick')
        os.chmod(target, 0o600)

    have_symlink = probe(touch_then(os.symlink))
    have_hardlink = probe(touch_then(lambda target, link: os.link(target, link)))
    have_chmod = probe(touch_then(chmod_sticks))
    os_helper.can_symlink = lambda: have_symlink
    os_helper.fs_is_case_insensitive = lambda path: False
    def rmtree(path):
        # Like CPython's os_helper.rmtree: tests leave unreadable dirs behind, so restore access and retry
        def onexc(func, p, exc):
            os.chmod(os.path.dirname(p) or '.', 0o700)
            os.chmod(p, 0o700)
            func(p) if func is not os.open else shutil.rmtree(p, onexc=onexc)
        if os.path.lexists(path):
            shutil.rmtree(path, onexc=onexc)
    os_helper.rmtree = rmtree
    # Skip decorators
    os_helper.skip_unless_xattr = unittest.skip("xattr not available")
    os_helper.skip_unless_working_chmod = unittest.skipUnless(have_chmod, "chmod does not work here")
    os_helper.skip_unless_symlink = unittest.skipUnless(have_symlink, "symlinks do not work here")
    os_helper.skip_if_dac_override = lambda f: f
    os_helper.skip_unless_hardlink = unittest.skipUnless(have_hardlink, "hard links do not work here")
    os_helper._longpath = lambda path: path

    @contextlib.contextmanager
    def change_cwd(path):
        old = os.getcwd()
        os.chdir(path)
        try:
            yield os.getcwd()
        finally:
            os.chdir(old)
    os_helper.change_cwd = change_cwd

    @contextlib.contextmanager
    def temp_umask(umask):
        old = os.umask(umask)
        try:
            yield
        finally:
            os.umask(old)
    os_helper.temp_umask = temp_umask

    @contextlib.contextmanager
    def subst_drive(path):
        raise unittest.SkipTest("subst drives not tested")
        yield
    os_helper.subst_drive = subst_drive
    class EnvironmentVarGuard(dict):
        """Edits os.environ (so the C library sees it) and restores it on exit"""
        def __enter__(self):
            self.saved = dict(os.environ)
            return self
        def __exit__(self, *args):
            os.environ.clear()
            os.environ.update(self.saved)
        def __setitem__(self, name, value): os.environ[name] = value
        def unset(self, *names):
            for name in names: os.environ.pop(name, None)
        def pop(self, name, *default): return os.environ.pop(name, *default)
    os_helper.EnvironmentVarGuard = EnvironmentVarGuard
    sys.modules['test.support.os_helper'] = os_helper



def run_single_class(class_info):
    """Run a single test class in a subprocess. Returns (class_name, results_dict)."""
    import os
    import tempfile
    import shutil

    class_name, module_name = class_info
    # A crash or hang in the C library shows up as a traceback instead of a silent stall
    import faulthandler
    faulthandler.enable()
    faulthandler.dump_traceback_later(CLASS_TIMEOUT, exit=True)

    # Create unique temp directory for this class
    unique_tmp = os.path.join(tempfile.gettempdir(), f'test_pathlib_{class_name}_{os.getpid()}')

    # Clear cached modules to force fresh import with new TESTFN
    for mod_name in list(sys.modules.keys()):
        if 'cpython_tests' in mod_name or mod_name == 'pathlib' or mod_name.startswith('test.'):
            del sys.modules[mod_name]

    # Re-setup environment in subprocess with unique temp dir
    setup_pathlib_patch(unique_tmp)
    sys.path.insert(0, str(TEST_DIR.parent))

    try:
        __import__(module_name)
        module = sys.modules[module_name]
        test_class = getattr(module, class_name)
    except Exception as e:
        return (class_name, {'error': str(e), 'tests_run': 0, 'failures': [], 'errors': [], 'skipped': []})

    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    for method_name in loader.getTestCaseNames(test_class):
        suite.addTest(test_class(method_name))

    # Run with a stream that captures output
    import io
    stream = io.StringIO()
    runner = unittest.TextTestRunner(stream=stream, verbosity=0)
    result = runner.run(suite)

    # Cleanup unique temp directory
    if os.path.exists(unique_tmp):
        shutil.rmtree(unique_tmp, ignore_errors=True)

    # Convert result to serializable dict
    def test_to_tuple(test, tb):
        test = getattr(test, 'test_case', test)  # a failed subTest reports its parent test
        return (test.__class__.__name__, test._testMethodName, tb)

    return (class_name, {
        'tests_run': result.testsRun,
        'failures': [test_to_tuple(t, tb) for t, tb in result.failures],
        'errors': [test_to_tuple(t, tb) for t, tb in result.errors],
        'skipped': [test_to_tuple(t, tb) for t, tb in result.skipped],
    })


def run_tests():
    """Run CPython tests against snakepath."""
    import multiprocessing
    from concurrent.futures import ProcessPoolExecutor
    import os

    setup_tests()
    setup_pathlib_patch()

    # Add test dir to path
    sys.path.insert(0, str(TEST_DIR.parent))

    print("\nRunning CPython pathlib tests against snakepath (parallel by class)\n")

    # Discover test classes
    module_name = f"cpython_tests.{TEST_FILE[:-3]}"
    test_classes = []

    try:
        __import__(module_name)
        module = sys.modules[module_name]

        loader = unittest.TestLoader()
        for name in dir(module):
            obj = getattr(module, name)
            if isinstance(obj, type) and issubclass(obj, unittest.TestCase):
                if obj is unittest.TestCase:
                    continue
                # Check it has test methods
                if loader.getTestCaseNames(obj):
                    test_classes.append((name, module_name))
    except Exception as e:
        print(f"  ERROR loading {TEST_FILE}: {e}")
        import traceback
        traceback.print_exc()
        return 1

    print(f"Found {len(test_classes)} test classes, running in parallel...\n")

    # Run test classes in parallel using spawn context for clean isolation
    num_workers = min(len(test_classes), os.cpu_count() or 4)
    # Unlike multiprocessing.Pool, the executor raises BrokenProcessPool if a worker dies instead of waiting forever
    with ProcessPoolExecutor(num_workers, mp_context=multiprocessing.get_context('spawn')) as pool:
        results_list = list(pool.map(run_single_class, test_classes))

    # Aggregate (class_name, method_name, traceback) tuples from all classes
    tests_run, failures, errors, skipped = 0, [], [], []
    for class_name, res in results_list:
        if 'error' in res:
            print(f"  ERROR in {class_name}: {res['error']}")
            continue
        tests_run += res['tests_run']
        failures += res['failures']
        errors += res['errors']
        skipped += res['skipped']

    # Expected failures must fail for their documented reason; anything else is unexpected
    expected_by_reason = {}  # reason -> list of test names
    def unexpected(results):
        out = []
        for cls, method, tb in results:
            reason = _EXPECTED_FAILURES_BY_TEST.get((cls, method))
            if reason is not None and reason in tb:
                expected_by_reason.setdefault(reason, []).append(f"{cls}.{method}")
            else:
                out.append((f"{cls}.{method}", tb))
        return out
    unexpected_failures = unexpected(failures)
    unexpected_errors = unexpected(errors)

    # Expected failures that neither failed nor were skipped passed unexpectedly
    not_passed = {(cls, method) for cls, method, _ in failures + errors + skipped}
    unexpected_successes = [f"{cls}.{method}" for cls, method in _EXPECTED_FAILURES_BY_TEST
                            if (cls, method) not in not_passed]

    # Summary
    expected_total = sum(len(tests) for tests in expected_by_reason.values())
    print(f"\nRan {tests_run} tests, {expected_total} expected failures")

    if expected_by_reason:
        print("\nExpected failures by reason:")
        for reason, tests in sorted(expected_by_reason.items(), key=lambda x: -len(x[1])):
            print(f"  {reason!r}: {len(tests)} tests")

    if unexpected_successes:
        print(f"\nUNEXPECTED SUCCESSES ({len(unexpected_successes)} tests passed that were expected to fail):")
        for test_name in sorted(unexpected_successes):
            print(f"  {test_name}")

    if not unexpected_failures and not unexpected_errors and not unexpected_successes:
        print("\nSUCCESS")
        return_code = 0
    else:
        if unexpected_failures or unexpected_errors:
            print(f"\nUNEXPECTED: {len(unexpected_failures)} failures, {len(unexpected_errors)} errors")
            for test, tb in unexpected_failures:
                print(f"  FAIL: {test}")
                print(tb)
            for test, tb in unexpected_errors:
                print(f"  ERROR: {test}")
                print(tb)
        return_code = 1

    return return_code



# A program run as python -m would run it, pathlib's classes swapped for snakepath's first on the snakepath side, so both
# sides pay the same launcher
SWAP_LAUNCHER = """
import pathlib, runpy, sys
if sys.argv[1] == 'snakepath':
    import snakepath
    for name in ('PurePath', 'PurePosixPath', 'PureWindowsPath', 'Path', 'PosixPath', 'WindowsPath'):
        setattr(pathlib, name, getattr(snakepath, name))
sys.argv = sys.argv[2:]
runpy.run_module(sys.argv[0], run_name='__main__', alter_sys=True)
"""


def bench(runs, cwd, program):
    """Time program under pathlib and under snakepath, alternating sides so drift hits both alike, after one warm-up
    run each (which also fills the bytecode caches); both sides must print the same, timings aside"""
    import statistics
    import time

    def run(side):
        start = time.perf_counter()
        done = subprocess.run([sys.executable, "-c", SWAP_LAUNCHER, side, *program], cwd=cwd, capture_output=True,
                              text=True)
        return time.perf_counter() - start, (done.returncode, re.sub(r"[\d.]+s\b", "", done.stdout), done.stderr)

    sides = ["pathlib", "snakepath"]
    outputs = {side: run(side)[1] for side in sides}
    times = {side: [] for side in sides}
    for i in range(runs):
        for side in sides if i % 2 == 0 else sides[::-1]:
            times[side].append(run(side)[0])

    print(f"python -m {' '.join(program)} in {cwd}, {runs} runs per side:")
    for side in sides:
        print(f"  {side:9}  median {statistics.median(times[side]):7.3f} s  min {min(times[side]):7.3f} s")
    print(f"  snakepath is {statistics.median(times['pathlib']) / statistics.median(times['snakepath']):.2f}x "
          f"as fast (medians)")
    last = outputs["snakepath"][1].strip().splitlines()[-1:]
    print(f"  exit code {outputs['snakepath'][0]}, last line: {last[0] if last else ''}")
    if outputs["pathlib"] != outputs["snakepath"]:
        print("  OUTPUTS DIFFER:")
        for side in sides:
            print(f"--- {side}: exit {outputs[side][0]}\n{outputs[side][1][-3000:]}\n{outputs[side][2][-3000:]}")
        return 1
    print("  same output on both sides")
    return 0


def main():
    if sys.argv[1:2] == ["bench"]:
        return bench(int(sys.argv[2]), sys.argv[3], sys.argv[4:])
    # Windows consoles default to cp1252, which cannot print test names like 'İ'
    if sys.platform == 'win32' and sys.stdout.encoding != 'utf-8':
        import io
        sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
        sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')
    checks_failed = check_frames(THIS_DIR / "snakepath.h") | check_docs(THIS_DIR) | check_fuzz()
    return run_tests() or checks_failed


if __name__ == "__main__":
    sys.exit(main())
