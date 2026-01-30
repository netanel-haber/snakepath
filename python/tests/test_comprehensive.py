#!/usr/bin/env python3
"""
Comprehensive test suite for snakepath.

Based on CPython's pathlib test suite patterns, adapted for the public API.
Tests pure path operations (no filesystem I/O).
"""

import sys
import os
import unittest

# Add our module to path (parent of tests/ directory)
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import snakepath
from snakepath import PurePath, PurePosixPath, PureWindowsPath


class PurePosixPathTest(unittest.TestCase):
    """Tests for PurePosixPath - adapted from CPython."""

    cls = PurePosixPath

    def test_constructor(self):
        P = self.cls
        p = P('a')
        self.assertIsInstance(p, P)
        P()
        P('a', 'b', 'c')
        P('/a', 'b', 'c')
        P('a/b/c')
        P('/a/b/c')

    def test_str(self):
        P = self.cls
        self.assertEqual(str(P('/')), '/')
        self.assertEqual(str(P('a')), 'a')
        self.assertEqual(str(P('a/b')), 'a/b')
        self.assertEqual(str(P('a/b/c')), 'a/b/c')
        self.assertEqual(str(P('/a')), '/a')
        self.assertEqual(str(P('/a/b')), '/a/b')
        self.assertEqual(str(P('/a/b/c')), '/a/b/c')
        self.assertEqual(str(P('a//b')), 'a/b')  # Normalized
        self.assertEqual(str(P('a/./b')), 'a/./b')  # Not resolved
        self.assertEqual(str(P('a/../b')), 'a/../b')  # Not resolved

    def test_repr(self):
        P = self.cls
        self.assertEqual(repr(P('a')), "PurePosixPath('a')")
        self.assertEqual(repr(P('/a/b')), "PurePosixPath('/a/b')")

    def test_eq(self):
        P = self.cls
        self.assertEqual(P('a/b'), P('a/b'))
        self.assertEqual(P('a/b'), P('a//b'))  # Normalized
        self.assertNotEqual(P('a/b'), P('a/c'))
        self.assertNotEqual(P('/a/b'), P('a/b'))
        self.assertNotEqual(P('a/b'), 'a/b')

    def test_hash(self):
        P = self.cls
        self.assertEqual(hash(P('a/b')), hash(P('a/b')))
        self.assertEqual(hash(P('a//b')), hash(P('a/b')))  # Normalized

    def test_lt(self):
        P = self.cls
        self.assertTrue(P('a') < P('b'))
        self.assertFalse(P('b') < P('a'))
        self.assertFalse(P('a') < P('a'))

    def test_le(self):
        P = self.cls
        self.assertTrue(P('a') <= P('b'))
        self.assertTrue(P('a') <= P('a'))
        self.assertFalse(P('b') <= P('a'))

    def test_gt(self):
        P = self.cls
        self.assertTrue(P('b') > P('a'))
        self.assertFalse(P('a') > P('b'))
        self.assertFalse(P('a') > P('a'))

    def test_ge(self):
        P = self.cls
        self.assertTrue(P('b') >= P('a'))
        self.assertTrue(P('a') >= P('a'))
        self.assertFalse(P('a') >= P('b'))

    def test_parts(self):
        P = self.cls
        self.assertEqual(P('.').parts, ('.',))
        self.assertEqual(P('/').parts, ('/',))
        self.assertEqual(P('a').parts, ('a',))
        self.assertEqual(P('a/b').parts, ('a', 'b'))
        self.assertEqual(P('/a').parts, ('/', 'a'))
        self.assertEqual(P('/a/b').parts, ('/', 'a', 'b'))
        self.assertEqual(P('/a/b/c').parts, ('/', 'a', 'b', 'c'))

    def test_drive(self):
        P = self.cls
        self.assertEqual(P('/a/b').drive, '')
        self.assertEqual(P('a/b').drive, '')

    def test_root(self):
        P = self.cls
        self.assertEqual(P('/a/b').root, '/')
        self.assertEqual(P('a/b').root, '')

    def test_anchor(self):
        P = self.cls
        self.assertEqual(P('/a/b').anchor, '/')
        self.assertEqual(P('a/b').anchor, '')

    def test_name(self):
        P = self.cls
        self.assertEqual(P('/').name, '')
        self.assertEqual(P('.').name, '.')
        self.assertEqual(P('/a/b').name, 'b')
        self.assertEqual(P('/a/b.py').name, 'b.py')
        self.assertEqual(P('a/b.py').name, 'b.py')

    def test_suffix(self):
        P = self.cls
        self.assertEqual(P('a/b.py').suffix, '.py')
        self.assertEqual(P('a/b.tar.gz').suffix, '.gz')
        self.assertEqual(P('a/b').suffix, '')
        self.assertEqual(P('a/.hidden').suffix, '')
        self.assertEqual(P('a/.hidden.txt').suffix, '.txt')

    def test_suffixes(self):
        P = self.cls
        self.assertEqual(P('a/b.py').suffixes, ['.py'])
        self.assertEqual(P('a/b.tar.gz').suffixes, ['.tar', '.gz'])
        self.assertEqual(P('a/b').suffixes, [])
        self.assertEqual(P('a/.hidden').suffixes, [])
        self.assertEqual(P('a/.hidden.txt').suffixes, ['.txt'])

    def test_stem(self):
        P = self.cls
        self.assertEqual(P('a/b.py').stem, 'b')
        self.assertEqual(P('a/b.tar.gz').stem, 'b.tar')
        self.assertEqual(P('a/b').stem, 'b')
        self.assertEqual(P('a/.hidden').stem, '.hidden')
        self.assertEqual(P('a/.hidden.txt').stem, '.hidden')

    def test_parent(self):
        P = self.cls
        self.assertEqual(P('/a/b/c').parent, P('/a/b'))
        self.assertEqual(P('/a/b').parent, P('/a'))
        self.assertEqual(P('/a').parent, P('/'))
        self.assertEqual(P('/').parent, P('/'))
        self.assertEqual(P('a/b/c').parent, P('a/b'))
        self.assertEqual(P('a/b').parent, P('a'))
        self.assertEqual(P('a').parent, P('.'))
        self.assertEqual(P('.').parent, P('.'))

    def test_parents(self):
        P = self.cls
        p = P('/a/b/c')
        parents = list(p.parents)
        self.assertEqual(parents, [P('/a/b'), P('/a'), P('/')])
        p = P('a/b/c')
        parents = list(p.parents)
        self.assertEqual(parents, [P('a/b'), P('a'), P('.')])

    def test_parents_indexing(self):
        P = self.cls
        p = P('/a/b/c')
        self.assertEqual(p.parents[0], P('/a/b'))
        self.assertEqual(p.parents[1], P('/a'))
        self.assertEqual(p.parents[2], P('/'))
        self.assertEqual(len(p.parents), 3)

    def test_joinpath(self):
        P = self.cls
        self.assertEqual(P('/a').joinpath('b'), P('/a/b'))
        self.assertEqual(P('/a').joinpath('b', 'c'), P('/a/b/c'))
        self.assertEqual(P('/a').joinpath(P('b')), P('/a/b'))
        self.assertEqual(P('/a').joinpath('/b'), P('/b'))  # Absolute resets
        self.assertEqual(P('a').joinpath('b'), P('a/b'))

    def test_div(self):
        P = self.cls
        self.assertEqual(P('/a') / 'b', P('/a/b'))
        self.assertEqual(P('/a') / 'b' / 'c', P('/a/b/c'))
        self.assertEqual(P('/a') / '/b', P('/b'))
        self.assertEqual('a' / P('b'), P('a/b'))

    def test_with_name(self):
        P = self.cls
        self.assertEqual(P('/a/b').with_name('c'), P('/a/c'))
        self.assertEqual(P('a/b').with_name('c'), P('a/c'))
        self.assertEqual(P('/a/b.py').with_name('c.txt'), P('/a/c.txt'))
        with self.assertRaises(ValueError):
            P('/').with_name('c')

    def test_with_stem(self):
        P = self.cls
        self.assertEqual(P('/a/b.py').with_stem('c'), P('/a/c.py'))
        self.assertEqual(P('/a/b.tar.gz').with_stem('c'), P('/a/c.gz'))
        self.assertEqual(P('/a/b').with_stem('c'), P('/a/c'))
        with self.assertRaises(ValueError):
            P('/').with_stem('c')

    def test_with_suffix(self):
        P = self.cls
        self.assertEqual(P('/a/b').with_suffix('.py'), P('/a/b.py'))
        self.assertEqual(P('/a/b.py').with_suffix('.txt'), P('/a/b.txt'))
        self.assertEqual(P('/a/b.py').with_suffix(''), P('/a/b'))
        with self.assertRaises(ValueError):
            P('/a/b').with_suffix('py')  # Missing dot
        with self.assertRaises(ValueError):
            P('/').with_suffix('.py')

    def test_is_absolute(self):
        P = self.cls
        self.assertTrue(P('/').is_absolute())
        self.assertTrue(P('/a').is_absolute())
        self.assertTrue(P('/a/b').is_absolute())
        self.assertFalse(P('.').is_absolute())
        self.assertFalse(P('a').is_absolute())
        self.assertFalse(P('a/b').is_absolute())

    def test_is_relative_to(self):
        P = self.cls
        self.assertTrue(P('/a/b').is_relative_to('/a'))
        self.assertTrue(P('/a/b').is_relative_to('/a/b'))
        self.assertTrue(P('/a/b').is_relative_to('/'))
        self.assertFalse(P('/a/b').is_relative_to('/c'))
        self.assertFalse(P('/a/b').is_relative_to('a'))
        self.assertTrue(P('a/b').is_relative_to('a'))
        self.assertFalse(P('a/b').is_relative_to('/a'))

    def test_relative_to(self):
        P = self.cls
        self.assertEqual(P('/a/b').relative_to('/a'), P('b'))
        self.assertEqual(P('/a/b').relative_to('/a/b'), P('.'))
        self.assertEqual(P('/a/b/c').relative_to('/a'), P('b/c'))
        self.assertEqual(P('/a/b').relative_to('/'), P('a/b'))
        with self.assertRaises(ValueError):
            P('/a/b').relative_to('/c')

    def test_as_posix(self):
        P = self.cls
        self.assertEqual(P('/a/b').as_posix(), '/a/b')

    def test_match(self):
        P = self.cls
        # Simple patterns
        self.assertTrue(P('a/b.py').match('*.py'))
        self.assertTrue(P('a/b.py').match('b.py'))
        self.assertFalse(P('a/b.py').match('*.txt'))
        # With directory
        self.assertTrue(P('a/b/c.py').match('b/*.py'))
        self.assertFalse(P('a/b/c.py').match('a/*.py'))

    def test_fspath(self):
        P = self.cls
        self.assertEqual(os.fspath(P('/a/b')), '/a/b')


class PureWindowsPathTest(unittest.TestCase):
    """Tests for PureWindowsPath - adapted from CPython."""

    cls = PureWindowsPath

    def test_constructor(self):
        P = self.cls
        p = P('a')
        self.assertIsInstance(p, P)
        P()
        P('a', 'b', 'c')
        P('C:/a', 'b', 'c')
        P('a/b/c')
        P('C:/a/b/c')

    def test_str(self):
        P = self.cls
        self.assertEqual(str(P('C:/')), 'C:\\')
        self.assertEqual(str(P('C:/a')), 'C:\\a')
        self.assertEqual(str(P('C:/a/b')), 'C:\\a\\b')
        self.assertEqual(str(P('//server/share')), '\\\\server\\share')
        self.assertEqual(str(P('//server/share/a')), '\\\\server\\share\\a')
        # Forward slashes normalized to backslashes
        self.assertEqual(str(P('a/b')), 'a\\b')

    def test_parts(self):
        P = self.cls
        self.assertEqual(P('C:/a/b').parts, ('C:\\', 'a', 'b'))
        self.assertEqual(P('//server/share/a').parts, ('\\\\server\\share\\', 'a'))
        self.assertEqual(P('a/b').parts, ('a', 'b'))

    def test_drive(self):
        P = self.cls
        self.assertEqual(P('C:').drive, 'C:')
        self.assertEqual(P('C:/').drive, 'C:')
        self.assertEqual(P('C:/a').drive, 'C:')
        self.assertEqual(P('//server/share').drive, '\\\\server\\share')
        self.assertEqual(P('//server/share/a').drive, '\\\\server\\share')
        self.assertEqual(P('a/b').drive, '')

    def test_root(self):
        P = self.cls
        self.assertEqual(P('C:').root, '')
        self.assertEqual(P('C:/').root, '\\')
        self.assertEqual(P('C:/a').root, '\\')
        self.assertEqual(P('//server/share').root, '')
        self.assertEqual(P('//server/share/a').root, '\\')
        self.assertEqual(P('a/b').root, '')

    def test_anchor(self):
        P = self.cls
        self.assertEqual(P('C:').anchor, 'C:')
        self.assertEqual(P('C:/').anchor, 'C:\\')
        self.assertEqual(P('C:/a').anchor, 'C:\\')
        self.assertEqual(P('//server/share').anchor, '\\\\server\\share')
        self.assertEqual(P('//server/share/a').anchor, '\\\\server\\share\\')
        self.assertEqual(P('a/b').anchor, '')

    def test_name(self):
        P = self.cls
        self.assertEqual(P('C:/').name, '')
        self.assertEqual(P('C:/a/b').name, 'b')
        self.assertEqual(P('//server/share').name, '')
        self.assertEqual(P('//server/share/a').name, 'a')

    def test_parent(self):
        P = self.cls
        self.assertEqual(P('C:/a/b').parent, P('C:/a'))
        self.assertEqual(P('C:/a').parent, P('C:/'))
        self.assertEqual(P('C:/').parent, P('C:/'))
        # UNC: parent of //server/share/a includes trailing slash
        self.assertEqual(P('//server/share/a').parent, P('//server/share/'))
        self.assertEqual(P('//server/share').parent, P('//server/share'))

    def test_joinpath(self):
        P = self.cls
        self.assertEqual(P('C:/a').joinpath('b'), P('C:/a/b'))
        self.assertEqual(P('C:/a').joinpath('/b'), P('C:/b'))
        self.assertEqual(P('C:/a').joinpath('D:'), P('D:'))
        self.assertEqual(P('C:/a').joinpath('D:/b'), P('D:/b'))
        self.assertEqual(P('//server/share').joinpath('a'), P('//server/share/a'))

    def test_div(self):
        P = self.cls
        self.assertEqual(P('C:/a') / 'b', P('C:/a/b'))
        self.assertEqual(P('C:/a') / '/b', P('C:/b'))
        self.assertEqual(P('C:/a') / 'D:/', P('D:/'))

    def test_is_absolute(self):
        P = self.cls
        # Windows: absolute needs both drive and root
        self.assertTrue(P('C:/').is_absolute())
        self.assertTrue(P('C:/a').is_absolute())
        self.assertTrue(P('//server/share').is_absolute())
        self.assertTrue(P('//server/share/a').is_absolute())
        self.assertFalse(P('C:').is_absolute())  # Drive only, no root
        self.assertFalse(P('C:a').is_absolute())
        self.assertFalse(P('/a').is_absolute())  # Root only, no drive
        self.assertFalse(P('a').is_absolute())

    def test_is_reserved(self):
        P = self.cls
        self.assertTrue(P('CON').is_reserved())
        self.assertTrue(P('NUL').is_reserved())
        self.assertTrue(P('COM1').is_reserved())
        self.assertTrue(P('LPT1').is_reserved())
        self.assertTrue(P('con.txt').is_reserved())
        self.assertFalse(P('foo.txt').is_reserved())
        # /CON is also reserved (name is CON)
        self.assertTrue(P('/CON').is_reserved())

    def test_as_posix(self):
        P = self.cls
        self.assertEqual(P('C:\\a\\b').as_posix(), 'C:/a/b')
        self.assertEqual(P('\\\\server\\share').as_posix(), '//server/share')


class JoinPosixTest(unittest.TestCase):
    """POSIX-specific join tests - adapted from CPython test_join_posix.py."""

    def test_join(self):
        P = PurePosixPath
        # Basic
        self.assertEqual(P('/etc').joinpath('passwd'), P('/etc/passwd'))
        self.assertEqual(P('/etc').joinpath('init.d', 'apache2'), P('/etc/init.d/apache2'))
        # Absolute resets
        self.assertEqual(P('/etc').joinpath('/tmp'), P('/tmp'))
        self.assertEqual(P('/etc/passwd').joinpath('/tmp'), P('/tmp'))
        # Double slashes (special in POSIX)
        self.assertEqual(P('//etc').joinpath('passwd'), P('//etc/passwd'))
        self.assertEqual(P('//etc').joinpath('//tmp'), P('//tmp'))

    def test_div(self):
        P = PurePosixPath
        self.assertEqual(P('/etc') / 'passwd', P('/etc/passwd'))
        self.assertEqual(P('/etc') / 'init.d' / 'apache2', P('/etc/init.d/apache2'))
        self.assertEqual(P('/etc') / '/tmp', P('/tmp'))


class JoinWindowsTest(unittest.TestCase):
    """Windows-specific join tests - adapted from CPython test_join_windows.py."""

    def test_join_drive_letter(self):
        P = PureWindowsPath
        # Basic
        self.assertEqual(P('C:/').joinpath('a'), P('C:/a'))
        self.assertEqual(P('C:/a').joinpath('b'), P('C:/a/b'))
        # Absolute path with root resets to same drive
        self.assertEqual(P('C:/a').joinpath('/b'), P('C:/b'))
        # Different drive resets completely
        self.assertEqual(P('C:/a').joinpath('D:'), P('D:'))
        self.assertEqual(P('C:/a').joinpath('D:/b'), P('D:/b'))
        # Relative drive (no root)
        self.assertEqual(P('C:a').joinpath('b'), P('C:a/b'))
        self.assertEqual(P('C:a').joinpath('C:b'), P('C:a/b'))  # Same drive, relative

    def test_join_unc(self):
        P = PureWindowsPath
        # UNC paths
        self.assertEqual(P('//server/share').joinpath('a'), P('//server/share/a'))
        self.assertEqual(P('//server/share/a').joinpath('b'), P('//server/share/a/b'))
        # Root resets within UNC
        self.assertEqual(P('//server/share/a').joinpath('/b'), P('//server/share/b'))

    def test_div_drive_letter(self):
        P = PureWindowsPath
        self.assertEqual(P('C:/a') / 'b', P('C:/a/b'))
        self.assertEqual(P('C:/a') / '/b', P('C:/b'))
        self.assertEqual(P('C:/a') / 'D:/', P('D:/'))


class NormalizationTest(unittest.TestCase):
    """Tests for path normalization."""

    def test_trailing_slash_posix(self):
        P = PurePosixPath
        self.assertEqual(str(P('a/')), 'a')
        self.assertEqual(str(P('a/b/')), 'a/b')
        self.assertEqual(str(P('/a/')), '/a')

    def test_double_slash_posix(self):
        P = PurePosixPath
        self.assertEqual(str(P('a//b')), 'a/b')
        self.assertEqual(str(P('a///b')), 'a/b')
        self.assertEqual(str(P('/a//b')), '/a/b')

    def test_empty_path_posix(self):
        P = PurePosixPath
        self.assertEqual(str(P('')), '.')

    def test_trailing_slash_windows(self):
        P = PureWindowsPath
        self.assertEqual(str(P('a\\')), 'a')
        self.assertEqual(str(P('a\\b\\')), 'a\\b')
        self.assertEqual(str(P('C:\\a\\')), 'C:\\a')

    def test_double_slash_windows(self):
        P = PureWindowsPath
        self.assertEqual(str(P('a\\\\b')), 'a\\b')
        self.assertEqual(str(P('C:\\\\a')), 'C:\\a')

    def test_forward_slash_windows(self):
        P = PureWindowsPath
        self.assertEqual(str(P('a/b')), 'a\\b')
        self.assertEqual(str(P('C:/a/b')), 'C:\\a\\b')


class EdgeCaseTest(unittest.TestCase):
    """Tests for edge cases."""

    def test_dotdot_not_resolved(self):
        """.. is preserved, not resolved (no I/O)."""
        P = PurePosixPath
        self.assertEqual(str(P('a/../b')), 'a/../b')
        self.assertEqual(str(P('/a/../b')), '/a/../b')

    def test_dot_in_path(self):
        """Single . is preserved."""
        P = PurePosixPath
        self.assertEqual(str(P('a/./b')), 'a/./b')
        self.assertEqual(str(P('/a/./b')), '/a/./b')

    def test_hidden_files(self):
        P = PurePosixPath
        self.assertEqual(P('.hidden').name, '.hidden')
        self.assertEqual(P('.hidden').stem, '.hidden')
        self.assertEqual(P('.hidden').suffix, '')
        self.assertEqual(P('.hidden.txt').suffix, '.txt')
        self.assertEqual(P('.hidden.txt').stem, '.hidden')

    def test_multiple_suffixes(self):
        P = PurePosixPath
        p = P('archive.tar.gz')
        self.assertEqual(p.suffixes, ['.tar', '.gz'])
        self.assertEqual(p.suffix, '.gz')
        self.assertEqual(p.stem, 'archive.tar')

    def test_version_numbers(self):
        """Version numbers are treated as suffixes."""
        P = PurePosixPath
        p = P('package-1.0.0.tar.gz')
        # This matches Python pathlib behavior
        self.assertEqual(p.suffixes, ['.0', '.0', '.tar', '.gz'])

    def test_relative_to_same_path(self):
        P = PurePosixPath
        self.assertEqual(P('/a/b').relative_to('/a/b'), P('.'))

    def test_parents_length(self):
        P = PurePosixPath
        self.assertEqual(len(P('/a/b/c').parents), 3)
        self.assertEqual(len(P('a/b/c').parents), 3)
        self.assertEqual(len(P('/').parents), 0)
        self.assertEqual(len(P('.').parents), 0)


if __name__ == '__main__':
    unittest.main(verbosity=2)
