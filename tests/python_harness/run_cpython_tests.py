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


def load_skip_list():
    """Load skip list from platform-specific skip files."""
    base_dir = os.path.dirname(__file__)
    skip_set = set()

    # Always load common skips
    skip_files = ["skip_common.txt"]

    # Add platform-specific skips
    if os.name == 'nt':
        skip_files.append("skip_windows.txt")
    else:
        skip_files.append("skip_posix.txt")

    for filename in skip_files:
        skip_file = os.path.join(base_dir, filename)
        if os.path.exists(skip_file):
            with open(skip_file) as f:
                for line in f:
                    line = line.strip()
                    if line and not line.startswith('#'):
                        skip_set.add(line)

    return skip_set


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

    # Load skip list
    skip_set = load_skip_list()

    # Add test dir to path
    sys.path.insert(0, os.path.dirname(TEST_DIR))

    print("\n" + "=" * 70)
    print("Running CPython pathlib tests against snakepath")
    print("=" * 70 + "\n")

    # Discover and filter tests
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    loaded_count = 0
    skipped_count = 0

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
                    full_name = f"{module_name}.{name}.{method_name}"
                    if full_name in skip_set:
                        skipped_count += 1
                        continue
                    suite.addTest(obj(method_name))
                    class_loaded = True

                if class_loaded:
                    print(f"  LOAD: {name}")
                    loaded_count += 1
    except Exception as e:
        print(f"  ERROR loading {TEST_FILE}: {e}")
        import traceback
        traceback.print_exc()

    print(f"\nLoaded {loaded_count} test classes, skipped {skipped_count} individual tests\n")

    # Run
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)

    # Summary
    print("\n" + "=" * 70)
    print(f"Ran {result.testsRun} CPython tests against snakepath")
    if result.wasSuccessful():
        print("SUCCESS")
    else:
        print(f"FAILURES: {len(result.failures)}, ERRORS: {len(result.errors)}")
    print("=" * 70)

    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(run_tests())
