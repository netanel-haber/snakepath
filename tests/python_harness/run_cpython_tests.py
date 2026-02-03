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
    # test_absolute_common - uses mock.patch("os.getcwd") which doesn't affect C
    # Our C library calls getcwd() directly, bypassing Python's mock
    # =========================================================================
    "!=": [
        ("PathSubclassTest", "test_absolute_common"),
        ("PathTest", "test_absolute_common"),
        ("PosixPathTest", "test_absolute_common"),
        ("WindowsPathTest", "test_absolute"),
        ("WindowsPathTest", "test_absolute_common"),
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

    "has no attribute 'expanduser'": [
        ("PathSubclassTest", "test_expanduser_common"),
        ("PathTest", "test_expanduser_common"),
        ("PosixPathTest", "test_expanduser_common"),
        ("WindowsPathTest", "test_expanduser_common"),
    ],

    "has no attribute 'unset'": [
        ("PosixPathTest", "test_expanduser"),
        ("WindowsPathTest", "test_expanduser"),
    ],

    "has no attribute 'group'": [
        ("PathSubclassTest", "test_group"),
        ("PathTest", "test_group"),
        ("PosixPathTest", "test_group"),
        ("WindowsPathAsPureTest", "test_group"),
        ("WindowsPathTest", "test_group"),
    ],

    "has no attribute 'hardlink_to'": [
        ("PathSubclassTest", "test_hardlink_to"),
        ("PathSubclassTest", "test_link_to_not_implemented"),
        ("PathTest", "test_hardlink_to"),
        ("PathTest", "test_link_to_not_implemented"),
        ("PosixPathTest", "test_hardlink_to"),
        ("PosixPathTest", "test_link_to_not_implemented"),
        ("WindowsPathTest", "test_hardlink_to"),
    ],

    # =========================================================================
    # home() not implemented
    # =========================================================================
    "has no attribute 'home'": [
        ("PathSubclassTest", "test_home"),
        ("PathTest", "test_home"),
        ("PosixPathTest", "test_home"),
        ("WindowsPathTest", "test_home"),
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

    "has no attribute 'iterdir'": [
        ("PathSubclassTest", "test_iterdir"),
        ("PathSubclassTest", "test_iterdir_nodir"),
        ("PathSubclassTest", "test_rmdir"),
        ("PathSubclassTest", "test_with"),
        ("PathTest", "test_iterdir"),
        ("PathTest", "test_iterdir_nodir"),
        ("PathTest", "test_rmdir"),
        ("PathTest", "test_with"),
        ("PosixPathTest", "test_iterdir"),
        ("PosixPathTest", "test_iterdir_nodir"),
        ("PosixPathTest", "test_rmdir"),
        ("PosixPathTest", "test_with"),
        ("WindowsPathTest", "test_iterdir"),
        ("WindowsPathTest", "test_iterdir_nodir"),
        ("WindowsPathTest", "test_rmdir"),
        ("WindowsPathTest", "test_with"),
    ],

    "has no attribute 'lstat'": [
        ("PathSubclassTest", "test_lstat_nosymlink"),
        ("PathTest", "test_lstat_nosymlink"),
        ("PosixPathTest", "test_lstat_nosymlink"),
        ("WindowsPathTest", "test_lstat_nosymlink"),
    ],

    "has no attribute 'resolve'": [
        ("PathSubclassTest", "test_mkdir_exist_ok_root"),
        ("PathSubclassTest", "test_resolve_nonexist_relative_issue38671"),
        ("PathTest", "test_mkdir_exist_ok_root"),
        ("PathTest", "test_resolve_nonexist_relative_issue38671"),
        ("PosixPathTest", "test_mkdir_exist_ok_root"),
        ("PosixPathTest", "test_resolve_nonexist_relative_issue38671"),
        ("PosixPathTest", "test_resolve_root"),
        ("WindowsPathTest", "test_mkdir_exist_ok_root"),
        ("WindowsPathTest", "test_resolve_nonexist_relative_issue38671"),
    ],

    "has no attribute 'open'": [
        ("PathSubclassTest", "test_open_common"),
        ("PathTest", "test_open_common"),
        ("PosixPathTest", "test_open_common"),
        ("PosixPathTest", "test_open_mode"),
        ("WindowsPathTest", "test_open_common"),
    ],

    "has no attribute 'owner'": [
        ("PathSubclassTest", "test_owner"),
        ("PathTest", "test_owner"),
        ("PosixPathTest", "test_owner"),
        ("WindowsPathAsPureTest", "test_owner"),
        ("WindowsPathTest", "test_owner"),
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

    "has no attribute 'rename'": [
        ("PathSubclassTest", "test_rename"),
        ("PathTest", "test_rename"),
        ("PosixPathTest", "test_rename"),
        ("WindowsPathTest", "test_rename"),
    ],

    "has no attribute 'replace'": [
        ("PathSubclassTest", "test_replace"),
        ("PathTest", "test_replace"),
        ("PosixPathTest", "test_replace"),
        ("WindowsPathTest", "test_replace"),
    ],

    "has no attribute 'samefile'": [
        ("PathSubclassTest", "test_samefile"),
        ("PathTest", "test_samefile"),
        ("PosixPathTest", "test_samefile"),
        ("WindowsPathTest", "test_samefile"),
    ],

    "lstat (follow_symlinks=False) not yet implemented": [
        ("PathSubclassTest", "test_stat_no_follow_symlinks_nosymlink"),
        ("PathTest", "test_stat_no_follow_symlinks_nosymlink"),
        ("PosixPathTest", "test_stat_no_follow_symlinks_nosymlink"),
        ("WindowsPathTest", "test_stat_no_follow_symlinks_nosymlink"),
    ],

    "has no attribute 'touch'": [
        ("PathSubclassTest", "test_touch_common"),
        ("PathSubclassTest", "test_touch_nochange"),
        ("PathTest", "test_touch_common"),
        ("PathTest", "test_touch_nochange"),
        ("PosixPathTest", "test_touch_common"),
        ("PosixPathTest", "test_touch_mode"),
        ("PosixPathTest", "test_touch_nochange"),
        ("WindowsPathTest", "test_touch_common"),
        ("WindowsPathTest", "test_touch_nochange"),
    ],

    "has no attribute 'unlink'": [
        ("PathSubclassTest", "test_unlink"),
        ("PathSubclassTest", "test_unlink_missing_ok"),
        ("PathTest", "test_unlink"),
        ("PathTest", "test_unlink_missing_ok"),
        ("PosixPathTest", "test_unlink"),
        ("PosixPathTest", "test_unlink_missing_ok"),
        ("WindowsPathTest", "test_unlink"),
        ("WindowsPathTest", "test_unlink_missing_ok"),
    ],

    "has no attribute 'walk'": [
        ("WalkTests", "test_file_like_path"),
        ("WalkTests", "test_walk_above_recursion_limit"),
        ("WalkTests", "test_walk_bad_dir"),
        ("WalkTests", "test_walk_bottom_up"),
        ("WalkTests", "test_walk_many_open_files"),
        ("WalkTests", "test_walk_prune"),
        ("WalkTests", "test_walk_topdown"),
    ],

    # =========================================================================
    # WindowsPathAsPureTest - runs only on Windows, tests pure path operations
    # =========================================================================
    "WindowsPath": [
        ("WindowsPathAsPureTest", "test_eq"),
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


def setup_pathlib_patch():
    """Patch pathlib module to use snakepath."""
    import types

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
    import tempfile
    import shutil
    os_helper = types.ModuleType('test.support.os_helper')
    os_helper.TESTFN = str(Path(tempfile.gettempdir()) / 'test_pathlib_tmp')
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


def run_tests():
    """Run CPython tests against snakepath."""
    setup_tests()
    setup_pathlib_patch()

    # Add test dir to path
    sys.path.insert(0, str(TEST_DIR.parent))

    print("\nRunning CPython pathlib tests against snakepath\n")

    # Discover and load tests
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    loaded_count = 0

    module_name = f"cpython_tests.{TEST_FILE[:-3]}"

    try:
        __import__(module_name)
        module = sys.modules[module_name]

        for name in dir(module):
            obj = getattr(module, name)
            if isinstance(obj, type) and issubclass(obj, unittest.TestCase):
                if obj is unittest.TestCase:
                    continue

                class_loaded = False
                for method_name in loader.getTestCaseNames(obj):
                    suite.addTest(obj(method_name))
                    class_loaded = True

                if class_loaded:
                    loaded_count += 1
    except Exception as e:
        print(f"  ERROR loading {TEST_FILE}: {e}")
        import traceback
        traceback.print_exc()

    print(f"\nLoaded {loaded_count} test classes\n")

    # Run with quiet runner - use verbosity=1 (dots) for shorter CI output
    runner = QuietRunner(verbosity=1)
    result = runner.run(suite)

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
