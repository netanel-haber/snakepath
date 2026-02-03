"""
snakepath - Python bindings for the snakepath C library.

Provides PurePath, PurePosixPath, and PureWindowsPath classes compatible
with Python's pathlib interface.
"""

import ctypes
import os
import pathlib
from ctypes import c_char_p, c_size_t, c_int, POINTER, Structure, byref, create_string_buffer

# Find and load the shared library
_lib_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
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

# Get structure sizes from library
SP_PATH_MAX = _lib.sp_path_max()
SP_MAX_SUFFIXES = _lib.sp_max_suffixes()
_sizeof_path = _lib.sp_sizeof_path()
_sizeof_parts_iter = _lib.sp_sizeof_parts_iter()
_sizeof_parents_iter = _lib.sp_sizeof_parents_iter()

# Flavor constants
SP_FLAVOR_NATIVE = 0
SP_FLAVOR_POSIX = 1
SP_FLAVOR_WINDOWS = 2

# Error codes (from C library)
SP_ERR_NONE = _lib.sp_err_none()
SP_ERR_NOT_RELATIVE = _lib.sp_err_not_relative()
SP_ERR_NO_NAME = _lib.sp_err_no_name()
SP_ERR_INVALID_ARG = _lib.sp_err_invalid_arg()
SP_MATCH_YES = _lib.sp_match_yes()
SP_MATCH_NO = _lib.sp_match_no()
SP_MATCH_ERR_EMPTY = _lib.sp_match_err_empty()
SP_MATCH_ERR_INVALID = _lib.sp_match_err_invalid()

# mkdir result codes
SP_MKDIR_OK = _lib.sp_mkdir_ok()
SP_MKDIR_ERR_EXISTS = _lib.sp_mkdir_err_exists()
SP_MKDIR_ERR_NOT_FOUND = _lib.sp_mkdir_err_not_found()
SP_MKDIR_ERR_NOT_DIR = _lib.sp_mkdir_err_not_dir()
SP_MKDIR_ERR_PERMISSION = _lib.sp_mkdir_err_permission()
SP_MKDIR_ERR_OTHER = _lib.sp_mkdir_err_other()
SP_MKDIR_ERR_EXISTS_NOT_DIR = _lib.sp_mkdir_err_exists_not_dir()

# Case sensitivity options for glob/match
SP_CASE_PLATFORM_DEFAULT = _lib.sp_case_platform_default()
SP_CASE_SENSITIVE = _lib.sp_case_sensitive()
SP_CASE_INSENSITIVE = _lib.sp_case_insensitive()


class _SpPath(Structure):
    """C SpPath structure"""
    _fields_ = [
        ("buf", ctypes.c_char * SP_PATH_MAX),
        ("len", c_size_t),
        ("flavor", c_int),
    ]


class _SpPartsIter(Structure):
    _fields_ = [("_data", ctypes.c_char * _sizeof_parts_iter)]


class _SpParentsIter(Structure):
    _fields_ = [("_data", ctypes.c_char * _sizeof_parents_iter)]


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
        ("valid", ctypes.c_bool),
    ]

    def __eq__(self, other):
        if isinstance(other, _SpStatResult):
            return bool(_lib.sp_stat_eq_wrap(byref(self), byref(other)))
        if hasattr(other, 'st_mode'):
            other_sp = _SpStatResult(**{n: getattr(other, n) for n, _ in _SpStatResult._fields_ if n != 'valid'}, valid=True)
            return bool(_lib.sp_stat_eq_wrap(byref(self), byref(other_sp)))
        return NotImplemented

    def __repr__(self):
        return (f"os.stat_result(st_mode={self.st_mode}, st_ino={self.st_ino}, "
                f"st_dev={self.st_dev}, st_nlink={self.st_nlink}, st_uid={self.st_uid}, "
                f"st_gid={self.st_gid}, st_size={self.st_size}, "
                f"st_atime={self.st_atime}, st_mtime={self.st_mtime}, st_ctime={self.st_ctime})")


# ============ Signature helpers ============

def _sig(name, argtypes, restype=None):
    fn = getattr(_lib, name)
    fn.argtypes = argtypes
    fn.restype = restype

_PP = POINTER(_SpPath)
_PStat = POINTER(_SpStatResult)

# Bulk signature setup by pattern
for n in ['drive', 'root', 'anchor', 'name', 'stem', 'suffix']:
    _sig(f'sp_{n}_wrap', [_PP, POINTER(c_char_p), POINTER(c_size_t)])

for n in ['parent', 'absolute']:
    _sig(f'sp_{n}_wrap', [_PP, _PP])

for n in ['with_name', 'with_stem', 'with_suffix', 'join_one']:
    _sig(f'sp_{n}_wrap', [_PP, c_char_p, _PP])

for n in ['joinpath', 'relative_to', 'relative_to_walk_up']:
    _sig(f'sp_{n}_wrap', [_PP, _PP, _PP])

for n in ['is_absolute', 'is_reserved', 'is_file', 'is_dir', 'exists',
          'path_is_error', 'path_error_code', 'relative_to_is_error']:
    _sig(f'sp_{n}_wrap', [_PP], c_int)

for n in ['is_relative_to', 'path_eq']:
    _sig(f'sp_{n}_wrap', [_PP, _PP], c_int)

for n in ['path_hash', 'parts_count', 'parents_count']:
    _sig(f'sp_{n}_wrap', [_PP], ctypes.c_ulong if n == 'path_hash' else c_size_t)

# Remaining signatures
_sig('sp_path_new_wrap', [c_char_p, c_int, _PP])
_sig('sp_path_new_len_wrap', [POINTER(ctypes.c_char), c_size_t, c_int, _PP])
_sig('sp_path_convert_wrap', [c_char_p, c_int, c_int, _PP])
_sig('sp_path_copy_wrap', [_PP, _PP])
_sig('sp_str_wrap', [_PP], c_char_p)
_sig('sp_join_one_len_wrap', [_PP, POINTER(ctypes.c_char), c_size_t, _PP])
_sig('sp_suffixes_wrap', [_PP, POINTER(c_char_p), POINTER(c_size_t), c_size_t], c_size_t)
_sig('sp_parts_iter_begin_wrap', [_PP, POINTER(_SpPartsIter)])
_sig('sp_parts_iter_next_wrap', [POINTER(_SpPartsIter), POINTER(c_char_p), POINTER(c_size_t)], c_int)
_sig('sp_parents_iter_begin_wrap', [_PP, POINTER(_SpParentsIter)])
_sig('sp_parents_iter_next_wrap', [POINTER(_SpParentsIter), _PP], c_int)
_sig('sp_is_relative_to_parts_wrap', [_PP, POINTER(c_char_p)], c_int)
_sig('sp_relative_to_parts_wrap', [_PP, POINTER(c_char_p), c_int, _PP])
_sig('sp_as_uri_wrap', [_PP, c_char_p, c_size_t], c_size_t)
_sig('sp_as_posix_wrap', [_PP, c_char_p, c_size_t])
_sig('sp_cwd_wrap', [c_int, _PP])
_sig('sp_path_cmp_wrap', [_PP, _PP], c_int)
_sig('sp_match_wrap', [_PP, c_char_p], c_int)
_sig('sp_match_ex_wrap', [_PP, c_char_p, c_int], c_int)
_sig('sp_stat_wrap', [_PP, _PStat])
_sig('sp_stat_eq_wrap', [_PStat, _PStat], c_int)
_sig('sp_mkdir_wrap', [_PP, ctypes.c_uint, c_int, c_int], c_int)
# Glob callback type: bool (*)(const SpPath *match, void *user_data)
_GlobCallback = ctypes.CFUNCTYPE(c_int, _PP, ctypes.c_void_p)
_sig('sp_glob_wrap', [_PP, c_char_p, c_int, _GlobCallback, ctypes.c_void_p], c_int)
_sig('sp_rglob_wrap', [_PP, c_char_p, c_int, _GlobCallback, ctypes.c_void_p], c_int)


# ============ Property descriptors ============

class _StrViewProp:
    """Descriptor for string-view properties (drive, root, anchor, name, stem, suffix)"""
    __slots__ = ('_func',)
    def __init__(self, name):
        self._func = getattr(_lib, f'sp_{name}_wrap')
    def __get__(self, obj, objtype=None):
        if obj is None: return self
        data, length = c_char_p(), c_size_t()
        self._func(byref(obj._sp), byref(data), byref(length))
        return '' if not data.value or not length.value else data.value[:length.value].decode('utf-8')


class _PathProp:
    """Descriptor for path-returning properties (parent)"""
    __slots__ = ('_func',)
    def __init__(self, name):
        self._func = getattr(_lib, f'sp_{name}_wrap')
    def __get__(self, obj, objtype=None):
        if obj is None: return self
        result = obj.__class__.__new__(obj.__class__)
        result._sp = _SpPath()
        self._func(byref(obj._sp), byref(result._sp))
        return result


class _BoolProp:
    """Descriptor for boolean properties"""
    __slots__ = ('_func',)
    def __init__(self, name):
        self._func = getattr(_lib, f'sp_{name}_wrap')
    def __get__(self, obj, objtype=None):
        if obj is None: return self
        return bool(self._func(byref(obj._sp)))


# ============ Helper functions ============

def _encode(s):
    """Encode string to bytes for C library"""
    if isinstance(s, bytes):
        raise TypeError(
            "argument should be a str or an os.PathLike object "
            "where __fspath__ returns a str, not 'bytes'"
        )
    return s.encode('utf-8', errors='surrogatepass') if s else b''


def _encode_buf(s):
    """Encode string to a ctypes buffer that preserves embedded nulls"""
    encoded = _encode(s) if isinstance(s, str) else (s if s else b'')
    return create_string_buffer(encoded, len(encoded))


def _decode(b):
    """Decode bytes from C library to string"""
    return '' if b is None else (b.decode('utf-8') if isinstance(b, bytes) else b)


def _get_pathlib_flavor(obj):
    """Get the flavor constant for a pathlib path object"""
    if isinstance(obj, pathlib.PureWindowsPath):
        return SP_FLAVOR_WINDOWS
    elif isinstance(obj, pathlib.PurePosixPath):
        return SP_FLAVOR_POSIX
    return None


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
                p = self._path.__class__.__new__(self._path.__class__)
                p._sp = _SpPath()
                _lib.sp_path_copy_wrap(byref(out), byref(p._sp))
                parents.append(p)
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
    drive = _StrViewProp('drive')
    root = _StrViewProp('root')
    anchor = _StrViewProp('anchor')
    name = _StrViewProp('name')
    stem = _StrViewProp('stem')
    suffix = _StrViewProp('suffix')

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

    def __init__(self, *args, **kwargs):
        self._sp = _SpPath()
        for arg in args:
            if isinstance(arg, bytes):
                raise TypeError(
                    "argument should be a str or an os.PathLike object "
                    "where __fspath__ returns a str, not 'bytes'"
                )

        if not args:
            _lib.sp_path_new_len_wrap(_encode_buf(b''), 0, self._flavor, byref(self._sp))
        elif len(args) == 1:
            arg = args[0]
            if isinstance(arg, PurePath):
                if arg._flavor == self._flavor:
                    _lib.sp_path_copy_wrap(byref(arg._sp), byref(self._sp))
                else:
                    _lib.sp_path_convert_wrap(_encode(str(arg)), arg._flavor, self._flavor, byref(self._sp))
                return
            src_flavor = _get_pathlib_flavor(arg)
            if src_flavor is not None:
                _lib.sp_path_convert_wrap(_encode(str(arg)), src_flavor, self._flavor, byref(self._sp))
            else:
                path_buf = _encode_buf(os.fspath(arg) if hasattr(os, 'fspath') else str(arg))
                _lib.sp_path_new_len_wrap(path_buf, len(path_buf.raw), self._flavor, byref(self._sp))
        else:
            self._init_multi(args)

    def _init_multi(self, args):
        """Initialize from multiple path segments"""
        first = args[0]
        if isinstance(first, PurePath):
            if first._flavor == self._flavor:
                _lib.sp_path_copy_wrap(byref(first._sp), byref(self._sp))
            else:
                _lib.sp_path_convert_wrap(_encode(str(first)), first._flavor, self._flavor, byref(self._sp))
        else:
            src_flavor = _get_pathlib_flavor(first)
            if src_flavor is not None:
                _lib.sp_path_convert_wrap(_encode(str(first)), src_flavor, self._flavor, byref(self._sp))
            else:
                first_buf = _encode_buf(os.fspath(first) if hasattr(os, 'fspath') else str(first))
                _lib.sp_path_new_len_wrap(first_buf, len(first_buf.raw), self._flavor, byref(self._sp))

        for arg in args[1:]:
            if isinstance(arg, PurePath):
                if arg._flavor == self._flavor:
                    _lib.sp_joinpath_wrap(byref(self._sp), byref(arg._sp), byref(self._sp))
                else:
                    tmp = _SpPath()
                    _lib.sp_path_convert_wrap(_encode(str(arg)), arg._flavor, self._flavor, byref(tmp))
                    _lib.sp_joinpath_wrap(byref(self._sp), byref(tmp), byref(self._sp))
            else:
                src_flavor = _get_pathlib_flavor(arg)
                if src_flavor is not None:
                    tmp = _SpPath()
                    _lib.sp_path_convert_wrap(_encode(str(arg)), src_flavor, self._flavor, byref(tmp))
                    _lib.sp_joinpath_wrap(byref(self._sp), byref(tmp), byref(self._sp))
                else:
                    arg_buf = _encode_buf(os.fspath(arg) if hasattr(os, 'fspath') else str(arg))
                    _lib.sp_join_one_len_wrap(byref(self._sp), arg_buf, len(arg_buf.raw), byref(self._sp))

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

    def __lt__(self, other):
        if not isinstance(other, PurePath): return NotImplemented
        return _lib.sp_path_cmp_wrap(byref(self._sp), byref(other._sp)) < 0

    def __le__(self, other):
        if not isinstance(other, PurePath): return NotImplemented
        return _lib.sp_path_cmp_wrap(byref(self._sp), byref(other._sp)) <= 0

    def __gt__(self, other):
        if not isinstance(other, PurePath): return NotImplemented
        return _lib.sp_path_cmp_wrap(byref(self._sp), byref(other._sp)) > 0

    def __ge__(self, other):
        if not isinstance(other, PurePath): return NotImplemented
        return _lib.sp_path_cmp_wrap(byref(self._sp), byref(other._sp)) >= 0

    def __truediv__(self, other):
        return self.joinpath(other)

    def __rtruediv__(self, other):
        return self.__class__(other, self)

    @property
    def suffixes(self):
        data_arr = (c_char_p * SP_MAX_SUFFIXES)()
        len_arr = (c_size_t * SP_MAX_SUFFIXES)()
        count = _lib.sp_suffixes_wrap(byref(self._sp), data_arr, len_arr, SP_MAX_SUFFIXES)
        return [data_arr[i][:len_arr[i]].decode('utf-8')
                for i in range(count) if data_arr[i] and len_arr[i] > 0]

    @property
    def parents(self):
        return _PathParents(self)

    @property
    def parts(self):
        parts = []
        it = _SpPartsIter()
        _lib.sp_parts_iter_begin_wrap(byref(self._sp), byref(it))
        data, length = c_char_p(), c_size_t()
        while _lib.sp_parts_iter_next_wrap(byref(it), byref(data), byref(length)):
            if data.value and length.value > 0:
                parts.append(data.value[:length.value].decode('utf-8'))
        return tuple(parts)

    def as_posix(self):
        buf = create_string_buffer(SP_PATH_MAX)
        _lib.sp_as_posix_wrap(byref(self._sp), buf, SP_PATH_MAX)
        return buf.value.decode('utf-8')

    def as_uri(self):
        if not self.is_absolute():
            raise ValueError("relative path can't be expressed as a file URI")
        buf = create_string_buffer(SP_PATH_MAX * 3)
        result_len = _lib.sp_as_uri_wrap(byref(self._sp), buf, SP_PATH_MAX * 3)
        if result_len == 0:
            raise ValueError("relative path can't be expressed as a file URI")
        return buf.value.decode('utf-8')

    def is_absolute(self):
        return bool(_lib.sp_is_absolute_wrap(byref(self._sp)))

    def is_relative_to(self, *args):
        if not args:
            raise TypeError("is_relative_to() requires at least 1 argument")
        for arg in args:
            if isinstance(arg, bytes):
                raise TypeError("argument should be a str or os.PathLike object, not bytes")
        parts = [_encode(os.fspath(a) if hasattr(a, '__fspath__') else str(a)) for a in args]
        parts.append(None)
        parts_arr = (c_char_p * len(parts))(*parts)
        return bool(_lib.sp_is_relative_to_parts_wrap(byref(self._sp), parts_arr))

    def relative_to(self, *args, walk_up=False):
        if not args:
            raise TypeError("relative_to() requires at least 1 argument")
        for arg in args:
            if isinstance(arg, bytes):
                raise TypeError("argument should be a str or os.PathLike object, not bytes")

        parts = [_encode(os.fspath(a) if hasattr(a, '__fspath__') else str(a)) for a in args]
        parts.append(None)
        parts_arr = (c_char_p * len(parts))(*parts)

        result = self.__class__.__new__(self.__class__)
        result._sp = _SpPath()
        _lib.sp_relative_to_parts_wrap(byref(self._sp), parts_arr, 1 if walk_up else 0, byref(result._sp))

        if _lib.sp_relative_to_is_error_wrap(byref(result._sp)):
            other_str = str(self.__class__(*args))
            raise ValueError(f"{str(self)!r} is not relative to {other_str!r}")
        return result

    def joinpath(self, *others):
        for other in others:
            if isinstance(other, bytes):
                raise TypeError(
                    "argument should be a str or an os.PathLike object "
                    "where __fspath__ returns a str, not 'bytes'"
                )
        result = self.__class__.__new__(self.__class__)
        result._sp = _SpPath()
        _lib.sp_path_copy_wrap(byref(self._sp), byref(result._sp))

        for other in others:
            if isinstance(other, PurePath):
                _lib.sp_joinpath_wrap(byref(result._sp), byref(other._sp), byref(result._sp))
            else:
                other_buf = _encode_buf(os.fspath(other) if hasattr(os, 'fspath') else str(other))
                _lib.sp_join_one_len_wrap(byref(result._sp), other_buf, len(other_buf.raw), byref(result._sp))
        return result

    def _with_field(self, func_name, value, type_name, err_no_name, err_invalid):
        if not isinstance(value, str):
            raise TypeError(f"expected str, not {type(value).__name__}")
        result = self.__class__.__new__(self.__class__)
        result._sp = _SpPath()
        getattr(_lib, f'sp_{func_name}_wrap')(byref(self._sp), _encode(value), byref(result._sp))
        err = _lib.sp_path_error_code_wrap(byref(result._sp))
        if err == SP_ERR_NO_NAME:
            raise ValueError(f"{self!r} has an empty name")
        if err == SP_ERR_INVALID_ARG:
            raise ValueError(f"Invalid {type_name} {value!r}")
        return result

    def with_name(self, name):
        return self._with_field('with_name', name, 'name', SP_ERR_NO_NAME, SP_ERR_INVALID_ARG)

    def with_stem(self, stem):
        return self._with_field('with_stem', stem, 'stem', SP_ERR_NO_NAME, SP_ERR_INVALID_ARG)

    def with_suffix(self, suffix):
        return self._with_field('with_suffix', suffix, 'suffix', SP_ERR_NO_NAME, SP_ERR_INVALID_ARG)

    def match(self, pattern, *, case_sensitive=None):
        if isinstance(pattern, bytes):
            raise TypeError("argument should be a str or os.PathLike object, not bytes")
        cs_value = -1 if case_sensitive is None else (1 if case_sensitive else 0)
        result = _lib.sp_match_ex_wrap(byref(self._sp), _encode(str(pattern)), cs_value)
        if result == SP_MATCH_ERR_EMPTY:
            raise ValueError("empty pattern")
        if result == SP_MATCH_ERR_INVALID:
            raise ValueError(f"Invalid pattern: {pattern!r}")
        return result == SP_MATCH_YES

    def is_reserved(self):
        return bool(_lib.sp_is_reserved_wrap(byref(self._sp)))


class PurePosixPath(PurePath):
    """Pure path with POSIX semantics."""
    __slots__ = ()
    _flavor = SP_FLAVOR_POSIX
    parser = __import__('posixpath')

    def __new__(cls, *args, **kwargs):
        return object.__new__(cls)


class PureWindowsPath(PurePath):
    """Pure path with Windows semantics."""
    __slots__ = ()
    _flavor = SP_FLAVOR_WINDOWS
    parser = __import__('ntpath')

    def __new__(cls, *args, **kwargs):
        return object.__new__(cls)


# ============ Concrete Path classes ============

class Path(PurePath):
    """Path object with I/O operations."""
    __slots__ = ()

    def __new__(cls, *args, **kwargs):
        if cls is Path:
            cls = PosixPath if os.name != 'nt' else WindowsPath
        return object.__new__(cls)

    @classmethod
    def cwd(cls):
        result = cls.__new__(cls)
        result._sp = _SpPath()
        _lib.sp_cwd_wrap(result._flavor, byref(result._sp))
        return result

    def absolute(self):
        result = self.__class__.__new__(self.__class__)
        result._sp = _SpPath()
        _lib.sp_absolute_wrap(byref(self._sp), byref(result._sp))
        return result

    def is_file(self):
        return bool(_lib.sp_is_file_wrap(byref(self._sp)))

    def is_dir(self):
        return bool(_lib.sp_is_dir_wrap(byref(self._sp)))

    def exists(self):
        return bool(_lib.sp_exists_wrap(byref(self._sp)))

    def stat(self, *, follow_symlinks=True):
        assert follow_symlinks, "lstat (follow_symlinks=False) not yet implemented"
        result = _SpStatResult()
        _lib.sp_stat_wrap(byref(self._sp), byref(result))
        if not result.valid:
            raise FileNotFoundError(2, "No such file or directory", str(self))
        return result

    def mkdir(self, mode=0o777, parents=False, exist_ok=False):
        result = _lib.sp_mkdir_wrap(byref(self._sp), mode, 1 if parents else 0, 1 if exist_ok else 0)
        if result == SP_MKDIR_OK:
            return
        path_str = str(self)
        if result == SP_MKDIR_ERR_EXISTS or result == SP_MKDIR_ERR_EXISTS_NOT_DIR:
            raise FileExistsError(17, "File exists", path_str)
        elif result == SP_MKDIR_ERR_NOT_FOUND:
            raise FileNotFoundError(2, "No such file or directory", path_str)
        elif result == SP_MKDIR_ERR_NOT_DIR:
            raise NotADirectoryError(20, "Not a directory", path_str)
        elif result == SP_MKDIR_ERR_PERMISSION:
            raise PermissionError(13, "Permission denied", path_str)
        else:
            raise OSError(0, "Unknown error", path_str)

    def glob(self, pattern, *, case_sensitive=None):
        """Iterate over this subtree and yield all existing files matching pattern."""
        if isinstance(pattern, bytes):
            raise TypeError("argument should be a str or os.PathLike object, not bytes")
        pattern_str = str(pattern)
        if not pattern_str:
            raise ValueError("Unacceptable pattern: ''")

        # Convert case_sensitive to enum constant
        if case_sensitive is None:
            cs = SP_CASE_PLATFORM_DEFAULT
        elif case_sensitive:
            cs = SP_CASE_SENSITIVE
        else:
            cs = SP_CASE_INSENSITIVE

        # Collect results via callback
        results = []
        path_class = self.__class__

        @_GlobCallback
        def collect(match_ptr, _user_data):
            result = path_class.__new__(path_class)
            result._sp = _SpPath()
            _lib.sp_path_copy_wrap(match_ptr, byref(result._sp))
            results.append(result)
            return 1  # continue

        _lib.sp_glob_wrap(byref(self._sp), _encode(pattern_str), cs, collect, None)
        return iter(results)

    def rglob(self, pattern, *, case_sensitive=None):
        """Recursively yield all existing files matching pattern."""
        if isinstance(pattern, bytes):
            raise TypeError("argument should be a str or os.PathLike object, not bytes")
        pattern_str = str(pattern)

        # Convert case_sensitive to enum constant
        if case_sensitive is None:
            cs = SP_CASE_PLATFORM_DEFAULT
        elif case_sensitive:
            cs = SP_CASE_SENSITIVE
        else:
            cs = SP_CASE_INSENSITIVE

        # Collect results via callback
        results = []
        path_class = self.__class__

        @_GlobCallback
        def collect(match_ptr, _user_data):
            result = path_class.__new__(path_class)
            result._sp = _SpPath()
            _lib.sp_path_copy_wrap(match_ptr, byref(result._sp))
            results.append(result)
            return 1  # continue

        _lib.sp_rglob_wrap(byref(self._sp), _encode(pattern_str), cs, collect, None)
        return iter(results)


class PosixPath(Path, PurePosixPath):
    __slots__ = ()
    def __new__(cls, *args, **kwargs):
        return object.__new__(cls)


class WindowsPath(Path, PureWindowsPath):
    __slots__ = ()
    def __new__(cls, *args, **kwargs):
        return object.__new__(cls)


__all__ = [
    'PurePath', 'PurePosixPath', 'PureWindowsPath',
    'Path', 'PosixPath', 'WindowsPath',
]
