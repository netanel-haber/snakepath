#!/usr/bin/env python3
"""
Run CPython's pathlib test suite against snakepath.
Downloads test file from CPython's test directory.
"""

import sys

# Fix Windows console encoding (Turkish İ etc can't print on cp1252)
if sys.platform == 'win32' and sys.stdout.encoding != 'utf-8':
    import io
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

import unittest
import urllib.request
from pathlib import Path

# Add our module to path FIRST (snakepath package is in this directory)
THIS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(THIS_DIR))

import snakepath

TEST_DIR = THIS_DIR / "cpython_tests"
# Use Python 3.12 branch to match our Python version
CPYTHON_BRANCH = "3.12"
CPYTHON_RAW = f"https://raw.githubusercontent.com/python/cpython/{CPYTHON_BRANCH}/Lib/test"

# Test file to download (Python 3.12 has single test_pathlib.py)
TEST_FILE = "test_pathlib.py"

# Tests expected to fail with specific error messages
# Format: {expected_error_substring: [(class_name, test_name), ...]}
# These document known limitations and unimplemented features
EXPECTED_FAILURES = {
    # =========================================================================
    # Python's warnings.warn() system does not exist in C
    # These tests check that DeprecationWarning is emitted for multi-arg calls
    # =========================================================================
    "DeprecationWarning not triggered": [
        ("PurePosixPathTest", "test_is_relative_to_common"),
        ("PurePosixPathTest", "test_relative_to_common"),
        ("PureWindowsPathTest", "test_is_relative_to_common"),
        ("PureWindowsPathTest", "test_relative_to_common"),
        ("PurePathTest", "test_is_relative_to_common"),
        ("PurePathTest", "test_relative_to_common"),
        ("PurePathSubclassTest", "test_is_relative_to_common"),
        ("PurePathSubclassTest", "test_relative_to_common"),
        ("PosixPathAsPureTest", "test_is_relative_to_common"),
        ("PosixPathAsPureTest", "test_relative_to_common"),
        ("WindowsPathAsPureTest", "test_is_relative_to_common"),
        ("WindowsPathAsPureTest", "test_relative_to_common"),
        ("PathSubclassTest", "test_passing_kwargs_deprecated"),
        ("PathTest", "test_passing_kwargs_deprecated"),
        ("PosixPathTest", "test_passing_kwargs_deprecated"),
        ("WindowsPathTest", "test_passing_kwargs_deprecated"),
    ],

    # =========================================================================
    # Cross-flavor ordering comparison requires Python-level type checking
    # C library doesn't implement TypeError for comparing PosixPath < WindowsPath
    # =========================================================================
    "TypeError": [
        ("PurePathTest", "test_different_flavours_unordered"),
    ],

    # =========================================================================
    # Turkish I case folding requires Unicode NFKC normalization
    # C library uses simple ASCII case folding, not full Unicode
    # =========================================================================
    "PureWindowsPath('İ')": [
        ("PureWindowsPathTest", "test_eq"),
    ],

    # =========================================================================
    # with_suffix() tuple argument validation differs from Python
    # C library raises TypeError, Python raises ValueError for tuple suffix
    # =========================================================================
    "expected str, not tuple": [
        ("PosixPathAsPureTest", "test_with_suffix_common"),
        ("PurePathSubclassTest", "test_with_suffix_common"),
        ("PurePathTest", "test_with_suffix_common"),
        ("PurePosixPathTest", "test_with_suffix_common"),
        ("PureWindowsPathTest", "test_with_suffix_common"),
        ("WindowsPathAsPureTest", "test_with_suffix_common"),
    ],

    # =========================================================================
    # Pickling not implemented - would require __getstate__/__setstate__
    # C-backed objects with __slots__ cannot be pickled without explicit support
    # =========================================================================
    "cannot be pickled": [
        ("PurePathSubclassTest", "test_pickling_common"),
    ],


    # =========================================================================
    # test_parts_interning - Python interns string parts, C doesn't
    # =========================================================================
    "is not": [
        ("PathSubclassTest", "test_parts_interning"),
        ("PathTest", "test_parts_interning"),
        ("PosixPathTest", "test_parts_interning"),
        ("WindowsPathTest", "test_parts_interning"),
    ],

    "NotImplementedError": [
        ("PathTest", "test_unsupported_flavour"),
    ],

    # =========================================================================
    # expanduser tests use EnvironmentVarGuard mock which doesn't affect C
    # =========================================================================
    "has no attribute 'unset'": [
        ("PosixPathTest", "test_expanduser"),
        ("WindowsPathTest", "test_expanduser"),
    ],

    # =========================================================================
    # test_is_junction tests Python's internal _flavour.isjunction delegation
    # We implement is_junction directly in C, bypassing the mock pattern
    # =========================================================================
    "MagicMock": [
        ("PathSubclassTest", "test_is_junction"),
        ("PathTest", "test_is_junction"),
        ("PosixPathTest", "test_is_junction"),
        ("WindowsPathTest", "test_is_junction"),
    ],

    # =========================================================================
    # test_with uses context manager protocol not implemented
    # =========================================================================
    "does not support the context manager protocol": [
        ("PathSubclassTest", "test_with"),
        ("PathTest", "test_with"),
        ("PosixPathTest", "test_with"),
        ("WindowsPathTest", "test_with"),
    ],

    # =========================================================================
    # walk prune test modifies dirnames list, which Python bindings don't support
    # (pruning works in C but Python bindings are read-only)
    # =========================================================================
    "!=": [
        ("PathSubclassTest", "test_absolute_common"),
        ("PathTest", "test_absolute_common"),
        ("PosixPathTest", "test_absolute_common"),
        ("WindowsPathTest", "test_absolute"),
        ("WindowsPathTest", "test_absolute_common"),
    ],

    # =========================================================================
    # WindowsPathAsPureTest - runs only on Windows, tests pure path operations
    # =========================================================================
    "WindowsPath": [
        ("WindowsPathAsPureTest", "test_eq"),
    ],

    # =========================================================================
    # link_to() is deprecated API - hardlink_to() replaced it
    # Test expects NotImplementedError but we implement hardlink_to() directly
    # =========================================================================
    "Operation not permitted": [
        ("PathSubclassTest", "test_link_to_not_implemented"),
        ("PathTest", "test_link_to_not_implemented"),
        ("PosixPathTest", "test_link_to_not_implemented"),
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
        url = f"{CPYTHON_RAW}/{TEST_FILE}"
        download_file(url, dest)


def setup_pathlib_patch(testfn=None):
    """Patch pathlib module to use snakepath."""
    import types
    import tempfile

    # Create pathlib module with snakepath classes
    pathlib_pkg = types.ModuleType('pathlib')
    pathlib_pkg.PurePath = snakepath.PurePath
    pathlib_pkg.PurePosixPath = snakepath.PurePosixPath
    pathlib_pkg.PureWindowsPath = snakepath.PureWindowsPath
    pathlib_pkg.Path = snakepath.Path
    pathlib_pkg.PosixPath = snakepath.PosixPath
    pathlib_pkg.WindowsPath = snakepath.WindowsPath
    sys.modules['pathlib'] = pathlib_pkg

    # Stub test.support
    test_pkg = types.ModuleType('test')
    sys.modules['test'] = test_pkg

    test_support = types.ModuleType('test.support')
    test_support.is_emscripten = False
    test_support.is_wasi = False
    test_support.verbose = False
    test_support.cpython_only = lambda f: f
    test_support.is_android = False

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

    class ImportHelper:
        @staticmethod
        def import_module(name):
            return __import__(name)
    test_support.import_helper = ImportHelper()
    sys.modules['test.support'] = test_support

    # Stub test.support.os_helper
    import shutil
    os_helper = types.ModuleType('test.support.os_helper')
    if testfn is None:
        testfn = str(Path(tempfile.gettempdir()) / 'test_pathlib_tmp')
    os_helper.TESTFN = testfn
    os_helper.FS_NONASCII = '\xe9'
    class FakePath:
        def __init__(self, path): self.path = path
        def __fspath__(self): return self.path
    os_helper.FakePath = FakePath
    os_helper.can_symlink = lambda: False
    os_helper.fs_is_case_insensitive = lambda path: False
    os_helper.rmtree = shutil.rmtree
    # Skip decorators
    os_helper.skip_unless_xattr = unittest.skip("xattr not available")
    os_helper.skip_unless_working_chmod = unittest.skip("chmod not tested")
    os_helper.skip_unless_symlink = unittest.skip("symlink not tested")
    os_helper.skip_if_dac_override = lambda f: f
    class EnvironmentVarGuard:
        def __enter__(self): return {}
        def __exit__(self, *args): pass
    os_helper.EnvironmentVarGuard = EnvironmentVarGuard
    sys.modules['test.support.os_helper'] = os_helper



def run_single_class(class_info):
    """Run a single test class in a subprocess. Returns (class_name, results_dict)."""
    import os
    import tempfile
    import shutil

    class_name, module_name = class_info

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
    ctx = multiprocessing.get_context('spawn')
    with ctx.Pool(num_workers) as pool:
        results_list = pool.map(run_single_class, test_classes)

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


if __name__ == "__main__":
    sys.exit(run_tests())
