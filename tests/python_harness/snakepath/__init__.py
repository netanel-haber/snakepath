"""
snakepath - Python bindings for the snakepath C library.

Provides PurePath, PurePosixPath, and PureWindowsPath classes compatible
with Python's pathlib interface.
"""

import ctypes
import os
import pathlib
import sys
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


class _SpPath(Structure):
    """C SpPath structure"""
    _fields_ = [
        ("buf", ctypes.c_char * SP_PATH_MAX),
        ("len", c_size_t),
        ("flavor", c_int),
    ]


class _SpPartsIter(Structure):
    """C SpPartsIter structure"""
    _fields_ = [
        ("_data", ctypes.c_char * _sizeof_parts_iter),
    ]


class _SpParentsIter(Structure):
    """C SpParentsIter structure"""
    _fields_ = [
        ("_data", ctypes.c_char * _sizeof_parents_iter),
    ]


# Function signatures
_lib.sp_path_new_wrap.argtypes = [c_char_p, c_int, POINTER(_SpPath)]
_lib.sp_path_new_wrap.restype = None

_lib.sp_path_convert_wrap.argtypes = [c_char_p, c_int, c_int, POINTER(_SpPath)]
_lib.sp_path_convert_wrap.restype = None

_lib.sp_path_copy_wrap.argtypes = [POINTER(_SpPath), POINTER(_SpPath)]
_lib.sp_path_copy_wrap.restype = None

_lib.sp_str_wrap.argtypes = [POINTER(_SpPath)]
_lib.sp_str_wrap.restype = c_char_p

_lib.sp_drive_wrap.argtypes = [POINTER(_SpPath), POINTER(c_char_p), POINTER(c_size_t)]
_lib.sp_drive_wrap.restype = None

_lib.sp_root_wrap.argtypes = [POINTER(_SpPath), POINTER(c_char_p), POINTER(c_size_t)]
_lib.sp_root_wrap.restype = None

_lib.sp_anchor_wrap.argtypes = [POINTER(_SpPath), POINTER(c_char_p), POINTER(c_size_t)]
_lib.sp_anchor_wrap.restype = None

_lib.sp_name_wrap.argtypes = [POINTER(_SpPath), POINTER(c_char_p), POINTER(c_size_t)]
_lib.sp_name_wrap.restype = None

_lib.sp_stem_wrap.argtypes = [POINTER(_SpPath), POINTER(c_char_p), POINTER(c_size_t)]
_lib.sp_stem_wrap.restype = None

_lib.sp_suffix_wrap.argtypes = [POINTER(_SpPath), POINTER(c_char_p), POINTER(c_size_t)]
_lib.sp_suffix_wrap.restype = None

_lib.sp_suffixes_wrap.argtypes = [POINTER(_SpPath), POINTER(c_char_p), POINTER(c_size_t), c_size_t]
_lib.sp_suffixes_wrap.restype = c_size_t

_lib.sp_parent_wrap.argtypes = [POINTER(_SpPath), POINTER(_SpPath)]
_lib.sp_parent_wrap.restype = None

_lib.sp_parts_count_wrap.argtypes = [POINTER(_SpPath)]
_lib.sp_parts_count_wrap.restype = c_size_t

_lib.sp_parts_iter_begin_wrap.argtypes = [POINTER(_SpPath), POINTER(_SpPartsIter)]
_lib.sp_parts_iter_begin_wrap.restype = None

_lib.sp_parts_iter_next_wrap.argtypes = [POINTER(_SpPartsIter), POINTER(c_char_p), POINTER(c_size_t)]
_lib.sp_parts_iter_next_wrap.restype = c_int

_lib.sp_parents_iter_begin_wrap.argtypes = [POINTER(_SpPath), POINTER(_SpParentsIter)]
_lib.sp_parents_iter_begin_wrap.restype = None

_lib.sp_parents_iter_next_wrap.argtypes = [POINTER(_SpParentsIter), POINTER(_SpPath)]
_lib.sp_parents_iter_next_wrap.restype = c_int

_lib.sp_join_one_wrap.argtypes = [POINTER(_SpPath), c_char_p, POINTER(_SpPath)]
_lib.sp_join_one_wrap.restype = None

_lib.sp_joinpath_wrap.argtypes = [POINTER(_SpPath), POINTER(_SpPath), POINTER(_SpPath)]
_lib.sp_joinpath_wrap.restype = None

_lib.sp_with_name_wrap.argtypes = [POINTER(_SpPath), c_char_p, POINTER(_SpPath)]
_lib.sp_with_name_wrap.restype = None

_lib.sp_with_stem_wrap.argtypes = [POINTER(_SpPath), c_char_p, POINTER(_SpPath)]
_lib.sp_with_stem_wrap.restype = None

_lib.sp_with_suffix_wrap.argtypes = [POINTER(_SpPath), c_char_p, POINTER(_SpPath)]
_lib.sp_with_suffix_wrap.restype = None

_lib.sp_relative_to_wrap.argtypes = [POINTER(_SpPath), POINTER(_SpPath), POINTER(_SpPath)]
_lib.sp_relative_to_wrap.restype = None

_lib.sp_relative_to_walk_up_wrap.argtypes = [POINTER(_SpPath), POINTER(_SpPath), POINTER(_SpPath)]
_lib.sp_relative_to_walk_up_wrap.restype = None

_lib.sp_relative_to_is_error_wrap.argtypes = [POINTER(_SpPath)]
_lib.sp_relative_to_is_error_wrap.restype = c_int

_lib.sp_is_relative_to_parts_wrap.argtypes = [POINTER(_SpPath), POINTER(c_char_p)]
_lib.sp_is_relative_to_parts_wrap.restype = c_int

_lib.sp_relative_to_parts_wrap.argtypes = [POINTER(_SpPath), POINTER(c_char_p), c_int, POINTER(_SpPath)]
_lib.sp_relative_to_parts_wrap.restype = None

_lib.sp_as_uri_wrap.argtypes = [POINTER(_SpPath), c_char_p, c_size_t]
_lib.sp_as_uri_wrap.restype = c_size_t

_lib.sp_is_relative_to_wrap.argtypes = [POINTER(_SpPath), POINTER(_SpPath)]
_lib.sp_is_relative_to_wrap.restype = c_int

_lib.sp_is_absolute_wrap.argtypes = [POINTER(_SpPath)]
_lib.sp_is_absolute_wrap.restype = c_int

_lib.sp_exists_wrap.argtypes = [POINTER(_SpPath)]
_lib.sp_exists_wrap.restype = c_int

_lib.sp_is_file_wrap.argtypes = [POINTER(_SpPath)]
_lib.sp_is_file_wrap.restype = c_int

_lib.sp_is_dir_wrap.argtypes = [POINTER(_SpPath)]
_lib.sp_is_dir_wrap.restype = c_int

_lib.sp_is_symlink_wrap.argtypes = [POINTER(_SpPath)]
_lib.sp_is_symlink_wrap.restype = c_int

_lib.sp_cwd_wrap.argtypes = [c_int, POINTER(_SpPath)]
_lib.sp_cwd_wrap.restype = None

_lib.sp_home_wrap.argtypes = [c_int, POINTER(_SpPath)]
_lib.sp_home_wrap.restype = None

_lib.sp_expanduser_wrap.argtypes = [POINTER(_SpPath), POINTER(_SpPath)]
_lib.sp_expanduser_wrap.restype = None

_lib.sp_absolute_wrap.argtypes = [POINTER(_SpPath), POINTER(_SpPath)]
_lib.sp_absolute_wrap.restype = None

_lib.sp_path_eq_wrap.argtypes = [POINTER(_SpPath), POINTER(_SpPath)]
_lib.sp_path_eq_wrap.restype = c_int

_lib.sp_path_cmp_wrap.argtypes = [POINTER(_SpPath), POINTER(_SpPath)]
_lib.sp_path_cmp_wrap.restype = c_int

_lib.sp_path_hash_wrap.argtypes = [POINTER(_SpPath)]
_lib.sp_path_hash_wrap.restype = ctypes.c_ulong

_lib.sp_match_wrap.argtypes = [POINTER(_SpPath), c_char_p]
_lib.sp_match_wrap.restype = c_int

_lib.sp_match_ex_wrap.argtypes = [POINTER(_SpPath), c_char_p, c_int]
_lib.sp_match_ex_wrap.restype = c_int

_lib.sp_is_reserved_wrap.argtypes = [POINTER(_SpPath)]
_lib.sp_is_reserved_wrap.restype = c_int

_lib.sp_path_is_error_wrap.argtypes = [POINTER(_SpPath)]
_lib.sp_path_is_error_wrap.restype = c_int

_lib.sp_path_error_code_wrap.argtypes = [POINTER(_SpPath)]
_lib.sp_path_error_code_wrap.restype = c_int

_lib.sp_as_posix_wrap.argtypes = [POINTER(_SpPath), c_char_p, c_size_t]
_lib.sp_as_posix_wrap.restype = None


def _encode(s):
    """Encode string to bytes for C library"""
    if isinstance(s, bytes):
        raise TypeError(
            "argument should be a str or an os.PathLike object "
            "where __fspath__ returns a str, not 'bytes'"
        )
    return s.encode('utf-8') if s else b''


def _decode(b):
    """Decode bytes from C library to string"""
    if b is None:
        return ''
    if isinstance(b, bytes):
        return b.decode('utf-8')
    return b


def _get_pathlib_flavor(obj):
    """Get the flavor constant for a pathlib path object"""
    if isinstance(obj, pathlib.PureWindowsPath):
        return SP_FLAVOR_WINDOWS
    elif isinstance(obj, pathlib.PurePosixPath):
        return SP_FLAVOR_POSIX
    return None


class _PathParents:
    """Sequence of parent paths, like pathlib._PathParents"""

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
        if isinstance(idx, slice):
            return tuple(parents[idx])
        return parents[idx]

    def __repr__(self):
        return f"<{self._path.__class__.__name__}.parents>"


class PurePath:
    """
    Pure path object - no filesystem access.
    Compatible with pathlib.PurePath interface.
    """

    __slots__ = ('_sp',)
    _flavor = SP_FLAVOR_NATIVE
    # Native parser (posixpath on Unix, ntpath on Windows)
    parser = __import__('posixpath') if os.name != 'nt' else __import__('ntpath')

    @property
    def _flavour(self):
        """British spelling alias for CPython tests - returns parser module."""
        return self.parser

    def __new__(cls, *args, **kwargs):
        if cls is PurePath:
            # Auto-select based on platform
            cls = PurePosixPath if os.name != 'nt' else PureWindowsPath
        return object.__new__(cls)

    def __init__(self, *args, **kwargs):
        self._sp = _SpPath()

        # Check for bytes arguments (not allowed)
        for arg in args:
            if isinstance(arg, bytes):
                raise TypeError(
                    "argument should be a str or an os.PathLike object "
                    "where __fspath__ returns a str, not 'bytes'"
                )

        if not args:
            path_str = b''
            _lib.sp_path_new_wrap(path_str, self._flavor, byref(self._sp))
        elif len(args) == 1:
            arg = args[0]
            if isinstance(arg, PurePath):
                # Same flavor: just copy; different flavor: convert
                if arg._flavor == self._flavor:
                    _lib.sp_path_copy_wrap(byref(arg._sp), byref(self._sp))
                else:
                    _lib.sp_path_convert_wrap(_encode(str(arg)), arg._flavor, self._flavor, byref(self._sp))
                return
            # Check if arg is a pathlib path (for cross-flavor conversion)
            src_flavor = _get_pathlib_flavor(arg)
            if src_flavor is not None:
                path_str = _encode(str(arg))
                _lib.sp_path_convert_wrap(path_str, src_flavor, self._flavor, byref(self._sp))
            else:
                path_str = _encode(os.fspath(arg) if hasattr(os, 'fspath') else str(arg))
                _lib.sp_path_new_wrap(path_str, self._flavor, byref(self._sp))
        else:
            # Join multiple args using C library's join (handles absolute paths correctly)
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
                    first_str = os.fspath(first) if hasattr(os, 'fspath') else str(first)
                    _lib.sp_path_new_wrap(_encode(first_str), self._flavor, byref(self._sp))
            for arg in args[1:]:
                if isinstance(arg, PurePath):
                    if arg._flavor == self._flavor:
                        _lib.sp_joinpath_wrap(byref(self._sp), byref(arg._sp), byref(self._sp))
                    else:
                        # Convert and join
                        tmp = _SpPath()
                        _lib.sp_path_convert_wrap(_encode(str(arg)), arg._flavor, self._flavor, byref(tmp))
                        _lib.sp_joinpath_wrap(byref(self._sp), byref(tmp), byref(self._sp))
                else:
                    src_flavor = _get_pathlib_flavor(arg)
                    if src_flavor is not None:
                        # Convert and join
                        tmp = _SpPath()
                        _lib.sp_path_convert_wrap(_encode(str(arg)), src_flavor, self._flavor, byref(tmp))
                        _lib.sp_joinpath_wrap(byref(self._sp), byref(tmp), byref(self._sp))
                    else:
                        arg_str = os.fspath(arg) if hasattr(os, 'fspath') else str(arg)
                        _lib.sp_join_one_wrap(byref(self._sp), _encode(arg_str), byref(self._sp))

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
        if not isinstance(other, PurePath):
            return NotImplemented
        return _lib.sp_path_cmp_wrap(byref(self._sp), byref(other._sp)) < 0

    def __le__(self, other):
        if not isinstance(other, PurePath):
            return NotImplemented
        return _lib.sp_path_cmp_wrap(byref(self._sp), byref(other._sp)) <= 0

    def __gt__(self, other):
        if not isinstance(other, PurePath):
            return NotImplemented
        return _lib.sp_path_cmp_wrap(byref(self._sp), byref(other._sp)) > 0

    def __ge__(self, other):
        if not isinstance(other, PurePath):
            return NotImplemented
        return _lib.sp_path_cmp_wrap(byref(self._sp), byref(other._sp)) >= 0

    def __truediv__(self, other):
        return self.joinpath(other)

    def __rtruediv__(self, other):
        return self.__class__(other, self)

    def _get_str_view(self, func):
        """Helper to get string view from C library"""
        data = c_char_p()
        length = c_size_t()
        func(byref(self._sp), byref(data), byref(length))
        if data.value is None or length.value == 0:
            return ''
        return data.value[:length.value].decode('utf-8')

    @property
    def drive(self):
        return self._get_str_view(_lib.sp_drive_wrap)

    @property
    def root(self):
        return self._get_str_view(_lib.sp_root_wrap)

    @property
    def anchor(self):
        return self._get_str_view(_lib.sp_anchor_wrap)

    @property
    def name(self):
        return self._get_str_view(_lib.sp_name_wrap)

    @property
    def stem(self):
        return self._get_str_view(_lib.sp_stem_wrap)

    @property
    def suffix(self):
        return self._get_str_view(_lib.sp_suffix_wrap)

    @property
    def suffixes(self):
        data_arr = (c_char_p * SP_MAX_SUFFIXES)()
        len_arr = (c_size_t * SP_MAX_SUFFIXES)()
        count = _lib.sp_suffixes_wrap(byref(self._sp), data_arr, len_arr, SP_MAX_SUFFIXES)
        result = []
        for i in range(count):
            if data_arr[i] and len_arr[i] > 0:
                result.append(data_arr[i][:len_arr[i]].decode('utf-8'))
        return result

    @property
    def parent(self):
        result = self.__class__.__new__(self.__class__)
        result._sp = _SpPath()
        _lib.sp_parent_wrap(byref(self._sp), byref(result._sp))
        return result

    @property
    def parents(self):
        return _PathParents(self)

    @property
    def parts(self):
        parts = []
        it = _SpPartsIter()
        _lib.sp_parts_iter_begin_wrap(byref(self._sp), byref(it))
        data = c_char_p()
        length = c_size_t()
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
        buf = create_string_buffer(SP_PATH_MAX * 3)  # URL encoding can expand
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
        # Convert args to NULL-terminated array of C strings
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

        # Convert args to NULL-terminated array of C strings
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
                other_str = os.fspath(other) if hasattr(os, 'fspath') else str(other)
                _lib.sp_join_one_wrap(byref(result._sp), _encode(other_str), byref(result._sp))

        return result

    def with_name(self, name):
        if not isinstance(name, str):
            raise TypeError(f"expected str, not {type(name).__name__}")
        result = self.__class__.__new__(self.__class__)
        result._sp = _SpPath()
        _lib.sp_with_name_wrap(byref(self._sp), _encode(name), byref(result._sp))
        err = _lib.sp_path_error_code_wrap(byref(result._sp))
        if err == SP_ERR_NO_NAME:
            raise ValueError(f"{self!r} has an empty name")
        if err == SP_ERR_INVALID_ARG:
            raise ValueError(f"Invalid name {name!r}")
        return result

    def with_stem(self, stem):
        if not isinstance(stem, str):
            raise TypeError(f"expected str, not {type(stem).__name__}")
        result = self.__class__.__new__(self.__class__)
        result._sp = _SpPath()
        _lib.sp_with_stem_wrap(byref(self._sp), _encode(stem), byref(result._sp))
        err = _lib.sp_path_error_code_wrap(byref(result._sp))
        if err == SP_ERR_NO_NAME:
            raise ValueError(f"{self!r} has an empty name")
        if err == SP_ERR_INVALID_ARG:
            raise ValueError(f"Invalid stem {stem!r}")
        return result

    def with_suffix(self, suffix):
        if not isinstance(suffix, str):
            raise TypeError(f"expected str, not {type(suffix).__name__}")
        result = self.__class__.__new__(self.__class__)
        result._sp = _SpPath()
        _lib.sp_with_suffix_wrap(byref(self._sp), _encode(suffix), byref(result._sp))
        err = _lib.sp_path_error_code_wrap(byref(result._sp))
        if err == SP_ERR_NO_NAME:
            raise ValueError(f"{self!r} has an empty name")
        if err == SP_ERR_INVALID_ARG:
            raise ValueError(f"Invalid suffix {suffix!r}")
        return result

    def match(self, pattern, *, case_sensitive=None):
        """Match this path against a glob pattern."""
        if isinstance(pattern, bytes):
            raise TypeError("argument should be a str or os.PathLike object, not bytes")
        pattern = str(pattern)
        # Convert case_sensitive to C convention: -1=default, 0=insensitive, 1=sensitive
        cs_value = -1 if case_sensitive is None else (1 if case_sensitive else 0)
        result = _lib.sp_match_ex_wrap(byref(self._sp), _encode(pattern), cs_value)
        if result == SP_MATCH_ERR_EMPTY:
            raise ValueError("empty pattern")
        if result == SP_MATCH_ERR_INVALID:
            raise ValueError(f"Invalid pattern: {pattern!r}")
        return result == SP_MATCH_YES

    def is_reserved(self):
        """Return True if the path is reserved under Windows."""
        return bool(_lib.sp_is_reserved_wrap(byref(self._sp)))

    def exists(self):
        """Whether this path exists."""
        return bool(_lib.sp_exists_wrap(byref(self._sp)))

    def is_file(self):
        """Whether this path is a regular file."""
        return bool(_lib.sp_is_file_wrap(byref(self._sp)))

    def is_dir(self):
        """Whether this path is a directory."""
        return bool(_lib.sp_is_dir_wrap(byref(self._sp)))

    def is_symlink(self):
        """Whether this path is a symbolic link."""
        return bool(_lib.sp_is_symlink_wrap(byref(self._sp)))

    def expanduser(self):
        """Expand ~ and ~user constructs."""
        result = self.__class__.__new__(self.__class__)
        result._sp = _SpPath()
        _lib.sp_expanduser_wrap(byref(self._sp), byref(result._sp))
        return result

    @classmethod
    def home(cls):
        """Return the home directory."""
        result = cls.__new__(cls)
        result._sp = _SpPath()
        flavor = cls._flavor if hasattr(cls, '_flavor') else SP_FLAVOR_NATIVE
        _lib.sp_home_wrap(flavor, byref(result._sp))
        return result


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


# Concrete Path classes (for compatibility, but without I/O)
class Path(PurePath):
    """
    Path object - NOTE: I/O operations not implemented.
    This is a pure-computation library.
    """
    __slots__ = ()

    def __new__(cls, *args, **kwargs):
        if cls is Path:
            cls = PosixPath if os.name != 'nt' else WindowsPath
        return object.__new__(cls)

    @classmethod
    def cwd(cls):
        """Return a new path pointing to the current working directory."""
        result = cls.__new__(cls)
        result._sp = _SpPath()
        _lib.sp_cwd_wrap(result._flavor, byref(result._sp))
        return result

    def absolute(self):
        """Return an absolute version of this path."""
        result = self.__class__.__new__(self.__class__)
        result._sp = _SpPath()
        _lib.sp_absolute_wrap(byref(self._sp), byref(result._sp))
        return result


class PosixPath(Path, PurePosixPath):
    """Path with POSIX semantics."""
    __slots__ = ()

    def __new__(cls, *args, **kwargs):
        return object.__new__(cls)


class WindowsPath(Path, PureWindowsPath):
    """Path with Windows semantics."""
    __slots__ = ()

    def __new__(cls, *args, **kwargs):
        return object.__new__(cls)


__all__ = [
    'PurePath', 'PurePosixPath', 'PureWindowsPath',
    'Path', 'PosixPath', 'WindowsPath',
]
