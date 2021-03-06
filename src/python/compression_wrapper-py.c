/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2013  Tomas Mlcoch
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#include <Python.h>
#include <assert.h>
#include <stddef.h>

#include "compression_wrapper-py.h"
#include "exception-py.h"
#include "contentstat-py.h"
#include "typeconversion.h"

/*
 * Module functions
 */

PyObject *
py_compression_suffix(PyObject *self, PyObject *args)
{
    int type;

    CR_UNUSED(self);

    if (!PyArg_ParseTuple(args, "i:py_compression_suffix", &type))
        return NULL;

    return PyStringOrNone_FromString(cr_compression_suffix(type));
}

PyObject *
py_detect_compression(PyObject *self, PyObject *args)
{
    long type;
    char *filename;
    GError *tmp_err = NULL;

    CR_UNUSED(self);

    if (!PyArg_ParseTuple(args, "s:py_detect_compression", &filename))
        return NULL;

    type = cr_detect_compression(filename, &tmp_err);
    if (tmp_err) {
        nice_exception(&tmp_err, NULL);
        return NULL;
    }

    return PyLong_FromLong(type);
}

/*
 * CrFile object
 */

typedef struct {
    PyObject_HEAD
    CR_FILE *f;
    PyObject *py_stat;
} _CrFileObject;

static PyObject * py_close(_CrFileObject *self, void *nothing);

static int
check_CrFileStatus(const _CrFileObject *self)
{
    assert(self != NULL);
    assert(CrFileObject_Check(self));
    if (self->f == NULL) {
        PyErr_SetString(CrErr_Exception,
            "Improper createrepo_c CrFile object (Already closed file?).");
        return -1;
    }
    return 0;
}

/* Function on the type */

static PyObject *
crfile_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    CR_UNUSED(args);
    CR_UNUSED(kwds);
    _CrFileObject *self = (_CrFileObject *)type->tp_alloc(type, 0);
    if (self) {
        self->f = NULL;
        self->py_stat = NULL;
    }
    return (PyObject *)self;
}

static int
crfile_init(_CrFileObject *self, PyObject *args, PyObject *kwds)
{
    char *path;
    int mode, comtype;
    GError *err = NULL;
    PyObject *py_stat, *ret;
    cr_ContentStat *stat;

    CR_UNUSED(kwds);

    if (!PyArg_ParseTuple(args, "siiO|:crfile_init",
                          &path, &mode, &comtype, &py_stat))
        return -1;

    /* Check arguments */

    if (mode != CR_CW_MODE_READ && mode != CR_CW_MODE_WRITE) {
        PyErr_SetString(PyExc_ValueError, "Bad open mode");
        return -1;
    }

    if (comtype < 0 || comtype >= CR_CW_COMPRESSION_SENTINEL) {
        PyErr_SetString(PyExc_ValueError, "Unknown compression type");
        return -1;
    }

    if (py_stat == Py_None) {
        stat = NULL;
    } else if (ContentStatObject_Check(py_stat)) {
        stat = ContentStat_FromPyObject(py_stat);
    } else {
        PyErr_SetString(PyExc_TypeError, "Use ContentStat or None");
        return -1;
    }

    /* Free all previous resources when reinitialization */
    ret = py_close(self, NULL);
    Py_XDECREF(ret);
    Py_XDECREF(self->py_stat);
    self->py_stat = NULL;
    if (ret == NULL) {
        // Error encountered!
        return -1;
    }

    /* Init */
    self->f = cr_sopen(path, mode, comtype, stat, &err);
    if (err) {
        nice_exception(&err, "CrFile %s init failed: ", path);
        return -1;
    }

    self->py_stat = py_stat;
    Py_XINCREF(py_stat);

    return 0;
}

static void
crfile_dealloc(_CrFileObject *self)
{
    cr_close(self->f, NULL);
    Py_XDECREF(self->py_stat);
    Py_TYPE(self)->tp_free(self);
}

static PyObject *
crfile_repr(_CrFileObject *self)
{
    char *mode;

    switch (self->f->mode) {
        case CR_CW_MODE_READ:
            mode = "Read mode";
            break;
        case CR_CW_MODE_WRITE:
            mode = "Write mode";
            break;
        default:
            mode = "Unknown mode";
    }

    return PyString_FromFormat("<createrepo_c.CrFile %s object>", mode);
}

/* CrFile methods */

PyDoc_STRVAR(write__doc__,
"write() -> None\n\n"
"Write a data to the file");

static PyObject *
py_write(_CrFileObject *self, PyObject *args)
{
    char *str;
    int len;
    GError *tmp_err = NULL;

    if (!PyArg_ParseTuple(args, "s#:set_num_of_pkgs", &str, &len))
        return NULL;

    if (check_CrFileStatus(self))
        return NULL;

    cr_write(self->f, str, len, &tmp_err);
    if (tmp_err) {
        nice_exception(&tmp_err, NULL);
        return NULL;
    }

    Py_RETURN_NONE;
}

PyDoc_STRVAR(close__doc__,
"close() -> None\n\n"
"Close the file");

static PyObject *
py_close(_CrFileObject *self, void *nothing)
{
    GError *tmp_err = NULL;

    CR_UNUSED(nothing);

    if (self->f) {
        cr_close(self->f, &tmp_err);
        self->f = NULL;
    }

    Py_XDECREF(self->py_stat);
    self->py_stat = NULL;

    if (tmp_err) {
        nice_exception(&tmp_err, "Close error: ");
        return NULL;
    }

    Py_RETURN_NONE;
}

static struct PyMethodDef crfile_methods[] = {
    {"write", (PyCFunction)py_write, METH_VARARGS, write__doc__},
    {"close", (PyCFunction)py_close, METH_NOARGS, close__doc__},
    {NULL} /* sentinel */
};

PyTypeObject CrFile_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                              /* ob_size */
    "createrepo_c.CrFile",          /* tp_name */
    sizeof(_CrFileObject),          /* tp_basicsize */
    0,                              /* tp_itemsize */
    (destructor) crfile_dealloc,    /* tp_dealloc */
    0,                              /* tp_print */
    0,                              /* tp_getattr */
    0,                              /* tp_setattr */
    0,                              /* tp_compare */
    (reprfunc) crfile_repr,         /* tp_repr */
    0,                              /* tp_as_number */
    0,                              /* tp_as_sequence */
    0,                              /* tp_as_mapping */
    0,                              /* tp_hash */
    0,                              /* tp_call */
    0,                              /* tp_str */
    0,                              /* tp_getattro */
    0,                              /* tp_setattro */
    0,                              /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE, /* tp_flags */
    "CrFile object",                /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    PyObject_SelfIter,              /* tp_iter */
    0,                              /* tp_iternext */
    crfile_methods,                 /* tp_methods */
    0,                              /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    (initproc) crfile_init,         /* tp_init */
    0,                              /* tp_alloc */
    crfile_new,                     /* tp_new */
    0,                              /* tp_free */
    0,                              /* tp_is_gc */
};
