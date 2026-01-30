"""
snakepath - Python bindings for the snakepath C library.

Provides PurePath, PurePosixPath, and PureWindowsPath classes compatible
with Python's pathlib interface.
"""

import ctypes
import os
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

_lib.sp_is_relative_to_wrap.argtypes = [POINTER(_SpPath), POINTER(_SpPath)]
_lib.sp_is_relative_to_wrap.restype = c_int

_lib.sp_is_absolute_wrap.argtypes = [POINTER(_SpPath)]
_lib.sp_is_absolute_wrap.restype = c_int

_lib.sp_path_eq_wrap.argtypes = [POINTER(_SpPath), POINTER(_SpPath)]
_lib.sp_path_eq_wrap.restype = c_int

_lib.sp_path_cmp_wrap.argtypes = [POINTER(_SpPath), POINTER(_SpPath)]
_lib.sp_path_cmp_wrap.restype = c_int

_lib.sp_path_hash_wrap.argtypes = [POINTER(_SpPath)]
_lib.sp_path_hash_wrap.restype = ctypes.c_ulong

_lib.sp_match_wrap.argtypes = [POINTER(_SpPath), c_char_p]
_lib.sp_match_wrap.restype = c_int

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
        return s
    return s.encode('utf-8') if s else b''


def _decode(b):
    """Decode bytes from C library to string"""
    if b is None:
        return ''
    if isinstance(b, bytes):
        return b.decode('utf-8')
    return b


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

    def __new__(cls, *args):
        if cls is PurePath:
            # Auto-select based on platform
            cls = PurePosixPath if os.name != 'nt' else PureWindowsPath
        return object.__new__(cls)

    def __init__(self, *args):
        self._sp = _SpPath()

        if not args:
            path_str = b''
        elif len(args) == 1:
            arg = args[0]
            if isinstance(arg, PurePath):
                _lib.sp_path_copy_wrap(byref(arg._sp), byref(self._sp))
                return
            path_str = _encode(os.fspath(arg) if hasattr(os, 'fspath') else str(arg))
        else:
            # Join multiple args using C library's join (handles absolute paths correctly)
            first = args[0]
            if isinstance(first, PurePath):
                first_str = str(first)
            else:
                first_str = os.fspath(first) if hasattr(os, 'fspath') else str(first)
            _lib.sp_path_new_wrap(_encode(first_str), self._flavor, byref(self._sp))
            for arg in args[1:]:
                if isinstance(arg, PurePath):
                    arg_str = str(arg)
                else:
                    arg_str = os.fspath(arg) if hasattr(os, 'fspath') else str(arg)
                _lib.sp_join_one_wrap(byref(self._sp), _encode(arg_str), byref(self._sp))
            return

        _lib.sp_path_new_wrap(path_str, self._flavor, byref(self._sp))

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

    def is_absolute(self):
        return bool(_lib.sp_is_absolute_wrap(byref(self._sp)))

    def is_relative_to(self, other):
        if isinstance(other, bytes):
            raise TypeError("argument should be a str or os.PathLike object, not bytes")
        if not isinstance(other, PurePath):
            other = self.__class__(other)
        return bool(_lib.sp_is_relative_to_wrap(byref(self._sp), byref(other._sp)))

    def relative_to(self, other, walk_up=False):
        if isinstance(other, bytes):
            raise TypeError("argument should be a str or os.PathLike object, not bytes")
        if not isinstance(other, PurePath):
            other = self.__class__(other)

        result = self.__class__.__new__(self.__class__)
        result._sp = _SpPath()

        if walk_up:
            _lib.sp_relative_to_walk_up_wrap(byref(self._sp), byref(other._sp), byref(result._sp))
        else:
            _lib.sp_relative_to_wrap(byref(self._sp), byref(other._sp), byref(result._sp))

        if _lib.sp_relative_to_is_error_wrap(byref(result._sp)):
            raise ValueError(f"{str(self)!r} is not relative to {str(other)!r}")

        return result

    def joinpath(self, *others):
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

    def match(self, pattern):
        """Match this path against a glob pattern."""
        pattern = str(pattern)
        result = _lib.sp_match_wrap(byref(self._sp), _encode(pattern))
        if result == SP_MATCH_ERR_EMPTY:
            raise ValueError("empty pattern")
        if result == SP_MATCH_ERR_INVALID:
            raise ValueError(f"Invalid pattern: {pattern!r}")
        return result == SP_MATCH_YES

    def is_reserved(self):
        """Return True if the path is reserved under Windows."""
        return bool(_lib.sp_is_reserved_wrap(byref(self._sp)))


class PurePosixPath(PurePath):
    """Pure path with POSIX semantics."""
    __slots__ = ()
    _flavor = SP_FLAVOR_POSIX
    parser = __import__('posixpath')

    def __new__(cls, *args):
        return object.__new__(cls)


class PureWindowsPath(PurePath):
    """Pure path with Windows semantics."""
    __slots__ = ()
    _flavor = SP_FLAVOR_WINDOWS
    parser = __import__('ntpath')

    def __new__(cls, *args):
        return object.__new__(cls)


# Concrete Path classes (for compatibility, but without I/O)
class Path(PurePath):
    """
    Path object - NOTE: I/O operations not implemented.
    This is a pure-computation library.
    """
    __slots__ = ()

    def __new__(cls, *args):
        if cls is Path:
            cls = PosixPath if os.name != 'nt' else WindowsPath
        return object.__new__(cls)


class PosixPath(Path, PurePosixPath):
    """Path with POSIX semantics."""
    __slots__ = ()

    def __new__(cls, *args):
        return object.__new__(cls)


class WindowsPath(Path, PureWindowsPath):
    """Path with Windows semantics."""
    __slots__ = ()

    def __new__(cls, *args):
        return object.__new__(cls)


__all__ = [
    'PurePath', 'PurePosixPath', 'PureWindowsPath',
    'Path', 'PosixPath', 'WindowsPath',
]
