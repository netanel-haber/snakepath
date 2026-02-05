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
        ("st_atime_ns", ctypes.c_longlong),
        ("st_mtime_ns", ctypes.c_longlong),
        ("st_ctime_ns", ctypes.c_longlong),
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
# SpTerm-returning functions: pass buffer, buffer size, receive length
for n in ['drive', 'root', 'anchor', 'name', 'stem', 'suffix']:
    _sig(f'sp_{n}_wrap', [_PP, c_char_p, c_size_t, POINTER(c_size_t)])

for n in ['parent', 'absolute']:
    _sig(f'sp_{n}_wrap', [_PP, _PP])

for n in ['with_name', 'with_stem', 'with_suffix', 'join_one']:
    _sig(f'sp_{n}_wrap', [_PP, c_char_p, _PP])

for n in ['joinpath', 'relative_to', 'relative_to_walk_up']:
    _sig(f'sp_{n}_wrap', [_PP, _PP, _PP])

for n in ['is_absolute', 'is_reserved', 'is_file', 'is_dir', 'exists',
          'is_symlink', 'is_block_device', 'is_char_device', 'is_fifo',
          'is_socket', 'is_mount', 'is_junction',
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
_sig('sp_lstat_wrap', [_PP, _PStat])
_sig('sp_stat_eq_wrap', [_PStat, _PStat], c_int)
# Symlink & link operations
_sig('sp_readlink_wrap', [_PP, _PP])
_sig('sp_resolve_wrap', [_PP, c_int, _PP])
_sig('sp_symlink_to_wrap', [_PP, _PP, c_int], c_int)
_sig('sp_hardlink_to_wrap', [_PP, _PP], c_int)
_sig('sp_samefile_wrap', [_PP, _PP], c_int)
_sig('sp_mkdir_wrap', [_PP, ctypes.c_uint, c_int, c_int], c_int)
# Glob iterator
_sizeof_glob_iter = _lib.sp_sizeof_glob_iter()
class _SpGlobIter(Structure):
    _fields_ = [('_opaque', ctypes.c_char * _sizeof_glob_iter)]
_PGlobIter = POINTER(_SpGlobIter)
_sig('sp_glob_begin_wrap', [_PP, c_char_p, c_int, _PGlobIter])
_sig('sp_rglob_begin_wrap', [_PP, c_char_p, c_int, _PGlobIter])
_sig('sp_glob_next_wrap', [_PGlobIter, _PP], c_int)
_sig('sp_glob_end_wrap', [_PGlobIter])
_sig('sp_glob_depth_wrap', [_PGlobIter], c_int)
# File/directory modification operations
_sig('sp_touch_wrap', [_PP, ctypes.c_uint, c_int], c_int)
_sig('sp_unlink_wrap', [_PP, c_int], c_int)
_sig('sp_rmdir_wrap', [_PP], c_int)
_sig('sp_rename_wrap', [_PP, _PP, _PP])
_sig('sp_replace_wrap', [_PP, _PP, _PP])
_sig('sp_chmod_wrap', [_PP, ctypes.c_uint], c_int)
# Home directory and user expansion
_sig('sp_home_wrap', [c_int, _PP])
_sig('sp_expanduser_wrap', [_PP, _PP])
# User/group info
_sig('sp_owner_wrap', [_PP, POINTER(c_char_p), POINTER(c_size_t)], c_int)
_sig('sp_group_wrap', [_PP, POINTER(c_char_p), POINTER(c_size_t)], c_int)
# iterdir iterator
_sizeof_iterdir_iter = _lib.sp_sizeof_iterdir_iter()
class _SpIterdirIter(Structure):
    _fields_ = [('_opaque', ctypes.c_char * _sizeof_iterdir_iter)]
_PIterdirIter = POINTER(_SpIterdirIter)
_sig('sp_iterdir_begin_wrap', [_PP, _PIterdirIter])
_sig('sp_iterdir_next_wrap', [_PIterdirIter, _PP], c_int)
_sig('sp_iterdir_end_wrap', [_PIterdirIter])
_sig('sp_iterdir_done_wrap', [_PIterdirIter], c_int)
# walk - BYOS iterator API
_walk_max_entries = _lib.sp_walk_max_entries()
_walk_name_max = _lib.sp_walk_name_max()
_walk_iter_size = _lib.sp_walk_iter_size()
_walk_path_max = _lib.sp_walk_path_max()
_walk_error_ctx_size = _lib.sp_walk_error_context_size()

class _SpWalkIter(Structure):
    _fields_ = [('_opaque', ctypes.c_char * _walk_iter_size)]
_PWalkIter = POINTER(_SpWalkIter)

# Walk error callback type: void (*)(const char *path, int error_code, void *user_data)
_WalkErrorFn = ctypes.CFUNCTYPE(None, c_char_p, c_int, ctypes.c_void_p)

# Walk error context struct
class _SpWalkErrorContext(Structure):
    _fields_ = [
        ('error_callback', _WalkErrorFn),
        ('user_data', ctypes.c_void_p),
    ]
_PWalkErrorContext = POINTER(_SpWalkErrorContext)

_sig('sp_walk_begin_wrap', [_PWalkIter, _PP, c_int, c_int, ctypes.c_void_p, c_size_t, _PWalkErrorContext], c_int)
_sig('sp_walk_next_wrap', [_PWalkIter], c_int)
_sig('sp_walk_end_wrap', [_PWalkIter])
_sig('sp_walk_dirpath_wrap', [_PWalkIter, _PP])
_sig('sp_walk_dirname_count_wrap', [_PWalkIter], c_size_t)
_sig('sp_walk_filename_count_wrap', [_PWalkIter], c_size_t)
_sig('sp_walk_dirname_wrap', [_PWalkIter, c_size_t], c_char_p)
_sig('sp_walk_filename_wrap', [_PWalkIter, c_size_t], c_char_p)
_sig('sp_walk_set_dirname_count_wrap', [_PWalkIter, c_size_t])


# ============ Property descriptors ============

class _TermProp:
    """Descriptor for SpTerm properties (drive, root, anchor, name, stem, suffix)"""
    __slots__ = ('_func',)
    _TERM_BUF_SIZE = 256  # matches SP_TERM_MAX
    def __init__(self, name):
        self._func = getattr(_lib, f'sp_{name}_wrap')
    def __get__(self, obj, objtype=None):
        if obj is None: return self
        buf = ctypes.create_string_buffer(self._TERM_BUF_SIZE)
        length = c_size_t()
        self._func(byref(obj._sp), buf, self._TERM_BUF_SIZE, byref(length))
        return '' if not length.value else buf.value[:length.value].decode('utf-8')


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

    def is_symlink(self):
        return bool(_lib.sp_is_symlink_wrap(byref(self._sp)))

    def is_block_device(self):
        return bool(_lib.sp_is_block_device_wrap(byref(self._sp)))

    def is_char_device(self):
        return bool(_lib.sp_is_char_device_wrap(byref(self._sp)))

    def is_fifo(self):
        return bool(_lib.sp_is_fifo_wrap(byref(self._sp)))

    def is_socket(self):
        return bool(_lib.sp_is_socket_wrap(byref(self._sp)))

    def is_mount(self):
        return bool(_lib.sp_is_mount_wrap(byref(self._sp)))

    def is_junction(self):
        return bool(_lib.sp_is_junction_wrap(byref(self._sp)))

    def stat(self, *, follow_symlinks=True):
        result = _SpStatResult()
        if follow_symlinks:
            _lib.sp_stat_wrap(byref(self._sp), byref(result))
        else:
            _lib.sp_lstat_wrap(byref(self._sp), byref(result))
        if not result.valid:
            raise FileNotFoundError(2, "No such file or directory", str(self))
        return result

    def lstat(self):
        """Like stat(), but does not follow symbolic links."""
        return self.stat(follow_symlinks=False)

    def readlink(self):
        """Return the path to which the symbolic link points."""
        result = self.__class__.__new__(self.__class__)
        result._sp = _SpPath()
        _lib.sp_readlink_wrap(byref(self._sp), byref(result._sp))
        if _lib.sp_path_is_error_wrap(byref(result._sp)):
            raise OSError(22, "Invalid argument", str(self))
        return result

    def resolve(self, strict=False):
        """Make the path absolute, resolving all symlinks."""
        result = self.__class__.__new__(self.__class__)
        result._sp = _SpPath()
        _lib.sp_resolve_wrap(byref(self._sp), 1 if strict else 0, byref(result._sp))
        if _lib.sp_path_is_error_wrap(byref(result._sp)):
            raise FileNotFoundError(2, "No such file or directory", str(self))
        return result

    def symlink_to(self, target, target_is_directory=False):
        """Make this path a symlink pointing to target."""
        if isinstance(target, PurePath):
            target_sp = target._sp
        else:
            target_path = self.__class__.__new__(self.__class__)
            target_path._sp = _SpPath()
            target_buf = _encode_buf(os.fspath(target) if hasattr(os, 'fspath') else str(target))
            _lib.sp_path_new_len_wrap(target_buf, len(target_buf.raw), self._flavor, byref(target_path._sp))
            target_sp = target_path._sp
        if not _lib.sp_symlink_to_wrap(byref(self._sp), byref(target_sp), 1 if target_is_directory else 0):
            raise OSError(1, "Operation not permitted", str(self))

    def hardlink_to(self, target):
        """Make this path a hard link pointing to target."""
        if isinstance(target, PurePath):
            target_sp = target._sp
        else:
            target_path = self.__class__.__new__(self.__class__)
            target_path._sp = _SpPath()
            target_buf = _encode_buf(os.fspath(target) if hasattr(os, 'fspath') else str(target))
            _lib.sp_path_new_len_wrap(target_buf, len(target_buf.raw), self._flavor, byref(target_path._sp))
            target_sp = target_path._sp
        if not _lib.sp_hardlink_to_wrap(byref(self._sp), byref(target_sp)):
            raise OSError(1, "Operation not permitted", str(self))

    def samefile(self, other_path):
        """Return True if both paths refer to the same file."""
        # First check that both paths exist (Python raises FileNotFoundError if either doesn't exist)
        if not self.exists():
            raise FileNotFoundError(2, "No such file or directory", str(self))
        if isinstance(other_path, PurePath):
            other_sp = other_path._sp
            if not _lib.sp_exists_wrap(byref(other_sp)):
                raise FileNotFoundError(2, "No such file or directory", str(other_path))
        else:
            other = self.__class__.__new__(self.__class__)
            other._sp = _SpPath()
            other_buf = _encode_buf(os.fspath(other_path) if hasattr(os, 'fspath') else str(other_path))
            _lib.sp_path_new_len_wrap(other_buf, len(other_buf.raw), self._flavor, byref(other._sp))
            other_sp = other._sp
            if not _lib.sp_exists_wrap(byref(other_sp)):
                raise FileNotFoundError(2, "No such file or directory", str(other_path))
        return bool(_lib.sp_samefile_wrap(byref(self._sp), byref(other_sp)))

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

        cs = SP_CASE_PLATFORM_DEFAULT if case_sensitive is None else (SP_CASE_SENSITIVE if case_sensitive else SP_CASE_INSENSITIVE)

        results = []
        path_class = self.__class__
        it = _SpGlobIter()
        match = _SpPath()
        _lib.sp_glob_begin_wrap(byref(self._sp), _encode(pattern_str), cs, byref(it))
        while _lib.sp_glob_next_wrap(byref(it), byref(match)):
            result = path_class.__new__(path_class)
            result._sp = _SpPath()
            _lib.sp_path_copy_wrap(byref(match), byref(result._sp))
            results.append(result)
        _lib.sp_glob_end_wrap(byref(it))
        return iter(results)

    def rglob(self, pattern, *, case_sensitive=None):
        """Recursively yield all existing files matching pattern."""
        if isinstance(pattern, bytes):
            raise TypeError("argument should be a str or os.PathLike object, not bytes")
        pattern_str = str(pattern)

        cs = SP_CASE_PLATFORM_DEFAULT if case_sensitive is None else (SP_CASE_SENSITIVE if case_sensitive else SP_CASE_INSENSITIVE)

        results = []
        path_class = self.__class__
        it = _SpGlobIter()
        match = _SpPath()
        _lib.sp_rglob_begin_wrap(byref(self._sp), _encode(pattern_str), cs, byref(it))
        while _lib.sp_glob_next_wrap(byref(it), byref(match)):
            result = path_class.__new__(path_class)
            result._sp = _SpPath()
            _lib.sp_path_copy_wrap(byref(match), byref(result._sp))
            results.append(result)
        _lib.sp_glob_end_wrap(byref(it))
        return iter(results)

    def touch(self, mode=0o666, exist_ok=True):
        """Create file or update timestamps."""
        if not _lib.sp_touch_wrap(byref(self._sp), mode, 1 if exist_ok else 0):
            raise FileExistsError(17, "File exists", str(self))

    def unlink(self, missing_ok=False):
        """Remove the file."""
        if not _lib.sp_unlink_wrap(byref(self._sp), 1 if missing_ok else 0):
            raise FileNotFoundError(2, "No such file or directory", str(self))

    def rmdir(self):
        """Remove the empty directory."""
        if not _lib.sp_rmdir_wrap(byref(self._sp)):
            raise OSError(1, "Operation not permitted", str(self))

    def rename(self, target):
        """Rename this file/directory to the given target."""
        if isinstance(target, PurePath):
            target_sp = target._sp
        else:
            target_path = self.__class__.__new__(self.__class__)
            target_path._sp = _SpPath()
            target_buf = _encode_buf(os.fspath(target) if hasattr(os, 'fspath') else str(target))
            _lib.sp_path_new_len_wrap(target_buf, len(target_buf.raw), self._flavor, byref(target_path._sp))
            target_sp = target_path._sp

        result = self.__class__.__new__(self.__class__)
        result._sp = _SpPath()
        _lib.sp_rename_wrap(byref(self._sp), byref(target_sp), byref(result._sp))
        if _lib.sp_path_is_error_wrap(byref(result._sp)):
            raise OSError(1, "Operation not permitted", str(self), str(target))
        return result

    def replace(self, target):
        """Replace target with this file (atomic operation)."""
        if isinstance(target, PurePath):
            target_sp = target._sp
        else:
            target_path = self.__class__.__new__(self.__class__)
            target_path._sp = _SpPath()
            target_buf = _encode_buf(os.fspath(target) if hasattr(os, 'fspath') else str(target))
            _lib.sp_path_new_len_wrap(target_buf, len(target_buf.raw), self._flavor, byref(target_path._sp))
            target_sp = target_path._sp

        result = self.__class__.__new__(self.__class__)
        result._sp = _SpPath()
        _lib.sp_replace_wrap(byref(self._sp), byref(target_sp), byref(result._sp))
        if _lib.sp_path_is_error_wrap(byref(result._sp)):
            raise OSError(1, "Operation not permitted", str(self), str(target))
        return result

    def chmod(self, mode):
        """Change the file mode (permissions)."""
        if not _lib.sp_chmod_wrap(byref(self._sp), mode):
            raise OSError(1, "Operation not permitted", str(self))

    def open(self, mode='r', buffering=-1, encoding=None, errors=None, newline=None):
        """Open the file pointed to by this path."""
        import io
        return io.open(str(self), mode, buffering, encoding, errors, newline)

    @classmethod
    def home(cls):
        """Return user's home directory."""
        result = cls.__new__(cls)
        result._sp = _SpPath()
        _lib.sp_home_wrap(result._flavor, byref(result._sp))
        if _lib.sp_path_is_error_wrap(byref(result._sp)):
            raise RuntimeError("Could not determine home directory")
        return result

    def expanduser(self):
        """Expand ~ to user's home directory."""
        result = self.__class__.__new__(self.__class__)
        result._sp = _SpPath()
        _lib.sp_expanduser_wrap(byref(self._sp), byref(result._sp))
        if _lib.sp_path_is_error_wrap(byref(result._sp)):
            raise RuntimeError("Could not expand user")
        return result

    def owner(self):
        """Return the file owner name."""
        data, length = c_char_p(), c_size_t()
        result = _lib.sp_owner_wrap(byref(self._sp), byref(data), byref(length))
        if result == -1:
            raise NotImplementedError("Path.owner() is unsupported on this system")
        if result != 0 or not data.value or not length.value:
            raise FileNotFoundError(2, "No such file or directory", str(self))
        return data.value[:length.value].decode('utf-8')

    def group(self):
        """Return the file group name."""
        data, length = c_char_p(), c_size_t()
        result = _lib.sp_group_wrap(byref(self._sp), byref(data), byref(length))
        if result == -1:
            raise NotImplementedError("Path.group() is unsupported on this system")
        if result != 0 or not data.value or not length.value:
            raise FileNotFoundError(2, "No such file or directory", str(self))
        return data.value[:length.value].decode('utf-8')

    def iterdir(self):
        """Yield path objects of directory contents."""
        if not self.is_dir():
            raise NotADirectoryError(20, "Not a directory", str(self))
        it = _SpIterdirIter()
        _lib.sp_iterdir_begin_wrap(byref(self._sp), byref(it))
        if _lib.sp_iterdir_done_wrap(byref(it)) < 0:
            raise OSError(1, "Operation not permitted", str(self))
        try:
            out = _SpPath()
            while _lib.sp_iterdir_next_wrap(byref(it), byref(out)):
                result = self.__class__.__new__(self.__class__)
                result._sp = _SpPath()
                _lib.sp_path_copy_wrap(byref(out), byref(result._sp))
                yield result
        finally:
            _lib.sp_iterdir_end_wrap(byref(it))

    def walk(self, top_down=True, on_error=None, follow_symlinks=False):
        """Walk directory tree, yielding (dirpath, dirnames, filenames) tuples."""
        if not self.is_dir():
            return

        path_cls = self.__class__

        # Error callback and context
        error_ctx = None
        c_error_callback = None  # Keep reference to prevent GC
        if on_error:
            def error_callback(path_bytes, error_code, user_data):
                path_str = path_bytes.decode('utf-8') if path_bytes else ''
                os_err = OSError(error_code, os.strerror(error_code), path_str)
                on_error(os_err)
            c_error_callback = _WalkErrorFn(error_callback)
            error_ctx = _SpWalkErrorContext()
            error_ctx.error_callback = c_error_callback
            error_ctx.user_data = None

        # Allocate iterator and pending buffer (256 pending dirs = 256KB for SP_PATH_MAX=1024)
        it = _SpWalkIter()
        pending_count = 256
        pending_buf = (ctypes.c_char * (pending_count * _walk_path_max))()

        # Begin walk
        if not _lib.sp_walk_begin_wrap(byref(it), byref(self._sp),
                                       1 if top_down else 0, 1 if follow_symlinks else 0,
                                       pending_buf, len(pending_buf),
                                       byref(error_ctx) if error_ctx else None):
            return

        try:
            while _lib.sp_walk_next_wrap(byref(it)):
                # Get dirpath
                dp = path_cls.__new__(path_cls)
                dp._sp = _SpPath()
                _lib.sp_walk_dirpath_wrap(byref(it), byref(dp._sp))

                # Get dirnames
                dirname_count = _lib.sp_walk_dirname_count_wrap(byref(it))
                dns = []
                for i in range(dirname_count):
                    name = _lib.sp_walk_dirname_wrap(byref(it), i)
                    if name:
                        dns.append(name.decode('utf-8'))

                # Get filenames
                filename_count = _lib.sp_walk_filename_count_wrap(byref(it))
                fns = []
                for i in range(filename_count):
                    name = _lib.sp_walk_filename_wrap(byref(it), i)
                    if name:
                        fns.append(name.decode('utf-8'))

                yield (dp, dns, fns)
        finally:
            _lib.sp_walk_end_wrap(byref(it))


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
