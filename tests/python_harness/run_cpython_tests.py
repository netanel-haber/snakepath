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
    # test_absolute_common - uses mock.patch("os.getcwd") which doesn't affect C
    # Our C library calls getcwd() directly, bypassing Python's mock
    # =========================================================================
    ("PathSubclassTest", "test_absolute_common"): "!=",
    ("PathTest", "test_absolute_common"): "!=",
    ("PosixPathTest", "test_absolute_common"): "!=",

    # =========================================================================
    # test_parts_interning - Python interns string parts, C doesn't
    # =========================================================================
    ("PathSubclassTest", "test_parts_interning"): "is not",
    ("PathTest", "test_parts_interning"): "is not",
    ("PosixPathTest", "test_parts_interning"): "is not",

    # =========================================================================
    # test_passing_kwargs_deprecated - DeprecationWarning for kwargs not in C
    # =========================================================================
    ("PathSubclassTest", "test_passing_kwargs_deprecated"): "DeprecationWarning not triggered",
    ("PathTest", "test_passing_kwargs_deprecated"): "DeprecationWarning not triggered",
    ("PosixPathTest", "test_passing_kwargs_deprecated"): "DeprecationWarning not triggered",

    # =========================================================================
    # test_unsupported_flavour - NotImplementedError for wrong platform
    # =========================================================================
    ("PathTest", "test_unsupported_flavour"): "NotImplementedError",

    # =========================================================================
    # PathSubclassTest - concrete Path tests require filesystem I/O (NOT_PLANNED)
    # =========================================================================
    ("PathSubclassTest", "test_empty_path"): "has no attribute 'stat'",
    ("PathSubclassTest", "test_expanduser_common"): "has no attribute 'expanduser'",
    ("PathSubclassTest", "test_glob_above_recursion_limit"): "has no attribute 'mkdir'",
    ("PathSubclassTest", "test_glob_case_sensitive"): "has no attribute 'glob'",
    ("PathSubclassTest", "test_glob_common"): "has no attribute 'glob'",
    ("PathSubclassTest", "test_glob_dotdot"): "has no attribute 'glob'",
    ("PathSubclassTest", "test_glob_many_open_files"): "has no attribute 'mkdir'",
    ("PathSubclassTest", "test_group"): "has no attribute 'stat'",
    ("PathSubclassTest", "test_hardlink_to"): "has no attribute 'stat'",
    ("PathSubclassTest", "test_home"): "has no attribute 'home'",
    ("PathSubclassTest", "test_is_block_device_false"): "has no attribute 'is_block_device'",
    ("PathSubclassTest", "test_is_char_device_false"): "has no attribute 'is_char_device'",
    ("PathSubclassTest", "test_is_char_device_true"): "has no attribute 'is_char_device'",
    ("PathSubclassTest", "test_is_fifo_false"): "has no attribute 'is_fifo'",
    ("PathSubclassTest", "test_is_fifo_true"): "has no attribute 'is_fifo'",
    ("PathSubclassTest", "test_is_junction"): "has no attribute 'is_junction'",
    ("PathSubclassTest", "test_link_to_not_implemented"): "has no attribute 'hardlink_to'",
    ("PathSubclassTest", "test_is_mount"): "has no attribute 'is_mount'",
    ("PathSubclassTest", "test_is_socket_false"): "has no attribute 'is_socket'",
    ("PathSubclassTest", "test_is_socket_true"): "has no attribute 'is_socket'",
    ("PathSubclassTest", "test_is_symlink"): "has no attribute 'is_symlink'",
    ("PathSubclassTest", "test_iterdir"): "has no attribute 'iterdir'",
    ("PathSubclassTest", "test_iterdir_nodir"): "has no attribute 'iterdir'",
    ("PathSubclassTest", "test_lstat_nosymlink"): "has no attribute 'stat'",
    ("PathSubclassTest", "test_mkdir"): "has no attribute 'mkdir'",
    ("PathSubclassTest", "test_mkdir_concurrent_parent_creation"): "has no attribute 'mkdir'",
    ("PathSubclassTest", "test_mkdir_exist_ok"): "has no attribute 'stat'",
    ("PathSubclassTest", "test_mkdir_exist_ok_root"): "has no attribute 'resolve'",
    ("PathSubclassTest", "test_mkdir_exist_ok_with_parent"): "has no attribute 'mkdir'",
    ("PathSubclassTest", "test_mkdir_no_parents_file"): "has no attribute 'mkdir'",
    ("PathSubclassTest", "test_mkdir_parents"): "has no attribute 'mkdir'",
    ("PathSubclassTest", "test_mkdir_with_child_file"): "has no attribute 'mkdir'",
    ("PathSubclassTest", "test_mkdir_with_unknown_drive"): "has no attribute 'mkdir'",
    ("PathSubclassTest", "test_open_common"): "has no attribute 'open'",
    ("PathSubclassTest", "test_owner"): "has no attribute 'stat'",
    ("PathSubclassTest", "test_pickling_common"): "has no attribute 'stat'",
    ("PathSubclassTest", "test_read_write_bytes"): "has no attribute 'write_bytes'",
    ("PathSubclassTest", "test_read_write_text"): "has no attribute 'write_text'",
    ("PathSubclassTest", "test_rename"): "has no attribute 'stat'",
    ("PathSubclassTest", "test_replace"): "has no attribute 'stat'",
    ("PathSubclassTest", "test_resolve_nonexist_relative_issue38671"): "has no attribute 'resolve'",
    ("PathSubclassTest", "test_rglob_common"): "has no attribute 'rglob'",
    ("PathSubclassTest", "test_rmdir"): "has no attribute 'iterdir'",
    ("PathSubclassTest", "test_samefile"): "has no attribute 'samefile'",
    ("PathSubclassTest", "test_stat_no_follow_symlinks_nosymlink"): "has no attribute 'stat'",
    ("PathSubclassTest", "test_touch_common"): "has no attribute 'touch'",
    ("PathSubclassTest", "test_touch_nochange"): "has no attribute 'touch'",
    ("PathSubclassTest", "test_unlink"): "has no attribute 'unlink'",
    ("PathSubclassTest", "test_unlink_missing_ok"): "has no attribute 'unlink'",
    ("PathSubclassTest", "test_with"): "has no attribute 'iterdir'",
    ("PathSubclassTest", "test_with_segments"): "has no attribute 'session_id'",
    ("PathSubclassTest", "test_write_text_with_newlines"): "has no attribute 'write_text'",

    # =========================================================================
    # PathTest - concrete Path tests require filesystem I/O (NOT_PLANNED)
    # =========================================================================
    ("PathTest", "test_empty_path"): "has no attribute 'stat'",
    ("PathTest", "test_expanduser_common"): "has no attribute 'expanduser'",
    ("PathTest", "test_glob_above_recursion_limit"): "has no attribute 'mkdir'",
    ("PathTest", "test_glob_case_sensitive"): "has no attribute 'glob'",
    ("PathTest", "test_glob_common"): "has no attribute 'glob'",
    ("PathTest", "test_glob_dotdot"): "has no attribute 'glob'",
    ("PathTest", "test_glob_empty_pattern"): "has no attribute 'glob'",
    ("PathTest", "test_glob_many_open_files"): "has no attribute 'mkdir'",
    ("PathTest", "test_group"): "has no attribute 'stat'",
    ("PathTest", "test_hardlink_to"): "has no attribute 'stat'",
    ("PathTest", "test_home"): "has no attribute 'home'",
    ("PathTest", "test_is_block_device_false"): "has no attribute 'is_block_device'",
    ("PathTest", "test_is_char_device_false"): "has no attribute 'is_char_device'",
    ("PathTest", "test_is_char_device_true"): "has no attribute 'is_char_device'",
    ("PathTest", "test_is_fifo_false"): "has no attribute 'is_fifo'",
    ("PathTest", "test_is_fifo_true"): "has no attribute 'is_fifo'",
    ("PathTest", "test_is_junction"): "has no attribute 'is_junction'",
    ("PathTest", "test_link_to_not_implemented"): "has no attribute 'hardlink_to'",
    ("PathTest", "test_is_mount"): "has no attribute 'is_mount'",
    ("PathTest", "test_is_socket_false"): "has no attribute 'is_socket'",
    ("PathTest", "test_is_socket_true"): "has no attribute 'is_socket'",
    ("PathTest", "test_is_symlink"): "has no attribute 'is_symlink'",
    ("PathTest", "test_iterdir"): "has no attribute 'iterdir'",
    ("PathTest", "test_iterdir_nodir"): "has no attribute 'iterdir'",
    ("PathTest", "test_lstat_nosymlink"): "has no attribute 'stat'",
    ("PathTest", "test_mkdir"): "has no attribute 'mkdir'",
    ("PathTest", "test_mkdir_concurrent_parent_creation"): "has no attribute 'mkdir'",
    ("PathTest", "test_mkdir_exist_ok"): "has no attribute 'stat'",
    ("PathTest", "test_mkdir_exist_ok_root"): "has no attribute 'resolve'",
    ("PathTest", "test_mkdir_exist_ok_with_parent"): "has no attribute 'mkdir'",
    ("PathTest", "test_mkdir_no_parents_file"): "has no attribute 'mkdir'",
    ("PathTest", "test_mkdir_parents"): "has no attribute 'mkdir'",
    ("PathTest", "test_mkdir_with_child_file"): "has no attribute 'mkdir'",
    ("PathTest", "test_mkdir_with_unknown_drive"): "has no attribute 'mkdir'",
    ("PathTest", "test_open_common"): "has no attribute 'open'",
    ("PathTest", "test_owner"): "has no attribute 'stat'",
    ("PathTest", "test_pickling_common"): "has no attribute 'stat'",
    ("PathTest", "test_read_write_bytes"): "has no attribute 'write_bytes'",
    ("PathTest", "test_read_write_text"): "has no attribute 'write_text'",
    ("PathTest", "test_rename"): "has no attribute 'stat'",
    ("PathTest", "test_replace"): "has no attribute 'stat'",
    ("PathTest", "test_resolve_nonexist_relative_issue38671"): "has no attribute 'resolve'",
    ("PathTest", "test_rglob_common"): "has no attribute 'rglob'",
    ("PathTest", "test_rmdir"): "has no attribute 'iterdir'",
    ("PathTest", "test_samefile"): "has no attribute 'samefile'",
    ("PathTest", "test_stat_no_follow_symlinks_nosymlink"): "has no attribute 'stat'",
    ("PathTest", "test_touch_common"): "has no attribute 'touch'",
    ("PathTest", "test_touch_nochange"): "has no attribute 'touch'",
    ("PathTest", "test_unlink"): "has no attribute 'unlink'",
    ("PathTest", "test_unlink_missing_ok"): "has no attribute 'unlink'",
    ("PathTest", "test_with"): "has no attribute 'iterdir'",
    ("PathTest", "test_with_segments"): "has no attribute 'session_id'",
    ("PathTest", "test_write_text_with_newlines"): "has no attribute 'write_text'",

    # =========================================================================
    # PosixPathTest - concrete Path tests require filesystem I/O (NOT_PLANNED)
    # =========================================================================
    ("PosixPathTest", "test_empty_path"): "has no attribute 'stat'",
    ("PosixPathTest", "test_expanduser"): "has no attribute 'unset'",
    ("PosixPathTest", "test_expanduser_common"): "has no attribute 'expanduser'",
    ("PosixPathTest", "test_glob"): "has no attribute 'glob'",
    ("PosixPathTest", "test_glob_above_recursion_limit"): "has no attribute 'mkdir'",
    ("PosixPathTest", "test_glob_case_sensitive"): "has no attribute 'glob'",
    ("PosixPathTest", "test_glob_common"): "has no attribute 'glob'",
    ("PosixPathTest", "test_glob_dotdot"): "has no attribute 'glob'",
    ("PosixPathTest", "test_glob_many_open_files"): "has no attribute 'mkdir'",
    ("PosixPathTest", "test_group"): "has no attribute 'stat'",
    ("PosixPathTest", "test_hardlink_to"): "has no attribute 'stat'",
    ("PosixPathTest", "test_home"): "has no attribute 'home'",
    ("PosixPathTest", "test_is_block_device_false"): "has no attribute 'is_block_device'",
    ("PosixPathTest", "test_is_char_device_false"): "has no attribute 'is_char_device'",
    ("PosixPathTest", "test_is_char_device_true"): "has no attribute 'is_char_device'",
    ("PosixPathTest", "test_is_fifo_false"): "has no attribute 'is_fifo'",
    ("PosixPathTest", "test_is_fifo_true"): "has no attribute 'is_fifo'",
    ("PosixPathTest", "test_is_junction"): "has no attribute 'is_junction'",
    ("PosixPathTest", "test_link_to_not_implemented"): "has no attribute 'hardlink_to'",
    ("PosixPathTest", "test_is_mount"): "has no attribute 'is_mount'",
    ("PosixPathTest", "test_is_socket_false"): "has no attribute 'is_socket'",
    ("PosixPathTest", "test_is_socket_true"): "has no attribute 'is_socket'",
    ("PosixPathTest", "test_is_symlink"): "has no attribute 'is_symlink'",
    ("PosixPathTest", "test_iterdir"): "has no attribute 'iterdir'",
    ("PosixPathTest", "test_iterdir_nodir"): "has no attribute 'iterdir'",
    ("PosixPathTest", "test_lstat_nosymlink"): "has no attribute 'stat'",
    ("PosixPathTest", "test_mkdir"): "has no attribute 'mkdir'",
    ("PosixPathTest", "test_mkdir_concurrent_parent_creation"): "has no attribute 'mkdir'",
    ("PosixPathTest", "test_mkdir_exist_ok"): "has no attribute 'stat'",
    ("PosixPathTest", "test_mkdir_exist_ok_root"): "has no attribute 'resolve'",
    ("PosixPathTest", "test_mkdir_exist_ok_with_parent"): "has no attribute 'mkdir'",
    ("PosixPathTest", "test_mkdir_no_parents_file"): "has no attribute 'mkdir'",
    ("PosixPathTest", "test_mkdir_parents"): "has no attribute 'mkdir'",
    ("PosixPathTest", "test_mkdir_with_child_file"): "has no attribute 'mkdir'",
    ("PosixPathTest", "test_open_common"): "has no attribute 'open'",
    ("PosixPathTest", "test_open_mode"): "has no attribute 'open'",
    ("PosixPathTest", "test_owner"): "has no attribute 'stat'",
    ("PosixPathTest", "test_pickling_common"): "has no attribute 'stat'",
    ("PosixPathTest", "test_read_write_bytes"): "has no attribute 'write_bytes'",
    ("PosixPathTest", "test_read_write_text"): "has no attribute 'write_text'",
    ("PosixPathTest", "test_rename"): "has no attribute 'stat'",
    ("PosixPathTest", "test_replace"): "has no attribute 'stat'",
    ("PosixPathTest", "test_resolve_nonexist_relative_issue38671"): "has no attribute 'resolve'",
    ("PosixPathTest", "test_resolve_root"): "has no attribute 'resolve'",
    ("PosixPathTest", "test_rglob"): "has no attribute 'rglob'",
    ("PosixPathTest", "test_rglob_common"): "has no attribute 'rglob'",
    ("PosixPathTest", "test_rmdir"): "has no attribute 'iterdir'",
    ("PosixPathTest", "test_samefile"): "has no attribute 'samefile'",
    ("PosixPathTest", "test_stat_no_follow_symlinks_nosymlink"): "has no attribute 'stat'",
    ("PosixPathTest", "test_touch_common"): "has no attribute 'touch'",
    ("PosixPathTest", "test_touch_mode"): "has no attribute 'touch'",
    ("PosixPathTest", "test_touch_nochange"): "has no attribute 'touch'",
    ("PosixPathTest", "test_unlink"): "has no attribute 'unlink'",
    ("PosixPathTest", "test_unlink_missing_ok"): "has no attribute 'unlink'",
    ("PosixPathTest", "test_with"): "has no attribute 'iterdir'",
    ("PosixPathTest", "test_with_segments"): "has no attribute 'session_id'",
    ("PosixPathTest", "test_write_text_with_newlines"): "has no attribute 'write_text'",

    # =========================================================================
    # WalkTests - walk() not implemented, requires filesystem I/O (NOT_PLANNED)
    # =========================================================================
    ("WalkTests", "test_file_like_path"): "has no attribute 'walk'",
    ("WalkTests", "test_walk_above_recursion_limit"): "has no attribute 'mkdir'",
    ("WalkTests", "test_walk_bad_dir"): "has no attribute 'walk'",
    ("WalkTests", "test_walk_bottom_up"): "has no attribute 'walk'",
    ("WalkTests", "test_walk_many_open_files"): "has no attribute 'mkdir'",
    ("WalkTests", "test_walk_prune"): "has no attribute 'walk'",
    ("WalkTests", "test_walk_topdown"): "has no attribute 'walk'",

    # =========================================================================
    # WindowsPathAsPureTest - runs only on Windows, tests pure path operations
    # =========================================================================
    ("WindowsPathAsPureTest", "test_eq"): "WindowsPath",
    ("WindowsPathAsPureTest", "test_group"): "has no attribute 'group'",
    ("WindowsPathAsPureTest", "test_owner"): "has no attribute 'owner'",
    ("WindowsPathAsPureTest", "test_with_segments_common"): "has no attribute 'session_id'",
    ("WindowsPathAsPureTest", "test_with_suffix_common"): "expected str, not tuple",

    # =========================================================================
    # WindowsPathTest - concrete Path tests on Windows, require filesystem I/O
    # These run only on Windows systems
    # =========================================================================
    ("WindowsPathTest", "test_absolute"): "!=",
    ("WindowsPathTest", "test_absolute_common"): "!=",
    ("WindowsPathTest", "test_parts_interning"): "is not",
    ("WindowsPathTest", "test_passing_kwargs_deprecated"): "DeprecationWarning not triggered",
    ("WindowsPathTest", "test_with_segments"): "has no attribute 'session_id'",
    # TEMPORARILY COMMENTED OUT - need to get specific attribute names from CI
    # ("WindowsPathTest", "test_empty_path"): "has no attribute",
    # ("WindowsPathTest", "test_expanduser"): "has no attribute",
    # ("WindowsPathTest", "test_expanduser_common"): "has no attribute",
    # ("WindowsPathTest", "test_glob"): "has no attribute",
    # ("WindowsPathTest", "test_glob_above_recursion_limit"): "has no attribute",
    # ("WindowsPathTest", "test_glob_case_sensitive"): "has no attribute",
    # ("WindowsPathTest", "test_glob_common"): "has no attribute",
    # ("WindowsPathTest", "test_glob_dotdot"): "has no attribute",
    # ("WindowsPathTest", "test_glob_many_open_files"): "has no attribute",
    # ("WindowsPathTest", "test_group"): "has no attribute",
    # ("WindowsPathTest", "test_hardlink_to"): "has no attribute",
    # ("WindowsPathTest", "test_home"): "has no attribute",
    # ("WindowsPathTest", "test_is_block_device_false"): "has no attribute",
    # ("WindowsPathTest", "test_is_char_device_false"): "has no attribute",
    # ("WindowsPathTest", "test_is_char_device_true"): "has no attribute",
    # ("WindowsPathTest", "test_is_fifo_false"): "has no attribute",
    # ("WindowsPathTest", "test_is_fifo_true"): "has no attribute",
    # ("WindowsPathTest", "test_is_junction"): "has no attribute",
    # ("WindowsPathTest", "test_is_mount"): "has no attribute",
    # ("WindowsPathTest", "test_is_socket_false"): "has no attribute",
    # ("WindowsPathTest", "test_is_socket_true"): "has no attribute",
    # ("WindowsPathTest", "test_is_symlink"): "has no attribute",
    # ("WindowsPathTest", "test_iterdir"): "has no attribute",
    # ("WindowsPathTest", "test_iterdir_nodir"): "has no attribute",
    # ("WindowsPathTest", "test_lstat_nosymlink"): "has no attribute",
    # ("WindowsPathTest", "test_mkdir"): "has no attribute",
    # ("WindowsPathTest", "test_mkdir_concurrent_parent_creation"): "has no attribute",
    # ("WindowsPathTest", "test_mkdir_exist_ok"): "has no attribute",
    # ("WindowsPathTest", "test_mkdir_exist_ok_root"): "has no attribute",
    # ("WindowsPathTest", "test_mkdir_exist_ok_with_parent"): "has no attribute",
    # ("WindowsPathTest", "test_mkdir_no_parents_file"): "has no attribute",
    # ("WindowsPathTest", "test_mkdir_parents"): "has no attribute",
    # ("WindowsPathTest", "test_mkdir_with_child_file"): "has no attribute",
    # ("WindowsPathTest", "test_mkdir_with_unknown_drive"): "has no attribute",
    # ("WindowsPathTest", "test_open_common"): "has no attribute",
    # ("WindowsPathTest", "test_owner"): "has no attribute",
    # ("WindowsPathTest", "test_pickling_common"): "has no attribute",
    # ("WindowsPathTest", "test_read_write_bytes"): "has no attribute",
    # ("WindowsPathTest", "test_read_write_text"): "has no attribute",
    # ("WindowsPathTest", "test_rename"): "has no attribute",
    # ("WindowsPathTest", "test_replace"): "has no attribute",
    # ("WindowsPathTest", "test_resolve_nonexist_relative_issue38671"): "has no attribute",
    # ("WindowsPathTest", "test_rglob"): "has no attribute",
    # ("WindowsPathTest", "test_rglob_common"): "has no attribute",
    # ("WindowsPathTest", "test_rmdir"): "has no attribute",
    # ("WindowsPathTest", "test_samefile"): "has no attribute",
    # ("WindowsPathTest", "test_stat_no_follow_symlinks_nosymlink"): "has no attribute",
    # ("WindowsPathTest", "test_touch_common"): "has no attribute",
    # ("WindowsPathTest", "test_touch_nochange"): "has no attribute",
    # ("WindowsPathTest", "test_unlink"): "has no attribute",
    # ("WindowsPathTest", "test_unlink_missing_ok"): "has no attribute",
    # ("WindowsPathTest", "test_with"): "has no attribute",
    # ("WindowsPathTest", "test_write_text_with_newlines"): "has no attribute",
}


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


def run_tests():
    """Run CPython tests against snakepath."""
    setup_tests()
    setup_pathlib_patch()

    # Add test dir to path
    sys.path.insert(0, str(TEST_DIR.parent))

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
