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

# Get structure sizes from library
SP_PATH_MAX = _lib.sp_path_max()
SP_MAX_SUFFIXES = _lib.sp_max_suffixes()
_sizeof_parts_iter = _lib.sp_sizeof_parts_iter()
_sizeof_parents_iter = _lib.sp_sizeof_parents_iter()

# Flavor constants
SP_FLAVOR_NATIVE = 0
SP_FLAVOR_POSIX = 1
SP_FLAVOR_WINDOWS = 2

# Result and error codes (from C library)
SP_OK = _lib.sp_ok()
SP_ERR_EXISTS = _lib.sp_err_exists()
SP_ERR_NOT_FOUND = _lib.sp_err_not_found()
SP_ERR_NOT_DIR = _lib.sp_err_not_dir()
SP_ERR_PERMISSION = _lib.sp_err_permission()
SP_ERR_EXISTS_NOT_DIR = _lib.sp_err_exists_not_dir()
SP_ERR_OPEN = _lib.sp_err_open()
SP_ERR_NO_NAME = _lib.sp_err_no_name()
SP_ERR_INVALID_ARG = _lib.sp_err_invalid_arg()
SP_MATCH_YES = _lib.sp_match_yes()
SP_MATCH_ERR_EMPTY = _lib.sp_match_err_empty()
SP_MATCH_ERR_INVALID = _lib.sp_match_err_invalid()

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

for n in ['with_name', 'with_stem', 'with_suffix']:
    _sig(f'sp_{n}_wrap', [_PP, c_char_p, _PP])

_sig('sp_joinpath_wrap', [_PP, _PP, _PP])

for n in ['is_absolute', 'is_reserved', 'is_file', 'is_dir', 'exists',
          'is_symlink', 'is_block_device', 'is_char_device', 'is_fifo',
          'is_socket', 'is_mount', 'is_junction',
          'path_is_error', 'path_error_code', 'relative_to_is_error']:
    _sig(f'sp_{n}_wrap', [_PP], c_int)

_sig('sp_path_eq_wrap', [_PP, _PP], c_int)

_sig('sp_path_hash_wrap', [_PP], ctypes.c_ulong)

# Remaining signatures
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
_sig('sp_with_segments_wrap', [_PP, POINTER(c_char_p), c_size_t, _PP])
_sig('sp_is_relative_to_parts_wrap', [_PP, POINTER(c_char_p)], c_int)
_sig('sp_relative_to_parts_wrap', [_PP, POINTER(c_char_p), c_int, _PP])
_sig('sp_as_uri_wrap', [_PP, c_char_p, c_size_t], c_size_t)
_sig('sp_as_posix_wrap', [_PP, c_char_p, c_size_t])
_sig('sp_cwd_wrap', [c_int, _PP])
_sig('sp_path_cmp_wrap', [_PP, _PP], c_int)
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
# File/directory modification operations
_sig('sp_touch_wrap', [_PP, ctypes.c_uint, c_int], c_int)
_sig('sp_unlink_wrap', [_PP, c_int], c_int)
_sig('sp_rmdir_wrap', [_PP], c_int)
_sig('sp_rename_wrap', [_PP, _PP, _PP])
_sig('sp_replace_wrap', [_PP, _PP, _PP])
_sig('sp_chmod_wrap', [_PP, ctypes.c_uint], c_int)
# File I/O
_sig('sp_read_file_wrap', [_PP, c_char_p, c_size_t, POINTER(c_size_t), POINTER(c_int)])
_sig('sp_write_file_wrap', [_PP, c_char_p, c_size_t, POINTER(c_size_t), POINTER(c_int)])
# Home directory and user expansion
_sig('sp_home_wrap', [c_int, _PP])
_sig('sp_expanduser_wrap', [_PP, _PP])
# User/group info (SpTerm-based, buffer API) - not available on Windows builds
_HAS_OWNER_GROUP = hasattr(_lib, 'sp_owner_wrap') and hasattr(_lib, 'sp_group_wrap')
if _HAS_OWNER_GROUP:
    for n in ['owner', 'group']:
        _sig(f'sp_{n}_wrap', [_PP, c_char_p, c_size_t, POINTER(c_size_t)])
# iterdir iterator
_sizeof_iterdir_iter = _lib.sp_sizeof_iterdir_iter()
class _SpIterdirIter(Structure):
    _fields_ = [('_opaque', ctypes.c_char * _sizeof_iterdir_iter)]
_PIterdirIter = POINTER(_SpIterdirIter)
_sig('sp_iterdir_begin_wrap', [_PP, _PIterdirIter])
_sig('sp_iterdir_next_wrap', [_PIterdirIter, _PP], c_int)
_sig('sp_iterdir_end_wrap', [_PIterdirIter])
_sig('sp_iterdir_done_wrap', [_PIterdirIter], c_int)
# walk - Python walk() composes iterdir + is_dir (see walk() docstring for why)


# ============ Property descriptors ============

_TERM_BUF_SIZE = 256  # matches SP_TERM_MAX

def _term(func, sp):
    """Decode the SpTerm that func writes for sp ('' when empty)"""
    buf = create_string_buffer(_TERM_BUF_SIZE)
    length = c_size_t()
    func(byref(sp), buf, _TERM_BUF_SIZE, byref(length))
    return buf.value[:length.value].decode('utf-8') if length.value else ''


class _TermProp:
    """Descriptor for SpTerm properties (drive, root, anchor, name, stem, suffix)"""
    __slots__ = ('_func',)
    def __init__(self, name):
        self._func = getattr(_lib, f'sp_{name}_wrap')
    def __get__(self, obj, objtype=None):
        return self if obj is None else _term(self._func, obj._sp)


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
    return '' if b is None else (b.decode('utf-8', errors='surrogatepass') if isinstance(b, bytes) else b)


def _parts_array(args, what):
    """NULL-terminated C string array of path arguments (for the *_parts C functions)"""
    if not args:
        raise TypeError(f"{what}() requires at least 1 argument")
    for arg in args:
        if isinstance(arg, bytes):
            raise TypeError("argument should be a str or os.PathLike object, not bytes")
    parts = [_encode(os.fspath(a)) for a in args] + [None]
    return (c_char_p * len(parts))(*parts)


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

    def __init__(self, *args, **kwargs):
        self._sp = _SpPath()
        for arg in args:
            if isinstance(arg, bytes):
                raise TypeError(
                    "argument should be a str or an os.PathLike object "
                    "where __fspath__ returns a str, not 'bytes'"
                )
        self._load(args[0] if args else '', self._sp)
        self._join_into(self._sp, args[1:])

    def _load(self, arg, out):
        """Parse one path argument into out, converting from another flavor if needed"""
        if isinstance(arg, PurePath) and arg._flavor == self._flavor:
            _lib.sp_path_copy_wrap(byref(arg._sp), byref(out))
            return
        src_flavor = arg._flavor if isinstance(arg, PurePath) else _get_pathlib_flavor(arg)
        if src_flavor is not None:
            _lib.sp_path_convert_wrap(_encode(str(arg)), src_flavor, self._flavor, byref(out))
        else:
            buf = _encode_buf(os.fspath(arg))
            _lib.sp_path_new_len_wrap(buf, len(buf.raw), self._flavor, byref(out))

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

    def with_segments(self, *pathsegments):
        """Construct a new path object from any number of path-like objects."""
        parts = [_encode(os.fspath(a)) for a in pathsegments]
        parts_arr = (c_char_p * len(parts))(*parts) if parts else None
        out = _SpPath()
        _lib.sp_with_segments_wrap(byref(self._sp), parts_arr, len(parts), byref(out))
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

    is_absolute = _bool_method('is_absolute')
    is_reserved = _bool_method('is_reserved')

    def is_relative_to(self, *args):
        return bool(_lib.sp_is_relative_to_parts_wrap(byref(self._sp), _parts_array(args, 'is_relative_to')))

    def relative_to(self, *args, walk_up=False):
        out = _SpPath()
        parts = _parts_array(args, 'relative_to')
        _lib.sp_relative_to_parts_wrap(byref(self._sp), parts, 1 if walk_up else 0, byref(out))
        if _lib.sp_relative_to_is_error_wrap(byref(out)):
            raise ValueError(f"{str(self)!r} is not relative to {str(self.with_segments(*args))!r}")
        return self._from_sp(out)

    def joinpath(self, *others):
        for other in others:
            if isinstance(other, bytes):
                raise TypeError(
                    "argument should be a str or an os.PathLike object "
                    "where __fspath__ returns a str, not 'bytes'"
                )
        out = _SpPath()
        _lib.sp_path_copy_wrap(byref(self._sp), byref(out))
        self._join_into(out, others)
        return self._from_sp(out)

    def _with_field(self, type_name, value):
        if not isinstance(value, str):
            raise TypeError(f"expected str, not {type(value).__name__}")
        out = _SpPath()
        getattr(_lib, f'sp_with_{type_name}_wrap')(byref(self._sp), _encode(value), byref(out))
        err = _lib.sp_path_error_code_wrap(byref(out))
        if err == SP_ERR_NO_NAME:
            raise ValueError(f"{self!r} has an empty name")
        if err == SP_ERR_INVALID_ARG:
            raise ValueError(f"Invalid {type_name} {value!r}")
        return self._from_sp(out)

    def with_name(self, name):
        return self._with_field('name', name)

    def with_stem(self, stem):
        return self._with_field('stem', stem)

    def with_suffix(self, suffix):
        return self._with_field('suffix', suffix)

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
    def cwd(cls):
        result = cls.__new__(cls)
        result._sp = _SpPath()
        _lib.sp_cwd_wrap(result._flavor, byref(result._sp))
        return result

    def absolute(self):
        out = _SpPath()
        _lib.sp_absolute_wrap(byref(self._sp), byref(out))
        return self._from_sp(out)

    is_file = _bool_method('is_file')
    is_dir = _bool_method('is_dir')
    exists = _bool_method('exists')
    is_symlink = _bool_method('is_symlink')
    is_block_device = _bool_method('is_block_device')
    is_char_device = _bool_method('is_char_device')
    is_fifo = _bool_method('is_fifo')
    is_socket = _bool_method('is_socket')
    is_mount = _bool_method('is_mount')
    is_junction = _bool_method('is_junction')

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
        out = _SpPath()
        _lib.sp_readlink_wrap(byref(self._sp), byref(out))
        if _lib.sp_path_is_error_wrap(byref(out)):
            raise OSError(22, "Invalid argument", str(self))
        return self._from_sp(out)

    def resolve(self, strict=False):
        """Make the path absolute, resolving all symlinks."""
        out = _SpPath()
        _lib.sp_resolve_wrap(byref(self._sp), 1 if strict else 0, byref(out))
        if _lib.sp_path_is_error_wrap(byref(out)):
            raise FileNotFoundError(2, "No such file or directory", str(self))
        return self._from_sp(out)

    def _sp_of(self, other):
        """C path for a PurePath or path-like argument (parsed in this path's flavor)"""
        if isinstance(other, PurePath):
            return other._sp
        sp = _SpPath()
        buf = _encode_buf(os.fspath(other))
        _lib.sp_path_new_len_wrap(buf, len(buf.raw), self._flavor, byref(sp))
        return sp

    def symlink_to(self, target, target_is_directory=False):
        """Make this path a symlink pointing to target."""
        if not _lib.sp_symlink_to_wrap(byref(self._sp), byref(self._sp_of(target)), 1 if target_is_directory else 0):
            raise OSError(1, "Operation not permitted", str(self))

    def hardlink_to(self, target):
        """Make this path a hard link pointing to target."""
        if not _lib.sp_hardlink_to_wrap(byref(self._sp), byref(self._sp_of(target))):
            raise OSError(1, "Operation not permitted", str(self))

    def samefile(self, other_path):
        """Return True if both paths refer to the same file."""
        # First check that both paths exist (Python raises FileNotFoundError if either doesn't exist)
        if not self.exists():
            raise FileNotFoundError(2, "No such file or directory", str(self))
        other_sp = self._sp_of(other_path)
        if not _lib.sp_exists_wrap(byref(other_sp)):
            raise FileNotFoundError(2, "No such file or directory", str(other_path))
        return bool(_lib.sp_samefile_wrap(byref(self._sp), byref(other_sp)))

    def mkdir(self, mode=0o777, parents=False, exist_ok=False):
        result = _lib.sp_mkdir_wrap(byref(self._sp), mode, 1 if parents else 0, 1 if exist_ok else 0)
        if result == SP_OK:
            return
        path_str = str(self)
        if result == SP_ERR_EXISTS or result == SP_ERR_EXISTS_NOT_DIR:
            raise FileExistsError(17, "File exists", path_str)
        elif result == SP_ERR_NOT_FOUND:
            raise FileNotFoundError(2, "No such file or directory", path_str)
        elif result == SP_ERR_NOT_DIR:
            raise NotADirectoryError(20, "Not a directory", path_str)
        elif result == SP_ERR_PERMISSION:
            raise PermissionError(13, "Permission denied", path_str)
        else:
            raise OSError(0, "Unknown error", path_str)

    def _glob(self, begin, pattern, case_sensitive):
        if isinstance(pattern, bytes):
            raise TypeError("argument should be a str or os.PathLike object, not bytes")
        cs = SP_CASE_PLATFORM_DEFAULT if case_sensitive is None else (SP_CASE_SENSITIVE if case_sensitive else SP_CASE_INSENSITIVE)
        results = []
        it = _SpGlobIter()
        match = _SpPath()
        begin(byref(self._sp), _encode(str(pattern)), cs, byref(it))
        while _lib.sp_glob_next_wrap(byref(it), byref(match)):
            results.append(self._from_sp(match))
        _lib.sp_glob_end_wrap(byref(it))
        return iter(results)

    def glob(self, pattern, *, case_sensitive=None):
        """Iterate over this subtree and yield all existing files matching pattern."""
        if not isinstance(pattern, bytes) and not str(pattern):
            raise ValueError("Unacceptable pattern: ''")
        return self._glob(_lib.sp_glob_begin_wrap, pattern, case_sensitive)

    def rglob(self, pattern, *, case_sensitive=None):
        """Recursively yield all existing files matching pattern."""
        return self._glob(_lib.sp_rglob_begin_wrap, pattern, case_sensitive)

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

    def _move(self, func, target):
        out = _SpPath()
        func(byref(self._sp), byref(self._sp_of(target)), byref(out))
        if _lib.sp_path_is_error_wrap(byref(out)):
            raise OSError(1, "Operation not permitted", str(self), str(target))
        return self._from_sp(out)

    def rename(self, target):
        """Rename this file/directory to the given target."""
        return self._move(_lib.sp_rename_wrap, target)

    def replace(self, target):
        """Replace target with this file (atomic operation)."""
        return self._move(_lib.sp_replace_wrap, target)

    def chmod(self, mode):
        """Change the file mode (permissions)."""
        if not _lib.sp_chmod_wrap(byref(self._sp), mode):
            raise OSError(1, "Operation not permitted", str(self))

    def read_bytes(self):
        """Read the file contents as bytes."""
        st = self.stat()
        size = st.st_size
        if size == 0:
            return b''
        buf = create_string_buffer(size)
        bytes_out = c_size_t()
        error_out = c_int()
        _lib.sp_read_file_wrap(byref(self._sp), buf, size, byref(bytes_out), byref(error_out))
        err = error_out.value
        if err == SP_ERR_OPEN:
            raise FileNotFoundError(2, "No such file or directory", str(self))
        if err != SP_OK:
            raise OSError(5, "I/O error", str(self))
        return buf.raw[:bytes_out.value]

    def write_bytes(self, data):
        """Write bytes to the file."""
        if not isinstance(data, (bytes, bytearray, memoryview)):
            raise TypeError(f"expected bytes-like object, not {type(data).__name__}")
        data = bytes(data)
        bytes_out = c_size_t()
        error_out = c_int()
        _lib.sp_write_file_wrap(byref(self._sp), data, len(data), byref(bytes_out), byref(error_out))
        err = error_out.value
        if err == SP_ERR_OPEN:
            raise FileNotFoundError(2, "No such file or directory", str(self))
        if err != SP_OK:
            raise OSError(5, "I/O error", str(self))
        return bytes_out.value

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
        out = _SpPath()
        _lib.sp_expanduser_wrap(byref(self._sp), byref(out))
        if _lib.sp_path_is_error_wrap(byref(out)):
            raise RuntimeError("Could not expand user")
        return self._from_sp(out)

    def _id_name(self, which):
        if not _HAS_OWNER_GROUP:
            raise NotImplementedError(f"Path.{which}() is unsupported on this system")
        name = _term(getattr(_lib, f'sp_{which}_wrap'), self._sp)
        if not name:
            raise FileNotFoundError(2, "No such file or directory", str(self))
        return name

    def owner(self):
        """Return the file owner name."""
        return self._id_name('owner')

    def group(self):
        """Return the file group name."""
        return self._id_name('group')

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
                yield self._from_sp(out)
        finally:
            _lib.sp_iterdir_end_wrap(byref(it))

    def walk(self, top_down=True, on_error=None, follow_symlinks=False):
        """Walk directory tree, yielding (dirpath, dirnames, filenames) tuples.
        Uses iterdir + is_dir rather than sp_walk because Python's walk API
        requires generator semantics with in-place dirnames pruning between yields,
        which can't be expressed through a C callback that runs to completion."""
        if not self.is_dir():
            return

        def _scan(dirpath):
            try:
                entries = sorted(dirpath.iterdir(), key=lambda e: str(e))
            except OSError as e:
                if on_error is not None:
                    on_error(e)
                return None, None
            dirs, files = [], []
            for entry in entries:
                if follow_symlinks:
                    is_dir = entry.is_dir()
                else:
                    is_dir = entry.is_dir() and not entry.is_symlink()
                if is_dir:
                    dirs.append(entry.name)
                else:
                    files.append(entry.name)
            return dirs, files

        if top_down:
            stack = [self]
            while stack:
                dirpath = stack.pop()
                dirs, files = _scan(dirpath)
                if dirs is None:
                    continue
                yield (dirpath, dirs, files)
                stack.extend(reversed([dirpath / d for d in dirs]))
        else:
            stack = [(self, False)]
            while stack:
                dirpath, visited = stack[-1]
                if visited:
                    stack.pop()
                    dirs, files = _scan(dirpath)
                    if dirs is not None:
                        yield (dirpath, dirs, files)
                else:
                    stack[-1] = (dirpath, True)
                    dirs, files = _scan(dirpath)
                    if dirs is None:
                        stack.pop()
                        continue
                    for d in reversed(dirs):
                        stack.append((dirpath / d, False))


class PosixPath(Path, PurePosixPath):
    __slots__ = ()


class WindowsPath(Path, PureWindowsPath):
    __slots__ = ()


__all__ = [
    'PurePath', 'PurePosixPath', 'PureWindowsPath',
    'Path', 'PosixPath', 'WindowsPath',
]
