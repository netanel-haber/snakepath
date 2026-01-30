#!/usr/bin/env python3
"""
Test runner for snakepath against CPython's pathlib test suite.

Patches pathlib imports to use snakepath and runs the pure path tests.
"""

import sys
import os
import unittest
import importlib.util

# Add our module to path (parent of tests/ directory)
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# Import our snakepath module
import snakepath

# Patch pathlib with our implementation
class FakePathlib:
    """Fake pathlib module that uses snakepath"""
    PurePath = snakepath.PurePath
    PurePosixPath = snakepath.PurePosixPath
    PureWindowsPath = snakepath.PureWindowsPath
    Path = snakepath.Path
    PosixPath = snakepath.PosixPath
    WindowsPath = snakepath.WindowsPath

sys.modules['pathlib'] = FakePathlib()


class TestPurePath(unittest.TestCase):
    """Tests for PurePath - adapted from CPython test_pathlib.py"""

    def test_constructor_common(self):
        P = snakepath.PurePath
        p = P('a')
        self.assertIsInstance(p, P)
        p = P('a', 'b')
        self.assertIsInstance(p, P)
        p = P('/a')
        self.assertIsInstance(p, P)
        p = P('/a', 'b')
        self.assertIsInstance(p, P)

    def test_str_common(self):
        # Check string representation
        P = snakepath.PurePosixPath
        self.assertEqual(str(P('a')), 'a')
        self.assertEqual(str(P('a/b')), 'a/b')
        self.assertEqual(str(P('a/b/c')), 'a/b/c')
        self.assertEqual(str(P('/')), '/')
        self.assertEqual(str(P('/a')), '/a')
        self.assertEqual(str(P('/a/b')), '/a/b')
        self.assertEqual(str(P('/a/b/c')), '/a/b/c')

    def test_str_windows(self):
        P = snakepath.PureWindowsPath
        self.assertEqual(str(P('C:/')), 'C:\\')
        self.assertEqual(str(P('C:/a')), 'C:\\a')
        self.assertEqual(str(P('C:/a/b')), 'C:\\a\\b')

    def test_eq_common(self):
        P = snakepath.PurePosixPath
        self.assertEqual(P('a/b'), P('a/b'))
        self.assertEqual(P('/a/b'), P('/a/b'))
        self.assertNotEqual(P('a/b'), P('a/c'))

    def test_parts_common(self):
        P = snakepath.PurePosixPath
        self.assertEqual(P('a/b').parts, ('a', 'b'))
        self.assertEqual(P('/a/b').parts, ('/', 'a', 'b'))
        self.assertEqual(P('/a/b/c').parts, ('/', 'a', 'b', 'c'))

    def test_parts_windows(self):
        P = snakepath.PureWindowsPath
        self.assertEqual(P('C:/a/b').parts, ('C:\\', 'a', 'b'))
        # UNC anchor includes trailing slash
        self.assertEqual(P('//server/share/a').parts, ('\\\\server\\share\\', 'a'))

    def test_drive_common(self):
        P = snakepath.PurePosixPath
        self.assertEqual(P('/a/b').drive, '')
        self.assertEqual(P('a/b').drive, '')

    def test_drive_windows(self):
        P = snakepath.PureWindowsPath
        # Python doesn't uppercase drive letters
        self.assertEqual(P('c:').drive, 'c:')
        self.assertEqual(P('C:a/b').drive, 'C:')
        self.assertEqual(P('C:/a').drive, 'C:')
        self.assertEqual(P('//server/share').drive, '\\\\server\\share')

    def test_root_common(self):
        P = snakepath.PurePosixPath
        self.assertEqual(P('a/b').root, '')
        self.assertEqual(P('/a/b').root, '/')

    def test_root_windows(self):
        P = snakepath.PureWindowsPath
        self.assertEqual(P('C:').root, '')
        self.assertEqual(P('C:/').root, '\\')
        self.assertEqual(P('C:/a').root, '\\')
        self.assertEqual(P('//server/share').root, '')

    def test_anchor_common(self):
        P = snakepath.PurePosixPath
        self.assertEqual(P('a/b').anchor, '')
        self.assertEqual(P('/a/b').anchor, '/')

    def test_anchor_windows(self):
        P = snakepath.PureWindowsPath
        self.assertEqual(P('C:').anchor, 'C:')
        self.assertEqual(P('C:/').anchor, 'C:\\')
        self.assertEqual(P('C:/a').anchor, 'C:\\')

    def test_name_common(self):
        P = snakepath.PurePosixPath
        self.assertEqual(P('/a/b').name, 'b')
        self.assertEqual(P('/a/b.py').name, 'b.py')
        self.assertEqual(P('a/b.py').name, 'b.py')
        self.assertEqual(P('/').name, '')
        self.assertEqual(P('.').name, '.')

    def test_suffix_common(self):
        P = snakepath.PurePosixPath
        self.assertEqual(P('a/b.py').suffix, '.py')
        self.assertEqual(P('a/b.tar.gz').suffix, '.gz')
        self.assertEqual(P('a/b').suffix, '')
        self.assertEqual(P('/a/b').suffix, '')

    def test_suffixes_common(self):
        P = snakepath.PurePosixPath
        self.assertEqual(P('a/b.py').suffixes, ['.py'])
        self.assertEqual(P('a/b.tar.gz').suffixes, ['.tar', '.gz'])
        self.assertEqual(P('a/b').suffixes, [])

    def test_stem_common(self):
        P = snakepath.PurePosixPath
        self.assertEqual(P('a/b.py').stem, 'b')
        self.assertEqual(P('a/b.tar.gz').stem, 'b.tar')
        self.assertEqual(P('a/b').stem, 'b')

    def test_parent_common(self):
        P = snakepath.PurePosixPath
        self.assertEqual(P('a/b/c').parent, P('a/b'))
        self.assertEqual(P('/a/b/c').parent, P('/a/b'))
        self.assertEqual(P('/a').parent, P('/'))
        self.assertEqual(P('/').parent, P('/'))
        self.assertEqual(P('.').parent, P('.'))

    def test_parents_common(self):
        P = snakepath.PurePosixPath
        p = P('a/b/c')
        parents = list(p.parents)
        self.assertEqual(parents, [P('a/b'), P('a'), P('.')])
        p = P('/a/b/c')
        parents = list(p.parents)
        self.assertEqual(parents, [P('/a/b'), P('/a'), P('/')])

    def test_joinpath_common(self):
        P = snakepath.PurePosixPath
        p = P('/a')
        self.assertEqual(p.joinpath('b'), P('/a/b'))
        self.assertEqual(p.joinpath('b', 'c'), P('/a/b/c'))
        self.assertEqual(p / 'b', P('/a/b'))
        self.assertEqual(p / 'b' / 'c', P('/a/b/c'))

    def test_joinpath_absolute(self):
        P = snakepath.PurePosixPath
        p = P('/a')
        self.assertEqual(p.joinpath('/b'), P('/b'))
        self.assertEqual(p / '/b', P('/b'))

    def test_joinpath_windows(self):
        P = snakepath.PureWindowsPath
        p = P('C:/a')
        self.assertEqual(p.joinpath('b'), P('C:/a/b'))
        self.assertEqual(p.joinpath('/b'), P('C:/b'))
        self.assertEqual(p.joinpath('D:'), P('D:'))
        self.assertEqual(p.joinpath('D:/b'), P('D:/b'))

    def test_with_name_common(self):
        P = snakepath.PurePosixPath
        self.assertEqual(P('a/b').with_name('c'), P('a/c'))
        self.assertEqual(P('/a/b').with_name('c'), P('/a/c'))
        self.assertEqual(P('a/b.py').with_name('c.txt'), P('a/c.txt'))

    def test_with_name_empty(self):
        P = snakepath.PurePosixPath
        with self.assertRaises(ValueError):
            P('/').with_name('c')

    def test_with_stem_common(self):
        P = snakepath.PurePosixPath
        self.assertEqual(P('a/b.py').with_stem('c'), P('a/c.py'))
        self.assertEqual(P('a/b.tar.gz').with_stem('c'), P('a/c.gz'))

    def test_with_suffix_common(self):
        P = snakepath.PurePosixPath
        self.assertEqual(P('a/b').with_suffix('.py'), P('a/b.py'))
        self.assertEqual(P('a/b.py').with_suffix('.txt'), P('a/b.txt'))
        self.assertEqual(P('a/b.py').with_suffix(''), P('a/b'))

    def test_with_suffix_invalid(self):
        P = snakepath.PurePosixPath
        with self.assertRaises(ValueError):
            P('a/b').with_suffix('py')  # Missing dot

    def test_is_absolute_posix(self):
        P = snakepath.PurePosixPath
        self.assertTrue(P('/a/b').is_absolute())
        self.assertFalse(P('a/b').is_absolute())
        self.assertTrue(P('/').is_absolute())

    def test_is_absolute_windows(self):
        P = snakepath.PureWindowsPath
        self.assertTrue(P('C:/a').is_absolute())
        self.assertFalse(P('C:a').is_absolute())
        self.assertFalse(P('/a').is_absolute())
        self.assertFalse(P('a').is_absolute())

    def test_is_relative_to_common(self):
        P = snakepath.PurePosixPath
        self.assertTrue(P('/a/b').is_relative_to('/a'))
        self.assertTrue(P('/a/b').is_relative_to('/a/b'))
        self.assertFalse(P('/a/b').is_relative_to('/c'))
        self.assertFalse(P('/a/b').is_relative_to('a'))

    def test_relative_to_common(self):
        P = snakepath.PurePosixPath
        self.assertEqual(P('/a/b').relative_to('/a'), P('b'))
        self.assertEqual(P('/a/b').relative_to('/a/b'), P('.'))
        self.assertEqual(P('/a/b/c').relative_to('/a'), P('b/c'))

    def test_relative_to_error(self):
        P = snakepath.PurePosixPath
        with self.assertRaises(ValueError):
            P('/a/b').relative_to('/c')

    def test_as_posix(self):
        P = snakepath.PureWindowsPath
        self.assertEqual(P('C:\\a\\b').as_posix(), 'C:/a/b')

    def test_match_common(self):
        P = snakepath.PurePosixPath
        self.assertTrue(P('a/b.py').match('*.py'))
        self.assertTrue(P('/a/b.py').match('*.py'))
        self.assertTrue(P('/a/b.py').match('b.py'))
        self.assertFalse(P('/a/b.py').match('*.txt'))

    def test_hash(self):
        P = snakepath.PurePosixPath
        self.assertEqual(hash(P('a/b')), hash(P('a/b')))
        # Different paths should (usually) have different hashes
        self.assertNotEqual(hash(P('a/b')), hash(P('a/c')))

    def test_comparison(self):
        P = snakepath.PurePosixPath
        self.assertTrue(P('a') < P('b'))
        self.assertTrue(P('a') <= P('a'))
        self.assertTrue(P('b') > P('a'))
        self.assertTrue(P('b') >= P('b'))


class TestJoinPosix(unittest.TestCase):
    """Tests for POSIX path joining"""

    def test_join(self):
        P = snakepath.PurePosixPath
        # Basic joining
        self.assertEqual(P('/etc').joinpath('passwd'), P('/etc/passwd'))
        self.assertEqual(P('/etc').joinpath('init.d', 'apache2'), P('/etc/init.d/apache2'))

        # Absolute path resets
        self.assertEqual(P('/etc').joinpath('/tmp'), P('/tmp'))
        self.assertEqual(P('/etc/passwd').joinpath('/tmp'), P('/tmp'))

        # Double slashes
        self.assertEqual(P('//etc').joinpath('passwd'), P('//etc/passwd'))
        self.assertEqual(P('//etc').joinpath('//tmp'), P('//tmp'))

    def test_div(self):
        P = snakepath.PurePosixPath
        self.assertEqual(P('/etc') / 'passwd', P('/etc/passwd'))
        self.assertEqual(P('/etc') / 'init.d' / 'apache2', P('/etc/init.d/apache2'))
        self.assertEqual(P('/etc') / '/tmp', P('/tmp'))


class TestJoinWindows(unittest.TestCase):
    """Tests for Windows path joining"""

    def test_join_drive(self):
        P = snakepath.PureWindowsPath
        # Basic joining
        self.assertEqual(P('C:/Users').joinpath('Admin'), P('C:/Users/Admin'))

        # Absolute path with same drive
        self.assertEqual(P('C:/Users').joinpath('/Windows'), P('C:/Windows'))

        # Different drive resets
        self.assertEqual(P('C:/Users').joinpath('D:/'), P('D:/'))
        self.assertEqual(P('C:/Users').joinpath('D:foo'), P('D:foo'))

    def test_join_unc(self):
        P = snakepath.PureWindowsPath
        self.assertEqual(P('//server/share').joinpath('foo'), P('//server/share/foo'))
        self.assertEqual(P('//server/share/foo').joinpath('/bar'), P('//server/share/bar'))


class TestNormalization(unittest.TestCase):
    """Tests for path normalization"""

    def test_trailing_slash(self):
        P = snakepath.PurePosixPath
        self.assertEqual(str(P('a/b/')), 'a/b')
        self.assertEqual(str(P('/a/b/')), '/a/b')

    def test_double_slash(self):
        P = snakepath.PurePosixPath
        self.assertEqual(str(P('a//b')), 'a/b')
        self.assertEqual(str(P('/a//b//c')), '/a/b/c')

    def test_empty_path(self):
        P = snakepath.PurePosixPath
        self.assertEqual(str(P('')), '.')

    def test_windows_normalization(self):
        P = snakepath.PureWindowsPath
        self.assertEqual(str(P('a/b')), 'a\\b')
        self.assertEqual(str(P('a\\b')), 'a\\b')
        self.assertEqual(str(P('a/b\\')), 'a\\b')


if __name__ == '__main__':
    # Run with verbosity
    unittest.main(verbosity=2)
