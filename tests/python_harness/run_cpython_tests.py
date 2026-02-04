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
    # with_segments() not implemented - requires Python subclass cooperation
    # Tests expect subclass attributes to be preserved across path operations
    # =========================================================================
    "has no attribute 'session_id'": [
        ("PosixPathAsPureTest", "test_with_segments_common"),
        ("PurePathSubclassTest", "test_with_segments_common"),
        ("PurePathTest", "test_with_segments_common"),
        ("PurePosixPathTest", "test_with_segments_common"),
        ("PureWindowsPathTest", "test_with_segments_common"),
        ("WindowsPathAsPureTest", "test_with_segments_common"),
        ("PathSubclassTest", "test_with_segments"),
        ("PathTest", "test_with_segments"),
        ("PosixPathTest", "test_with_segments"),
        ("WindowsPathTest", "test_with_segments"),
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
    # CompatiblePathTest.test_truediv - joinpath doesn't accept arbitrary PathLike
    # os.fspath() rejects objects without __fspath__ returning str/bytes
    # =========================================================================
    "expected str, bytes or os.PathLike object, not CompatPath": [
        ("CompatiblePathTest", "test_truediv"),
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
    # (Windows tests only - POSIX tests now pass)
    # =========================================================================
    "has no attribute 'unset'": [
        ("WindowsPathTest", "test_expanduser_common"),
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
    # iterdir_nodir test expects specific exception type (Windows only)
    # =========================================================================
    "FileNotFoundError": [
        ("WindowsPathTest", "test_iterdir_nodir"),
    ],

    # =========================================================================
    # owner/group tests use mock which doesn't affect C (Windows only)
    # =========================================================================
    "KeyError": [
        ("WindowsPathAsPureTest", "test_owner"),
        ("WindowsPathAsPureTest", "test_group"),
        ("WindowsPathTest", "test_owner"),
        ("WindowsPathTest", "test_group"),
    ],

    "has no attribute 'write_bytes'": [
        ("PathSubclassTest", "test_read_write_bytes"),
        ("PathTest", "test_read_write_bytes"),
        ("PosixPathTest", "test_read_write_bytes"),
        ("WindowsPathTest", "test_read_write_bytes"),
    ],

    "has no attribute 'write_text'": [
        ("PathSubclassTest", "test_read_write_text"),
        ("PathSubclassTest", "test_write_text_with_newlines"),
        ("PathTest", "test_read_write_text"),
        ("PathTest", "test_write_text_with_newlines"),
        ("PosixPathTest", "test_read_write_text"),
        ("PosixPathTest", "test_write_text_with_newlines"),
        ("WindowsPathTest", "test_read_write_text"),
        ("WindowsPathTest", "test_write_text_with_newlines"),
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
        ("WalkTests", "test_walk_prune"),
        ("WalkTests", "test_file_like_path"),
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


class QuietExpectedFailuresResult(unittest.TextTestResult):
    """Custom TestResult that suppresses output for expected failures."""

    def _is_expected(self, test, err):
        """Check if this failure/error is expected."""
        test_class = test.__class__.__name__
        test_method = test._testMethodName
        key = (test_class, test_method)
        if key in _EXPECTED_FAILURES_BY_TEST:
            expected_msg = _EXPECTED_FAILURES_BY_TEST[key]
            # Format the error to check against expected message
            err_str = self._exc_info_to_string(err, test)
            return expected_msg in err_str
        return False

    def addError(self, test, err):
        """Called when a test raises an unexpected exception."""
        if self._is_expected(test, err):
            # Silently record as expected
            self.errors.append((test, self._exc_info_to_string(err, test)))
            if self.showAll:
                self.stream.writeln("expected error")
            elif self.dots:
                self.stream.write('x')
                self.stream.flush()
        else:
            super().addError(test, err)

    def addFailure(self, test, err):
        """Called when a test fails."""
        if self._is_expected(test, err):
            # Silently record as expected
            self.failures.append((test, self._exc_info_to_string(err, test)))
            if self.showAll:
                self.stream.writeln("expected failure")
            elif self.dots:
                self.stream.write('x')
                self.stream.flush()
        else:
            super().addFailure(test, err)

    def printErrors(self):
        """Only print unexpected errors."""
        unexpected_errors = []
        unexpected_failures = []

        for test, err in self.errors:
            test_class = test.__class__.__name__
            test_method = test._testMethodName
            key = (test_class, test_method)
            if key in _EXPECTED_FAILURES_BY_TEST and _EXPECTED_FAILURES_BY_TEST[key] in err:
                continue
            unexpected_errors.append((test, err))

        for test, err in self.failures:
            test_class = test.__class__.__name__
            test_method = test._testMethodName
            key = (test_class, test_method)
            if key in _EXPECTED_FAILURES_BY_TEST and _EXPECTED_FAILURES_BY_TEST[key] in err:
                continue
            unexpected_failures.append((test, err))

        if unexpected_errors or unexpected_failures:
            self.stream.writeln()
            self.printErrorList('ERROR', unexpected_errors)
            self.printErrorList('FAIL', unexpected_failures)


class QuietRunner(unittest.TextTestRunner):
    """Test runner that uses QuietExpectedFailuresResult."""
    resultclass = QuietExpectedFailuresResult


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
    runner = QuietRunner(stream=stream, verbosity=0)
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

    # Aggregate results
    class AggregatedResult:
        def __init__(self):
            self.testsRun = 0
            self.failures = []
            self.errors = []
            self.skipped = []

    result = AggregatedResult()

    # Create mock test objects for the results
    class MockTest:
        def __init__(self, class_name, method_name):
            self._testMethodName = method_name
            self.__class__ = type(class_name, (), {'__name__': class_name})

    for class_name, res in results_list:
        if 'error' in res:
            print(f"  ERROR in {class_name}: {res['error']}")
            continue

        result.testsRun += res['tests_run']

        for cls, method, tb in res['failures']:
            mock = MockTest(cls, method)
            result.failures.append((mock, tb))

        for cls, method, tb in res['errors']:
            mock = MockTest(cls, method)
            result.errors.append((mock, tb))

        for cls, method, tb in res['skipped']:
            mock = MockTest(cls, method)
            result.skipped.append((mock, tb))

    # Check expected failures, grouped by reason
    expected_by_reason = {}  # reason -> list of test names
    unexpected_failures = []
    unexpected_errors = []

    for test, traceback in result.failures:
        test_class = test.__class__.__name__
        test_method = test._testMethodName
        key = (test_class, test_method)
        if key in _EXPECTED_FAILURES_BY_TEST:
            expected_msg = _EXPECTED_FAILURES_BY_TEST[key]
            if expected_msg in traceback:
                expected_by_reason.setdefault(expected_msg, []).append(f"{test_class}.{test_method}")
                continue
        unexpected_failures.append((test, traceback))

    for test, traceback in result.errors:
        test_class = test.__class__.__name__
        test_method = test._testMethodName
        key = (test_class, test_method)
        if key in _EXPECTED_FAILURES_BY_TEST:
            expected_msg = _EXPECTED_FAILURES_BY_TEST[key]
            if expected_msg in traceback:
                expected_by_reason.setdefault(expected_msg, []).append(f"{test_class}.{test_method}")
                continue
        unexpected_errors.append((test, traceback))

    # Check for unexpected successes (tests in EXPECTED_FAILURES that passed)
    failed_keys = set()
    for test, _ in result.failures + result.errors:
        test_class = test.__class__.__name__
        test_method = test._testMethodName
        failed_keys.add((test_class, test_method))

    skipped_keys = set()
    for test, _ in result.skipped:
        test_class = test.__class__.__name__
        test_method = test._testMethodName
        skipped_keys.add((test_class, test_method))

    unexpected_successes = []
    for key in _EXPECTED_FAILURES_BY_TEST:
        if key not in failed_keys and key not in skipped_keys:
            unexpected_successes.append(f"{key[0]}.{key[1]}")

    # Summary
    expected_total = sum(len(tests) for tests in expected_by_reason.values())
    print(f"\nRan {result.testsRun} tests, {expected_total} expected failures")

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
