/* _snakepath.c - the C side of the snakepath Python package: pathlib's PurePath and Path over snakepath.h, as methods
 * with pathlib's own signatures, so no Python frame runs between a call and the library. Glue only (argument
 * coercion, Python's exceptions and protocols): the path semantics are the header's. A call past the header's fixed
 * limits (SP_PATH_MAX, SP_MAX_SUFFIXES, glob's depth and pattern length, the walk's names buffer) is handed to
 * snakepath._defer, which makes it on CPython's pathlib. */
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <errno.h>
#include <string.h>

#ifndef SP_PATH_MAX
#ifdef _WIN32
#define SP_PATH_MAX SP_PATH_MAX_WINDOWS
#else
#define SP_PATH_MAX SP_PATH_MAX_LINUX
#endif
#endif
#define SNAKEPATH_IMPLEMENTATION
#include "snakepath.h"

#define WALK_BUF_SIZE (16 << 20) /* the names of the directories being walked; untouched pages cost nothing */

/* A path: its normalized text as a str ('' for the empty path, which str() shows as '.'), its UTF-8 bytes when it
 * holds lone surrogates (which a str's own UTF-8 can't), and where its drive and anchor end. Text past SP_PATH_MAX
 * is pathlib's, and every call on it is deferred. */
typedef struct {
    PyObject_HEAD PyObject *text;
    PyObject *wtf8;
    Py_hash_t hash;
    size_t anchor;
    size_t drive;
    int flavor;
} PathObject;

static PyTypeObject PurePathType;
static PyTypeObject PathType;
static PyTypeObject WalkType;
static PyObject *module;
static PyObject *UnsupportedOperation;
static PyObject *stat_result;
static Py_ssize_t stat_size;
static signed char stat_value[64];
static PyObject *no_args;
static PyObject *str_flavor;
static PyObject *str_native;
static PyObject *str_with_segments;
static PyObject *str_empty;

/* A C-style cast for the method tables' keyword functions */
#define KW (PyCFunction)(void (*)(void))

static int is_path(PyObject *o) { return PyObject_TypeCheck(o, &PurePathType); }

/* ============ Text ============ */

static PyObject *decode(const char *s, size_t len) { return PyUnicode_DecodeUTF8(s, (Py_ssize_t)len, "surrogatepass"); }

/* s as snakepath takes text: UTF-8, a lone surrogate as itself (WTF-8); *owner keeps the bytes alive when s's own
 * UTF-8 can't hold them */
static const char *utf8(PyObject *s, Py_ssize_t *len, PyObject **owner) {
    const char *u = PyUnicode_AsUTF8AndSize(s, len);
    *owner = NULL;
    if (u || !PyErr_ExceptionMatches(PyExc_UnicodeEncodeError))
        return u;

    PyErr_Clear();
    *owner = PyUnicode_AsEncodedString(s, "utf-8", "surrogatepass");
    if (!*owner)
        return NULL;
    *len = PyBytes_GET_SIZE(*owner);
    return PyBytes_AS_STRING(*owner);
}

/* os.fspath(arg)'s str as UTF-8 (a bytes path is pathlib's TypeError); *owner keeps it alive */
static const char *fspath_text(PyObject *arg, Py_ssize_t *len, PyObject **owner) {
    PyObject *s = PyOS_FSPath(arg);
    if (s && !PyUnicode_Check(s)) {
        PyErr_Format(PyExc_TypeError,
                     "argument should be a str or an os.PathLike object where __fspath__ returns a str, not '%s'",
                     Py_TYPE(s)->tp_name);
        Py_CLEAR(s);
    }

    const char *u = s ? utf8(s, len, owner) : NULL;
    if (u && !*owner)
        *owner = Py_NewRef(s);
    Py_XDECREF(s);
    return u;
}

static const char *path_text(PathObject *p, Py_ssize_t *len) {
    if (p->wtf8) {
        *len = PyBytes_GET_SIZE(p->wtf8);
        return PyBytes_AS_STRING(p->wtf8);
    }
    return PyUnicode_AsUTF8AndSize(p->text, len); /* made when the text was set, so it can't fail */
}

/* p as the header takes it, or -1 when its text is past SP_PATH_MAX */
static int load(PathObject *p, SpPath *sp) {
    Py_ssize_t len;
    const char *s = path_text(p, &len);
    if (len >= SP_PATH_MAX)
        return -1;

    memcpy(sp->buf, s, (size_t)len);
    sp->buf[len] = '\0';
    sp->len = (size_t)len;
    sp->anchor = p->anchor;
    sp->drive = p->drive;
    sp->flavor = (SpFlavor)p->flavor;
    sp->error = SP_OK;
    return 0;
}

/* p's text becomes text (a reference given to p), with its WTF-8 bytes when its own UTF-8 can't hold it */
static int set_text(PathObject *p, PyObject *text) {
    PyObject *wtf8 = NULL;
    if (!text)
        return -1;
    if (!PyUnicode_AsUTF8AndSize(text, NULL)) {
        PyErr_Clear();
        wtf8 = PyUnicode_AsEncodedString(text, "utf-8", "surrogatepass");
        if (!wtf8) {
            Py_DECREF(text);
            return -1;
        }
    }

    Py_XSETREF(p->text, text);
    Py_XSETREF(p->wtf8, wtf8);
    p->hash = -1;
    return 0;
}

static int set_path(PathObject *p, const SpPath *sp) {
    p->anchor = sp->anchor;
    p->drive = sp->drive;
    p->flavor = sp->flavor;
    return set_text(p, decode(sp->buf, sp->len));
}

/* ============ Errors and deferred calls ============ */

/* The exception for an SpError: the OS's errors are OSErrors with path (and target) as their filenames */
static PyObject *error_of(SpError err, PyObject *path, PyObject *target) {
    static const struct {
        SpError error;
        int code;
    } codes[] = {{SP_ERR_IO, EIO},
                 {SP_ERR_NOT_FOUND, ENOENT},
                 {SP_ERR_EXISTS, EEXIST},
                 {SP_ERR_NOT_DIR, ENOTDIR},
                 {SP_ERR_IS_DIR, EISDIR},
                 {SP_ERR_NOT_EMPTY, ENOTEMPTY},
                 {SP_ERR_PERMISSION, EACCES},
                 {SP_ERR_LOOP, ELOOP},
                 {SP_ERR_NOT_LINK, EINVAL},
                 {SP_ERR_CROSS_DEVICE, EXDEV},
                 {SP_ERR_SAME_FILE, EINVAL},
                 {SP_ERR_TOO_LONG, ENAMETOOLONG},
                 {SP_ERR_LIMIT, E2BIG},
                 {SP_ERR_INVALID_ARG, EINVAL}};
    for (size_t i = 0; i < SP_ARRAY_LEN(codes); i++) {
        if (codes[i].error != err)
            continue;

        PyObject *a = path ? PyOS_FSPath(path) : Py_NewRef(Py_None);
        PyObject *b = target ? PyOS_FSPath(target) : Py_NewRef(Py_None);
        PyObject *e = a && b ? PyObject_CallFunction(PyExc_OSError, "isOOO", codes[i].code, strerror(codes[i].code), a,
                                                     Py_None, b)
                             : NULL;
        Py_XDECREF(a);
        Py_XDECREF(b);
        return e;
    }

    if (err == SP_ERR_NUL)
        return PyObject_CallFunction(PyExc_ValueError, "s", "embedded null byte");
    if (err == SP_ERR_UNSUPPORTED)
        return PyObject_CallFunction(UnsupportedOperation, "s", sp_error_str(err));
    if (err == SP_ERR_NO_HOME)
        return PyObject_CallFunction(PyExc_RuntimeError, "s", "Could not determine home directory.");
    return PyObject_CallFunction(PyExc_ValueError, "s", sp_error_str(err));
}

/* Raises error_of's exception; returns NULL */
static PyObject *fail(SpError err, PyObject *path, PyObject *target) {
    PyObject *e = error_of(err, path, target);
    if (e) {
        PyErr_SetObject((PyObject *)Py_TYPE(e), e);
        Py_DECREF(e);
    }
    return NULL;
}

/* Where the header's fixed limits stop it short of pathlib's answer */
static int past_limits(SpError err) { return err == SP_ERR_TOO_LONG || err == SP_ERR_LIMIT; }

/* A call as snakepath._defer makes it on pathlib */
typedef struct {
    PyObject *self;   /* the path, or the class of cwd, home and from_uri */
    const char *name; /* NULL: the constructor */
    PyObject *args;   /* NULL: a property */
    PyObject *kwargs;
    PyObject *arg; /* a one-argument method's argument, instead of args */
} Call;

static PyObject *defer(const Call *c) {
    PyObject *args = c->arg ? PyTuple_Pack(1, c->arg) : Py_NewRef(c->args ? c->args : Py_None);
    PyObject *name = c->name ? PyUnicode_FromString(c->name) : Py_NewRef(Py_None);
    PyObject *r = args && name ? PyObject_CallMethod(module, "_defer", "OOOO", c->self, name, args,
                                                     c->kwargs ? c->kwargs : Py_None)
                               : NULL;
    Py_XDECREF(args);
    Py_XDECREF(name);
    return r;
}

/* ============ Making paths ============ */

/* PurePath() and Path() make the platform's class */
static PyTypeObject *native(PyTypeObject *type) {
    PyObject *native = PyDict_GetItemWithError(type->tp_dict, str_native);
    return native && PyType_Check(native) ? (PyTypeObject *)native : type;
}

/* A path of type from sp, like's __dict__ carried over (pathlib derives paths through with_segments, which a subclass
 * overrides to keep its attributes), or sp's error raised for like (and target) */
static PyObject *make(PyTypeObject *type, PyObject *like, const SpPath *sp, PyObject *target) {
    if (sp->error != SP_OK)
        return fail(sp->error, like, target);

    PathObject *p = (PathObject *)type->tp_alloc(type, 0);
    if (!p || set_path(p, sp) < 0) {
        Py_XDECREF(p);
        return NULL;
    }
    if (!like || !type->tp_dictoffset)
        return (PyObject *)p;

    PyObject *dict = PyObject_GenericGetDict(like, NULL);
    PyObject *copy = dict && PyDict_GET_SIZE(dict) > 0 ? PyDict_Copy(dict) : NULL;
    int ok = dict && (PyDict_GET_SIZE(dict) == 0 || (copy && PyObject_GenericSetDict((PyObject *)p, copy, NULL) == 0));
    Py_XDECREF(dict);
    Py_XDECREF(copy);
    if (!ok)
        Py_CLEAR(p);
    return (PyObject *)p;
}

/* The path a call made: a path like the one it was called on (an instance of the class of a class method), its error
 * raised, or past the header's limits the call deferred */
static PyObject *answer(const Call *c, const SpPath *sp, PyObject *target) {
    if (past_limits(sp->error))
        return defer(c);
    if (PyType_Check(c->self))
        return make(native((PyTypeObject *)c->self), NULL, sp, target);
    return make(Py_TYPE(c->self), c->self, sp, target);
}

/* A class's _flavor (0 is the platform's), or -1 with the error raised */
static int class_flavor(PyTypeObject *type) {
    PyObject *f = PyObject_GetAttr((PyObject *)type, str_flavor);
    long flavor = f ? PyLong_AsLong(f) : -1;
    Py_XDECREF(f);
    if (flavor == SP_FLAVOR_NATIVE)
#ifdef _WIN32
        flavor = SP_FLAVOR_WINDOWS;
#else
        flavor = SP_FLAVOR_POSIX;
#endif
    return (int)flavor;
}

/* An argument as a path in flavor, the way pathlib's with_segments makes one (another flavor's path converted): 0, 1
 * past the header's limits, or -1 with the error raised */
static int arg_path(PyObject *arg, int flavor, SpPath *out) {
    if (is_path(arg)) {
        if (load((PathObject *)arg, out) < 0)
            return 1;
        if (out->flavor != (SpFlavor)flavor) {
            SpPath from = *out;
            *out = sp_path_convert(from.buf, from.flavor, (SpFlavor)flavor);
        }
    } else {
        PyObject *owner;
        Py_ssize_t len;
        const char *s = fspath_text(arg, &len, &owner);
        if (!s)
            return -1;
        *out = sp_path_from_n(s, (size_t)len, (SpFlavor)flavor);
        Py_DECREF(owner);
    }

    if (past_limits(out->error))
        return 1;
    if (out->error != SP_OK) {
        fail(out->error, arg, NULL);
        return -1;
    }
    return 0;
}

/* sp with arg joined on as pathlib joins it (a path's text, or a str as it is): 0, or -1 with the error raised; past
 * the header's limits, sp->error is SP_ERR_TOO_LONG */
static int join_arg(SpPath *sp, PyObject *arg) {
    if (sp->error != SP_OK)
        return 0;

    if (is_path(arg)) {
        SpPath part;
        int parsed = arg_path(arg, sp->flavor, &part);
        if (parsed > 0)
            sp->error = SP_ERR_TOO_LONG;
        else if (parsed == 0)
            *sp = sp_join_n(sp, part.buf, part.len);
        return parsed < 0 ? -1 : 0;
    }

    PyObject *owner;
    Py_ssize_t len;
    const char *s = fspath_text(arg, &len, &owner);
    if (!s)
        return -1;
    *sp = sp_join_n(sp, s, (size_t)len);
    Py_DECREF(owner);
    return 0;
}

/* ============ PurePath ============ */

/* An empty path of the class's flavor until __init__ */
static PyObject *path_new(PyTypeObject *type, PyObject *Py_UNUSED(args), PyObject *Py_UNUSED(kwargs)) {
    type = native(type);
    int flavor = class_flavor(type);
    PathObject *p = flavor < 0 ? NULL : (PathObject *)type->tp_alloc(type, 0);
    if (!p)
        return NULL;

    p->text = Py_NewRef(str_empty);
    p->hash = -1;
    p->flavor = flavor;
    return (PyObject *)p;
}

static int path_init(PathObject *self, PyObject *args, PyObject *kwargs) {
    if (kwargs && PyDict_GET_SIZE(kwargs) > 0) {
        PyErr_SetString(PyExc_TypeError, "PurePath() takes no keyword arguments");
        return -1;
    }

    SpPath sp = sp_path_from_n("", 0, (SpFlavor)self->flavor);
    for (Py_ssize_t i = 0; i < PyTuple_GET_SIZE(args); i++)
        if (join_arg(&sp, PyTuple_GET_ITEM(args, i)) < 0)
            return -1;
    if (sp.error == SP_OK)
        return set_path(self, &sp);
    if (!past_limits(sp.error)) {
        fail(sp.error, NULL, NULL);
        return -1;
    }

    /* Past the header's limits: pathlib's text, parsed again when that fits */
    Call c = {.self = (PyObject *)self, .args = args};
    self->anchor = 0;
    self->drive = 0;
    if (set_text(self, defer(&c)) < 0)
        return -1;
    if (load(self, &sp) < 0)
        return 0;
    SpPath parsed = sp_path_from_n(sp.buf, sp.len, sp.flavor);
    return set_path(self, &parsed);
}

static void path_dealloc(PathObject *self) {
    Py_XDECREF(self->text);
    Py_XDECREF(self->wtf8);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *path_str(PathObject *self) {
    if (PyUnicode_GET_LENGTH(self->text) == 0)
        return PyUnicode_FromString(".");
    return Py_NewRef(self->text);
}

static PyObject *path_fspath(PathObject *self, PyObject *Py_UNUSED(ignored)) { return path_str(self); }

static Py_hash_t path_hash(PathObject *self) {
    SpPath sp;
    if (self->hash != -1)
        return self->hash;

    if (load(self, &sp) < 0) {
        Call c = {.self = (PyObject *)self, .name = "__hash__", .args = no_args};
        PyObject *h = defer(&c);
        self->hash = h ? PyLong_AsSsize_t(h) : -1;
        Py_XDECREF(h);
        return self->hash;
    }
    Py_hash_t h = (Py_hash_t)sp_path_hash(&sp);
    self->hash = h == -1 ? -2 : h;
    return self->hash;
}

/* == across flavors is false; ordering them is a TypeError (NotImplemented both ways) */
static PyObject *path_richcompare(PyObject *a, PyObject *b, int op) {
    static const char *const names[] = {"__lt__", "__le__", "__eq__", "__ne__", "__gt__", "__ge__"};
    int ordered = op != Py_EQ && op != Py_NE;
    if (!is_path(b) || (ordered && ((PathObject *)a)->flavor != ((PathObject *)b)->flavor))
        Py_RETURN_NOTIMPLEMENTED;

    SpPath x;
    SpPath y;
    if (load((PathObject *)a, &x) < 0 || load((PathObject *)b, &y) < 0) {
        Call c = {.self = a, .name = names[op], .arg = b};
        return defer(&c);
    }
    if (!ordered)
        return PyBool_FromLong(sp_path_eq(&x, &y) == (op == Py_EQ));
    int cmp = sp_path_cmp(&x, &y);
    Py_RETURN_RICHCOMPARE(cmp, 0, op);
}

static PyObject *path_joinpath(PathObject *self, PyObject *args) {
    Call c = {.self = (PyObject *)self, .name = "joinpath", .args = args};
    SpPath sp;
    if (load(self, &sp) < 0)
        return defer(&c);

    for (Py_ssize_t i = 0; i < PyTuple_GET_SIZE(args); i++)
        if (join_arg(&sp, PyTuple_GET_ITEM(args, i)) < 0)
            return NULL;
    return answer(&c, &sp, NULL);
}

/* a / b: a joined with b, or b.with_segments(a, b) when only b is a path */
static PyObject *path_div(PyObject *a, PyObject *b) {
    PyObject *r;
    if (is_path(a)) {
        Call c = {.self = a, .name = "joinpath", .arg = b};
        SpPath sp;
        if (load((PathObject *)a, &sp) < 0)
            r = defer(&c);
        else
            r = join_arg(&sp, b) < 0 ? NULL : answer(&c, &sp, NULL);
    } else {
        r = PyObject_CallMethodObjArgs(b, str_with_segments, a, b, NULL);
    }

    if (!r && PyErr_ExceptionMatches(PyExc_TypeError)) {
        PyErr_Clear();
        Py_RETURN_NOTIMPLEMENTED;
    }
    return r;
}

/* drive, root, anchor, name, stem and suffix, by the getset's closure */
static const char *const term_names[] = {"drive", "root", "anchor", "name", "stem", "suffix"};
static SpTerm (*const terms[])(const SpPath *) = {sp_drive, sp_root, sp_anchor, sp_name, sp_stem, sp_suffix};

static PyObject *get_term(PathObject *self, void *which) {
    SpPath sp;
    if (load(self, &sp) < 0) {
        Call c = {.self = (PyObject *)self, .name = term_names[(intptr_t)which]};
        return defer(&c);
    }
    SpTerm t = terms[(intptr_t)which](&sp);
    return decode(t.buf, t.len);
}

static PyObject *get_parent(PathObject *self, void *Py_UNUSED(closure)) {
    Call c = {.self = (PyObject *)self, .name = "parent"};
    SpPath sp;
    if (load(self, &sp) < 0)
        return defer(&c);
    SpPath parent = sp_parent(&sp);
    return answer(&c, &parent, NULL);
}

/* A tuple (pathlib's is a sequence of its own) */
static PyObject *get_parents(PathObject *self, void *Py_UNUSED(closure)) {
    Call c = {.self = (PyObject *)self, .name = "parents"};
    SpPath sp;
    SpPath parent;
    if (load(self, &sp) < 0)
        return defer(&c);

    PyObject *list = PyList_New(0);
    for (SpParentsIter it = sp_parents_begin(&sp); list && sp_parents_next(&it, &parent);) {
        PyObject *p = make(Py_TYPE(self), (PyObject *)self, &parent, NULL);
        if (!p || PyList_Append(list, p) < 0)
            Py_CLEAR(list);
        Py_XDECREF(p);
    }
    PyObject *parents = list ? PyList_AsTuple(list) : NULL;
    Py_XDECREF(list);
    return parents;
}

static PyObject *get_parts(PathObject *self, void *Py_UNUSED(closure)) {
    Call c = {.self = (PyObject *)self, .name = "parts"};
    SpPath sp;
    SpStr part;
    if (load(self, &sp) < 0)
        return defer(&c);

    Py_ssize_t count = 0;
    for (SpPartsIter it = sp_parts_begin(&sp); sp_parts_next(&it, &part);)
        count++;

    PyObject *parts = PyTuple_New(count);
    SpPartsIter it = sp_parts_begin(&sp);
    for (Py_ssize_t i = 0; parts && sp_parts_next(&it, &part); i++) {
        PyObject *s = decode(part.data, part.len);
        if (s)
            PyTuple_SET_ITEM(parts, i, s);
        else
            Py_CLEAR(parts);
    }
    return parts;
}

static PyObject *get_suffixes(PathObject *self, void *Py_UNUSED(closure)) {
    Call c = {.self = (PyObject *)self, .name = "suffixes"};
    SpPath sp;
    if (load(self, &sp) < 0)
        return defer(&c);
    SpSuffixes s = sp_suffixes(&sp);
    if (s.error != SP_OK)
        return past_limits(s.error) ? defer(&c) : fail(s.error, (PyObject *)self, NULL);

    PyObject *list = PyList_New((Py_ssize_t)s.count);
    for (size_t i = 0; list && i < s.count; i++) {
        PyObject *item = decode(s.items[i].data, s.items[i].len);
        if (item)
            PyList_SET_ITEM(list, (Py_ssize_t)i, item);
        else
            Py_CLEAR(list);
    }
    return list;
}

/* with_name, with_stem and with_suffix: method is "with_" and the part's kind */
static PyObject *with_part(PathObject *self, PyObject *value, SpPath (*with)(const SpPath *, const char *),
                           const char *method) {
    const char *kind = method + strlen("with_");
    Call c = {.self = (PyObject *)self, .name = method, .arg = value};
    PyObject *owner;
    Py_ssize_t len;
    SpPath sp;
    if (!PyUnicode_Check(value))
        return PyErr_Format(PyExc_TypeError, "expected str, not %s", Py_TYPE(value)->tp_name);
    if (load(self, &sp) < 0)
        return defer(&c);
    const char *s = utf8(value, &len, &owner);
    if (!s)
        return NULL;

    SpPath w = with(&sp, s);
    Py_XDECREF(owner);
    if (w.error == SP_ERR_NO_NAME)
        return PyErr_Format(PyExc_ValueError, "%R has an empty name", self);
    if (w.error == SP_ERR_INVALID_ARG)
        return PyErr_Format(PyExc_ValueError, "Invalid %s %R", kind, value);
    return answer(&c, &w, NULL);
}

static PyObject *path_with_name(PathObject *self, PyObject *v) { return with_part(self, v, sp_with_name, "with_name"); }
static PyObject *path_with_stem(PathObject *self, PyObject *v) { return with_part(self, v, sp_with_stem, "with_stem"); }
static PyObject *path_with_suffix(PathObject *self, PyObject *v) {
    return with_part(self, v, sp_with_suffix, "with_suffix");
}

static PyObject *path_relative_to(PathObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"other", "walk_up", NULL};
    Call c = {.self = (PyObject *)self, .name = "relative_to", .args = args, .kwargs = kwargs};
    PyObject *other;
    int walk_up = 0;
    SpPath sp;
    SpPath o;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|$p", kwlist, &other, &walk_up))
        return NULL;
    int parsed = arg_path(other, self->flavor, &o);
    if (parsed < 0)
        return NULL;
    if (parsed > 0 || load(self, &sp) < 0)
        return defer(&c);

    SpPath r = sp_relative_to(&sp, &o, walk_up != 0);
    if (r.error != SP_ERR_NOT_RELATIVE)
        return answer(&c, &r, NULL);
    PyObject *a = path_str(self);
    PyObject *b = decode(o.len ? o.buf : ".", o.len ? o.len : 1);
    if (a && b)
        PyErr_Format(PyExc_ValueError, "%R is not relative to %R", a, b);
    Py_XDECREF(a);
    Py_XDECREF(b);
    return NULL;
}

static PyObject *path_is_relative_to(PathObject *self, PyObject *other) {
    Call c = {.self = (PyObject *)self, .name = "is_relative_to", .arg = other};
    SpPath sp;
    SpPath o;
    int parsed = arg_path(other, self->flavor, &o);
    if (parsed < 0)
        return NULL;
    if (parsed > 0 || load(self, &sp) < 0)
        return defer(&c);
    return PyBool_FromLong(sp_is_relative_to(&sp, &o));
}

static PyObject *path_is_absolute(PathObject *self, PyObject *Py_UNUSED(ignored)) {
    Call c = {.self = (PyObject *)self, .name = "is_absolute", .args = no_args};
    SpPath sp;
    if (load(self, &sp) < 0)
        return defer(&c);
    return PyBool_FromLong(sp_is_absolute(&sp));
}

static PyObject *path_as_posix(PathObject *self, PyObject *Py_UNUSED(ignored)) {
    Call c = {.self = (PyObject *)self, .name = "as_posix", .args = no_args};
    SpPath sp;
    if (load(self, &sp) < 0)
        return defer(&c);
    SpTerm t = sp_as_posix(&sp);
    return decode(t.buf, t.len);
}

static PyObject *path_as_uri(PathObject *self, PyObject *Py_UNUSED(ignored)) {
    Call c = {.self = (PyObject *)self, .name = "as_uri", .args = no_args};
    char uri[SP_PATH_MAX * 3 + 8]; /* every byte percent-encoded, after "file:///" */
    SpPath sp;
    if (load(self, &sp) < 0)
        return defer(&c);

    SpError err = sp_as_uri(&sp, uri, sizeof uri);
    if (err == SP_ERR_NOT_ABSOLUTE)
        return PyErr_Format(PyExc_ValueError, "relative path can't be expressed as a file URI");
    return err != SP_OK ? fail(err, (PyObject *)self, NULL) : PyUnicode_FromString(uri);
}

static SpCaseSensitivity case_of(PyObject *case_sensitive) {
    if (case_sensitive == Py_None)
        return SP_CASE_DEFAULT;
    return PyObject_IsTrue(case_sensitive) ? SP_CASE_SENSITIVE : SP_CASE_INSENSITIVE;
}

/* match and full_match, on the pattern's text as given; match's pattern must have a part, as pathlib's must */
static PyObject *match(PathObject *self, PyObject *args, PyObject *kwargs, bool full) {
    static char *kwlist[] = {"pattern", "case_sensitive", NULL};
    static char *match_kwlist[] = {"path_pattern", "case_sensitive", NULL};
    Call c = {.self = (PyObject *)self, .name = full ? "full_match" : "match", .args = args, .kwargs = kwargs};
    PyObject *pattern;
    PyObject *case_sensitive = Py_None;
    PyObject *owner;
    Py_ssize_t len;
    SpPath sp;
    SpPath pat;
    SpStr part;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|$O", full ? kwlist : match_kwlist, &pattern, &case_sensitive))
        return NULL;
    int parsed = arg_path(pattern, self->flavor, &pat);
    if (parsed < 0)
        return NULL;
    if (parsed > 0 || load(self, &sp) < 0)
        return defer(&c);
    SpPartsIter parts = sp_parts_begin(&pat);
    if (!full && !sp_parts_next(&parts, &part))
        return PyErr_Format(PyExc_ValueError, "empty pattern");

    const char *text = fspath_text(pattern, &len, &owner);
    if (!text)
        return NULL;
    SpMatchOptions options = {full, case_of(case_sensitive)};
    bool matched = sp_match(&sp, text, options);
    Py_DECREF(owner);
    return PyBool_FromLong(matched);
}

static PyObject *path_match(PathObject *self, PyObject *args, PyObject *kw) { return match(self, args, kw, false); }
static PyObject *path_full_match(PathObject *self, PyObject *args, PyObject *kw) { return match(self, args, kw, true); }

static PyGetSetDef pure_getset[] = {
    {"drive", (getter)get_term, NULL, NULL, (void *)0},
    {"root", (getter)get_term, NULL, NULL, (void *)1},
    {"anchor", (getter)get_term, NULL, NULL, (void *)2},
    {"name", (getter)get_term, NULL, NULL, (void *)3},
    {"stem", (getter)get_term, NULL, NULL, (void *)4},
    {"suffix", (getter)get_term, NULL, NULL, (void *)5},
    {"parent", (getter)get_parent, NULL, NULL, NULL},
    {"parents", (getter)get_parents, NULL, NULL, NULL},
    {"parts", (getter)get_parts, NULL, NULL, NULL},
    {"suffixes", (getter)get_suffixes, NULL, NULL, NULL},
    {0},
};

static PyMethodDef pure_methods[] = {
    {"__fspath__", (PyCFunction)path_fspath, METH_NOARGS, NULL},
    {"joinpath", (PyCFunction)path_joinpath, METH_VARARGS, NULL},
    {"with_name", (PyCFunction)path_with_name, METH_O, NULL},
    {"with_stem", (PyCFunction)path_with_stem, METH_O, NULL},
    {"with_suffix", (PyCFunction)path_with_suffix, METH_O, NULL},
    {"relative_to", KW path_relative_to, METH_VARARGS | METH_KEYWORDS, NULL},
    {"is_relative_to", (PyCFunction)path_is_relative_to, METH_O, NULL},
    {"is_absolute", (PyCFunction)path_is_absolute, METH_NOARGS, NULL},
    {"as_posix", (PyCFunction)path_as_posix, METH_NOARGS, NULL},
    {"as_uri", (PyCFunction)path_as_uri, METH_NOARGS, NULL},
    {"match", KW path_match, METH_VARARGS | METH_KEYWORDS, NULL},
    {"full_match", KW path_full_match, METH_VARARGS | METH_KEYWORDS, NULL},
    {0},
};

static PyNumberMethods path_as_number = {.nb_true_divide = path_div};

static PyTypeObject PurePathType = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "_snakepath.PurePathBase",
    .tp_basicsize = sizeof(PathObject),
    .tp_dealloc = (destructor)path_dealloc,
    .tp_str = (reprfunc)path_str,
    .tp_hash = (hashfunc)path_hash,
    .tp_richcompare = path_richcompare,
    .tp_as_number = &path_as_number,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_getset = pure_getset,
    .tp_methods = pure_methods,
    .tp_init = (initproc)path_init,
    .tp_new = path_new,
};

/* ============ Path: the file system ============ */

/* os.stat_result's fields after the ten a tuple shows, which each platform keeps in its own places */
static const char *const stat_names[] = {"st_atime",    "st_mtime",    "st_ctime",      "st_atime_ns",
                                         "st_mtime_ns", "st_ctime_ns", "st_reparse_tag"};

/* Which value each of stat_result's fields gets (stat_value_of's), found once: a stat_result made of 0, 1, 2, ... shows
 * where each name is */
static int find_stat_fields(void) {
    PyObject *n = PyObject_GetAttrString(stat_result, "n_fields");
    stat_size = n ? PyLong_AsSsize_t(n) : -1;
    Py_XDECREF(n);
    PyObject *numbers = stat_size >= 10 && stat_size <= (Py_ssize_t)sizeof stat_value ? PyTuple_New(stat_size) : NULL;
    for (Py_ssize_t i = 0; numbers && i < stat_size; i++) {
        PyTuple_SET_ITEM(numbers, i, PyLong_FromSsize_t(i));
        stat_value[i] = (signed char)(i < 10 ? i : -1);
    }
    PyObject *sample = numbers ? PyObject_CallOneArg(stat_result, numbers) : NULL;
    Py_XDECREF(numbers);
    if (!sample)
        return -1;

    for (size_t k = 0; k < SP_ARRAY_LEN(stat_names); k++) {
        PyObject *at = PyObject_GetAttrString(sample, stat_names[k]);
        if (at)
            stat_value[PyLong_AsSsize_t(at)] = (signed char)(10 + k);
        else
            PyErr_Clear(); /* st_reparse_tag is Windows' */
        Py_XDECREF(at);
    }
    Py_DECREF(sample);
    return 0;
}

static PyObject *stat_value_of(const SpStatResult *st, int value) {
    switch (value) {
    case 0:
        return PyLong_FromUnsignedLong(st->sp_mode);
    case 1:
        return PyLong_FromUnsignedLongLong(st->sp_ino);
    case 2:
        return PyLong_FromUnsignedLongLong(st->sp_dev);
    case 3:
        return PyLong_FromUnsignedLongLong(st->sp_nlink);
    case 4:
        return PyLong_FromUnsignedLong(st->sp_uid);
    case 5:
        return PyLong_FromUnsignedLong(st->sp_gid);
    case 6:
        return PyLong_FromLongLong(st->sp_size);
    case 7:
        return PyLong_FromLongLong(st->sp_atime_ns / 1000000000);
    case 8:
        return PyLong_FromLongLong(st->sp_mtime_ns / 1000000000);
    case 9:
        return PyLong_FromLongLong(st->sp_ctime_ns / 1000000000);
    case 10:
        return PyFloat_FromDouble(st->sp_atime);
    case 11:
        return PyFloat_FromDouble(st->sp_mtime);
    case 12:
        return PyFloat_FromDouble(st->sp_ctime);
    case 13:
        return PyLong_FromLongLong(st->sp_atime_ns);
    case 14:
        return PyLong_FromLongLong(st->sp_mtime_ns);
    case 15:
        return PyLong_FromLongLong(st->sp_ctime_ns);
    case 16:
        return PyLong_FromUnsignedLong(st->sp_reparse_tag);
    default:
        return Py_NewRef(Py_None); /* what the C library doesn't read, like st_blocks */
    }
}

static PyObject *stat_of(PyObject *self, const Call *c, bool follow) {
    SpPath sp;
    if (load((PathObject *)self, &sp) < 0)
        return defer(c);
    SpStatResult st = sp_stat(&sp, follow);
    if (st.error != SP_OK)
        return past_limits(st.error) ? defer(c) : fail(st.error, self, NULL);

    PyObject *r = PyStructSequence_New((PyTypeObject *)stat_result);
    for (Py_ssize_t i = 0; r && i < stat_size; i++) {
        PyObject *value = stat_value_of(&st, stat_value[i]);
        if (value)
            PyStructSequence_SetItem(r, i, value);
        else
            Py_CLEAR(r);
    }
    return r;
}

static PyObject *p_stat(PyObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"follow_symlinks", NULL};
    Call c = {.self = self, .name = "stat", .args = args, .kwargs = kwargs};
    int follow = 1;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|$p", kwlist, &follow))
        return NULL;
    return stat_of(self, &c, follow != 0);
}

static PyObject *p_lstat(PyObject *self, PyObject *Py_UNUSED(ignored)) {
    Call c = {.self = self, .name = "lstat", .args = no_args};
    return stat_of(self, &c, false);
}

/* exists and the is_* predicates */
static PyObject *is_of(PyObject *self, const Call *c, SpFileType type, bool follow) {
    SpPath sp;
    if (load((PathObject *)self, &sp) < 0)
        return defer(c);
    return PyBool_FromLong(sp_is(&sp, type, follow));
}

/* exists, is_dir and is_file, which take follow_symlinks */
static PyObject *is_following(PyObject *self, PyObject *args, PyObject *kwargs, const char *name, SpFileType type) {
    static char *kwlist[] = {"follow_symlinks", NULL};
    Call c = {.self = self, .name = name, .args = args, .kwargs = kwargs};
    int follow = 1;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|$p", kwlist, &follow))
        return NULL;
    return is_of(self, &c, type, follow != 0);
}

static PyObject *is_plain(PyObject *self, const char *name, SpFileType type) {
    Call c = {.self = self, .name = name, .args = no_args};
    return is_of(self, &c, type, true);
}

static PyObject *p_exists(PyObject *s, PyObject *a, PyObject *k) { return is_following(s, a, k, "exists", SP_ANY); }
static PyObject *p_is_dir(PyObject *s, PyObject *a, PyObject *k) { return is_following(s, a, k, "is_dir", SP_DIR); }
static PyObject *p_is_file(PyObject *s, PyObject *a, PyObject *k) { return is_following(s, a, k, "is_file", SP_FILE); }
static PyObject *p_is_symlink(PyObject *s, PyObject *Py_UNUSED(a)) { return is_plain(s, "is_symlink", SP_SYMLINK); }
static PyObject *p_is_junction(PyObject *s, PyObject *Py_UNUSED(a)) { return is_plain(s, "is_junction", SP_JUNCTION); }
static PyObject *p_is_mount(PyObject *s, PyObject *Py_UNUSED(a)) { return is_plain(s, "is_mount", SP_MOUNT); }
static PyObject *p_is_fifo(PyObject *s, PyObject *Py_UNUSED(a)) { return is_plain(s, "is_fifo", SP_FIFO); }
static PyObject *p_is_socket(PyObject *s, PyObject *Py_UNUSED(a)) { return is_plain(s, "is_socket", SP_SOCKET); }
static PyObject *p_is_block_device(PyObject *s, PyObject *Py_UNUSED(a)) {
    return is_plain(s, "is_block_device", SP_BLOCK_DEVICE);
}
static PyObject *p_is_char_device(PyObject *s, PyObject *Py_UNUSED(a)) {
    return is_plain(s, "is_char_device", SP_CHAR_DEVICE);
}

/* absolute, expanduser and readlink: a path made from the path */
static PyObject *path_op(PyObject *self, const char *name, SpPath (*op)(const SpPath *)) {
    Call c = {.self = self, .name = name, .args = no_args};
    SpPath sp;
    if (load((PathObject *)self, &sp) < 0)
        return defer(&c);
    SpPath r = op(&sp);
    return answer(&c, &r, NULL);
}

static PyObject *p_absolute(PyObject *s, PyObject *Py_UNUSED(a)) { return path_op(s, "absolute", sp_absolute); }
static PyObject *p_expanduser(PyObject *s, PyObject *Py_UNUSED(a)) { return path_op(s, "expanduser", sp_expanduser); }
static PyObject *p_readlink(PyObject *s, PyObject *Py_UNUSED(a)) { return path_op(s, "readlink", sp_readlink); }

static PyObject *p_resolve(PyObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"strict", NULL};
    Call c = {.self = self, .name = "resolve", .args = args, .kwargs = kwargs};
    int strict = 0;
    SpPath sp;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|p", kwlist, &strict))
        return NULL;
    if (load((PathObject *)self, &sp) < 0)
        return defer(&c);
    SpPath r = sp_resolve(&sp, strict != 0);
    return answer(&c, &r, NULL);
}

/* The class methods cwd() and home() */
static PyObject *class_op(PyObject *cls, const char *name, SpPath (*op)(SpFlavor)) {
    Call c = {.self = cls, .name = name, .args = no_args};
    int flavor = class_flavor((PyTypeObject *)cls);
    if (flavor < 0)
        return NULL;
    SpPath r = op((SpFlavor)flavor);
    return answer(&c, &r, NULL);
}

static PyObject *p_cwd(PyObject *cls, PyObject *Py_UNUSED(a)) { return class_op(cls, "cwd", sp_cwd); }
static PyObject *p_home(PyObject *cls, PyObject *Py_UNUSED(a)) { return class_op(cls, "home", sp_home); }

static PyObject *p_from_uri(PyObject *cls, PyObject *uri) {
    Call c = {.self = cls, .name = "from_uri", .arg = uri};
    int flavor = class_flavor((PyTypeObject *)cls);
    const char *s = flavor < 0 ? NULL : PyUnicode_AsUTF8(uri);
    if (!s)
        return NULL;

    SpPath p = sp_from_uri(s, (SpFlavor)flavor);
    if (p.error == SP_ERR_INVALID_ARG || p.error == SP_ERR_NOT_ABSOLUTE)
        return PyErr_Format(PyExc_ValueError, "URI is not absolute: %R", uri);
    return answer(&c, &p, NULL);
}

/* An action's result: None, its error raised, or past the header's limits the call deferred */
static PyObject *done(const Call *c, SpError err, PyObject *path, PyObject *target) {
    if (past_limits(err))
        return defer(c);
    if (err != SP_OK)
        return fail(err, path, target);
    Py_RETURN_NONE;
}

static PyObject *p_mkdir(PyObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"mode", "parents", "exist_ok", "parent_mode", NULL};
    Call c = {.self = self, .name = "mkdir", .args = args, .kwargs = kwargs};
    unsigned int mode = SP_MODE_DIR;
    unsigned int parent_mode = SP_MODE_DIR;
    int parents = 0;
    int exist_ok = 0;
    SpPath sp;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|Ipp$I", kwlist, &mode, &parents, &exist_ok, &parent_mode))
        return NULL;
    if (load((PathObject *)self, &sp) < 0)
        return defer(&c);
    SpMkdirOptions options = {parents != 0, exist_ok != 0, parent_mode};
    return done(&c, sp_mkdir(&sp, mode, options), self, NULL);
}

static PyObject *p_touch(PyObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"mode", "exist_ok", NULL};
    Call c = {.self = self, .name = "touch", .args = args, .kwargs = kwargs};
    unsigned int mode = SP_MODE_FILE;
    int exist_ok = 1;
    SpPath sp;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|Ip", kwlist, &mode, &exist_ok))
        return NULL;
    if (load((PathObject *)self, &sp) < 0)
        return defer(&c);
    return done(&c, sp_touch(&sp, mode, exist_ok != 0), self, NULL);
}

static PyObject *p_unlink(PyObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"missing_ok", NULL};
    Call c = {.self = self, .name = "unlink", .args = args, .kwargs = kwargs};
    int missing_ok = 0;
    SpPath sp;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|p", kwlist, &missing_ok))
        return NULL;
    if (load((PathObject *)self, &sp) < 0)
        return defer(&c);
    SpRemoveOptions options = {false, missing_ok != 0};
    return done(&c, sp_remove(&sp, options), self, NULL);
}

static PyObject *p_rmdir(PyObject *self, PyObject *Py_UNUSED(ignored)) {
    Call c = {.self = self, .name = "rmdir", .args = no_args};
    SpRemoveOptions options = {true, false};
    SpPath sp;
    if (load((PathObject *)self, &sp) < 0)
        return defer(&c);
    return done(&c, sp_remove(&sp, options), self, NULL);
}

static PyObject *p_chmod(PyObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"mode", "follow_symlinks", NULL};
    Call c = {.self = self, .name = "chmod", .args = args, .kwargs = kwargs};
    unsigned int mode;
    int follow = 1;
    SpPath sp;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "I|$p", kwlist, &mode, &follow))
        return NULL;
    if (load((PathObject *)self, &sp) < 0)
        return defer(&c);
    return done(&c, sp_chmod(&sp, mode, follow != 0), self, NULL);
}

static PyObject *p_lchmod(PyObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"mode", NULL};
    Call c = {.self = self, .name = "lchmod", .args = args, .kwargs = kwargs};
    unsigned int mode;
    SpPath sp;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "I", kwlist, &mode))
        return NULL;
    if (load((PathObject *)self, &sp) < 0)
        return defer(&c);
    return done(&c, sp_chmod(&sp, mode, false), self, NULL);
}

/* The path and the target of a call that takes one, the target made in the path's flavor: 0, 1 past the header's
 * limits, or -1 with the error raised */
static int load_pair(PyObject *self, PyObject *target, SpPath *sp, SpPath *t) {
    int parsed = arg_path(target, ((PathObject *)self)->flavor, t);
    if (parsed != 0)
        return parsed;
    return load((PathObject *)self, sp) < 0 ? 1 : 0;
}

static PyObject *p_symlink_to(PyObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"target", "target_is_directory", NULL};
    Call c = {.self = self, .name = "symlink_to", .args = args, .kwargs = kwargs};
    PyObject *target;
    int target_is_dir = 0;
    SpPath sp;
    SpPath t;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|p", kwlist, &target, &target_is_dir))
        return NULL;
    int pair = load_pair(self, target, &sp, &t);
    if (pair != 0)
        return pair < 0 ? NULL : defer(&c);
    SpLinkOptions options = {false, target_is_dir != 0};
    return done(&c, sp_link_to(&sp, &t, options), target, self);
}

static PyObject *p_hardlink_to(PyObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"target", NULL};
    Call c = {.self = self, .name = "hardlink_to", .args = args, .kwargs = kwargs};
    PyObject *target;
    SpPath sp;
    SpPath t;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kwlist, &target))
        return NULL;
    int pair = load_pair(self, target, &sp, &t);
    if (pair != 0)
        return pair < 0 ? NULL : defer(&c);
    SpLinkOptions options = {true, false};
    return done(&c, sp_link_to(&sp, &t, options), target, self);
}

/* pathlib raises stat()'s error for a missing file, where sp_samefile is false */
static PyObject *p_samefile(PyObject *self, PyObject *other) {
    Call c = {.self = self, .name = "samefile", .arg = other};
    SpPath sp;
    SpPath t;
    int pair = load_pair(self, other, &sp, &t);
    if (pair != 0)
        return pair < 0 ? NULL : defer(&c);

    SpStatResult a = sp_stat(&sp, true);
    SpStatResult b = sp_stat(&t, true);
    if (a.error != SP_OK)
        return fail(a.error, self, NULL);
    if (b.error != SP_OK)
        return fail(b.error, other, NULL);
    return PyBool_FromLong(sp_samefile(&sp, &t));
}

/* rename and replace, move and move_into: the path moved to */
enum { RENAME, REPLACE, MOVE, MOVE_INTO };

static PyObject *move_to(PyObject *self, PyObject *args, PyObject *kwargs, const char *name, int how) {
    static char *kwlist[] = {"target", NULL};
    static char *into_kwlist[] = {"target_dir", NULL};
    Call c = {.self = self, .name = name, .args = args, .kwargs = kwargs};
    PyObject *target;
    SpPath sp;
    SpPath t;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", how == MOVE_INTO ? into_kwlist : kwlist, &target))
        return NULL;
    int pair = load_pair(self, target, &sp, &t);
    if (pair != 0)
        return pair < 0 ? NULL : defer(&c);

    SpPath r = how < MOVE ? sp_rename(&sp, &t, how == REPLACE) : sp_move(&sp, &t, how == MOVE_INTO);
    return answer(&c, &r, target);
}

static PyObject *p_rename(PyObject *s, PyObject *a, PyObject *k) { return move_to(s, a, k, "rename", RENAME); }
static PyObject *p_replace(PyObject *s, PyObject *a, PyObject *k) { return move_to(s, a, k, "replace", REPLACE); }
static PyObject *p_move(PyObject *s, PyObject *a, PyObject *k) { return move_to(s, a, k, "move", MOVE); }
static PyObject *p_move_into(PyObject *s, PyObject *a, PyObject *k) { return move_to(s, a, k, "move_into", MOVE_INTO); }

/* copy and copy_into: the copy */
static PyObject *copy_to(PyObject *self, PyObject *args, PyObject *kwargs, bool into) {
    static char *kwlist[] = {"target", "follow_symlinks", "preserve_metadata", NULL};
    static char *into_kwlist[] = {"target_dir", "follow_symlinks", "preserve_metadata", NULL};
    Call c = {.self = self, .name = into ? "copy_into" : "copy", .args = args, .kwargs = kwargs};
    PyObject *target;
    int follow = 1;
    int preserve = 0;
    SpPath sp;
    SpPath t;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|$pp", into ? into_kwlist : kwlist, &target, &follow, &preserve))
        return NULL;
    int pair = load_pair(self, target, &sp, &t);
    if (pair != 0)
        return pair < 0 ? NULL : defer(&c);

    SpCopyOptions options = {follow != 0, preserve != 0, into};
    SpPath r = sp_copy(&sp, &t, options);
    return answer(&c, &r, target);
}

static PyObject *p_copy(PyObject *s, PyObject *a, PyObject *k) { return copy_to(s, a, k, false); }
static PyObject *p_copy_into(PyObject *s, PyObject *a, PyObject *k) { return copy_to(s, a, k, true); }

/* owner and group */
static PyObject *id_name(PyObject *self, PyObject *args, PyObject *kwargs, bool group) {
    static char *kwlist[] = {"follow_symlinks", NULL};
    Call c = {.self = self, .name = group ? "group" : "owner", .args = args, .kwargs = kwargs};
    int follow = 1;
    SpPath sp;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|$p", kwlist, &follow))
        return NULL;
    if (load((PathObject *)self, &sp) < 0)
        return defer(&c);
    SpTerm t = group ? sp_group(&sp, follow != 0) : sp_owner(&sp, follow != 0);
    return t.error != SP_OK ? fail(t.error, self, NULL) : decode(t.buf, t.len);
}

static PyObject *p_owner(PyObject *s, PyObject *a, PyObject *k) { return id_name(s, a, k, false); }
static PyObject *p_group(PyObject *s, PyObject *a, PyObject *k) { return id_name(s, a, k, true); }

static PyObject *p_read_bytes(PyObject *self, PyObject *Py_UNUSED(ignored)) {
    Call c = {.self = self, .name = "read_bytes", .args = no_args};
    SpPath sp;
    if (load((PathObject *)self, &sp) < 0)
        return defer(&c);
    SpStatResult st = sp_stat(&sp, true);
    size_t size = st.error == SP_OK && st.sp_size > 0 ? (size_t)st.sp_size : 4096;

    /* Read again into a bigger buffer while the file is bigger than stat() said (it grew, or it is in /proc) */
    for (;;) {
        PyObject *data = PyBytes_FromStringAndSize(NULL, (Py_ssize_t)size);
        if (!data)
            return NULL;
        SpIOResult r = sp_read_file(&sp, PyBytes_AS_STRING(data), size);
        if (r.error == SP_OK)
            return _PyBytes_Resize(&data, (Py_ssize_t)r.bytes) < 0 ? NULL : data;
        Py_DECREF(data);
        if (r.error != SP_ERR_TOO_LONG)
            return fail(r.error, self, NULL);
        size = r.bytes > size ? r.bytes : size * 2;
    }
}

static PyObject *p_write_bytes(PyObject *self, PyObject *data) {
    Call c = {.self = self, .name = "write_bytes", .arg = data};
    Py_buffer view;
    SpPath sp;
    if (load((PathObject *)self, &sp) < 0)
        return defer(&c);
    if (PyObject_GetBuffer(data, &view, PyBUF_SIMPLE) < 0)
        return NULL;

    SpIOResult w = sp_write_file(&sp, (const char *)view.buf, (size_t)view.len);
    PyBuffer_Release(&view);
    return w.error != SP_OK ? fail(w.error, self, NULL) : PyLong_FromSize_t(w.bytes);
}

/* iterdir's entries and glob's matches are listed in C, then iterated (pathlib's are iterators too) */
static PyObject *iterate(PyObject *list) {
    PyObject *it = list ? PyObject_GetIter(list) : NULL;
    Py_XDECREF(list);
    return it;
}

static PyObject *p_iterdir(PyObject *self, PyObject *Py_UNUSED(ignored)) {
    Call c = {.self = self, .name = "iterdir", .args = no_args};
    SpPath sp;
    SpPath entry;
    if (load((PathObject *)self, &sp) < 0)
        return defer(&c);
    SpIterdirIter it = sp_iterdir_begin(&sp);
    if (it.error != SP_OK)
        return fail(it.error, self, NULL);

    PyObject *list = PyList_New(0);
    while (list && sp_iterdir_next(&it, &entry)) {
        PyObject *child = make(Py_TYPE(self), self, &entry, NULL);
        if (!child || PyList_Append(list, child) < 0)
            Py_CLEAR(list);
        Py_XDECREF(child);
    }
    sp_iterdir_end(&it);
    if (list && it.error != SP_OK) {
        Py_CLEAR(list);
        return past_limits(it.error) ? defer(&c) : fail(it.error, self, NULL);
    }
    return iterate(list);
}

/* glob and rglob, on the pattern's text as given */
static PyObject *glob(PyObject *self, PyObject *args, PyObject *kwargs, bool recursive) {
    static char *kwlist[] = {"pattern", "case_sensitive", "recurse_symlinks", NULL};
    Call c = {.self = self, .name = recursive ? "rglob" : "glob", .args = args, .kwargs = kwargs};
    PyObject *pattern;
    PyObject *case_sensitive = Py_None;
    int recurse_symlinks = 0;
    PyObject *owner;
    Py_ssize_t len;
    SpPath sp;
    SpPath match;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|$Op", kwlist, &pattern, &case_sensitive, &recurse_symlinks))
        return NULL;
    if (load((PathObject *)self, &sp) < 0)
        return defer(&c);
    const char *text = fspath_text(pattern, &len, &owner);
    SpGlobIter *it = text ? PyMem_Malloc(sizeof(SpGlobIter)) : NULL; /* about 40 KB */
    if (!it) {
        Py_XDECREF(owner);
        return text ? PyErr_NoMemory() : NULL;
    }

    SpGlobOptions options = {recursive, recurse_symlinks != 0, case_of(case_sensitive)};
    *it = sp_glob_begin(&sp, text, options);
    Py_DECREF(owner);
    PyObject *list = NULL;
    if (it->error == SP_ERR_UNSUPPORTED)
        PyErr_SetString(PyExc_NotImplementedError, "Non-relative patterns are unsupported");
    else if (it->error == SP_ERR_INVALID_ARG)
        PyErr_Format(PyExc_ValueError, "Unacceptable pattern: %R", pattern);
    else
        list = PyList_New(0);
    while (list && sp_glob_next(it, &match)) {
        PyObject *path = make(Py_TYPE(self), self, &match, NULL);
        if (!path || PyList_Append(list, path) < 0)
            Py_CLEAR(list);
        Py_XDECREF(path);
    }
    sp_glob_end(it);

    SpError err = list ? it->error : SP_OK;
    PyMem_Free(it);
    if (err == SP_OK)
        return iterate(list);
    Py_DECREF(list);
    return past_limits(err) ? defer(&c) : fail(err, self, NULL);
}

static PyObject *p_glob(PyObject *s, PyObject *a, PyObject *k) { return glob(s, a, k, false); }
static PyObject *p_rglob(PyObject *s, PyObject *a, PyObject *k) { return glob(s, a, k, true); }

/* ============ walk ============ */

/* os.walk's iterator over the header's walk, the names kept in buf. Top-down, the dirnames of the entry given last
 * (as the caller left them) are handed back at the next step. When the names outgrow buf before the first entry, the
 * walk is pathlib's. */
typedef struct {
    PyObject_HEAD PyObject *top;
    PyObject *args;
    PyObject *kwargs;
    PyObject *on_error;
    PyObject *dirnames; /* the last entry's, top-down */
    PyObject *keep;     /* what the dirnames handed back point into, alive until the walk ends */
    PyObject *deferred; /* pathlib's walk */
    bool started;
    char *buf;
    SpWalkIter it;
} WalkObject;

static void walk_dealloc(WalkObject *w) {
    Py_XDECREF(w->top);
    Py_XDECREF(w->args);
    Py_XDECREF(w->kwargs);
    Py_XDECREF(w->on_error);
    Py_XDECREF(w->dirnames);
    Py_XDECREF(w->keep);
    Py_XDECREF(w->deferred);
    PyMem_RawFree(w->buf);
    PyObject_Free(w);
}

/* The dirnames the caller left (a list of str) as the walk's next subdirectories */
static int walk_give_back(WalkObject *w, PyObject *names) {
    Py_ssize_t n = PyList_Check(names) ? PyList_GET_SIZE(names) : 0;
    PyObject *array = PyBytes_FromStringAndSize(NULL, (n + 1) * (Py_ssize_t)sizeof(char *));
    int ok = array && PyList_Append(w->keep, array) == 0;
    for (Py_ssize_t i = 0; ok && i < n; i++) {
        PyObject *name = PyList_GET_ITEM(names, i);
        PyObject *owner = NULL;
        Py_ssize_t len;
        const char *s = PyUnicode_Check(name) ? utf8(name, &len, &owner) : NULL;
        if (!s && !PyErr_Occurred())
            PyErr_Format(PyExc_TypeError, "dirnames must be str, not %s", Py_TYPE(name)->tp_name);
        PyObject *copy = s ? PyBytes_FromStringAndSize(s, len) : NULL;
        ok = copy && PyList_Append(w->keep, copy) == 0;
        if (ok)
            ((char **)PyBytes_AS_STRING(array))[i] = PyBytes_AS_STRING(copy);
        Py_XDECREF(copy);
        Py_XDECREF(owner);
    }

    if (ok) {
        w->it.entry.dirnames = (char **)PyBytes_AS_STRING(array);
        w->it.entry.dirname_count = (size_t)n;
    }
    Py_XDECREF(array);
    return ok ? 0 : -1;
}

static PyObject *names_list(char **names, size_t count) {
    PyObject *list = PyList_New((Py_ssize_t)count);
    for (size_t i = 0; list && i < count; i++) {
        PyObject *s = decode(names[i], strlen(names[i]));
        if (s)
            PyList_SET_ITEM(list, (Py_ssize_t)i, s);
        else
            Py_CLEAR(list);
    }
    return list;
}

/* on_error(the OSError for the directory that couldn't be listed) */
static int walk_error(WalkObject *w, const SpWalkEntry *e) {
    if (w->on_error == Py_None)
        return 0;
    PyObject *dirpath = make(Py_TYPE(w->top), w->top, &e->dirpath, NULL);
    PyObject *error = dirpath ? error_of(e->error, dirpath, NULL) : NULL;
    PyObject *r = error ? PyObject_CallOneArg(w->on_error, error) : NULL;
    Py_XDECREF(dirpath);
    Py_XDECREF(error);
    Py_XDECREF(r);
    return r ? 0 : -1;
}

static PyObject *walk_next(WalkObject *w) {
    if (w->deferred)
        return PyIter_Next(w->deferred);
    if (w->dirnames) {
        int given = walk_give_back(w, w->dirnames);
        Py_CLEAR(w->dirnames);
        if (given < 0)
            return NULL;
    }

    SpWalkEntry *e;
    while ((e = sp_walk_next(&w->it)) && e->error != SP_OK)
        if (walk_error(w, e) < 0)
            return NULL;
    if (!e && past_limits(w->it.error) && !w->started) {
        Call c = {.self = w->top, .name = "walk", .args = w->args, .kwargs = w->kwargs};
        w->deferred = defer(&c);
        return w->deferred ? PyIter_Next(w->deferred) : NULL;
    }
    if (!e)
        return w->it.error != SP_OK ? fail(w->it.error, w->top, NULL) : NULL;

    PyObject *dirpath = make(Py_TYPE(w->top), w->top, &e->dirpath, NULL);
    PyObject *dirnames = names_list(e->dirnames, e->dirname_count);
    PyObject *filenames = names_list(e->filenames, e->filename_count);
    PyObject *r = dirpath && dirnames && filenames ? PyTuple_Pack(3, dirpath, dirnames, filenames) : NULL;
    if (r && !w->it.priv_.options.bottom_up)
        w->dirnames = Py_NewRef(dirnames);
    w->started = true;
    Py_XDECREF(dirpath);
    Py_XDECREF(dirnames);
    Py_XDECREF(filenames);
    return r;
}

static PyTypeObject WalkType = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "_snakepath.Walk",
    .tp_basicsize = sizeof(WalkObject),
    .tp_dealloc = (destructor)walk_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_iternext = (iternextfunc)walk_next,
};

static PyObject *p_walk(PyObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"top_down", "on_error", "follow_symlinks", NULL};
    Call c = {.self = self, .name = "walk", .args = args, .kwargs = kwargs};
    PyObject *on_error = Py_None;
    int top_down = 1;
    int follow = 0;
    SpPath sp;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|pOp", kwlist, &top_down, &on_error, &follow))
        return NULL;
    if (load((PathObject *)self, &sp) < 0)
        return defer(&c);

    WalkObject *w = PyObject_New(WalkObject, &WalkType);
    if (!w)
        return NULL;
    w->top = Py_NewRef(self);
    w->args = Py_NewRef(args);
    w->kwargs = Py_XNewRef(kwargs);
    w->on_error = Py_NewRef(on_error);
    w->dirnames = NULL;
    w->keep = PyList_New(0);
    w->deferred = NULL;
    w->started = false;
    w->buf = PyMem_RawMalloc(WALK_BUF_SIZE);
    if (!w->keep || !w->buf) {
        Py_DECREF(w);
        return PyErr_NoMemory();
    }
    SpWalkOptions options = {top_down == 0, follow != 0};
    w->it = sp_walk_begin(&sp, options, w->buf, WALK_BUF_SIZE);
    return (PyObject *)w;
}

static PyMethodDef path_methods[] = {
    {"stat", KW p_stat, METH_VARARGS | METH_KEYWORDS, NULL},
    {"lstat", (PyCFunction)p_lstat, METH_NOARGS, NULL},
    {"exists", KW p_exists, METH_VARARGS | METH_KEYWORDS, NULL},
    {"is_dir", KW p_is_dir, METH_VARARGS | METH_KEYWORDS, NULL},
    {"is_file", KW p_is_file, METH_VARARGS | METH_KEYWORDS, NULL},
    {"is_symlink", (PyCFunction)p_is_symlink, METH_NOARGS, NULL},
    {"is_junction", (PyCFunction)p_is_junction, METH_NOARGS, NULL},
    {"is_mount", (PyCFunction)p_is_mount, METH_NOARGS, NULL},
    {"is_fifo", (PyCFunction)p_is_fifo, METH_NOARGS, NULL},
    {"is_socket", (PyCFunction)p_is_socket, METH_NOARGS, NULL},
    {"is_block_device", (PyCFunction)p_is_block_device, METH_NOARGS, NULL},
    {"is_char_device", (PyCFunction)p_is_char_device, METH_NOARGS, NULL},
    {"absolute", (PyCFunction)p_absolute, METH_NOARGS, NULL},
    {"expanduser", (PyCFunction)p_expanduser, METH_NOARGS, NULL},
    {"readlink", (PyCFunction)p_readlink, METH_NOARGS, NULL},
    {"resolve", KW p_resolve, METH_VARARGS | METH_KEYWORDS, NULL},
    {"cwd", (PyCFunction)p_cwd, METH_NOARGS | METH_CLASS, NULL},
    {"home", (PyCFunction)p_home, METH_NOARGS | METH_CLASS, NULL},
    {"from_uri", (PyCFunction)p_from_uri, METH_O | METH_CLASS, NULL},
    {"mkdir", KW p_mkdir, METH_VARARGS | METH_KEYWORDS, NULL},
    {"touch", KW p_touch, METH_VARARGS | METH_KEYWORDS, NULL},
    {"unlink", KW p_unlink, METH_VARARGS | METH_KEYWORDS, NULL},
    {"rmdir", (PyCFunction)p_rmdir, METH_NOARGS, NULL},
    {"chmod", KW p_chmod, METH_VARARGS | METH_KEYWORDS, NULL},
    {"lchmod", KW p_lchmod, METH_VARARGS | METH_KEYWORDS, NULL},
    {"symlink_to", KW p_symlink_to, METH_VARARGS | METH_KEYWORDS, NULL},
    {"hardlink_to", KW p_hardlink_to, METH_VARARGS | METH_KEYWORDS, NULL},
    {"samefile", (PyCFunction)p_samefile, METH_O, NULL},
    {"rename", KW p_rename, METH_VARARGS | METH_KEYWORDS, NULL},
    {"replace", KW p_replace, METH_VARARGS | METH_KEYWORDS, NULL},
    {"copy", KW p_copy, METH_VARARGS | METH_KEYWORDS, NULL},
    {"copy_into", KW p_copy_into, METH_VARARGS | METH_KEYWORDS, NULL},
    {"move", KW p_move, METH_VARARGS | METH_KEYWORDS, NULL},
    {"move_into", KW p_move_into, METH_VARARGS | METH_KEYWORDS, NULL},
    {"owner", KW p_owner, METH_VARARGS | METH_KEYWORDS, NULL},
    {"group", KW p_group, METH_VARARGS | METH_KEYWORDS, NULL},
    {"read_bytes", (PyCFunction)p_read_bytes, METH_NOARGS, NULL},
    {"write_bytes", (PyCFunction)p_write_bytes, METH_O, NULL},
    {"iterdir", (PyCFunction)p_iterdir, METH_NOARGS, NULL},
    {"glob", KW p_glob, METH_VARARGS | METH_KEYWORDS, NULL},
    {"rglob", KW p_rglob, METH_VARARGS | METH_KEYWORDS, NULL},
    {"walk", KW p_walk, METH_VARARGS | METH_KEYWORDS, NULL},
    {0},
};

static PyTypeObject PathType = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "_snakepath.PathBase",
    .tp_basicsize = sizeof(PathObject),
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_methods = path_methods,
    .tp_base = &PurePathType,
};

/* ============ Module ============ */

static struct PyModuleDef module_def = {PyModuleDef_HEAD_INIT, .m_name = "_snakepath", .m_size = -1};

PyMODINIT_FUNC PyInit__snakepath(void) {
    PyObject *os = PyImport_ImportModule("os");
    stat_result = os ? PyObject_GetAttrString(os, "stat_result") : NULL;
    Py_XDECREF(os);
    no_args = PyTuple_New(0);
    str_flavor = PyUnicode_InternFromString("_flavor");
    str_native = PyUnicode_InternFromString("_native");
    str_with_segments = PyUnicode_InternFromString("with_segments");
    str_empty = PyUnicode_FromString("");
    UnsupportedOperation = PyErr_NewExceptionWithDoc("snakepath.UnsupportedOperation",
                                                     "An operation the path doesn't support "
                                                     "(pathlib.UnsupportedOperation)",
                                                     PyExc_NotImplementedError, NULL);
    WalkType.tp_iter = PyObject_SelfIter; /* a DLL's function: MSVC takes its address at run time */
    if (!stat_result || find_stat_fields() < 0 || !no_args || !str_flavor || !str_native || !str_with_segments ||
        !str_empty || !UnsupportedOperation || PyType_Ready(&PurePathType) < 0 || PyType_Ready(&PathType) < 0 ||
        PyType_Ready(&WalkType) < 0)
        return NULL;

    module = PyModule_Create(&module_def);
    if (!module || PyModule_AddObjectRef(module, "UnsupportedOperation", UnsupportedOperation) < 0 ||
        PyModule_AddObjectRef(module, "PurePathBase", (PyObject *)&PurePathType) < 0 ||
        PyModule_AddObjectRef(module, "PathBase", (PyObject *)&PathType) < 0 ||
        PyModule_AddIntConstant(module, "POSIX", SP_FLAVOR_POSIX) < 0 ||
        PyModule_AddIntConstant(module, "WINDOWS", SP_FLAVOR_WINDOWS) < 0) {
        Py_CLEAR(module);
        return NULL;
    }
    return Py_NewRef(module);
}
