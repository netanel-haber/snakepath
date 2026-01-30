#!/usr/bin/env python3
"""
Run CPython's pathlib test suite against snakepath.
Downloads test files from CPython's test_pathlib directory.
"""

import sys
import os
import unittest
import urllib.request

# Add our module to path FIRST
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import snakepath

TEST_DIR = os.path.join(os.path.dirname(__file__), "cpython_tests")
CPYTHON_RAW = "https://raw.githubusercontent.com/python/cpython/main/Lib/test/test_pathlib"

# Test files to download
TEST_FILES = [
    "test_pathlib.py",
    "test_join.py",
    "test_join_posix.py",
    "test_join_windows.py",
    "test_copy.py",
    "test_read.py",
    "test_write.py",
]

# Support files to download
SUPPORT_FILES = [
    "support/__init__.py",
    "support/lexical_path.py",
    "support/local_path.py",
    "support/zip_path.py",
]

# Classes to skip (filesystem ops, internal APIs)
SKIP_CLASSES = {
    'Lexical',           # Internal API
    'UnsupportedOperationTest',  # Internal
    'LazyImportTest',    # Internal
    'CopyTest',          # Filesystem operations
    'ReadTest',          # Filesystem operations
    'WriteTest',         # Filesystem operations
    'ZipPathTest',       # Filesystem operations
    'LocalPathTest',     # Filesystem operations
}

# Exact class names to skip
SKIP_EXACT = {
    'PathJoinTest',         # From test_join.py - tests internal API
    'PurePathJoinTest',     # From test_join.py - tests internal API
    'PathTest',             # Filesystem operations
    'PosixPathTest',        # Filesystem operations
    'WindowsPathTest',      # Filesystem operations
    'PathSubclassTest',     # Filesystem operations
    'PathWalkTest',         # Filesystem operations
}


def download_file(url, dest):
    """Download a file."""
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    with urllib.request.urlopen(url) as r:
        content = r.read()
    with open(dest, 'wb') as f:
        f.write(content)


def setup_tests():
    """Download test files if needed."""
    os.makedirs(TEST_DIR, exist_ok=True)

    # Create package __init__.py
    init_path = os.path.join(TEST_DIR, "__init__.py")
    if not os.path.exists(init_path):
        with open(init_path, "w") as f:
            f.write("")

    # Download all files
    all_files = TEST_FILES + SUPPORT_FILES
    need_download = any(
        not os.path.exists(os.path.join(TEST_DIR, f)) for f in all_files
    )

    if need_download:
        print("Downloading CPython pathlib tests...")
        for filename in all_files:
            dest = os.path.join(TEST_DIR, filename)
            if not os.path.exists(dest):
                print(f"  {filename}")
                url = f"{CPYTHON_RAW}/{filename}"
                download_file(url, dest)


def setup_pathlib_patch():
    """Patch pathlib module to use snakepath."""
    import types

    # Create pathlib as a proper package
    pathlib_pkg = types.ModuleType('pathlib')
    pathlib_pkg.PurePath = snakepath.PurePath
    pathlib_pkg.PurePosixPath = snakepath.PurePosixPath
    pathlib_pkg.PureWindowsPath = snakepath.PureWindowsPath
    pathlib_pkg.Path = snakepath.Path
    pathlib_pkg.PosixPath = snakepath.PosixPath
    pathlib_pkg.WindowsPath = snakepath.WindowsPath
    sys.modules['pathlib'] = pathlib_pkg

    # Stub pathlib._os
    pathlib_os = types.ModuleType('pathlib._os')
    pathlib_os.vfspath = lambda p: str(p)
    sys.modules['pathlib._os'] = pathlib_os

    # Stub pathlib.types with minimal _JoinablePath
    pathlib_types = types.ModuleType('pathlib.types')
    class _JoinablePath:
        """Stub for pathlib.types._JoinablePath"""
        pass
    pathlib_types._JoinablePath = _JoinablePath
    pathlib_types._PathParser = object  # Stub
    sys.modules['pathlib.types'] = pathlib_types

    # Stub test.support (for test_pathlib.py, test_join.py)
    test_pkg = types.ModuleType('test')
    sys.modules['test'] = test_pkg

    test_support = types.ModuleType('test.support')
    test_support.is_emscripten = False
    test_support.is_wasi = False
    test_support.is_wasm32 = False
    test_support.cpython_only = lambda f: f
    test_support.infinite_recursion = lambda depth=None: __import__('contextlib').nullcontext()

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
    os_helper._longpath = lambda p: p
    # Skip decorators - return unittest.skip
    os_helper.skip_unless_xattr = unittest.skip("xattr not available")
    os_helper.skip_unless_working_chmod = unittest.skip("chmod not tested")
    os_helper.skip_if_dac_override = lambda f: f
    os_helper.skip_unless_hardlink = unittest.skip("hardlink not tested")
    class EnvironmentVarGuard:
        def __enter__(self): return {}
        def __exit__(self, *args): pass
    os_helper.EnvironmentVarGuard = EnvironmentVarGuard
    os_helper.change_cwd = lambda p: __import__('contextlib').nullcontext()
    os_helper.subst_drive = lambda p: __import__('contextlib').nullcontext('Z:')
    sys.modules['test.support.os_helper'] = os_helper

    # Stub test.support.threading_helper
    threading_helper = types.ModuleType('test.support.threading_helper')
    threading_helper.requires_working_threading = lambda: lambda f: f
    sys.modules['test.support.threading_helper'] = threading_helper


def should_skip_class(name):
    """Check if a test class should be skipped."""
    if name in SKIP_EXACT:
        return True
    for skip in SKIP_CLASSES:
        if skip in name:
            return True
    return False


def run_tests():
    """Run CPython tests against snakepath."""
    setup_tests()
    setup_pathlib_patch()

    # Add test dir to path
    sys.path.insert(0, os.path.dirname(TEST_DIR))

    print("\n" + "=" * 70)
    print("Running CPython pathlib PURE PATH tests against snakepath")
    print("=" * 70 + "\n")

    # Discover and filter tests
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    loaded_count = 0
    skipped_count = 0

    for filename in sorted(TEST_FILES):
        module_name = f"cpython_tests.{filename[:-3]}"

        try:
            __import__(module_name)
            module = sys.modules[module_name]

            for name in dir(module):
                obj = getattr(module, name)
                if isinstance(obj, type) and issubclass(obj, unittest.TestCase):
                    if obj is unittest.TestCase:
                        continue
                    if should_skip_class(name):
                        print(f"  SKIP: {name}")
                        skipped_count += 1
                        continue
                    print(f"  LOAD: {name}")
                    suite.addTests(loader.loadTestsFromTestCase(obj))
                    loaded_count += 1
        except Exception as e:
            print(f"  ERROR loading {filename}: {e}")

    print(f"\nLoaded {loaded_count} test classes, skipped {skipped_count}\n")

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
