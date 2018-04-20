/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfDictionary_ext.h
 * @author rdb
 * @date 2017-09-09
 */

#ifndef GLTFDICTIONARY_EXT_H
#define GLTFDICTIONARY_EXT_H

#ifndef CPPPARSER

#include "extension.h"
#include "py_panda.h"
#include "gltfDictionary.h"

#ifdef HAVE_PYTHON

/**
 * This class defines the extension methods for GltfDictionary, which are
 * called instead of any C++ methods with the same prototype.
 */
template<class T>
class Extension<GltfDictionary<T> > : public ExtensionBase<GltfDictionary<T> > {
public:
  PyObject *get(const string &key, PyObject *def = Py_None) const;
  PyObject *__getitem__(const string &key) const;

  void __setitem__(const string &key, nullptr_t);
  void __setitem__(const string &key, bool value);
  void __setitem__(const string &key, double value);
  void __setitem__(const string &key, string value);
  //void __setitem__(const string &key, GltfArray<GltfValue> value);
  //void __setitem__(const string &key, GltfDictionary<GltfValue> value);

  PyObject *__iter__() const;
  PyObject *keys() const;
  PyObject *values() const;
  PyObject *items() const;
};

#endif  // HAVE_PYTHON

#endif  // CPPPARSER

#endif  // GLTFDICTIONARY_EXT_H
