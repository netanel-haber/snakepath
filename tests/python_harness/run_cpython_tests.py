#!/usr/bin/env python3
"""
Run CPython's pathlib test suite against snakepath.
Downloads test file from CPython's test directory.
"""

import sys
import unittest
import urllib.request
from pathlib import Path

# Add our module to path FIRST (snakepath package is in this directory)
THIS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(THIS_DIR))

import snakepath

TEST_DIR = THIS_DIR / "cpython_tests"
CPYTHON_BRANCH = "3.12"
CPYTHON_RAW = f"https://raw.githubusercontent.com/python/cpython/{CPYTHON_BRANCH}/Lib/test"
TEST_FILE = "test_pathlib.py"

# Tests expected to fail - just a set of (class_name, test_name) tuples
# If a test passes unexpectedly, it will be reported so we can remove it
EXPECTED_FAILURES = {
    # DeprecationWarning tests - C doesn't emit Python warnings
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

    # lstat (follow_symlinks=False) not implemented
    ("PathSubclassTest", "test_stat_no_follow_symlinks_nosymlink"),
    ("PathTest", "test_stat_no_follow_symlinks_nosymlink"),
    ("PosixPathTest", "test_stat_no_follow_symlinks_nosymlink"),

    # Cross-flavor comparison - C doesn't raise TypeError
    ("PurePathTest", "test_different_flavours_unordered"),

    # Turkish I case folding - requires Unicode NFKC
    ("PureWindowsPathTest", "test_eq"),

    # with_segments() not implemented
    ("PosixPathAsPureTest", "test_with_segments_common"),
    ("PurePathSubclassTest", "test_with_segments_common"),
    ("PurePathTest", "test_with_segments_common"),
    ("PurePosixPathTest", "test_with_segments_common"),
    ("PureWindowsPathTest", "test_with_segments_common"),
    ("PathSubclassTest", "test_with_segments"),
    ("PathTest", "test_with_segments"),
    ("PosixPathTest", "test_with_segments"),

    # with_suffix() tuple validation differs
    ("PosixPathAsPureTest", "test_with_suffix_common"),
    ("PurePathSubclassTest", "test_with_suffix_common"),
    ("PurePathTest", "test_with_suffix_common"),
    ("PurePosixPathTest", "test_with_suffix_common"),
    ("PureWindowsPathTest", "test_with_suffix_common"),

    # Pickling not implemented
    ("PurePathSubclassTest", "test_pickling_common"),

    # PathLike compatibility
    ("CompatiblePathTest", "test_truediv"),

    # Mock doesn't affect C getcwd
    ("PathSubclassTest", "test_absolute_common"),
    ("PathTest", "test_absolute_common"),
    ("PosixPathTest", "test_absolute_common"),

    # String interning - C doesn't intern
    ("PathSubclassTest", "test_parts_interning"),
    ("PathTest", "test_parts_interning"),
    ("PosixPathTest", "test_parts_interning"),

    # NotImplementedError for wrong platform
    ("PathTest", "test_unsupported_flavour"),

    # Filesystem I/O methods not implemented
    ("PathSubclassTest", "test_expanduser_common"),
    ("PathSubclassTest", "test_glob_above_recursion_limit"),
    ("PathSubclassTest", "test_glob_case_sensitive"),
    ("PathSubclassTest", "test_glob_common"),
    ("PathSubclassTest", "test_glob_dotdot"),
    ("PathSubclassTest", "test_glob_many_open_files"),
    ("PathSubclassTest", "test_group"),
    ("PathSubclassTest", "test_hardlink_to"),
    ("PathSubclassTest", "test_home"),
    ("PathSubclassTest", "test_is_block_device_false"),
    ("PathSubclassTest", "test_is_char_device_false"),
    ("PathSubclassTest", "test_is_char_device_true"),
    ("PathSubclassTest", "test_is_fifo_false"),
    ("PathSubclassTest", "test_is_fifo_true"),
    ("PathSubclassTest", "test_is_junction"),
    ("PathSubclassTest", "test_link_to_not_implemented"),
    ("PathSubclassTest", "test_is_mount"),
    ("PathSubclassTest", "test_is_socket_false"),
    ("PathSubclassTest", "test_is_socket_true"),
    ("PathSubclassTest", "test_is_symlink"),
    ("PathSubclassTest", "test_iterdir"),
    ("PathSubclassTest", "test_iterdir_nodir"),
    ("PathSubclassTest", "test_lstat_nosymlink"),
    ("PathSubclassTest", "test_mkdir"),
    ("PathSubclassTest", "test_mkdir_concurrent_parent_creation"),
    ("PathSubclassTest", "test_mkdir_exist_ok"),
    ("PathSubclassTest", "test_mkdir_exist_ok_root"),
    ("PathSubclassTest", "test_mkdir_exist_ok_with_parent"),
    ("PathSubclassTest", "test_mkdir_no_parents_file"),
    ("PathSubclassTest", "test_mkdir_parents"),
    ("PathSubclassTest", "test_mkdir_with_child_file"),
    ("PathSubclassTest", "test_mkdir_with_unknown_drive"),
    ("PathSubclassTest", "test_open_common"),
    ("PathSubclassTest", "test_owner"),
    ("PathSubclassTest", "test_read_write_bytes"),
    ("PathSubclassTest", "test_read_write_text"),
    ("PathSubclassTest", "test_rename"),
    ("PathSubclassTest", "test_replace"),
    ("PathSubclassTest", "test_resolve_nonexist_relative_issue38671"),
    ("PathSubclassTest", "test_rglob_common"),
    ("PathSubclassTest", "test_rmdir"),
    ("PathSubclassTest", "test_samefile"),
    ("PathSubclassTest", "test_touch_common"),
    ("PathSubclassTest", "test_touch_nochange"),
    ("PathSubclassTest", "test_unlink"),
    ("PathSubclassTest", "test_unlink_missing_ok"),
    ("PathSubclassTest", "test_with"),
    ("PathSubclassTest", "test_write_text_with_newlines"),

    ("PathTest", "test_expanduser_common"),
    ("PathTest", "test_glob_above_recursion_limit"),
    ("PathTest", "test_glob_case_sensitive"),
    ("PathTest", "test_glob_common"),
    ("PathTest", "test_glob_dotdot"),
    ("PathTest", "test_glob_empty_pattern"),
    ("PathTest", "test_glob_many_open_files"),
    ("PathTest", "test_group"),
    ("PathTest", "test_hardlink_to"),
    ("PathTest", "test_home"),
    ("PathTest", "test_is_block_device_false"),
    ("PathTest", "test_is_char_device_false"),
    ("PathTest", "test_is_char_device_true"),
    ("PathTest", "test_is_fifo_false"),
    ("PathTest", "test_is_fifo_true"),
    ("PathTest", "test_is_junction"),
    ("PathTest", "test_link_to_not_implemented"),
    ("PathTest", "test_is_mount"),
    ("PathTest", "test_is_socket_false"),
    ("PathTest", "test_is_socket_true"),
    ("PathTest", "test_is_symlink"),
    ("PathTest", "test_iterdir"),
    ("PathTest", "test_iterdir_nodir"),
    ("PathTest", "test_lstat_nosymlink"),
    ("PathTest", "test_mkdir"),
    ("PathTest", "test_mkdir_concurrent_parent_creation"),
    ("PathTest", "test_mkdir_exist_ok"),
    ("PathTest", "test_mkdir_exist_ok_root"),
    ("PathTest", "test_mkdir_exist_ok_with_parent"),
    ("PathTest", "test_mkdir_no_parents_file"),
    ("PathTest", "test_mkdir_parents"),
    ("PathTest", "test_mkdir_with_child_file"),
    ("PathTest", "test_mkdir_with_unknown_drive"),
    ("PathTest", "test_open_common"),
    ("PathTest", "test_owner"),
    ("PathTest", "test_read_write_bytes"),
    ("PathTest", "test_read_write_text"),
    ("PathTest", "test_rename"),
    ("PathTest", "test_replace"),
    ("PathTest", "test_resolve_nonexist_relative_issue38671"),
    ("PathTest", "test_rglob_common"),
    ("PathTest", "test_rmdir"),
    ("PathTest", "test_samefile"),
    ("PathTest", "test_touch_common"),
    ("PathTest", "test_touch_nochange"),
    ("PathTest", "test_unlink"),
    ("PathTest", "test_unlink_missing_ok"),
    ("PathTest", "test_with"),
    ("PathTest", "test_write_text_with_newlines"),

    ("PosixPathTest", "test_expanduser"),
    ("PosixPathTest", "test_expanduser_common"),
    ("PosixPathTest", "test_glob"),
    ("PosixPathTest", "test_glob_above_recursion_limit"),
    ("PosixPathTest", "test_glob_case_sensitive"),
    ("PosixPathTest", "test_glob_common"),
    ("PosixPathTest", "test_glob_dotdot"),
    ("PosixPathTest", "test_glob_many_open_files"),
    ("PosixPathTest", "test_group"),
    ("PosixPathTest", "test_hardlink_to"),
    ("PosixPathTest", "test_home"),
    ("PosixPathTest", "test_is_block_device_false"),
    ("PosixPathTest", "test_is_char_device_false"),
    ("PosixPathTest", "test_is_char_device_true"),
    ("PosixPathTest", "test_is_fifo_false"),
    ("PosixPathTest", "test_is_fifo_true"),
    ("PosixPathTest", "test_is_junction"),
    ("PosixPathTest", "test_link_to_not_implemented"),
    ("PosixPathTest", "test_is_mount"),
    ("PosixPathTest", "test_is_socket_false"),
    ("PosixPathTest", "test_is_socket_true"),
    ("PosixPathTest", "test_is_symlink"),
    ("PosixPathTest", "test_iterdir"),
    ("PosixPathTest", "test_iterdir_nodir"),
    ("PosixPathTest", "test_lstat_nosymlink"),
    ("PosixPathTest", "test_mkdir"),
    ("PosixPathTest", "test_mkdir_concurrent_parent_creation"),
    ("PosixPathTest", "test_mkdir_exist_ok"),
    ("PosixPathTest", "test_mkdir_exist_ok_root"),
    ("PosixPathTest", "test_mkdir_exist_ok_with_parent"),
    ("PosixPathTest", "test_mkdir_no_parents_file"),
    ("PosixPathTest", "test_mkdir_parents"),
    ("PosixPathTest", "test_mkdir_with_child_file"),
    ("PosixPathTest", "test_open_common"),
    ("PosixPathTest", "test_open_mode"),
    ("PosixPathTest", "test_owner"),
    ("PosixPathTest", "test_read_write_bytes"),
    ("PosixPathTest", "test_read_write_text"),
    ("PosixPathTest", "test_rename"),
    ("PosixPathTest", "test_replace"),
    ("PosixPathTest", "test_resolve_nonexist_relative_issue38671"),
    ("PosixPathTest", "test_resolve_root"),
    ("PosixPathTest", "test_rglob"),
    ("PosixPathTest", "test_rglob_common"),
    ("PosixPathTest", "test_rmdir"),
    ("PosixPathTest", "test_samefile"),
    ("PosixPathTest", "test_touch_common"),
    ("PosixPathTest", "test_touch_mode"),
    ("PosixPathTest", "test_touch_nochange"),
    ("PosixPathTest", "test_unlink"),
    ("PosixPathTest", "test_unlink_missing_ok"),
    ("PosixPathTest", "test_with"),
    ("PosixPathTest", "test_write_text_with_newlines"),

    # WalkTests
    ("WalkTests", "test_file_like_path"),
    ("WalkTests", "test_walk_above_recursion_limit"),
    ("WalkTests", "test_walk_bad_dir"),
    ("WalkTests", "test_walk_bottom_up"),
    ("WalkTests", "test_walk_many_open_files"),
    ("WalkTests", "test_walk_prune"),
    ("WalkTests", "test_walk_topdown"),

    # WindowsPathAsPureTest
    ("WindowsPathAsPureTest", "test_eq"),
    ("WindowsPathAsPureTest", "test_group"),
    ("WindowsPathAsPureTest", "test_owner"),
    ("WindowsPathAsPureTest", "test_with_segments_common"),
    ("WindowsPathAsPureTest", "test_with_suffix_common"),

    # WindowsPathTest
    ("WindowsPathTest", "test_absolute"),
    ("WindowsPathTest", "test_absolute_common"),
    ("WindowsPathTest", "test_expanduser"),
    ("WindowsPathTest", "test_expanduser_common"),
    ("WindowsPathTest", "test_glob"),
    ("WindowsPathTest", "test_glob_above_recursion_limit"),
    ("WindowsPathTest", "test_glob_case_sensitive"),
    ("WindowsPathTest", "test_glob_common"),
    ("WindowsPathTest", "test_glob_dotdot"),
    ("WindowsPathTest", "test_glob_many_open_files"),
    ("WindowsPathTest", "test_group"),
    ("WindowsPathTest", "test_hardlink_to"),
    ("WindowsPathTest", "test_home"),
    ("WindowsPathTest", "test_is_block_device_false"),
    ("WindowsPathTest", "test_is_char_device_false"),
    ("WindowsPathTest", "test_is_char_device_true"),
    ("WindowsPathTest", "test_is_fifo_false"),
    ("WindowsPathTest", "test_is_fifo_true"),
    ("WindowsPathTest", "test_is_junction"),
    ("WindowsPathTest", "test_is_mount"),
    ("WindowsPathTest", "test_is_socket_false"),
    ("WindowsPathTest", "test_is_socket_true"),
    ("WindowsPathTest", "test_is_symlink"),
    ("WindowsPathTest", "test_iterdir"),
    ("WindowsPathTest", "test_iterdir_nodir"),
    ("WindowsPathTest", "test_lstat_nosymlink"),
    ("WindowsPathTest", "test_mkdir"),
    ("WindowsPathTest", "test_mkdir_concurrent_parent_creation"),
    ("WindowsPathTest", "test_mkdir_exist_ok"),
    ("WindowsPathTest", "test_mkdir_exist_ok_root"),
    ("WindowsPathTest", "test_mkdir_exist_ok_with_parent"),
    ("WindowsPathTest", "test_mkdir_no_parents_file"),
    ("WindowsPathTest", "test_mkdir_parents"),
    ("WindowsPathTest", "test_mkdir_with_child_file"),
    ("WindowsPathTest", "test_mkdir_with_unknown_drive"),
    ("WindowsPathTest", "test_open_common"),
    ("WindowsPathTest", "test_owner"),
    ("WindowsPathTest", "test_parts_interning"),
    ("WindowsPathTest", "test_passing_kwargs_deprecated"),
    ("WindowsPathTest", "test_read_write_bytes"),
    ("WindowsPathTest", "test_read_write_text"),
    ("WindowsPathTest", "test_rename"),
    ("WindowsPathTest", "test_replace"),
    ("WindowsPathTest", "test_resolve_nonexist_relative_issue38671"),
    ("WindowsPathTest", "test_rglob"),
    ("WindowsPathTest", "test_rglob_common"),
    ("WindowsPathTest", "test_rmdir"),
    ("WindowsPathTest", "test_samefile"),
    ("WindowsPathTest", "test_touch_common"),
    ("WindowsPathTest", "test_touch_nochange"),
    ("WindowsPathTest", "test_unlink"),
    ("WindowsPathTest", "test_unlink_missing_ok"),
    ("WindowsPathTest", "test_with"),
    ("WindowsPathTest", "test_with_segments"),
    ("WindowsPathTest", "test_write_text_with_newlines"),
}


def download_file(url, dest):
    dest.parent.mkdir(parents=True, exist_ok=True)
    with urllib.request.urlopen(url) as r:
        dest.write_bytes(r.read())


def setup_tests():
    TEST_DIR.mkdir(exist_ok=True)
    (TEST_DIR / "__init__.py").touch(exist_ok=True)
    dest = TEST_DIR / TEST_FILE
    if not dest.exists():
        print(f"Downloading {TEST_FILE}...")
        download_file(f"{CPYTHON_RAW}/{TEST_FILE}", dest)


def setup_pathlib_patch():
    import types
    import tempfile
    import shutil
    import contextlib

    pathlib_pkg = types.ModuleType('pathlib')
    for name in ['PurePath', 'PurePosixPath', 'PureWindowsPath', 'Path', 'PosixPath', 'WindowsPath']:
        setattr(pathlib_pkg, name, getattr(snakepath, name))
    sys.modules['pathlib'] = pathlib_pkg

    test_pkg = types.ModuleType('test')
    sys.modules['test'] = test_pkg

    test_support = types.ModuleType('test.support')
    test_support.is_emscripten = test_support.is_wasi = test_support.is_android = False
    test_support.verbose = False
    test_support.cpython_only = lambda f: f

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

    os_helper = types.ModuleType('test.support.os_helper')
    os_helper.TESTFN = str(Path(tempfile.gettempdir()) / 'test_pathlib_tmp')
    os_helper.FS_NONASCII = '\xe9'
    class FakePath:
        def __init__(self, path): self.path = path
        def __fspath__(self): return self.path
    os_helper.FakePath = FakePath
    os_helper.can_symlink = lambda: False
    os_helper.rmtree = shutil.rmtree
    os_helper.skip_unless_xattr = unittest.skip("xattr not available")
    os_helper.skip_unless_working_chmod = unittest.skip("chmod not tested")
    os_helper.skip_unless_symlink = unittest.skip("symlink not tested")
    os_helper.skip_if_dac_override = lambda f: f
    class EnvironmentVarGuard:
        def __enter__(self): return {}
        def __exit__(self, *args): pass
    os_helper.EnvironmentVarGuard = EnvironmentVarGuard
    sys.modules['test.support.os_helper'] = os_helper


class TestResult(unittest.TextTestResult):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.expected_failures_that_failed = []
        self.unexpected_successes = []

    def addSuccess(self, test):
        key = (test.__class__.__name__, test._testMethodName)
        if key in EXPECTED_FAILURES:
            self.unexpected_successes.append(test)
            if self.dots:
                self.stream.write('u')
                self.stream.flush()
        else:
            super().addSuccess(test)

    def addError(self, test, err):
        key = (test.__class__.__name__, test._testMethodName)
        if key in EXPECTED_FAILURES:
            self.expected_failures_that_failed.append(test)
            if self.dots:
                self.stream.write('x')
                self.stream.flush()
        else:
            super().addError(test, err)

    def addFailure(self, test, err):
        key = (test.__class__.__name__, test._testMethodName)
        if key in EXPECTED_FAILURES:
            self.expected_failures_that_failed.append(test)
            if self.dots:
                self.stream.write('x')
                self.stream.flush()
        else:
            super().addFailure(test, err)


class TestRunner(unittest.TextTestRunner):
    resultclass = TestResult


def run_tests():
    setup_tests()
    setup_pathlib_patch()
    sys.path.insert(0, str(TEST_DIR.parent))

    print("\nRunning CPython pathlib tests against snakepath\n")

    loader = unittest.TestLoader()
    suite = unittest.TestSuite()

    module_name = f"cpython_tests.{TEST_FILE[:-3]}"
    try:
        __import__(module_name)
        module = sys.modules[module_name]
        for name in dir(module):
            obj = getattr(module, name)
            if isinstance(obj, type) and issubclass(obj, unittest.TestCase) and obj is not unittest.TestCase:
                for method_name in loader.getTestCaseNames(obj):
                    suite.addTest(obj(method_name))
    except Exception as e:
        print(f"ERROR loading tests: {e}")
        import traceback
        traceback.print_exc()
        return 1

    runner = TestRunner(verbosity=1)
    result = runner.run(suite)

    print(f"\nRan {result.testsRun} tests")
    print(f"  Expected failures: {len(result.expected_failures_that_failed)}")
    print(f"  Skipped: {len(result.skipped)}")

    failed = False
    if result.unexpected_successes:
        failed = True
        print(f"\nUNEXPECTED SUCCESSES ({len(result.unexpected_successes)}) - remove from EXPECTED_FAILURES:")
        for test in result.unexpected_successes:
            print(f"  {test.__class__.__name__}.{test._testMethodName}")

    if result.failures or result.errors:
        failed = True
        print(f"\nUNEXPECTED FAILURES ({len(result.failures)}) / ERRORS ({len(result.errors)}):")
        for test, _ in result.failures + result.errors:
            print(f"  {test.__class__.__name__}.{test._testMethodName}")

    if not failed:
        print("\nSUCCESS")
        return 0
    return 1


if __name__ == "__main__":
    sys.exit(run_tests())
