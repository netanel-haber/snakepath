"""
snakepath - pathlib's paths over the snakepath C library, faster than pathlib.

PurePath, PurePosixPath, PureWindowsPath, Path, PosixPath and WindowsPath behave like pathlib's. Their methods are
_snakepath's, written in C over snakepath.h; a call past the C library's fixed limits (a path longer than
SP_PATH_MAX, more than SP_MAX_SUFFIXES suffixes, a glob deeper or longer than its limits) is made on CPython's pathlib.
"""

import io
import ntpath
import os
import pathlib
import posixpath

import _snakepath
from _snakepath import UnsupportedOperation

__all__ = ['PurePath', 'PurePosixPath', 'PureWindowsPath', 'Path', 'PosixPath', 'WindowsPath', 'UnsupportedOperation']


class PurePath(_snakepath.PurePathBase):
    """A path without file system access (pathlib.PurePath); PurePath() makes the platform's flavor"""
    __slots__ = ()
    _flavor = 0  # the platform's; the C library's flavors are _snakepath.POSIX and _snakepath.WINDOWS
    _pathlib = pathlib.PurePath
    parser = os.path

    @property
    def _flavour(self):
        return self.parser

    def with_segments(self, *pathsegments):
        return type(self)(*pathsegments)

    def __repr__(self):
        return f"{type(self).__name__}({self.as_posix()!r})"

    def __bytes__(self):
        return os.fsencode(self)

    def __reduce__(self):
        return type(self), (str(self),)


class PurePosixPath(PurePath):
    __slots__ = ()
    _flavor = _snakepath.POSIX
    _pathlib = pathlib.PurePosixPath
    parser = posixpath


class PureWindowsPath(PurePath):
    __slots__ = ()
    _flavor = _snakepath.WINDOWS
    _pathlib = pathlib.PureWindowsPath
    parser = ntpath


class Path(PurePath, _snakepath.PathBase):
    """A path on the file system (pathlib.Path); Path() makes the platform's flavor"""
    __slots__ = ()
    _pathlib = pathlib.Path

    def open(self, mode='r', buffering=-1, encoding=None, errors=None, newline=None):
        if 'b' not in mode:
            encoding = io.text_encoding(encoding)
        return io.open(self, mode, buffering, encoding, errors, newline)

    def read_text(self, encoding=None, errors=None, newline=None):
        with self.open(encoding=io.text_encoding(encoding), errors=errors, newline=newline) as f:
            return f.read()

    def write_text(self, data, encoding=None, errors=None, newline=None):
        if not isinstance(data, str):
            raise TypeError(f"data must be str, not {type(data).__name__}")
        with self.open('w', encoding=io.text_encoding(encoding), errors=errors, newline=newline) as f:
            return f.write(data)


class PosixPath(Path, PurePosixPath):
    __slots__ = ()
    _pathlib = pathlib.PosixPath if os.name != 'nt' else pathlib.PurePosixPath


class WindowsPath(Path, PureWindowsPath):
    __slots__ = ()
    _pathlib = pathlib.WindowsPath if os.name == 'nt' else pathlib.PureWindowsPath


# PurePath() and Path() make these (the C constructor looks in the class's own dictionary)
PurePath._native = PureWindowsPath if os.name == 'nt' else PurePosixPath
Path._native = WindowsPath if os.name == 'nt' else PosixPath


_PATHLIB_PURE = pathlib.PurePath  # pathlib's own, even once a program swaps pathlib's classes for these


def _real(value):
    """A snakepath path as pathlib's"""
    return value._pathlib(str(value)) if isinstance(value, PurePath) else value


def _defer(path, name, args, kwargs):
    """The call the C library can't make as pathlib does (path is the class of a class method), made on pathlib: name
    None is the constructor, which returns the path's text; args None is a property"""
    cls = path if isinstance(path, type) else type(path)
    real_args = [_real(a) for a in args or ()]
    if name is None:
        return str(cls._pathlib(*real_args))

    value = getattr(cls._pathlib if path is cls else _real(path), name)
    if args is not None:
        value = value(*real_args, **{k: _real(v) for k, v in (kwargs or {}).items()})
    make = cls if path is cls else path.with_segments
    if isinstance(value, _PATHLIB_PURE):
        return make(str(value))
    if name == 'parents':
        return tuple(make(str(p)) for p in value)
    if name in ('iterdir', 'glob', 'rglob'):
        return (make(str(p)) for p in value)
    if name == 'walk':
        return ((make(str(top)), dirnames, filenames) for top, dirnames, filenames in value)
    return value


_snakepath._defer = _defer
