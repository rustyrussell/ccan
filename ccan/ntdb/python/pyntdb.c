/*
   Unix SMB/CIFS implementation.

   Python interface to ntdb.  Simply modified from tdb version.

   Copyright (C) 2004-2006 Tim Potter <tpot@samba.org>
   Copyright (C) 2007-2008 Jelmer Vernooij <jelmer@samba.org>
   Copyright (C) 2011 Rusty Russell <rusty@rustcorp.com.au>

     ** NOTE! The following LGPL license applies to the ntdb
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include <Python.h>
#include "replace.h"
#include "system/filesys.h"

/* Include ntdb headers */
#include <ntdb.h>

typedef struct {
	PyObject_HEAD
	struct ntdb_context *ctx;
	bool closed;
} PyNtdbObject;

static PyTypeObject PyNtdb;

static void PyErr_SetTDBError(enum NTDB_ERROR e)
{
	PyErr_SetObject(PyExc_RuntimeError,
		Py_BuildValue("(i,s)", e, ntdb_errorstr(e)));
}

static NTDB_DATA PyString_AsNtdb_Data(PyObject *data)
{
	NTDB_DATA ret;
	ret.dptr = (unsigned char *)PyString_AsString(data);
	ret.dsize = PyString_Size(data);
	return ret;
}

static PyObject *PyString_FromNtdb_Data(NTDB_DATA data)
{
	PyObject *ret = PyString_FromStringAndSize((const char *)data.dptr,
						   data.dsize);
	free(data.dptr);
	return ret;
}

#define PyErr_NTDB_ERROR_IS_ERR_RAISE(ret) \
	if (ret != NTDB_SUCCESS) { \
		PyErr_SetTDBError(ret); \
		return NULL; \
	}

#define PyNtdb_CHECK_CLOSED(pyobj) \
	if (pyobj->closed) {\
		PyErr_SetObject(PyExc_RuntimeError, \
			Py_BuildValue("(i,s)", NTDB_ERR_EINVAL, "database is closed")); \
		return NULL; \
	}

static void stderr_log(struct ntdb_context *ntdb,
		       enum ntdb_log_level level,
		       enum NTDB_ERROR ecode,
		       const char *message,
		       void *data)
{
	fprintf(stderr, "%s:%s:%s\n",
		ntdb_name(ntdb), ntdb_errorstr(ecode), message);
}

static PyObject *py_ntdb_open(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
	char *name = NULL;
	int ntdb_flags = NTDB_DEFAULT, flags = O_RDWR, mode = 0600;
	struct ntdb_context *ctx;
	PyNtdbObject *ret;
	union ntdb_attribute logattr;
	const char *kwnames[] = { "name", "ntdb_flags", "flags", "mode", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|siii", cast_const2(char **, kwnames), &name, &ntdb_flags, &flags, &mode))
		return NULL;

	if (name == NULL) {
		ntdb_flags |= NTDB_INTERNAL;
		name = "<internal>";
	}

	logattr.log.base.attr = NTDB_ATTRIBUTE_LOG;
	logattr.log.base.next = NULL;
	logattr.log.fn = stderr_log;
	ctx = ntdb_open(name, ntdb_flags, flags, mode, &logattr);
	if (ctx == NULL) {
		PyErr_SetFromErrno(PyExc_IOError);
		return NULL;
	}

	ret = PyObject_New(PyNtdbObject, &PyNtdb);
	if (!ret) {
		ntdb_close(ctx);
		return NULL;
	}

	ret->ctx = ctx;
	ret->closed = false;
	return (PyObject *)ret;
}

static PyObject *obj_transaction_cancel(PyNtdbObject *self)
{
	PyNtdb_CHECK_CLOSED(self);
	ntdb_transaction_cancel(self->ctx);
	Py_RETURN_NONE;
}

static PyObject *obj_transaction_commit(PyNtdbObject *self)
{
	enum NTDB_ERROR ret;
	PyNtdb_CHECK_CLOSED(self);
	ret = ntdb_transaction_commit(self->ctx);
	PyErr_NTDB_ERROR_IS_ERR_RAISE(ret);
	Py_RETURN_NONE;
}

static PyObject *obj_transaction_prepare_commit(PyNtdbObject *self)
{
	enum NTDB_ERROR ret;
	PyNtdb_CHECK_CLOSED(self);
	ret = ntdb_transaction_prepare_commit(self->ctx);
	PyErr_NTDB_ERROR_IS_ERR_RAISE(ret);
	Py_RETURN_NONE;
}

static PyObject *obj_transaction_start(PyNtdbObject *self)
{
	enum NTDB_ERROR ret;
	PyNtdb_CHECK_CLOSED(self);
	ret = ntdb_transaction_start(self->ctx);
	PyErr_NTDB_ERROR_IS_ERR_RAISE(ret);
	Py_RETURN_NONE;
}

static PyObject *obj_lockall(PyNtdbObject *self)
{
	enum NTDB_ERROR ret;
	PyNtdb_CHECK_CLOSED(self);
	ret = ntdb_lockall(self->ctx);
	PyErr_NTDB_ERROR_IS_ERR_RAISE(ret);
	Py_RETURN_NONE;
}

static PyObject *obj_unlockall(PyNtdbObject *self)
{
	PyNtdb_CHECK_CLOSED(self);
	ntdb_unlockall(self->ctx);
	Py_RETURN_NONE;
}

static PyObject *obj_lockall_read(PyNtdbObject *self)
{
	enum NTDB_ERROR ret;
	PyNtdb_CHECK_CLOSED(self);
	ret = ntdb_lockall_read(self->ctx);
	PyErr_NTDB_ERROR_IS_ERR_RAISE(ret);
	Py_RETURN_NONE;
}

static PyObject *obj_unlockall_read(PyNtdbObject *self)
{
	PyNtdb_CHECK_CLOSED(self);
	ntdb_unlockall_read(self->ctx);
	Py_RETURN_NONE;
}

static PyObject *obj_close(PyNtdbObject *self)
{
	int ret;
	if (self->closed)
		Py_RETURN_NONE;
	ret = ntdb_close(self->ctx);
	self->closed = true;
	if (ret != 0) {
		PyErr_SetTDBError(NTDB_ERR_IO);
		return NULL;
	}
	Py_RETURN_NONE;
}

static PyObject *obj_get(PyNtdbObject *self, PyObject *args)
{
	NTDB_DATA key, data;
	PyObject *py_key;
	enum NTDB_ERROR ret;

	PyNtdb_CHECK_CLOSED(self);

	if (!PyArg_ParseTuple(args, "O", &py_key))
		return NULL;

	key = PyString_AsNtdb_Data(py_key);
	ret = ntdb_fetch(self->ctx, key, &data);
	if (ret == NTDB_ERR_NOEXIST)
		Py_RETURN_NONE;
	PyErr_NTDB_ERROR_IS_ERR_RAISE(ret);
	return PyString_FromNtdb_Data(data);
}

static PyObject *obj_append(PyNtdbObject *self, PyObject *args)
{
	NTDB_DATA key, data;
	PyObject *py_key, *py_data;
	enum NTDB_ERROR ret;

	PyNtdb_CHECK_CLOSED(self);

	if (!PyArg_ParseTuple(args, "OO", &py_key, &py_data))
		return NULL;

	key = PyString_AsNtdb_Data(py_key);
	data = PyString_AsNtdb_Data(py_data);

	ret = ntdb_append(self->ctx, key, data);
	PyErr_NTDB_ERROR_IS_ERR_RAISE(ret);
	Py_RETURN_NONE;
}

static PyObject *obj_firstkey(PyNtdbObject *self)
{
	enum NTDB_ERROR ret;
	NTDB_DATA key;

	PyNtdb_CHECK_CLOSED(self);

	ret = ntdb_firstkey(self->ctx, &key);
	if (ret == NTDB_ERR_NOEXIST)
		Py_RETURN_NONE;
	PyErr_NTDB_ERROR_IS_ERR_RAISE(ret);

	return PyString_FromNtdb_Data(key);
}

static PyObject *obj_nextkey(PyNtdbObject *self, PyObject *args)
{
	NTDB_DATA key;
	PyObject *py_key;
	enum NTDB_ERROR ret;

	PyNtdb_CHECK_CLOSED(self);

	if (!PyArg_ParseTuple(args, "O", &py_key))
		return NULL;

	/* Malloc here, since ntdb_nextkey frees. */
	key.dsize = PyString_Size(py_key);
	key.dptr = malloc(key.dsize);
	memcpy(key.dptr, PyString_AsString(py_key), key.dsize);

	ret = ntdb_nextkey(self->ctx, &key);
	if (ret == NTDB_ERR_NOEXIST)
		Py_RETURN_NONE;
	PyErr_NTDB_ERROR_IS_ERR_RAISE(ret);

	return PyString_FromNtdb_Data(key);
}

static PyObject *obj_delete(PyNtdbObject *self, PyObject *args)
{
	NTDB_DATA key;
	PyObject *py_key;
	enum NTDB_ERROR ret;

	PyNtdb_CHECK_CLOSED(self);

	if (!PyArg_ParseTuple(args, "O", &py_key))
		return NULL;

	key = PyString_AsNtdb_Data(py_key);
	ret = ntdb_delete(self->ctx, key);
	PyErr_NTDB_ERROR_IS_ERR_RAISE(ret);
	Py_RETURN_NONE;
}

static PyObject *obj_has_key(PyNtdbObject *self, PyObject *args)
{
	NTDB_DATA key;
	PyObject *py_key;

	PyNtdb_CHECK_CLOSED(self);

	if (!PyArg_ParseTuple(args, "O", &py_key))
		return NULL;

	key = PyString_AsNtdb_Data(py_key);
	if (ntdb_exists(self->ctx, key))
		return Py_True;
	return Py_False;
}

static PyObject *obj_store(PyNtdbObject *self, PyObject *args)
{
	NTDB_DATA key, value;
	enum NTDB_ERROR ret;
	int flag = NTDB_REPLACE;
	PyObject *py_key, *py_value;
	PyNtdb_CHECK_CLOSED(self);

	if (!PyArg_ParseTuple(args, "OO|i", &py_key, &py_value, &flag))
		return NULL;

	key = PyString_AsNtdb_Data(py_key);
	value = PyString_AsNtdb_Data(py_value);

	ret = ntdb_store(self->ctx, key, value, flag);
	PyErr_NTDB_ERROR_IS_ERR_RAISE(ret);
	Py_RETURN_NONE;
}

static PyObject *obj_add_flag(PyNtdbObject *self, PyObject *args)
{
	unsigned flag;
	PyNtdb_CHECK_CLOSED(self);

	if (!PyArg_ParseTuple(args, "I", &flag))
		return NULL;

	ntdb_add_flag(self->ctx, flag);
	Py_RETURN_NONE;
}

static PyObject *obj_remove_flag(PyNtdbObject *self, PyObject *args)
{
	unsigned flag;

	PyNtdb_CHECK_CLOSED(self);

	if (!PyArg_ParseTuple(args, "I", &flag))
		return NULL;

	ntdb_remove_flag(self->ctx, flag);
	Py_RETURN_NONE;
}

typedef struct {
	PyObject_HEAD
	NTDB_DATA current;
	bool end;
	PyNtdbObject *iteratee;
} PyNtdbIteratorObject;

static PyObject *ntdb_iter_next(PyNtdbIteratorObject *self)
{
	enum NTDB_ERROR e;
	PyObject *ret;
	if (self->end)
		return NULL;
	ret = PyString_FromStringAndSize((const char *)self->current.dptr,
					 self->current.dsize);
	e = ntdb_nextkey(self->iteratee->ctx, &self->current);
	if (e == NTDB_ERR_NOEXIST)
		self->end = true;
	else
		PyErr_NTDB_ERROR_IS_ERR_RAISE(e);
	return ret;
}

static void ntdb_iter_dealloc(PyNtdbIteratorObject *self)
{
	Py_DECREF(self->iteratee);
	PyObject_Del(self);
}

PyTypeObject PyNtdbIterator = {
	.tp_name = "Iterator",
	.tp_basicsize = sizeof(PyNtdbIteratorObject),
	.tp_iternext = (iternextfunc)ntdb_iter_next,
	.tp_dealloc = (destructor)ntdb_iter_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_iter = PyObject_SelfIter,
};

static PyObject *ntdb_object_iter(PyNtdbObject *self)
{
	PyNtdbIteratorObject *ret;
	enum NTDB_ERROR e;
	PyNtdb_CHECK_CLOSED(self);

	ret = PyObject_New(PyNtdbIteratorObject, &PyNtdbIterator);
	if (!ret)
		return NULL;
	e = ntdb_firstkey(self->ctx, &ret->current);
	if (e == NTDB_ERR_NOEXIST) {
		ret->end = true;
	} else {
		PyErr_NTDB_ERROR_IS_ERR_RAISE(e);
		ret->end = false;
	}
	ret->iteratee = self;
	Py_INCREF(self);
	return (PyObject *)ret;
}

static PyObject *obj_clear(PyNtdbObject *self)
{
	enum NTDB_ERROR ret;
	PyNtdb_CHECK_CLOSED(self);
	ret = ntdb_wipe_all(self->ctx);
	PyErr_NTDB_ERROR_IS_ERR_RAISE(ret);
	Py_RETURN_NONE;
}

static PyObject *obj_enable_seqnum(PyNtdbObject *self)
{
	PyNtdb_CHECK_CLOSED(self);
	ntdb_add_flag(self->ctx, NTDB_SEQNUM);
	Py_RETURN_NONE;
}

static PyMethodDef ntdb_object_methods[] = {
	{ "transaction_cancel", (PyCFunction)obj_transaction_cancel, METH_NOARGS,
		"S.transaction_cancel() -> None\n"
		"Cancel the currently active transaction." },
	{ "transaction_commit", (PyCFunction)obj_transaction_commit, METH_NOARGS,
		"S.transaction_commit() -> None\n"
		"Commit the currently active transaction." },
	{ "transaction_prepare_commit", (PyCFunction)obj_transaction_prepare_commit, METH_NOARGS,
		"S.transaction_prepare_commit() -> None\n"
		"Prepare to commit the currently active transaction" },
	{ "transaction_start", (PyCFunction)obj_transaction_start, METH_NOARGS,
		"S.transaction_start() -> None\n"
		"Start a new transaction." },
	{ "lock_all", (PyCFunction)obj_lockall, METH_NOARGS, NULL },
	{ "unlock_all", (PyCFunction)obj_unlockall, METH_NOARGS, NULL },
	{ "read_lock_all", (PyCFunction)obj_lockall_read, METH_NOARGS, NULL },
	{ "read_unlock_all", (PyCFunction)obj_unlockall_read, METH_NOARGS, NULL },
	{ "close", (PyCFunction)obj_close, METH_NOARGS, NULL },
	{ "get", (PyCFunction)obj_get, METH_VARARGS, "S.get(key) -> value\n"
		"Fetch a value." },
	{ "append", (PyCFunction)obj_append, METH_VARARGS, "S.append(key, value) -> None\n"
		"Append data to an existing key." },
	{ "firstkey", (PyCFunction)obj_firstkey, METH_NOARGS, "S.firstkey() -> data\n"
		"Return the first key in this database." },
	{ "nextkey", (PyCFunction)obj_nextkey, METH_NOARGS, "S.nextkey(key) -> data\n"
		"Return the next key in this database." },
	{ "delete", (PyCFunction)obj_delete, METH_VARARGS, "S.delete(key) -> None\n"
		"Delete an entry." },
	{ "has_key", (PyCFunction)obj_has_key, METH_VARARGS, "S.has_key(key) -> None\n"
		"Check whether key exists in this database." },
	{ "store", (PyCFunction)obj_store, METH_VARARGS, "S.store(key, data, flag=REPLACE) -> None"
		"Store data." },
	{ "add_flag", (PyCFunction)obj_add_flag, METH_VARARGS, "S.add_flag(flag) -> None" },
	{ "remove_flag", (PyCFunction)obj_remove_flag, METH_VARARGS, "S.remove_flag(flag) -> None" },
	{ "iterkeys", (PyCFunction)ntdb_object_iter, METH_NOARGS, "S.iterkeys() -> iterator" },
	{ "clear", (PyCFunction)obj_clear, METH_NOARGS, "S.clear() -> None\n"
		"Wipe the entire database." },
	{ "enable_seqnum", (PyCFunction)obj_enable_seqnum, METH_NOARGS,
		"S.enable_seqnum() -> None" },
	{ NULL }
};

static PyObject *obj_get_flags(PyNtdbObject *self, void *closure)
{
	PyNtdb_CHECK_CLOSED(self);
	return PyInt_FromLong(ntdb_get_flags(self->ctx));
}

static PyObject *obj_get_filename(PyNtdbObject *self, void *closure)
{
	PyNtdb_CHECK_CLOSED(self);
	return PyString_FromString(ntdb_name(self->ctx));
}

static PyObject *obj_get_seqnum(PyNtdbObject *self, void *closure)
{
	PyNtdb_CHECK_CLOSED(self);
	return PyInt_FromLong(ntdb_get_seqnum(self->ctx));
}


static PyGetSetDef ntdb_object_getsetters[] = {
	{ cast_const(char *, "flags"), (getter)obj_get_flags, NULL, NULL },
	{ cast_const(char *, "filename"), (getter)obj_get_filename, NULL,
	  cast_const(char *, "The filename of this NTDB file.")},
	{ cast_const(char *, "seqnum"), (getter)obj_get_seqnum, NULL, NULL },
	{ NULL }
};

static PyObject *ntdb_object_repr(PyNtdbObject *self)
{
	if (ntdb_get_flags(self->ctx) & NTDB_INTERNAL) {
		return PyString_FromString("Ntdb(<internal>)");
	} else {
		return PyString_FromFormat("Ntdb('%s')", ntdb_name(self->ctx));
	}
}

static void ntdb_object_dealloc(PyNtdbObject *self)
{
	if (!self->closed)
		ntdb_close(self->ctx);
	self->ob_type->tp_free(self);
}

static PyObject *obj_getitem(PyNtdbObject *self, PyObject *key)
{
	NTDB_DATA tkey, val;
	enum NTDB_ERROR ret;

	PyNtdb_CHECK_CLOSED(self);

	if (!PyString_Check(key)) {
		PyErr_SetString(PyExc_TypeError, "Expected string as key");
		return NULL;
	}

	tkey.dptr = (unsigned char *)PyString_AsString(key);
	tkey.dsize = PyString_Size(key);

	ret = ntdb_fetch(self->ctx, tkey, &val);
	if (ret == NTDB_ERR_NOEXIST) {
		PyErr_SetString(PyExc_KeyError, "No such NTDB entry");
		return NULL;
	} else {
		PyErr_NTDB_ERROR_IS_ERR_RAISE(ret);
		return PyString_FromNtdb_Data(val);
	}
}

static int obj_setitem(PyNtdbObject *self, PyObject *key, PyObject *value)
{
	NTDB_DATA tkey, tval;
	enum NTDB_ERROR ret;
	if (self->closed) {
		PyErr_SetObject(PyExc_RuntimeError,
			Py_BuildValue("(i,s)", NTDB_ERR_EINVAL, "database is closed"));
		return -1;
	}

	if (!PyString_Check(key)) {
		PyErr_SetString(PyExc_TypeError, "Expected string as key");
		return -1;
	}

	tkey = PyString_AsNtdb_Data(key);

	if (value == NULL) {
		ret = ntdb_delete(self->ctx, tkey);
	} else {
		if (!PyString_Check(value)) {
			PyErr_SetString(PyExc_TypeError, "Expected string as value");
			return -1;
		}

		tval = PyString_AsNtdb_Data(value);

		ret = ntdb_store(self->ctx, tkey, tval, NTDB_REPLACE);
	}

	if (ret != NTDB_SUCCESS) {
		PyErr_SetTDBError(ret);
		return -1;
	}

	return ret;
}

static PyMappingMethods ntdb_object_mapping = {
	.mp_subscript = (binaryfunc)obj_getitem,
	.mp_ass_subscript = (objobjargproc)obj_setitem,
};

static PyTypeObject PyNtdb = {
	.tp_name = "ntdb.Ntdb",
	.tp_basicsize = sizeof(PyNtdbObject),
	.tp_methods = ntdb_object_methods,
	.tp_getset = ntdb_object_getsetters,
	.tp_new = py_ntdb_open,
	.tp_doc = "A NTDB file",
	.tp_repr = (reprfunc)ntdb_object_repr,
	.tp_dealloc = (destructor)ntdb_object_dealloc,
	.tp_as_mapping = &ntdb_object_mapping,
	.tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE|Py_TPFLAGS_HAVE_ITER,
	.tp_iter = (getiterfunc)ntdb_object_iter,
};

static PyMethodDef ntdb_methods[] = {
	{ "open", (PyCFunction)py_ntdb_open, METH_VARARGS|METH_KEYWORDS, "open(name, hash_size=0, ntdb_flags=NTDB_DEFAULT, flags=O_RDWR, mode=0600)\n"
		"Open a NTDB file." },
	{ NULL }
};

void initntdb(void);
void initntdb(void)
{
	PyObject *m;

	if (PyType_Ready(&PyNtdb) < 0)
		return;

	if (PyType_Ready(&PyNtdbIterator) < 0)
		return;

	m = Py_InitModule3("ntdb", ntdb_methods, "NTDB is a simple key-value database similar to GDBM that supports multiple writers.");
	if (m == NULL)
		return;

	PyModule_AddObject(m, "REPLACE", PyInt_FromLong(NTDB_REPLACE));
	PyModule_AddObject(m, "INSERT", PyInt_FromLong(NTDB_INSERT));
	PyModule_AddObject(m, "MODIFY", PyInt_FromLong(NTDB_MODIFY));

	PyModule_AddObject(m, "DEFAULT", PyInt_FromLong(NTDB_DEFAULT));
	PyModule_AddObject(m, "INTERNAL", PyInt_FromLong(NTDB_INTERNAL));
	PyModule_AddObject(m, "NOLOCK", PyInt_FromLong(NTDB_NOLOCK));
	PyModule_AddObject(m, "NOMMAP", PyInt_FromLong(NTDB_NOMMAP));
	PyModule_AddObject(m, "CONVERT", PyInt_FromLong(NTDB_CONVERT));
	PyModule_AddObject(m, "NOSYNC", PyInt_FromLong(NTDB_NOSYNC));
	PyModule_AddObject(m, "SEQNUM", PyInt_FromLong(NTDB_SEQNUM));
	PyModule_AddObject(m, "ALLOW_NESTING", PyInt_FromLong(NTDB_ALLOW_NESTING));

	PyModule_AddObject(m, "__docformat__", PyString_FromString("restructuredText"));

	PyModule_AddObject(m, "__version__", PyString_FromString(PACKAGE_VERSION));

	Py_INCREF(&PyNtdb);
	PyModule_AddObject(m, "Ntdb", (PyObject *)&PyNtdb);

	Py_INCREF(&PyNtdbIterator);
}
