/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfDictionary_ext.cxx
 * @author rdb
 * @date 2017-09-09
 */

#include "gltfDictionary_ext.h"

#ifdef HAVE_PYTHON

#ifndef CPPPARSER
extern Dtool_PyTypedObject Dtool_GltfValueDictionary;
#endif

/**
 *
 */
PyObject *Extension<GltfValueDictionary>::
get(const string &key, PyObject *def) const {
  GltfValueDictionary::const_iterator it = _this->find(key);
  if (it == _this->end()) {
    Py_XINCREF(def);
    return def;
  }

  const GltfValue &v = it->second;
  switch (v._type) {
  case GltfValue::T_undefined:
    // Should never happen.
    return nullptr;

  case GltfValue::T_null:
    Py_INCREF(Py_None);
    return Py_None;

  case GltfValue::T_boolean:
    return PyBool_FromLong(v._value.as_boolean);

  case GltfValue::T_number:
    return PyFloat_FromDouble(v._value.as_number);

  case GltfValue::T_string:
#if PY_MAJOR_VERSION >= 3
    return PyUnicode_FromStringAndSize(
#else
    return PyString_FromStringAndSize(
#endif
        v._value.as_string->data(), v._value.as_string->size());

  case GltfValue::T_array:
    return nullptr;

  case GltfValue::T_object:
    return DTool_CreatePyInstance(v._value.as_object, Dtool_GltfValueDictionary, false, false);
  }

  return nullptr;
}

/**
 *
 */
PyObject *Extension<GltfValueDictionary>::
__getitem__(const string &key) const {
  PyObject *value = get(key, nullptr);
  if (value != nullptr) {
    return value;
  } else {
    PyErr_SetString(PyExc_KeyError, key.c_str());
    return nullptr;
  }
}

/**
 *
 */
void Extension<GltfValueDictionary>::
__setitem__(const string &key, nullptr_t) {
  GltfValue &v = (*_this)[key];
  v._type = GltfValue::T_null;
}

/**
 *
 */
void Extension<GltfValueDictionary>::
__setitem__(const string &key, bool value) {
  GltfValue &v = (*_this)[key];
  v._type = GltfValue::T_boolean;
  v._value.as_boolean = value;
}

/**
 *
 */
void Extension<GltfValueDictionary>::
__setitem__(const string &key, double value) {
  GltfValue &v = (*_this)[key];
  v._type = GltfValue::T_number;
  v._value.as_number = value;
}

/**
 *
 */
void Extension<GltfValueDictionary>::
__setitem__(const string &key, string value) {
  GltfValue &v = (*_this)[key];
  v._type = GltfValue::T_string;
  v._value.as_string = new string(move(value));
}

/**
 *
 */
/*void Extension<GltfValueDictionary>::
__setitem__(const string &key, nullptr_t) {
  GltfValue &v = (*_this)[key];
  v._type = GltfValue::T_nullptr;
}*/

/**
 *
 */
PyObject *Extension<GltfValueDictionary>::
__iter__() const {
  PyObject *keytuple = keys();
  nassertr(keytuple != nullptr, nullptr);
  PyObject *iter = PyObject_GetIter(keytuple);
  Py_DECREF(keytuple);
  return iter;
}

/**
 * Returns a tuple containing all the keys in the dictionary.
 */
template<>
PyObject *Extension<GltfValueDictionary>::
keys() const {
  PyObject *keys = PyTuple_New(_this->size());

  Py_ssize_t i = 0;

  GltfValueDictionary::const_iterator it;
  for (it = _this->begin(); it != _this->end(); ++it) {
#if PY_MAJOR_VERSION >= 3
    PyTuple_SET_ITEM(keys, i++, PyUnicode_FromStringAndSize(it->first.data(),
                                                            it->first.size()));
#else
    PyTuple_SET_ITEM(keys, i++, PyString_FromStringAndSize(it->first.data(),
                                                           it->first.size()));
#endif
  }

  return keys;
}

/**
 * Returns a tuple containing all the values in the dictionary.
 */
PyObject *Extension<GltfValueDictionary>::
values() const {
  PyObject *values = PyTuple_New(_this->size());

  Py_ssize_t i = 0;

  GltfValueDictionary::const_iterator it;
  for (it = _this->begin(); it != _this->end(); ++it) {
    PyTuple_SET_ITEM(values, i++, __getitem__(it->first));
  }

  return values;
}

/**
 * Returns a tuple containing all the key/value pairs in the dictionary.
 */
PyObject *Extension<GltfValueDictionary>::
items() const {
  PyObject *values = PyTuple_New(_this->size());

  Py_ssize_t i = 0;

  GltfValueDictionary::const_iterator it;
  for (it = _this->begin(); it != _this->end(); ++it) {
#if PY_MAJOR_VERSION >= 3
    PyTuple_SET_ITEM(values, i++, PyTuple_Pack(2,
      PyUnicode_FromStringAndSize(it->first.data(), it->first.size()),
      __getitem__(it->first)));
#else
    PyTuple_SET_ITEM(values, i++, PyTuple_Pack(2,
      PyString_FromStringAndSize(it->first.data(), it->first.size()),
      __getitem__(it->first)));
#endif
  }

  return values;
}

#endif  // HAVE_PYTHON
