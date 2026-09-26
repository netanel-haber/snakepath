"""
snakepath - Python bindings for the snakepath C library, and the checks nob runs.

Import it for PurePath/PurePosixPath/PureWindowsPath/Path/PosixPath/WindowsPath, compatible with
Python's pathlib and backed by the C library (test.c built with -DSP_FFI).

Run it (nob does: python snakepath.py) to check snakepath.h's real call depth and that README.md
embeds api_demo.c verbatim, then run CPython 3.15's own pathlib tests on these classes.
"""

import ctypes
import errno
import io
import os
import pathlib
import re
import subprocess
import sys
import unittest
import urllib.request
from ctypes import c_char_p, c_size_t, c_int, c_uint, POINTER, Structure, byref, create_string_buffer

# Find and load the shared library
_lib_dir = os.path.dirname(os.path.abspath(__file__))
_lib_names = ['libsnakepath.so', 'libsnakepath.dylib', 'snakepath.dll']

_lib = None
for name in _lib_names:
    try:
        _lib = ctypes.CDLL(os.path.join(_lib_dir, name))
        break
    except OSError:
        continue

if _lib is None:
    raise ImportError(f"Could not load snakepath library. Looked in {_lib_dir}")

# The library's constants: errors, flags, case sensitivity and sizes
for _name in ['SP_OK', 'SP_ERR_IO', 'SP_ERR_NOT_FOUND', 'SP_ERR_EXISTS', 'SP_ERR_NOT_DIR', 'SP_ERR_IS_DIR',
              'SP_ERR_NOT_EMPTY', 'SP_ERR_PERMISSION', 'SP_ERR_LOOP', 'SP_ERR_NOT_LINK', 'SP_ERR_CROSS_DEVICE',
              'SP_ERR_SAME_FILE', 'SP_ERR_NO_HOME', 'SP_ERR_TOO_LONG', 'SP_ERR_LIMIT', 'SP_ERR_NUL',
              'SP_ERR_INVALID_ARG', 'SP_ERR_NO_NAME', 'SP_ERR_NOT_RELATIVE', 'SP_ERR_NOT_ABSOLUTE',
              'SP_ERR_UNSUPPORTED', 'SP_MKDIR_PARENTS', 'SP_MKDIR_EXIST_OK', 'SP_COPY_FOLLOW_SYMLINKS',
              'SP_COPY_PRESERVE_METADATA', 'SP_WALK_TOP_DOWN', 'SP_WALK_FOLLOW_SYMLINKS', 'SP_CASE_DEFAULT',
              'SP_CASE_SENSITIVE', 'SP_CASE_INSENSITIVE', 'SP_PATH_MAX', 'SP_MAX_SUFFIXES']:
    globals()[_name] = getattr(_lib, f'sp_const_{_name}')()

SP_FLAVOR_NATIVE = 0
SP_FLAVOR_POSIX = 1
SP_FLAVOR_WINDOWS = 2

_WALK_BUF_SIZE = 1 << 22  # names kept for the directories being walked


class _SpPath(Structure):
    _fields_ = [("buf", ctypes.c_char * SP_PATH_MAX), ("len", c_size_t), ("flavor", c_int), ("error", c_int)]


class _SpTerm(Structure):
    _fields_ = [("buf", ctypes.c_char * SP_PATH_MAX), ("len", c_size_t), ("error", c_int)]


class _SpStr(Structure):
    _fields_ = [("data", ctypes.c_void_p), ("len", c_size_t)]


class _SpSuffixes(Structure):
    _fields_ = [("items", _SpStr * SP_MAX_SUFFIXES), ("count", c_size_t), ("error", c_int)]


class _SpIOResult(Structure):
    _fields_ = [("bytes", c_size_t), ("error", c_int)]


class _SpWalkEntry(Structure):
    _fields_ = [("dirpath", _SpPath), ("dirnames", POINTER(c_char_p)), ("dirname_count", c_size_t),
                ("filenames", POINTER(c_char_p)), ("filename_count", c_size_t), ("error", c_int)]


def _opaque(size_fn):
    """A ctypes structure as big as a C struct that Python only passes around"""
    return type('_Opaque', (Structure,), {'_fields_': [('_data', ctypes.c_char * size_fn())]})


_SpPartsIter = _opaque(_lib.sp_sizeof_parts_iter)
_SpParentsIter = _opaque(_lib.sp_sizeof_parents_iter)
_SpGlobIter = _opaque(_lib.sp_sizeof_glob_iter)
_SpIterdirIter = _opaque(_lib.sp_sizeof_iterdir_iter)
_SpWalkIter = _opaque(_lib.sp_sizeof_walk_iter)


class _SpStatResult(Structure):
    """C SpStatResult structure - mirrors Python's os.stat_result"""
    _fields_ = [
        ("st_mode", ctypes.c_uint),
        ("st_ino", ctypes.c_ulonglong),
        ("st_dev", ctypes.c_ulonglong),
        ("st_nlink", ctypes.c_ulonglong),
        ("st_uid", ctypes.c_uint),
        ("st_gid", ctypes.c_uint),
        ("st_size", ctypes.c_longlong),
        ("st_atime", ctypes.c_double),
        ("st_mtime", ctypes.c_double),
        ("st_ctime", ctypes.c_double),
        ("st_atime_ns", ctypes.c_longlong),
        ("st_mtime_ns", ctypes.c_longlong),
        ("st_ctime_ns", ctypes.c_longlong),
        ("error", c_int),
    ]

    def __eq__(self, other):
        if isinstance(other, _SpStatResult):
            return bool(_lib.sp_stat_eq_wrap(byref(self), byref(other)))
        if hasattr(other, 'st_mode'):
            other_sp = _SpStatResult(**{n: getattr(other, n) for n, _ in _SpStatResult._fields_ if n != 'error'})
            return bool(_lib.sp_stat_eq_wrap(byref(self), byref(other_sp)))
        return NotImplemented

    def __repr__(self):
        return (f"os.stat_result(st_mode={self.st_mode}, st_ino={self.st_ino}, "
                f"st_dev={self.st_dev}, st_nlink={self.st_nlink}, st_uid={self.st_uid}, "
                f"st_gid={self.st_gid}, st_size={self.st_size}, "
                f"st_atime={self.st_atime}, st_mtime={self.st_mtime}, st_ctime={self.st_ctime})")


# ============ Signatures ============

def _sig(name, argtypes, restype=None):
    fn = getattr(_lib, name)
    fn.argtypes = argtypes
    fn.restype = restype

_PP = POINTER(_SpPath)
_PT = POINTER(_SpTerm)
_PStat = POINTER(_SpStatResult)
_PBytes = POINTER(ctypes.c_char)

for n in ['drive', 'root', 'anchor', 'name', 'stem', 'suffix', 'as_posix']:
    _sig(f'sp_{n}_wrap', [_PP, _PT])
for n in ['owner', 'group']:
    _sig(f'sp_{n}_wrap', [_PP, c_int, _PT])
for n in ['parent', 'absolute', 'expanduser', 'readlink']:
    _sig(f'sp_{n}_wrap', [_PP, _PP])
for n in ['with_name', 'with_stem', 'with_suffix']:
    _sig(f'sp_{n}_wrap', [_PP, c_char_p, _PP])
for n in ['joinpath', 'rename', 'replace', 'move', 'move_into']:
    _sig(f'sp_{n}_wrap', [_PP, _PP, _PP])
for n in ['is_file', 'is_dir', 'exists']:
    _sig(f'sp_{n}_wrap', [_PP, c_int], c_int)
for n in ['is_absolute', 'is_symlink', 'is_block_device', 'is_char_device', 'is_fifo', 'is_socket', 'is_mount',
          'is_junction']:
    _sig(f'sp_{n}_wrap', [_PP], c_int)
for n in ['path_eq', 'is_relative_to', 'samefile', 'path_cmp']:
    _sig(f'sp_{n}_wrap', [_PP, _PP], c_int)

_sig('sp_error_str_wrap', [c_int], c_char_p)
_sig('sp_path_new_len_wrap', [_PBytes, c_size_t, c_int, _PP])
_sig('sp_path_convert_wrap', [c_char_p, c_int, c_int, _PP])
_sig('sp_path_copy_wrap', [_PP, _PP])
_sig('sp_str_wrap', [_PP], c_char_p)
_sig('sp_cwd_wrap', [c_int, _PP])
_sig('sp_home_wrap', [c_int, _PP])
_sig('sp_from_uri_wrap', [c_char_p, c_int, _PP])
_sig('sp_suffixes_wrap', [_PP, POINTER(_SpSuffixes)])
_sig('sp_as_uri_wrap', [_PP, c_char_p, c_size_t], c_int)
_sig('sp_join_one_len_wrap', [_PP, _PBytes, c_size_t, _PP])
_sig('sp_with_segments_wrap', [_PP, POINTER(c_char_p), c_size_t, _PP])
_sig('sp_relative_to_wrap', [_PP, _PP, c_int, _PP])
_sig('sp_resolve_wrap', [_PP, c_int, _PP])
_sig('sp_copy_wrap', [_PP, _PP, c_uint, _PP])
_sig('sp_copy_into_wrap', [_PP, _PP, c_uint, _PP])
_sig('sp_parts_iter_begin_wrap', [_PP, POINTER(_SpPartsIter)])
_sig('sp_parts_iter_next_wrap', [POINTER(_SpPartsIter), POINTER(_SpStr)], c_int)
_sig('sp_parents_iter_begin_wrap', [_PP, POINTER(_SpParentsIter)])
_sig('sp_parents_iter_next_wrap', [POINTER(_SpParentsIter), _PP], c_int)
_sig('sp_path_hash_wrap', [_PP], ctypes.c_ulong)
_sig('sp_match_wrap', [_PP, c_char_p, c_int], c_int)
_sig('sp_full_match_wrap', [_PP, c_char_p, c_int], c_int)
_sig('sp_stat_wrap', [_PP, _PStat])
_sig('sp_lstat_wrap', [_PP, _PStat])
_sig('sp_stat_eq_wrap', [_PStat, _PStat], c_int)
_sig('sp_mkdir_wrap', [_PP, c_uint, c_uint, c_uint], c_int)
_sig('sp_touch_wrap', [_PP, c_uint, c_int], c_int)
_sig('sp_unlink_wrap', [_PP, c_int], c_int)
_sig('sp_rmdir_wrap', [_PP], c_int)
_sig('sp_chmod_wrap', [_PP, c_uint, c_int], c_int)
_sig('sp_symlink_to_wrap', [_PP, _PP, c_int], c_int)
_sig('sp_hardlink_to_wrap', [_PP, _PP], c_int)
_sig('sp_read_file_wrap', [_PP, c_char_p, c_size_t, POINTER(_SpIOResult)])
_sig('sp_write_file_wrap', [_PP, c_char_p, c_size_t, POINTER(_SpIOResult)])
_sig('sp_iterdir_begin_wrap', [_PP, POINTER(_SpIterdirIter)])
_sig('sp_iterdir_next_wrap', [POINTER(_SpIterdirIter), _PP], c_int)
_sig('sp_iterdir_end_wrap', [POINTER(_SpIterdirIter)])
_sig('sp_iterdir_error_wrap', [POINTER(_SpIterdirIter)], c_int)
for n in ['glob', 'rglob']:
    _sig(f'sp_{n}_begin_wrap', [_PP, c_char_p, c_int, c_int, POINTER(_SpGlobIter)])
_sig('sp_glob_next_wrap', [POINTER(_SpGlobIter), _PP], c_int)
_sig('sp_glob_end_wrap', [POINTER(_SpGlobIter)])
_sig('sp_glob_error_wrap', [POINTER(_SpGlobIter)], c_int)
_sig('sp_walk_begin_wrap', [_PP, c_uint, ctypes.c_void_p, c_size_t, POINTER(_SpWalkIter)])
_sig('sp_walk_next_wrap', [POINTER(_SpWalkIter)], POINTER(_SpWalkEntry))
_sig('sp_walk_error_wrap', [POINTER(_SpWalkIter)], c_int)


# ============ Errors ============

class UnsupportedOperation(NotImplementedError):
    """An operation the path doesn't support (pathlib.UnsupportedOperation)"""


# The errno of each SpError that is an OSError
_ERRNO = {SP_ERR_IO: errno.EIO, SP_ERR_NOT_FOUND: errno.ENOENT, SP_ERR_EXISTS: errno.EEXIST,
          SP_ERR_NOT_DIR: errno.ENOTDIR, SP_ERR_IS_DIR: errno.EISDIR, SP_ERR_NOT_EMPTY: errno.ENOTEMPTY,
          SP_ERR_PERMISSION: errno.EACCES, SP_ERR_LOOP: errno.ELOOP, SP_ERR_NOT_LINK: errno.EINVAL,
          SP_ERR_CROSS_DEVICE: errno.EXDEV, SP_ERR_SAME_FILE: errno.EINVAL, SP_ERR_TOO_LONG: errno.ENAMETOOLONG,
          SP_ERR_LIMIT: errno.E2BIG, SP_ERR_INVALID_ARG: errno.EINVAL}


def _error(err, *filenames):
    """The Python exception for an SpError from the file system (filenames: the path, then the target)"""
    message = _decode(_lib.sp_error_str_wrap(err))
    if err == SP_ERR_NUL:
        return ValueError("embedded null byte")
    if err == SP_ERR_UNSUPPORTED:
        return UnsupportedOperation(message)
    if err == SP_ERR_NO_HOME:
        return RuntimeError("Could not determine home directory.")
    if err not in _ERRNO:
        return ValueError(message)
    code = _ERRNO[err]
    names = [os.fspath(f) for f in filenames]
    return OSError(code, os.strerror(code), *names[:1], *([None, names[1]] if len(names) > 1 else []))


def _check(err, *filenames):
    if err != SP_OK:
        raise _error(err, *filenames)


# ============ Property descriptors ============

def _text(term):
    """The text of an SpTerm"""
    return term.buf[:term.len].decode('utf-8', errors='surrogatepass')


class _TermProp:
    """Descriptor for SpTerm properties (drive, root, anchor, name, stem, suffix)"""
    __slots__ = ('_func',)
    def __init__(self, name):
        self._func = getattr(_lib, f'sp_{name}_wrap')
    def __get__(self, obj, objtype=None):
        if obj is None:
            return self
        term = _SpTerm()
        self._func(byref(obj._sp), byref(term))
        return _text(term)


class _PathProp:
    """Descriptor for path-returning properties (parent)"""
    __slots__ = ('_func',)
    def __init__(self, name):
        self._func = getattr(_lib, f'sp_{name}_wrap')
    def __get__(self, obj, objtype=None):
        if obj is None: return self
        out = _SpPath()
        self._func(byref(obj._sp), byref(out))
        return obj._from_sp(out)


def _bool_method(name):
    """Method returning the C predicate sp_<name>_wrap as a bool"""
    func = getattr(_lib, f'sp_{name}_wrap')
    def method(self):
        return bool(func(byref(self._sp)))
    method.__name__ = name
    return method


def _follow_method(name):
    """Method returning the C predicate sp_<name>_wrap(follow_symlinks) as a bool"""
    func = getattr(_lib, f'sp_{name}_wrap')
    def method(self, *, follow_symlinks=True):
        return bool(func(byref(self._sp), 1 if follow_symlinks else 0))
    method.__name__ = name
    return method


# ============ Helper functions ============

_BYTES_ERROR = "argument should be a str or an os.PathLike object where __fspath__ returns a str, not 'bytes'"


def _encode(s):
    """Encode string to bytes for C library"""
    if isinstance(s, bytes):
        raise TypeError(_BYTES_ERROR)
    return s.encode('utf-8', errors='surrogatepass') if s else b''


def _encode_buf(s):
    """Encode string to a ctypes buffer that preserves embedded nulls"""
    encoded = _encode(s)
    return create_string_buffer(encoded, len(encoded))


def _decode(b):
    """Decode bytes from C library to string"""
    return '' if b is None else (b.decode('utf-8', errors='surrogatepass') if isinstance(b, bytes) else b)


def _get_pathlib_flavor(obj):
    """Get the flavor constant for a pathlib path object"""
    if isinstance(obj, pathlib.PureWindowsPath):
        return SP_FLAVOR_WINDOWS
    elif isinstance(obj, pathlib.PurePosixPath):
        return SP_FLAVOR_POSIX
    return None


def _case(case_sensitive):
    return SP_CASE_DEFAULT if case_sensitive is None else SP_CASE_SENSITIVE if case_sensitive else SP_CASE_INSENSITIVE


# ============ PathParents ============

class _PathParents:
    """Sequence of parent paths, like pathlib._PathParents"""
    __slots__ = ('_path', '_parents')

    def __init__(self, path):
        self._path = path
        self._parents = None

    def _compute(self):
        if self._parents is None:
            parents = []
            it = _SpParentsIter()
            _lib.sp_parents_iter_begin_wrap(byref(self._path._sp), byref(it))
            out = _SpPath()
            while _lib.sp_parents_iter_next_wrap(byref(it), byref(out)):
                parents.append(self._path._from_sp(out))
            self._parents = tuple(parents)
        return self._parents

    def __len__(self):
        return len(self._compute())

    def __getitem__(self, idx):
        parents = self._compute()
        return tuple(parents[idx]) if isinstance(idx, slice) else parents[idx]

    def __repr__(self):
        return f"<{self._path.__class__.__name__}.parents>"


# ============ PurePath ============

class PurePath:
    """Pure path object - no filesystem access."""

    __slots__ = ('_sp',)
    _flavor = SP_FLAVOR_NATIVE
    parser = __import__('posixpath') if os.name != 'nt' else __import__('ntpath')

    # String-view properties via descriptors
    drive = _TermProp('drive')
    root = _TermProp('root')
    anchor = _TermProp('anchor')
    name = _TermProp('name')
    stem = _TermProp('stem')
    suffix = _TermProp('suffix')

    # Path-returning properties
    parent = _PathProp('parent')

    @property
    def _flavour(self):
        """British spelling alias for CPython tests."""
        return self.parser

    def __new__(cls, *args, **kwargs):
        if cls is PurePath:
            cls = PurePosixPath if os.name != 'nt' else PureWindowsPath
        return object.__new__(cls)

    def __init__(self, *args):
        self._sp = _SpPath()
        for arg in args:
            if isinstance(arg, bytes):
                raise TypeError(_BYTES_ERROR)
        self._load(args[0] if args else '', self._sp)
        self._join_into(self._sp, args[1:])

    def _load(self, arg, out):
        """Parse one path argument into out, converting from another flavor if needed"""
        if isinstance(arg, PurePath) and arg._flavor == self._flavor:
            _lib.sp_path_copy_wrap(byref(arg._sp), byref(out))
        elif (src_flavor := arg._flavor if isinstance(arg, PurePath) else _get_pathlib_flavor(arg)) is not None:
            _lib.sp_path_convert_wrap(_encode(str(arg)), src_flavor, self._flavor, byref(out))
        else:
            buf = _encode_buf(os.fspath(arg))
            _lib.sp_path_new_len_wrap(buf, len(buf.raw), self._flavor, byref(out))
        _check(out.error, arg)

    def _join_into(self, sp, others):
        """Join each argument onto sp in place; plain strings are joined as raw text"""
        for other in others:
            if isinstance(other, PurePath) or _get_pathlib_flavor(other) is not None:
                tmp = _SpPath()
                self._load(other, tmp)
                _lib.sp_joinpath_wrap(byref(sp), byref(tmp), byref(sp))
            else:
                buf = _encode_buf(os.fspath(other))
                _lib.sp_join_one_len_wrap(byref(sp), buf, len(buf.raw), byref(sp))
            _check(sp.error, other)

    def with_segments(self, *pathsegments):
        """Construct a new path object from any number of path-like objects."""
        parts = [_encode(os.fspath(a)) for a in pathsegments]
        parts_arr = (c_char_p * len(parts))(*parts) if parts else None
        out = _SpPath()
        _lib.sp_with_segments_wrap(byref(self._sp), parts_arr, len(parts), byref(out))
        _check(out.error, *pathsegments[:1])
        return self._from_sp(out)

    def _from_sp(self, sp_value):
        result = self.__class__.__new__(self.__class__)
        result._sp = _SpPath()
        _lib.sp_path_copy_wrap(byref(sp_value), byref(result._sp))
        if hasattr(self, "__dict__"):
            result.__dict__.update(self.__dict__)
        return result

    def __str__(self):
        return _decode(_lib.sp_str_wrap(byref(self._sp)))

    def __repr__(self):
        return f"{self.__class__.__name__}({self.as_posix()!r})"

    def __fspath__(self):
        return str(self)

    def __bytes__(self):
        return str(self).encode('utf-8')

    def __eq__(self, other):
        if not isinstance(other, PurePath):
            return NotImplemented
        return bool(_lib.sp_path_eq_wrap(byref(self._sp), byref(other._sp)))

    def __hash__(self):
        return _lib.sp_path_hash_wrap(byref(self._sp))

    def _cmp(self, other):
        if not isinstance(other, PurePath) or self.parser is not other.parser:
            return NotImplemented
        return _lib.sp_path_cmp_wrap(byref(self._sp), byref(other._sp))

    def __lt__(self, other):
        c = self._cmp(other)
        return c if c is NotImplemented else c < 0

    def __le__(self, other):
        c = self._cmp(other)
        return c if c is NotImplemented else c <= 0

    def __gt__(self, other):
        c = self._cmp(other)
        return c if c is NotImplemented else c > 0

    def __ge__(self, other):
        c = self._cmp(other)
        return c if c is NotImplemented else c >= 0

    def __truediv__(self, other):
        try:
            return self.joinpath(other)
        except TypeError:
            return NotImplemented

    def __rtruediv__(self, other):
        try:
            return self.with_segments(other, self)
        except TypeError:
            return NotImplemented

    @property
    def suffixes(self):
        s = _SpSuffixes()
        _lib.sp_suffixes_wrap(byref(self._sp), byref(s))
        _check(s.error, self)
        return [ctypes.string_at(item.data, item.len).decode('utf-8', errors='surrogatepass')
                for item in s.items[:s.count]]

    @property
    def parents(self):
        return _PathParents(self)

    @property
    def parts(self):
        parts = []
        it = _SpPartsIter()
        _lib.sp_parts_iter_begin_wrap(byref(self._sp), byref(it))
        part = _SpStr()
        while _lib.sp_parts_iter_next_wrap(byref(it), byref(part)):
            parts.append(ctypes.string_at(part.data, part.len).decode('utf-8', errors='surrogatepass'))
        return tuple(parts)

    def as_posix(self):
        term = _SpTerm()
        _lib.sp_as_posix_wrap(byref(self._sp), byref(term))
        return _text(term)

    def as_uri(self):
        buf = create_string_buffer(SP_PATH_MAX * 3 + 8)  # every byte percent-encoded, after "file:///"
        err = _lib.sp_as_uri_wrap(byref(self._sp), buf, len(buf))
        if err == SP_ERR_NOT_ABSOLUTE:
            raise ValueError("relative path can't be expressed as a file URI")
        _check(err, self)
        return buf.value.decode('utf-8')

    is_absolute = _bool_method('is_absolute')

    def _other(self, other):
        """C path for another path argument, parsed (or converted) in this path's flavor"""
        sp = _SpPath()
        self._load(other, sp)
        return sp

    def is_relative_to(self, other):
        return bool(_lib.sp_is_relative_to_wrap(byref(self._sp), byref(self._other(other))))

    def relative_to(self, other, *, walk_up=False):
        other_sp = self._other(other)
        out = _SpPath()
        _lib.sp_relative_to_wrap(byref(self._sp), byref(other_sp), 1 if walk_up else 0, byref(out))
        if out.error == SP_ERR_NOT_RELATIVE:
            raise ValueError(f"{str(self)!r} is not relative to {str(self._from_sp(other_sp))!r}")
        _check(out.error, self)
        return self._from_sp(out)

    def joinpath(self, *others):
        for other in others:
            if isinstance(other, bytes):
                raise TypeError(_BYTES_ERROR)
        out = _SpPath()
        _lib.sp_path_copy_wrap(byref(self._sp), byref(out))
        self._join_into(out, others)
        return self._from_sp(out)

    def _with_field(self, type_name, value):
        if not isinstance(value, str):
            raise TypeError(f"expected str, not {type(value).__name__}")
        out = _SpPath()
        getattr(_lib, f'sp_with_{type_name}_wrap')(byref(self._sp), _encode(value), byref(out))
        if out.error == SP_ERR_NO_NAME:
            raise ValueError(f"{self!r} has an empty name")
        if out.error == SP_ERR_INVALID_ARG:
            raise ValueError(f"Invalid {type_name} {value!r}")
        _check(out.error, self)
        return self._from_sp(out)

    def with_name(self, name):
        return self._with_field('name', name)

    def with_stem(self, stem):
        return self._with_field('stem', stem)

    def with_suffix(self, suffix):
        return self._with_field('suffix', suffix)

    def full_match(self, pattern, *, case_sensitive=None):
        pattern = os.fspath(pattern)
        return bool(_lib.sp_full_match_wrap(byref(self._sp), _encode(pattern), _case(case_sensitive)))

    def match(self, path_pattern, *, case_sensitive=None):
        # sp_match's precondition, raised as pathlib raises it
        if not self.with_segments(path_pattern).parts:
            raise ValueError("empty pattern")
        return bool(_lib.sp_match_wrap(byref(self._sp), _encode(os.fspath(path_pattern)), _case(case_sensitive)))


class PurePosixPath(PurePath):
    """Pure path with POSIX semantics."""
    __slots__ = ()
    _flavor = SP_FLAVOR_POSIX
    parser = __import__('posixpath')


class PureWindowsPath(PurePath):
    """Pure path with Windows semantics."""
    __slots__ = ()
    _flavor = SP_FLAVOR_WINDOWS
    parser = __import__('ntpath')


# ============ Concrete Path classes ============

class Path(PurePath):
    """Path object with I/O operations."""
    __slots__ = ()

    def __new__(cls, *args, **kwargs):
        if cls is Path:
            cls = PosixPath if os.name != 'nt' else WindowsPath
        return object.__new__(cls)

    @classmethod
    def _made(cls, make, *args):
        """A path of this class from a C function that makes one (cwd, home, from_uri)"""
        result = cls.__new__(cls)
        result._sp = _SpPath()
        make(*args, result._flavor, byref(result._sp))
        return result

    @classmethod
    def cwd(cls):
        result = cls._made(_lib.sp_cwd_wrap)
        _check(result._sp.error)
        return result

    @classmethod
    def home(cls):
        result = cls._made(_lib.sp_home_wrap)
        _check(result._sp.error)
        return result

    @classmethod
    def from_uri(cls, uri):
        """Return a new path from the given 'file' URI."""
        result = cls._made(_lib.sp_from_uri_wrap, _encode(uri))
        if result._sp.error in (SP_ERR_INVALID_ARG, SP_ERR_NOT_ABSOLUTE):
            raise ValueError(f"URI is not absolute: {uri!r}")
        _check(result._sp.error)
        return result

    def _path_op(self, func, *args, target=None):
        """A path from a C function of this path (and args), raising its error"""
        out = _SpPath()
        func(byref(self._sp), *args, byref(out))
        _check(out.error, self, *([target] if target is not None else []))
        return self._from_sp(out)

    def absolute(self):
        return self._path_op(_lib.sp_absolute_wrap)

    def expanduser(self):
        """Expand ~ to user's home directory."""
        return self._path_op(_lib.sp_expanduser_wrap)

    def readlink(self):
        """Return the path to which the symbolic link points."""
        return self._path_op(_lib.sp_readlink_wrap)

    def resolve(self, strict=False):
        """Make the path absolute, resolving all symlinks."""
        return self._path_op(_lib.sp_resolve_wrap, 1 if strict else 0)

    is_file = _follow_method('is_file')
    is_dir = _follow_method('is_dir')
    exists = _follow_method('exists')
    is_symlink = _bool_method('is_symlink')
    is_block_device = _bool_method('is_block_device')
    is_char_device = _bool_method('is_char_device')
    is_fifo = _bool_method('is_fifo')
    is_socket = _bool_method('is_socket')
    is_mount = _bool_method('is_mount')
    is_junction = _bool_method('is_junction')

    def stat(self, *, follow_symlinks=True):
        result = _SpStatResult()
        (_lib.sp_stat_wrap if follow_symlinks else _lib.sp_lstat_wrap)(byref(self._sp), byref(result))
        _check(result.error, self)
        return result

    def lstat(self):
        """Like stat(), but does not follow symbolic links."""
        return self.stat(follow_symlinks=False)

    def _sp_of(self, other):
        """C path for a PurePath or path-like argument (parsed in this path's flavor)"""
        if isinstance(other, PurePath):
            return other._sp
        sp = _SpPath()
        buf = _encode_buf(os.fspath(other))
        _lib.sp_path_new_len_wrap(buf, len(buf.raw), self._flavor, byref(sp))
        _check(sp.error, other)
        return sp

    def symlink_to(self, target, target_is_directory=False):
        """Make this path a symlink pointing to target."""
        err = _lib.sp_symlink_to_wrap(byref(self._sp), byref(self._sp_of(target)), 1 if target_is_directory else 0)
        _check(err, target, self)

    def hardlink_to(self, target):
        """Make this path a hard link pointing to target."""
        _check(_lib.sp_hardlink_to_wrap(byref(self._sp), byref(self._sp_of(target))), target, self)

    def samefile(self, other_path):
        """Return True if both paths refer to the same file."""
        # sp_samefile is false for a missing file, where pathlib raises stat()'s error
        other = self._from_sp(self._sp_of(other_path))
        self.stat()
        other.stat()
        return bool(_lib.sp_samefile_wrap(byref(self._sp), byref(other._sp)))

    def mkdir(self, mode=0o777, parents=False, exist_ok=False, *, parent_mode=None):
        flags = (SP_MKDIR_PARENTS if parents else 0) | (SP_MKDIR_EXIST_OK if exist_ok else 0)
        err = _lib.sp_mkdir_wrap(byref(self._sp), mode, flags, 0o777 if parent_mode is None else parent_mode)
        _check(err, self)

    def _glob(self, begin, pattern, case_sensitive, recurse_symlinks):
        it = _SpGlobIter()
        begin(byref(self._sp), _encode(os.fspath(pattern)), _case(case_sensitive), 1 if recurse_symlinks else 0,
              byref(it))
        err = _lib.sp_glob_error_wrap(byref(it))
        if err == SP_ERR_UNSUPPORTED:
            raise NotImplementedError("Non-relative patterns are unsupported")
        if err == SP_ERR_INVALID_ARG:
            raise ValueError(f"Unacceptable pattern: {os.fspath(pattern)!r}")
        results = []
        match = _SpPath()
        while _lib.sp_glob_next_wrap(byref(it), byref(match)):
            results.append(self._from_sp(match))
        _lib.sp_glob_end_wrap(byref(it))
        _check(_lib.sp_glob_error_wrap(byref(it)), self)
        return iter(results)

    def glob(self, pattern, *, case_sensitive=None, recurse_symlinks=False):
        """Iterate over this subtree and yield all existing files matching pattern."""
        return self._glob(_lib.sp_glob_begin_wrap, pattern, case_sensitive, recurse_symlinks)

    def rglob(self, pattern, *, case_sensitive=None, recurse_symlinks=False):
        """Recursively yield all existing files matching pattern."""
        return self._glob(_lib.sp_rglob_begin_wrap, pattern, case_sensitive, recurse_symlinks)

    def touch(self, mode=0o666, exist_ok=True):
        """Create file or update timestamps."""
        _check(_lib.sp_touch_wrap(byref(self._sp), mode, 1 if exist_ok else 0), self)

    def unlink(self, missing_ok=False):
        """Remove the file."""
        _check(_lib.sp_unlink_wrap(byref(self._sp), 1 if missing_ok else 0), self)

    def rmdir(self):
        """Remove the empty directory."""
        _check(_lib.sp_rmdir_wrap(byref(self._sp)), self)

    def rename(self, target):
        """Rename this file/directory to the given target."""
        return self._path_op(_lib.sp_rename_wrap, byref(self._sp_of(target)), target=target)

    def replace(self, target):
        """Replace target with this file (atomic operation)."""
        return self._path_op(_lib.sp_replace_wrap, byref(self._sp_of(target)), target=target)

    def copy(self, target, *, follow_symlinks=True, preserve_metadata=False):
        """Recursively copy this file or directory tree to the given destination."""
        flags = (SP_COPY_FOLLOW_SYMLINKS if follow_symlinks else 0) | (
            SP_COPY_PRESERVE_METADATA if preserve_metadata else 0)
        return self._path_op(_lib.sp_copy_wrap, byref(self._sp_of(target)), flags, target=target)

    def copy_into(self, target_dir, *, follow_symlinks=True, preserve_metadata=False):
        """Copy this file or directory tree into the given existing directory."""
        flags = (SP_COPY_FOLLOW_SYMLINKS if follow_symlinks else 0) | (
            SP_COPY_PRESERVE_METADATA if preserve_metadata else 0)
        return self._path_op(_lib.sp_copy_into_wrap, byref(self._sp_of(target_dir)), flags, target=target_dir)

    def move(self, target):
        """Recursively move this file or directory tree to the given destination."""
        return self._path_op(_lib.sp_move_wrap, byref(self._sp_of(target)), target=target)

    def move_into(self, target_dir):
        """Move this file or directory tree into the given existing directory."""
        return self._path_op(_lib.sp_move_into_wrap, byref(self._sp_of(target_dir)), target=target_dir)

    def chmod(self, mode, *, follow_symlinks=True):
        """Change the file mode (permissions)."""
        _check(_lib.sp_chmod_wrap(byref(self._sp), mode, 1 if follow_symlinks else 0), self)

    def lchmod(self, mode):
        """Like chmod(), but changes a symlink's own mode."""
        self.chmod(mode, follow_symlinks=False)

    def read_bytes(self):
        """Read the file contents as bytes."""
        size = self.stat().st_size
        while True:
            buf = create_string_buffer(max(size, 1))
            result = _SpIOResult()
            _lib.sp_read_file_wrap(byref(self._sp), buf, size, byref(result))
            if result.error != SP_ERR_TOO_LONG:
                _check(result.error, self)
                return buf.raw[:result.bytes]
            size = max(result.bytes, size * 2, 4096)  # the file grew, or its size is unknown

    def write_bytes(self, data):
        """Write bytes to the file."""
        if not isinstance(data, (bytes, bytearray, memoryview)):
            raise TypeError(f"expected bytes-like object, not {type(data).__name__}")
        data = bytes(data)
        result = _SpIOResult()
        _lib.sp_write_file_wrap(byref(self._sp), data, len(data), byref(result))
        _check(result.error, self)
        return result.bytes

    def read_text(self, encoding=None, errors=None):
        """Read the file contents as text."""
        return self.read_bytes().decode(encoding or 'utf-8', errors or 'strict')

    def write_text(self, data, encoding=None, errors=None, newline=None):
        """Write text to the file."""
        if not isinstance(data, str):
            raise TypeError('data must be str, not %s' % type(data).__name__)
        if newline is not None and newline not in ('', '\n', '\r', '\r\n'):
            raise ValueError('illegal newline value: %r' % (newline,))
        if newline is not None:
            if newline != '':
                data = data.replace('\n', newline)
        else:
            data = data.replace('\n', os.linesep)
        encoded = data.encode(encoding or 'utf-8', errors or 'strict')
        return self.write_bytes(encoded)

    def open(self, mode='r', buffering=-1, encoding=None, errors=None, newline=None):
        """Open the file pointed to by this path."""
        return io.open(str(self), mode, buffering, encoding, errors, newline)

    def _id_name(self, func, follow_symlinks):
        term = _SpTerm()
        func(byref(self._sp), 1 if follow_symlinks else 0, byref(term))
        _check(term.error, self)
        return _text(term)

    def owner(self, *, follow_symlinks=True):
        """Return the file owner name."""
        return self._id_name(_lib.sp_owner_wrap, follow_symlinks)

    def group(self, *, follow_symlinks=True):
        """Return the file group name."""
        return self._id_name(_lib.sp_group_wrap, follow_symlinks)

    def iterdir(self):
        """Yield path objects of directory contents."""
        it = _SpIterdirIter()
        _lib.sp_iterdir_begin_wrap(byref(self._sp), byref(it))
        _check(_lib.sp_iterdir_error_wrap(byref(it)), self)
        return self._iterdir(it)

    def _iterdir(self, it):
        try:
            out = _SpPath()
            while _lib.sp_iterdir_next_wrap(byref(it), byref(out)):
                yield self._from_sp(out)
            _check(_lib.sp_iterdir_error_wrap(byref(it)), self)
        finally:
            _lib.sp_iterdir_end_wrap(byref(it))

    def walk(self, top_down=True, on_error=None, follow_symlinks=False):
        """Walk the directory tree, yielding (dirpath, dirnames, filenames) like os.walk"""
        flags = (SP_WALK_TOP_DOWN if top_down else 0) | (SP_WALK_FOLLOW_SYMLINKS if follow_symlinks else 0)
        buf = create_string_buffer(_WALK_BUF_SIZE)
        it = _SpWalkIter()
        _lib.sp_walk_begin_wrap(byref(self._sp), flags, buf, len(buf), byref(it))
        pruned = []  # the dirnames given back to C, alive until the walk ends
        while entry := _lib.sp_walk_next_wrap(byref(it)):
            entry = entry.contents
            dirpath = self._from_sp(entry.dirpath)
            if entry.error != SP_OK:
                if on_error is not None:
                    on_error(_error(entry.error, dirpath))
                continue

            dirnames = [_decode(entry.dirnames[i]) for i in range(entry.dirname_count)]
            filenames = [_decode(entry.filenames[i]) for i in range(entry.filename_count)]
            yield dirpath, dirnames, filenames
            if top_down:
                pruned.append((c_char_p * max(len(dirnames), 1))(*[_encode(d) for d in dirnames]))
                entry.dirnames = ctypes.cast(pruned[-1], POINTER(c_char_p))
                entry.dirname_count = len(dirnames)
        _check(_lib.sp_walk_error_wrap(byref(it)), self)


class PosixPath(Path, PurePosixPath):
    __slots__ = ()


class WindowsPath(Path, PureWindowsPath):
    __slots__ = ()


__all__ = [
    'PurePath', 'PurePosixPath', 'PureWindowsPath',
    'Path', 'PosixPath', 'WindowsPath', 'UnsupportedOperation',
]


# ============ Checks and CPython's pathlib tests (python snakepath.py) ============

TOKEN_RE = re.compile(r"[A-Za-z_]\w*|[{}();]")
LITERAL_RE = re.compile(r"\"(?:\\.|[^\"\\\n])*\"|'(?:\\.|[^'\\\n])*'")


def preprocessed_library(header: pathlib.Path) -> str:
    """The header with its implementation and fluent API, as the compiler sees it"""
    defines = ["SNAKEPATH_IMPLEMENTATION", "SNAKEPATH_FLUENT", "SP_PATH_MAX=4096"]
    if os.name == "nt":
        cmd = ["cl", "/nologo", "/EP", "/TC", *[f"/D{d}" for d in defines], str(header)]
    else:
        cmd = [os.environ.get("CC", "cc"), "-E", "-P", "-x", "c", *[f"-D{d}" for d in defines], str(header)]
    return subprocess.run(cmd, capture_output=True, text=True, check=True).stdout


def call_graph(code: str) -> tuple[dict[str, set[str]], set[str]]:
    """The sp_ functions that preprocessed code defines, each mapped to the sp_ functions its body names (calls, or
    callbacks it hands to qsort), and the functions whose address is taken outside any function (the fluent table)"""
    tokens = TOKEN_RE.findall(LITERAL_RE.sub(" ", code))
    graph: dict[str, set[str]] = {}
    escaped: set[str] = set()
    owner = None
    depth = 0

    i = 0
    while i < len(tokens):
        tok = tokens[i]
        if tok == "{":
            depth += 1
        elif tok == "}":
            depth -= 1
            if depth == 0:
                owner = None
        elif owner is not None:
            graph[owner].add(tok)
        elif depth == 0 and tokens[i + 1:i + 2] == ["("]:
            # A name and its parameter list at file scope: a function definition when a body follows
            i += 1
            parens = 0
            while True:
                parens += {"(": 1, ")": -1}.get(tokens[i], 0)
                i += 1
                if parens == 0:
                    break
            if tokens[i:i + 1] == ["{"] and tok.startswith("sp_"):
                owner = tok
                graph.setdefault(tok, set())
            continue
        else:
            escaped.add(tok)
        i += 1

    graph = {name: (calls & graph.keys()) - {name} for name, calls in graph.items()}
    return graph, escaped & graph.keys()


def check_call_depth(header_path: pathlib.Path, max_depth: int) -> int:
    """At most max_depth snakepath frames on the stack below any public function, counting every function the
    preprocessed library defines; a fluent method is one more frame on top. A function calling itself is exempt."""
    graph, fluent = call_graph(preprocessed_library(header_path))
    chains: dict[str, list[str]] = {}

    def chain(name: str, active: tuple[str, ...] = ()) -> list[str]:
        """The longest chain of frames from name down"""
        if name in active:
            raise ValueError("mutual recursion: " + " -> ".join(active[active.index(name):] + (name,)))
        if name not in chains:
            below = max((chain(callee, active + (name,)) for callee in graph[name]), key=len, default=[])
            chains[name] = [name] + below
        return chains[name]

    limits = {name: max_depth + 1 for name in fluent}
    limits.update({name: max_depth for name in graph if not name.startswith("sp_priv_")})
    try:
        offenders = [chain(name) for name, limit in sorted(limits.items()) if len(chain(name)) > limit]
    except ValueError as err:
        print(f"call-depth check: FAILED ({err})")
        return 1

    deepest = max(len(path) for path in chains.values())
    print(f"call-depth check: functions={len(graph)} max_depth={deepest} limit={max_depth} "
          f"(fluent methods {max_depth + 1})")
    if not offenders:
        print("call-depth check: OK")
        return 0

    print("call-depth check: FAILED")
    for path in offenders:
        print(f"  {' -> '.join(path)}")
    return 1


def check_docs(root: pathlib.Path) -> int:
    ok = (root / "api_demo.c").read_text(encoding="utf-8") in (root / "README.md").read_text(encoding="utf-8")
    print("docs check: OK" if ok else "docs check: FAILED (README.md does not embed api_demo.c verbatim)")
    return 0 if ok else 1


def _outcome(fn):
    """What fn() returns, as comparable text, or the name of the exception it raises"""
    try:
        value = fn()
    except Exception as exc:  # the exception type is the behavior being compared
        return f"<{type(exc).__name__}>"
    if isinstance(value, (list, tuple)):
        return repr([str(v) for v in value])
    return repr(value) if isinstance(value, (bool, int)) else str(value)


def fuzz_inputs():
    """Deterministic path strings: every short string over an alphabet of anchors, dots and wildcards, token
    sequences for prefixes random generation never hits (UNC and device prefixes, drives, non-ASCII case pairs),
    and seeded random strings"""
    import itertools
    import random
    alphabet = ["/", "\\", "a", "c", ":", ".", "*", "?"]
    inputs = ["".join(t) for n in range(5) for t in itertools.product(alphabet, repeat=n)]
    tokens = ["\\\\?\\UNC\\", "\\\\.\\", "\\\\?\\", "//", "/", "\\", "c:", "C:", "..", ".", "a", "b.tar.gz", "~",
              "x.", ".y", "srv", "sh", "\xe9", "\xc9", "\u0130", "i", "\u03a3", "\u03c3", "\u00df", "[a-c]", "**"]
    inputs += ["".join(t) for n in (2, 3) for t in itertools.product(tokens, repeat=n)][::7]
    rng = random.Random(1729)
    wide = alphabet + ["b", "C", "~", "%", "|", " ", "[", "]", "!", "-", "\xe9", "\u03a3", "\u03c3", "\u0130", "\xdf",
                       "\u212a", "\u017f", "\U0001f600"]
    inputs += ["".join(rng.choice(wide) for _ in range(rng.randrange(1, 24))) for _ in range(1500)]
    return inputs


FUZZ_OTHERS = ["", ".", "a", "a/b", "/", "/a", "c:", "c:/", "c:a", "C:x", "//s/h", "//s/h/x", "\\\\?\\UNC\\s\\h\\x",
               "..", "../a", "~", "b.txt", ".hidden", "a.", "*", "**", "*.txt", "a/*", "[ab]*", "?", "x/./y",
               "\xe9", "\xc9", "\u03c3"]


def check_fuzz(limit=20):
    """Compare the bindings with the running Python's pathlib on generated paths, in both flavors"""
    import warnings
    warnings.simplefilter("ignore", DeprecationWarning)
    inputs = fuzz_inputs()
    mismatches = []
    checks = 0

    def compare(what, ours, real):
        nonlocal checks
        checks += 1
        a, b = _outcome(ours), _outcome(real)
        if a != b:
            mismatches.append(f"  {what}: snakepath {a!r}, pathlib {b!r}")

    for ours_cls, real_cls in [(PurePosixPath, pathlib.PurePosixPath), (PureWindowsPath, pathlib.PureWindowsPath)]:
        name = real_cls.__name__
        for i, s in enumerate(inputs):
            ours, real = ours_cls(s), real_cls(s)
            for attr in ["drive", "root", "anchor", "name", "stem", "suffix", "suffixes", "parts", "parent"]:
                compare(f"{name}({s!r}).{attr}", lambda: getattr(ours, attr), lambda: getattr(real, attr))
            compare(f"str({name}({s!r}))", lambda: str(ours), lambda: str(real))
            compare(f"{name}({s!r}).parents", lambda: list(ours.parents), lambda: list(real.parents))
            for method in ["is_absolute", "as_posix"]:
                compare(f"{name}({s!r}).{method}()", getattr(ours, method), getattr(real, method))
            if i % 20:
                continue  # binary operations on every 20th input keep the run short
            for o in FUZZ_OTHERS:
                oo, ro = ours_cls(o), real_cls(o)
                compare(f"{name}({s!r}) / {o!r}", lambda: ours / o, lambda: real / o)
                compare(f"{name}({o!r}) / {s!r}", lambda: oo / s, lambda: ro / s)
                for method in ["with_name", "with_stem", "with_suffix"]:
                    compare(f"{name}({s!r}).{method}({o!r})",
                            lambda: getattr(ours, method)(o), lambda: getattr(real, method)(o))
                compare(f"{name}({s!r}).relative_to({o!r})", lambda: ours.relative_to(o), lambda: real.relative_to(o))
                compare(f"{name}({s!r}).relative_to({o!r}, walk_up=True)",
                        lambda: ours.relative_to(o, walk_up=True), lambda: real.relative_to(o, walk_up=True))
                compare(f"{name}({s!r}).is_relative_to({o!r})", lambda: ours.is_relative_to(o),
                        lambda: real.is_relative_to(o))
                compare(f"{name}({s!r}) == {o!r}", lambda: ours == oo, lambda: real == ro)
                compare(f"{name}({s!r}) < {o!r}", lambda: ours < oo, lambda: real < ro)
                for cs in [None, True, False]:
                    compare(f"{name}({s!r}).match({o!r}, case_sensitive={cs})",
                            lambda: ours.match(o, case_sensitive=cs), lambda: real.match(o, case_sensitive=cs))
                    compare(f"{name}({o!r}).full_match({s!r}, case_sensitive={cs})",
                            lambda: oo.full_match(s, case_sensitive=cs), lambda: ro.full_match(s, case_sensitive=cs))
            if ours == ours_cls(s.upper()):
                compare(f"hash({name}({s!r})) == hash of its upper case", lambda: hash(ours) == hash(ours_cls(s.upper())),
                        lambda: True)

    print(f"fuzz check: {checks} comparisons with pathlib, {len(mismatches)} mismatches")
    for line in mismatches[:limit]:
        print(line)
    return 1 if mismatches else 0


THIS_DIR = pathlib.Path(__file__).resolve().parent
TEST_DIR = THIS_DIR / "cpython_tests"
# CPython's own pathlib tests: the 3.15 branch's Lib/test/test_pathlib/test_pathlib.py. The other
# files in that package only test Python-only machinery (pathlib.types protocols, zip/local backends).
CPYTHON_BRANCH = "3.15"
TEST_URL = f"https://raw.githubusercontent.com/python/cpython/{CPYTHON_BRANCH}/Lib/test/test_pathlib/test_pathlib.py"
TEST_FILE = "test_pathlib_3_15.py"
CLASS_TIMEOUT = 600  # seconds a test class may run before its worker dumps tracebacks and exits  # cached under a versioned name

# Tests expected to fail with specific error messages
# Format: {expected_error_substring: [(class_name, test_name), ...]}
# These exercise Python-only pathlib machinery with no C counterpart. Both flavors are listed where the host
# decides the flavor (PurePath, Path); the other flavor's tests are skipped, which counts as not passing.
_PURE = ["PurePathTest", "PurePosixPathTest", "PureWindowsPathTest", "PurePathSubclassTest"]
_PATH = ["PathTest", "PathSubclassTest", "PosixPathTest", "WindowsPathTest"]
EXPECTED_FAILURES = {
    # PurePath.as_uri() is deprecated in favor of Path.as_uri() - a warnings.warn() about Python classes
    "DeprecationWarning not triggered": [
        (cls, test) for cls in ["PurePathTest", "PurePosixPathTest", "PurePathSubclassTest"]
        for test in ["test_as_uri_posix", "test_as_uri_non_ascii", "test_as_uri_windows"]
    ] + [("PureWindowsPathTest", "test_as_uri_windows")],

    # Private CPython hooks: _parse_path (parser internals) and _delete (shutil.rmtree wrapper)
    "has no attribute '_parse_path'": [
        (cls, test) for cls in _PURE + _PATH
        for test in ["test_parse_path_common", "test_parse_path_posix", "test_parse_path_windows"]
    ],
    "has no attribute '_delete'": [
        (cls, f"test_delete_{what}") for cls in _PATH
        for what in ["dir", "file", "missing", "on_named_pipe", "does_not_choke_on_failing_lstat",
                     "inner_junction", "outer_junction", "symlink", "inner_symlink", "unwritable"]
    ],

    # Path.info (a caching PathInfo object) and pathlib.types protocols
    "has no attribute 'info'": [
        (cls, test) for cls in _PATH
        for test in ["test_info_exists_caching", "test_info_is_dir_caching", "test_info_is_file_caching",
                     "test_info_is_symlink_caching", "test_glob_posix"]
    ],
    "has no attribute 'types'": [(cls, "test_matches_writablepath_docstrings") for cls in _PATH],

    # Pickling: C-backed objects with __slots__, and the pathlib._local module path of 3.13 pickles
    "cannot be pickled": [("PurePathSubclassTest", "test_pickling_common")],
    "pathlib._local": [(cls, "test_unpicking_3_13") for cls in _PURE + _PATH],

    # Mocks of Python functions the C library never calls: parser.isjunction, os.getcwd, fast-copy syscalls
    "MagicMock": [(cls, "test_is_junction_true") for cls in _PATH],
    "!=": [(cls, test) for cls in _PATH for test in ["test_absolute_common", "test_absolute_windows"]],
    # Only POSIX has the patched fast-copy functions; on Windows the test passes
    "FileNotFoundError not raised": [
        (cls, "test_copy_error_handling") for cls in ["PathTest", "PathSubclassTest", "PosixPathTest"] if os.name != 'nt'
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
        download_file(TEST_URL, dest)


def _stub_module(name):
    """Module stand-in with a spec, which Python 3.15's ModuleNotFoundError formatting inspects via importlib.resources"""
    import importlib.machinery
    import types
    module = types.ModuleType(name)
    module.__spec__ = importlib.machinery.ModuleSpec(name, None)
    return module


def setup_pathlib_patch(testfn=None):
    """Patch pathlib module to use snakepath."""
    import types
    import tempfile

    # Create pathlib module with snakepath classes
    pathlib_pkg = _stub_module('pathlib')
    pathlib_pkg.PurePath = PurePath
    pathlib_pkg.PurePosixPath = PurePosixPath
    pathlib_pkg.PureWindowsPath = PureWindowsPath
    pathlib_pkg.Path = Path
    pathlib_pkg.PosixPath = PosixPath
    pathlib_pkg.WindowsPath = WindowsPath
    pathlib_pkg.UnsupportedOperation = UnsupportedOperation
    sys.modules['pathlib'] = pathlib_pkg

    # Stub test.support
    test_pkg = _stub_module('test')
    sys.modules['test'] = test_pkg

    test_support = _stub_module('test.support')
    test_support.is_emscripten = False
    test_support.is_wasi = False
    test_support.verbose = False
    test_support.cpython_only = lambda f: f
    test_support.is_android = False
    test_support.is_wasm32 = False
    is_root = hasattr(os, 'geteuid') and os.geteuid() == 0
    test_support.requires_root_user = unittest.skipUnless(is_root, "requires root")
    test_support.requires_non_root_user = unittest.skipIf(is_root, "requires non-root")

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

    @contextlib.contextmanager
    def infinite_recursion(max_depth=None):
        with set_recursion_limit(max_depth or 150):
            yield
    test_support.infinite_recursion = infinite_recursion

    class ImportHelper:
        @staticmethod
        def import_module(name):
            return __import__(name)

        @staticmethod
        def ensure_lazy_imports(*args, **kwargs):
            raise unittest.SkipTest("lazy imports: not meaningful in C")
    test_support.import_helper = ImportHelper()
    sys.modules['test.support'] = test_support

    # Stub test.support.os_helper
    import shutil
    os_helper = _stub_module('test.support.os_helper')
    if testfn is None:
        testfn = str(pathlib.Path(tempfile.gettempdir()) / 'test_pathlib_tmp')
    os_helper.TESTFN = testfn
    os_helper.FS_NONASCII = '\xe9'
    class FakePath:
        def __init__(self, path): self.path = path
        def __fspath__(self): return self.path
    os_helper.FakePath = FakePath
    def probe(make):
        """Whether this host can make the link or chmod that `make` tries in a scratch directory"""
        scratch = tempfile.mkdtemp()
        try:
            make(os.path.join(scratch, 'target'), os.path.join(scratch, 'link'))
            return True
        except (OSError, NotImplementedError, AttributeError):
            return False
        finally:
            shutil.rmtree(scratch, ignore_errors=True)

    def touch_then(action):
        def make(target, link):
            open(target, 'w').close()
            action(target, link)
        return make

    def chmod_sticks(target, link):
        os.chmod(target, 0o400)
        if os.stat(target).st_mode & 0o777 != 0o400:
            raise OSError('chmod does not stick')
        os.chmod(target, 0o600)

    # Windows symlink semantics (resolve through links, reparse tags, removing directory links) are not ported yet
    have_symlink = os.name != 'nt' and probe(touch_then(os.symlink))
    have_hardlink = probe(touch_then(lambda target, link: os.link(target, link)))
    have_chmod = probe(touch_then(chmod_sticks))
    os_helper.can_symlink = lambda: have_symlink
    os_helper.fs_is_case_insensitive = lambda path: False
    def rmtree(path):
        # Like CPython's os_helper.rmtree: tests leave unreadable dirs behind, so restore access and retry
        def onexc(func, p, exc):
            os.chmod(os.path.dirname(p) or '.', 0o700)
            os.chmod(p, 0o700)
            func(p) if func is not os.open else shutil.rmtree(p, onexc=onexc)
        if os.path.lexists(path):
            shutil.rmtree(path, onexc=onexc)
    os_helper.rmtree = rmtree
    # Skip decorators
    os_helper.skip_unless_xattr = unittest.skip("xattr not available")
    os_helper.skip_unless_working_chmod = unittest.skipUnless(have_chmod, "chmod does not work here")
    os_helper.skip_unless_symlink = unittest.skipUnless(have_symlink, "symlinks do not work here")
    os_helper.skip_if_dac_override = lambda f: f
    os_helper.skip_unless_hardlink = unittest.skipUnless(have_hardlink, "hard links do not work here")
    os_helper._longpath = lambda path: path

    @contextlib.contextmanager
    def change_cwd(path):
        old = os.getcwd()
        os.chdir(path)
        try:
            yield os.getcwd()
        finally:
            os.chdir(old)
    os_helper.change_cwd = change_cwd

    @contextlib.contextmanager
    def temp_umask(umask):
        old = os.umask(umask)
        try:
            yield
        finally:
            os.umask(old)
    os_helper.temp_umask = temp_umask

    @contextlib.contextmanager
    def subst_drive(path):
        raise unittest.SkipTest("subst drives not tested")
        yield
    os_helper.subst_drive = subst_drive
    class EnvironmentVarGuard(dict):
        """Edits os.environ (so the C library sees it) and restores it on exit"""
        def __enter__(self):
            self.saved = dict(os.environ)
            return self
        def __exit__(self, *args):
            os.environ.clear()
            os.environ.update(self.saved)
        def __setitem__(self, name, value): os.environ[name] = value
        def unset(self, *names):
            for name in names: os.environ.pop(name, None)
        def pop(self, name, *default): return os.environ.pop(name, *default)
    os_helper.EnvironmentVarGuard = EnvironmentVarGuard
    sys.modules['test.support.os_helper'] = os_helper



def run_single_class(class_info):
    """Run a single test class in a subprocess. Returns (class_name, results_dict)."""
    import os
    import tempfile
    import shutil

    class_name, module_name = class_info
    # A crash or hang in the C library shows up as a traceback instead of a silent stall
    import faulthandler
    faulthandler.enable()
    faulthandler.dump_traceback_later(CLASS_TIMEOUT, exit=True)

    # Create unique temp directory for this class
    unique_tmp = os.path.join(tempfile.gettempdir(), f'test_pathlib_{class_name}_{os.getpid()}')

    # Clear cached modules to force fresh import with new TESTFN
    for mod_name in list(sys.modules.keys()):
        if 'cpython_tests' in mod_name or mod_name == 'pathlib' or mod_name.startswith('test.'):
            del sys.modules[mod_name]

    # Re-setup environment in subprocess with unique temp dir
    setup_pathlib_patch(unique_tmp)
    sys.path.insert(0, str(TEST_DIR.parent))

    try:
        __import__(module_name)
        module = sys.modules[module_name]
        test_class = getattr(module, class_name)
    except Exception as e:
        return (class_name, {'error': str(e), 'tests_run': 0, 'failures': [], 'errors': [], 'skipped': []})

    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    for method_name in loader.getTestCaseNames(test_class):
        suite.addTest(test_class(method_name))

    # Run with a stream that captures output
    import io
    stream = io.StringIO()
    runner = unittest.TextTestRunner(stream=stream, verbosity=0)
    result = runner.run(suite)

    # Cleanup unique temp directory
    if os.path.exists(unique_tmp):
        shutil.rmtree(unique_tmp, ignore_errors=True)

    # Convert result to serializable dict
    def test_to_tuple(test, tb):
        test = getattr(test, 'test_case', test)  # a failed subTest reports its parent test
        return (test.__class__.__name__, test._testMethodName, tb)

    return (class_name, {
        'tests_run': result.testsRun,
        'failures': [test_to_tuple(t, tb) for t, tb in result.failures],
        'errors': [test_to_tuple(t, tb) for t, tb in result.errors],
        'skipped': [test_to_tuple(t, tb) for t, tb in result.skipped],
    })


def run_tests():
    """Run CPython tests against snakepath."""
    import multiprocessing
    from concurrent.futures import ProcessPoolExecutor
    import os

    setup_tests()
    setup_pathlib_patch()

    # Add test dir to path
    sys.path.insert(0, str(TEST_DIR.parent))

    print("\nRunning CPython pathlib tests against snakepath (parallel by class)\n")

    # Discover test classes
    module_name = f"cpython_tests.{TEST_FILE[:-3]}"
    test_classes = []

    try:
        __import__(module_name)
        module = sys.modules[module_name]

        loader = unittest.TestLoader()
        for name in dir(module):
            obj = getattr(module, name)
            if isinstance(obj, type) and issubclass(obj, unittest.TestCase):
                if obj is unittest.TestCase:
                    continue
                # Check it has test methods
                if loader.getTestCaseNames(obj):
                    test_classes.append((name, module_name))
    except Exception as e:
        print(f"  ERROR loading {TEST_FILE}: {e}")
        import traceback
        traceback.print_exc()
        return 1

    print(f"Found {len(test_classes)} test classes, running in parallel...\n")

    # Run test classes in parallel using spawn context for clean isolation
    num_workers = min(len(test_classes), os.cpu_count() or 4)
    # Unlike multiprocessing.Pool, the executor raises BrokenProcessPool if a worker dies instead of waiting forever
    with ProcessPoolExecutor(num_workers, mp_context=multiprocessing.get_context('spawn')) as pool:
        results_list = list(pool.map(run_single_class, test_classes))

    # Aggregate (class_name, method_name, traceback) tuples from all classes
    tests_run, failures, errors, skipped = 0, [], [], []
    for class_name, res in results_list:
        if 'error' in res:
            print(f"  ERROR in {class_name}: {res['error']}")
            continue
        tests_run += res['tests_run']
        failures += res['failures']
        errors += res['errors']
        skipped += res['skipped']

    # Expected failures must fail for their documented reason; anything else is unexpected
    expected_by_reason = {}  # reason -> list of test names
    def unexpected(results):
        out = []
        for cls, method, tb in results:
            reason = _EXPECTED_FAILURES_BY_TEST.get((cls, method))
            if reason is not None and reason in tb:
                expected_by_reason.setdefault(reason, []).append(f"{cls}.{method}")
            else:
                out.append((f"{cls}.{method}", tb))
        return out
    unexpected_failures = unexpected(failures)
    unexpected_errors = unexpected(errors)

    # Expected failures that neither failed nor were skipped passed unexpectedly
    not_passed = {(cls, method) for cls, method, _ in failures + errors + skipped}
    unexpected_successes = [f"{cls}.{method}" for cls, method in _EXPECTED_FAILURES_BY_TEST
                            if (cls, method) not in not_passed]

    # Summary
    expected_total = sum(len(tests) for tests in expected_by_reason.values())
    print(f"\nRan {tests_run} tests, {expected_total} expected failures")

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



def main():
    # Windows consoles default to cp1252, which cannot print test names like 'İ'
    if sys.platform == 'win32' and sys.stdout.encoding != 'utf-8':
        import io
        sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
        sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')
    checks_failed = check_call_depth(THIS_DIR / "snakepath.h", 4) | check_docs(THIS_DIR) | check_fuzz()
    return run_tests() or checks_failed


if __name__ == "__main__":
    sys.exit(main())
