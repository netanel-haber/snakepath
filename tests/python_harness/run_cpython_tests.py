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
# These document known limitations and unimplemented features
EXPECTED_FAILURES = {
    # =========================================================================
    # Python's warnings.warn() system does not exist in C
    # These tests check that DeprecationWarning is emitted for multi-arg calls
    # =========================================================================
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

    # =========================================================================
    # Cross-flavor ordering comparison requires Python-level type checking
    # C library doesn't implement TypeError for comparing PosixPath < WindowsPath
    # =========================================================================
    ("PurePathTest", "test_different_flavours_unordered"): "TypeError",

    # =========================================================================
    # Turkish I case folding requires Unicode NFKC normalization
    # C library uses simple ASCII case folding, not full Unicode
    # =========================================================================
    ("PureWindowsPathTest", "test_eq"): "PureWindowsPath('İ')",

    # =========================================================================
    # with_segments() not implemented - requires Python subclass cooperation
    # Tests expect subclass attributes to be preserved across path operations
    # =========================================================================
    ("PosixPathAsPureTest", "test_with_segments_common"): "has no attribute 'session_id'",
    ("PurePathSubclassTest", "test_with_segments_common"): "has no attribute 'session_id'",
    ("PurePathTest", "test_with_segments_common"): "has no attribute 'session_id'",
    ("PurePosixPathTest", "test_with_segments_common"): "has no attribute 'session_id'",
    ("PureWindowsPathTest", "test_with_segments_common"): "has no attribute 'session_id'",

    # =========================================================================
    # with_suffix() tuple argument validation differs from Python
    # C library raises TypeError, Python raises ValueError for tuple suffix
    # =========================================================================
    ("PosixPathAsPureTest", "test_with_suffix_common"): "expected str, not tuple",
    ("PurePathSubclassTest", "test_with_suffix_common"): "expected str, not tuple",
    ("PurePathTest", "test_with_suffix_common"): "expected str, not tuple",
    ("PurePosixPathTest", "test_with_suffix_common"): "expected str, not tuple",
    ("PureWindowsPathTest", "test_with_suffix_common"): "expected str, not tuple",

    # =========================================================================
    # Pickling not implemented - would require __getstate__/__setstate__
    # C-backed objects with __slots__ cannot be pickled without explicit support
    # =========================================================================
    ("PurePathSubclassTest", "test_pickling_common"): "cannot be pickled",

    # =========================================================================
    # CompatiblePathTest.test_truediv - joinpath doesn't accept arbitrary PathLike
    # os.fspath() rejects objects without __fspath__ returning str/bytes
    # =========================================================================
    ("CompatiblePathTest", "test_truediv"): "expected str, bytes or os.PathLike object, not CompatPath",

    # =========================================================================
    # PathSubclassTest - concrete Path tests require filesystem I/O (NOT_PLANNED)
    # test_absolute_common requires absolute() method which needs getcwd()
    # All other tests fail in setUp() because test directory already exists
    # =========================================================================
    ("PathSubclassTest", "test_absolute_common"): "has no attribute 'absolute'",
    ("PathSubclassTest", "test_cwd"): "exists",
    ("PathSubclassTest", "test_empty_path"): "exists",
    ("PathSubclassTest", "test_exists"): "exists",
    ("PathSubclassTest", "test_expanduser_common"): "exists",
    ("PathSubclassTest", "test_glob_above_recursion_limit"): "exists",
    ("PathSubclassTest", "test_glob_case_sensitive"): "exists",
    ("PathSubclassTest", "test_glob_common"): "exists",
    ("PathSubclassTest", "test_glob_dotdot"): "exists",
    ("PathSubclassTest", "test_glob_many_open_files"): "exists",
    ("PathSubclassTest", "test_group"): "exists",
    ("PathSubclassTest", "test_hardlink_to"): "exists",
    ("PathSubclassTest", "test_home"): "exists",
    ("PathSubclassTest", "test_is_block_device_false"): "exists",
    ("PathSubclassTest", "test_is_char_device_false"): "exists",
    ("PathSubclassTest", "test_is_char_device_true"): "exists",
    ("PathSubclassTest", "test_is_dir"): "exists",
    ("PathSubclassTest", "test_is_fifo_false"): "exists",
    ("PathSubclassTest", "test_is_fifo_true"): "exists",
    ("PathSubclassTest", "test_is_file"): "exists",
    ("PathSubclassTest", "test_is_junction"): "exists",
    ("PathSubclassTest", "test_is_mount"): "exists",
    ("PathSubclassTest", "test_is_socket_false"): "exists",
    ("PathSubclassTest", "test_is_socket_true"): "exists",
    ("PathSubclassTest", "test_is_symlink"): "exists",
    ("PathSubclassTest", "test_iterdir"): "exists",
    ("PathSubclassTest", "test_iterdir_nodir"): "exists",
    ("PathSubclassTest", "test_lstat_nosymlink"): "exists",
    ("PathSubclassTest", "test_mkdir"): "exists",
    ("PathSubclassTest", "test_mkdir_concurrent_parent_creation"): "exists",
    ("PathSubclassTest", "test_mkdir_exist_ok"): "exists",
    ("PathSubclassTest", "test_mkdir_exist_ok_root"): "exists",
    ("PathSubclassTest", "test_mkdir_exist_ok_with_parent"): "exists",
    ("PathSubclassTest", "test_mkdir_no_parents_file"): "exists",
    ("PathSubclassTest", "test_mkdir_parents"): "exists",
    ("PathSubclassTest", "test_mkdir_with_child_file"): "exists",
    ("PathSubclassTest", "test_open_common"): "exists",
    ("PathSubclassTest", "test_owner"): "exists",
    ("PathSubclassTest", "test_parts_interning"): "exists",
    ("PathSubclassTest", "test_passing_kwargs_deprecated"): "exists",
    ("PathSubclassTest", "test_pickling_common"): "exists",
    ("PathSubclassTest", "test_read_write_bytes"): "exists",
    ("PathSubclassTest", "test_read_write_text"): "exists",
    ("PathSubclassTest", "test_rename"): "exists",
    ("PathSubclassTest", "test_replace"): "exists",
    ("PathSubclassTest", "test_resolve_nonexist_relative_issue38671"): "exists",
    ("PathSubclassTest", "test_rglob_common"): "exists",
    ("PathSubclassTest", "test_rmdir"): "exists",
    ("PathSubclassTest", "test_samefile"): "exists",
    ("PathSubclassTest", "test_stat_no_follow_symlinks_nosymlink"): "exists",
    ("PathSubclassTest", "test_touch_common"): "exists",
    ("PathSubclassTest", "test_touch_nochange"): "exists",
    ("PathSubclassTest", "test_unlink"): "exists",
    ("PathSubclassTest", "test_unlink_missing_ok"): "exists",
    ("PathSubclassTest", "test_with"): "exists",
    ("PathSubclassTest", "test_with_segments"): "exists",
    ("PathSubclassTest", "test_write_text_with_newlines"): "exists",

    # =========================================================================
    # PathTest - concrete Path tests require filesystem I/O (NOT_PLANNED)
    # =========================================================================
    ("PathTest", "test_absolute_common"): "exists",
    ("PathTest", "test_concrete_class"): "exists",
    ("PathTest", "test_cwd"): "exists",
    ("PathTest", "test_empty_path"): "exists",
    ("PathTest", "test_exists"): "exists",
    ("PathTest", "test_expanduser_common"): "exists",
    ("PathTest", "test_glob_above_recursion_limit"): "exists",
    ("PathTest", "test_glob_case_sensitive"): "exists",
    ("PathTest", "test_glob_common"): "exists",
    ("PathTest", "test_glob_dotdot"): "exists",
    ("PathTest", "test_glob_empty_pattern"): "exists",
    ("PathTest", "test_glob_many_open_files"): "exists",
    ("PathTest", "test_group"): "exists",
    ("PathTest", "test_hardlink_to"): "exists",
    ("PathTest", "test_home"): "exists",
    ("PathTest", "test_is_block_device_false"): "exists",
    ("PathTest", "test_is_char_device_false"): "exists",
    ("PathTest", "test_is_char_device_true"): "exists",
    ("PathTest", "test_is_dir"): "exists",
    ("PathTest", "test_is_fifo_false"): "exists",
    ("PathTest", "test_is_fifo_true"): "exists",
    ("PathTest", "test_is_file"): "exists",
    ("PathTest", "test_is_junction"): "exists",
    ("PathTest", "test_is_mount"): "exists",
    ("PathTest", "test_is_socket_false"): "exists",
    ("PathTest", "test_is_socket_true"): "exists",
    ("PathTest", "test_is_symlink"): "exists",
    ("PathTest", "test_iterdir"): "exists",
    ("PathTest", "test_iterdir_nodir"): "exists",
    ("PathTest", "test_lstat_nosymlink"): "exists",
    ("PathTest", "test_mkdir"): "exists",
    ("PathTest", "test_mkdir_concurrent_parent_creation"): "exists",
    ("PathTest", "test_mkdir_exist_ok"): "exists",
    ("PathTest", "test_mkdir_exist_ok_root"): "exists",
    ("PathTest", "test_mkdir_exist_ok_with_parent"): "exists",
    ("PathTest", "test_mkdir_no_parents_file"): "exists",
    ("PathTest", "test_mkdir_parents"): "exists",
    ("PathTest", "test_mkdir_with_child_file"): "exists",
    ("PathTest", "test_open_common"): "exists",
    ("PathTest", "test_owner"): "exists",
    ("PathTest", "test_parts_interning"): "exists",
    ("PathTest", "test_passing_kwargs_deprecated"): "exists",
    ("PathTest", "test_pickling_common"): "exists",
    ("PathTest", "test_read_write_bytes"): "exists",
    ("PathTest", "test_read_write_text"): "exists",
    ("PathTest", "test_rename"): "exists",
    ("PathTest", "test_replace"): "exists",
    ("PathTest", "test_resolve_nonexist_relative_issue38671"): "exists",
    ("PathTest", "test_rglob_common"): "exists",
    ("PathTest", "test_rmdir"): "exists",
    ("PathTest", "test_samefile"): "exists",
    ("PathTest", "test_stat_no_follow_symlinks_nosymlink"): "exists",
    ("PathTest", "test_touch_common"): "exists",
    ("PathTest", "test_touch_nochange"): "exists",
    ("PathTest", "test_unlink"): "exists",
    ("PathTest", "test_unlink_missing_ok"): "exists",
    ("PathTest", "test_unsupported_flavour"): "exists",
    ("PathTest", "test_with"): "exists",
    ("PathTest", "test_with_segments"): "exists",
    ("PathTest", "test_write_text_with_newlines"): "exists",

    # =========================================================================
    # PosixPathTest - concrete Path tests require filesystem I/O (NOT_PLANNED)
    # =========================================================================
    ("PosixPathTest", "test_absolute"): "exists",
    ("PosixPathTest", "test_absolute_common"): "exists",
    ("PosixPathTest", "test_cwd"): "exists",
    ("PosixPathTest", "test_empty_path"): "exists",
    ("PosixPathTest", "test_exists"): "exists",
    ("PosixPathTest", "test_expanduser"): "exists",
    ("PosixPathTest", "test_expanduser_common"): "exists",
    ("PosixPathTest", "test_glob"): "exists",
    ("PosixPathTest", "test_glob_above_recursion_limit"): "exists",
    ("PosixPathTest", "test_glob_case_sensitive"): "exists",
    ("PosixPathTest", "test_glob_common"): "exists",
    ("PosixPathTest", "test_glob_dotdot"): "exists",
    ("PosixPathTest", "test_glob_many_open_files"): "exists",
    ("PosixPathTest", "test_group"): "exists",
    ("PosixPathTest", "test_hardlink_to"): "exists",
    ("PosixPathTest", "test_home"): "exists",
    ("PosixPathTest", "test_is_block_device_false"): "exists",
    ("PosixPathTest", "test_is_char_device_false"): "exists",
    ("PosixPathTest", "test_is_char_device_true"): "exists",
    ("PosixPathTest", "test_is_dir"): "exists",
    ("PosixPathTest", "test_is_fifo_false"): "exists",
    ("PosixPathTest", "test_is_fifo_true"): "exists",
    ("PosixPathTest", "test_is_file"): "exists",
    ("PosixPathTest", "test_is_junction"): "exists",
    ("PosixPathTest", "test_is_mount"): "exists",
    ("PosixPathTest", "test_is_socket_false"): "exists",
    ("PosixPathTest", "test_is_socket_true"): "exists",
    ("PosixPathTest", "test_is_symlink"): "exists",
    ("PosixPathTest", "test_iterdir"): "exists",
    ("PosixPathTest", "test_iterdir_nodir"): "exists",
    ("PosixPathTest", "test_lstat_nosymlink"): "exists",
    ("PosixPathTest", "test_mkdir"): "exists",
    ("PosixPathTest", "test_mkdir_concurrent_parent_creation"): "exists",
    ("PosixPathTest", "test_mkdir_exist_ok"): "exists",
    ("PosixPathTest", "test_mkdir_exist_ok_root"): "exists",
    ("PosixPathTest", "test_mkdir_exist_ok_with_parent"): "exists",
    ("PosixPathTest", "test_mkdir_no_parents_file"): "exists",
    ("PosixPathTest", "test_mkdir_parents"): "exists",
    ("PosixPathTest", "test_mkdir_with_child_file"): "exists",
    ("PosixPathTest", "test_open_common"): "exists",
    ("PosixPathTest", "test_open_mode"): "exists",
    ("PosixPathTest", "test_owner"): "exists",
    ("PosixPathTest", "test_parts_interning"): "exists",
    ("PosixPathTest", "test_passing_kwargs_deprecated"): "exists",
    ("PosixPathTest", "test_pickling_common"): "exists",
    ("PosixPathTest", "test_read_write_bytes"): "exists",
    ("PosixPathTest", "test_read_write_text"): "exists",
    ("PosixPathTest", "test_rename"): "exists",
    ("PosixPathTest", "test_replace"): "exists",
    ("PosixPathTest", "test_resolve_nonexist_relative_issue38671"): "exists",
    ("PosixPathTest", "test_resolve_root"): "exists",
    ("PosixPathTest", "test_rglob"): "exists",
    ("PosixPathTest", "test_rglob_common"): "exists",
    ("PosixPathTest", "test_rmdir"): "exists",
    ("PosixPathTest", "test_samefile"): "exists",
    ("PosixPathTest", "test_stat_no_follow_symlinks_nosymlink"): "exists",
    ("PosixPathTest", "test_touch_common"): "exists",
    ("PosixPathTest", "test_touch_mode"): "exists",
    ("PosixPathTest", "test_touch_nochange"): "exists",
    ("PosixPathTest", "test_unlink"): "exists",
    ("PosixPathTest", "test_unlink_missing_ok"): "exists",
    ("PosixPathTest", "test_with"): "exists",
    ("PosixPathTest", "test_with_segments"): "exists",
    ("PosixPathTest", "test_write_text_with_newlines"): "exists",

    # =========================================================================
    # WalkTests - walk() not implemented, requires filesystem I/O (NOT_PLANNED)
    # =========================================================================
    ("WalkTests", "test_file_like_path"): "has no attribute 'walk'",
    ("WalkTests", "test_walk_above_recursion_limit"): "exists",
    ("WalkTests", "test_walk_bad_dir"): "exists",
    ("WalkTests", "test_walk_bottom_up"): "exists",
    ("WalkTests", "test_walk_many_open_files"): "exists",
    ("WalkTests", "test_walk_prune"): "exists",
    ("WalkTests", "test_walk_topdown"): "exists",

    # =========================================================================
    # WindowsPathAsPureTest - runs only on Windows, tests pure path operations
    # group/owner require getgrgid/getpwuid which aren't implemented
    # with_segments/with_suffix have same issues as other pure path tests
    # =========================================================================
    ("WindowsPathAsPureTest", "test_group"): "has no attribute 'group'",
    ("WindowsPathAsPureTest", "test_owner"): "has no attribute 'owner'",
    ("WindowsPathAsPureTest", "test_with_segments_common"): "has no attribute 'session_id'",
    ("WindowsPathAsPureTest", "test_with_suffix_common"): "expected str, not tuple",

    # =========================================================================
    # WindowsPathTest - concrete Path tests on Windows, require filesystem I/O
    # =========================================================================
    ("WindowsPathTest", "test_absolute"): "exists",
    ("WindowsPathTest", "test_absolute_common"): "exists",
    ("WindowsPathTest", "test_cwd"): "exists",
    ("WindowsPathTest", "test_empty_path"): "exists",
    ("WindowsPathTest", "test_exists"): "exists",
    ("WindowsPathTest", "test_expanduser"): "exists",
    ("WindowsPathTest", "test_expanduser_common"): "exists",
    ("WindowsPathTest", "test_glob"): "exists",
    ("WindowsPathTest", "test_glob_above_recursion_limit"): "exists",
    ("WindowsPathTest", "test_glob_case_sensitive"): "exists",
    ("WindowsPathTest", "test_glob_common"): "exists",
    ("WindowsPathTest", "test_glob_dotdot"): "exists",
    ("WindowsPathTest", "test_glob_many_open_files"): "exists",
    ("WindowsPathTest", "test_group"): "exists",
    ("WindowsPathTest", "test_hardlink_to"): "exists",
    ("WindowsPathTest", "test_home"): "exists",
    ("WindowsPathTest", "test_is_block_device_false"): "exists",
    ("WindowsPathTest", "test_is_char_device_false"): "exists",
    ("WindowsPathTest", "test_is_char_device_true"): "exists",
    ("WindowsPathTest", "test_is_dir"): "exists",
    ("WindowsPathTest", "test_is_fifo_false"): "exists",
    ("WindowsPathTest", "test_is_fifo_true"): "exists",
    ("WindowsPathTest", "test_is_file"): "exists",
    ("WindowsPathTest", "test_is_junction"): "exists",
    ("WindowsPathTest", "test_is_mount"): "exists",
    ("WindowsPathTest", "test_is_socket_false"): "exists",
    ("WindowsPathTest", "test_is_socket_true"): "exists",
    ("WindowsPathTest", "test_is_symlink"): "exists",
    ("WindowsPathTest", "test_iterdir"): "exists",
    ("WindowsPathTest", "test_iterdir_nodir"): "exists",
    ("WindowsPathTest", "test_lstat_nosymlink"): "exists",
    ("WindowsPathTest", "test_mkdir"): "exists",
    ("WindowsPathTest", "test_mkdir_concurrent_parent_creation"): "exists",
    ("WindowsPathTest", "test_mkdir_exist_ok"): "exists",
    ("WindowsPathTest", "test_mkdir_exist_ok_root"): "exists",
    ("WindowsPathTest", "test_mkdir_exist_ok_with_parent"): "exists",
    ("WindowsPathTest", "test_mkdir_no_parents_file"): "exists",
    ("WindowsPathTest", "test_mkdir_parents"): "exists",
    ("WindowsPathTest", "test_mkdir_with_child_file"): "exists",
    ("WindowsPathTest", "test_mkdir_with_unknown_drive"): "exists",
    ("WindowsPathTest", "test_open_common"): "exists",
    ("WindowsPathTest", "test_owner"): "exists",
    ("WindowsPathTest", "test_parts_interning"): "exists",
    ("WindowsPathTest", "test_passing_kwargs_deprecated"): "exists",
    ("WindowsPathTest", "test_pickling_common"): "exists",
    ("WindowsPathTest", "test_read_write_bytes"): "exists",
    ("WindowsPathTest", "test_read_write_text"): "exists",
    ("WindowsPathTest", "test_rename"): "exists",
    ("WindowsPathTest", "test_replace"): "exists",
    ("WindowsPathTest", "test_resolve_nonexist_relative_issue38671"): "exists",
    ("WindowsPathTest", "test_rglob"): "exists",
    ("WindowsPathTest", "test_rglob_common"): "exists",
    ("WindowsPathTest", "test_rmdir"): "exists",
    ("WindowsPathTest", "test_samefile"): "exists",
    ("WindowsPathTest", "test_stat_no_follow_symlinks_nosymlink"): "exists",
    ("WindowsPathTest", "test_touch_common"): "exists",
    ("WindowsPathTest", "test_touch_nochange"): "exists",
    ("WindowsPathTest", "test_unlink"): "exists",
    ("WindowsPathTest", "test_unlink_missing_ok"): "exists",
    ("WindowsPathTest", "test_with"): "exists",
    ("WindowsPathTest", "test_with_segments"): "exists",
    ("WindowsPathTest", "test_write_text_with_newlines"): "exists",

    # =========================================================================
    # PathTest Windows-specific tests
    # =========================================================================
    ("PathTest", "test_mkdir_with_unknown_drive"): "exists",
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
        print(f"Expected failures (documented in EXPECTED_FAILURES): {expected_failure_count}")

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
