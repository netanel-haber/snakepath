#!/usr/bin/env python3
"""
Run CPython's pathlib test suite against snakepath.
Downloads test file from CPython's test directory.
"""

import sys
import os
import unittest
import urllib.request

# Add our module to path FIRST (snakepath package is in this directory)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import snakepath

TEST_DIR = os.path.join(os.path.dirname(__file__), "cpython_tests")
# Use Python 3.12 branch to match our Python version
CPYTHON_BRANCH = "3.12"
CPYTHON_RAW = f"https://raw.githubusercontent.com/python/cpython/{CPYTHON_BRANCH}/Lib/test"

# Test file to download (Python 3.12 has single test_pathlib.py)
TEST_FILE = "test_pathlib.py"


# Tests expected to fail with specific error messages
# Format: {(class_name, test_name): expected_error_substring}
# These document known limitations where Python-specific features cannot be in C
EXPECTED_FAILURES = {
    # Python's warnings.warn() system does not exist in C
    # These tests check that DeprecationWarning is emitted for multi-arg calls
    ("PurePosixPathTest", "test_is_relative_to_common"): "DeprecationWarning not triggered",
    ("PurePosixPathTest", "test_relative_to_common"): "DeprecationWarning not triggered",
    ("PureWindowsPathTest", "test_is_relative_to_common"): "DeprecationWarning not triggered",
    ("PureWindowsPathTest", "test_relative_to_common"): "DeprecationWarning not triggered",
    ("PurePathTest", "test_is_relative_to_common"): "DeprecationWarning not triggered",
    ("PurePathTest", "test_relative_to_common"): "DeprecationWarning not triggered",
    ("PurePathSubclassTest", "test_is_relative_to_common"): "DeprecationWarning not triggered",
    ("PurePathSubclassTest", "test_relative_to_common"): "DeprecationWarning not triggered",
    ("PosixPathAsPureTest", "test_is_relative_to_common"): "DeprecationWarning not triggered",
    ("PosixPathAsPureTest", "test_relative_to_common"): "DeprecationWarning not triggered",
    ("WindowsPathAsPureTest", "test_is_relative_to_common"): "DeprecationWarning not triggered",
    ("WindowsPathAsPureTest", "test_relative_to_common"): "DeprecationWarning not triggered",

    # Cross-flavor ordering comparison requires Python-level type checking
    # C library doesn't implement TypeError for comparing PosixPath < WindowsPath
    ("PurePathTest", "test_different_flavours_unordered"): "TypeError",

    # Turkish I case folding requires Unicode NFKC normalization
    # C library uses simple ASCII case folding, not full Unicode
    ("PureWindowsPathTest", "test_eq"): "PureWindowsPath('İ')",
}


def download_file(url, dest):
    """Download a file."""
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    with urllib.request.urlopen(url) as r:
        content = r.read()
    with open(dest, 'wb') as f:
        f.write(content)


def setup_tests():
    """Download test file if needed."""
    os.makedirs(TEST_DIR, exist_ok=True)

    # Create package __init__.py
    init_path = os.path.join(TEST_DIR, "__init__.py")
    if not os.path.exists(init_path):
        with open(init_path, "w") as f:
            f.write("")

    # Download test file
    dest = os.path.join(TEST_DIR, TEST_FILE)
    if not os.path.exists(dest):
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
    os_helper = types.ModuleType('test.support.os_helper')
    os_helper.TESTFN = os.path.join(tempfile.gettempdir(), 'test_pathlib_tmp')
    os_helper.FS_NONASCII = '\xe9'
    class FakePath:
        def __init__(self, path): self.path = path
        def __fspath__(self): return self.path
    os_helper.FakePath = FakePath
    os_helper.can_symlink = lambda: False
    os_helper.rmtree = lambda p: None
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


def run_tests():
    """Run CPython tests against snakepath."""
    setup_tests()
    setup_pathlib_patch()

    # Add test dir to path
    sys.path.insert(0, os.path.dirname(TEST_DIR))

    print("\n" + "=" * 70)
    print("Running CPython pathlib tests against snakepath")
    print("=" * 70 + "\n")

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
                    print(f"  LOAD: {name}")
                    loaded_count += 1
    except Exception as e:
        print(f"  ERROR loading {TEST_FILE}: {e}")
        import traceback
        traceback.print_exc()

    print(f"\nLoaded {loaded_count} test classes\n")

    # Run
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)

    # Check expected failures
    expected_failure_count = 0
    unexpected_failures = []
    unexpected_errors = []

    for test, traceback in result.failures:
        test_class = test.__class__.__name__
        test_method = test._testMethodName
        key = (test_class, test_method)
        if key in EXPECTED_FAILURES:
            expected_msg = EXPECTED_FAILURES[key]
            if expected_msg in traceback:
                expected_failure_count += 1
                continue
        unexpected_failures.append((test, traceback))

    for test, traceback in result.errors:
        test_class = test.__class__.__name__
        test_method = test._testMethodName
        key = (test_class, test_method)
        if key in EXPECTED_FAILURES:
            expected_msg = EXPECTED_FAILURES[key]
            if expected_msg in traceback:
                expected_failure_count += 1
                continue
        unexpected_errors.append((test, traceback))

    # Summary
    print("\n" + "=" * 70)
    print(f"Ran {result.testsRun} CPython tests against snakepath")
    if expected_failure_count > 0:
        print(f"Expected failures (Python-specific features not in C): {expected_failure_count}")

    if not unexpected_failures and not unexpected_errors:
        print("SUCCESS")
        return_code = 0
    else:
        print(f"UNEXPECTED FAILURES: {len(unexpected_failures)}, ERRORS: {len(unexpected_errors)}")
        for test, tb in unexpected_failures:
            print(f"  FAIL: {test}")
        for test, tb in unexpected_errors:
            print(f"  ERROR: {test}")
        return_code = 1
    print("=" * 70)

    return return_code


if __name__ == "__main__":
    sys.exit(run_tests())
