/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfDictionary.h
 * @author rdb
 * @date 2017-07-29
 */

#ifndef GLTFDICTIONARY_H
#define GLTFDICTIONARY_H

#include "config_gltf.h"
#include "pmap.h"
#include "gltfValue.h"

class GltfParser;
class GltfWriter;

/**
 * Abstract base class for GltfDictionary.
 */
class EXPCL_GLTF GltfDictionaryBase {
public:
  virtual void clear()=0;

protected:
  virtual bool parse_property(GltfParser &parser, const string &key)=0;
  virtual void write_properties(GltfWriter &writer) const=0;

  friend class GltfParser;
  friend class GltfWriter;
};

/**
 * A dictionary maps strings to glTF objects of a given type.  The difference
 * with a GltfObject is that a GltfDictionary does not match the keys against
 * a known schema and all its values have the same type.
 */
template<class T>
class EXPCL_GLTF GltfDictionary : public GltfDictionaryBase, public map<string, T> {
public:
  void clear() override FINAL {
    map<string, T>::clear();
  }

protected:
  virtual bool parse_property(GltfParser &parser, const string &key);
  virtual void write_properties(GltfWriter &writer) const;
};

#ifdef CPPPARSER
// Interrogate doesn't understand extensions inside specializations, so we
// redefine an "equivalent interface" for its benefit.
class GltfValueDictionary FINAL : public GltfDictionaryBase {
PUBLISHED:
  GltfValueDictionary() DEFAULT_CTOR;

  void clear() override;
  size_t size() const;

  EXTENSION(PyObject *get(const string &key, PyObject *def = Py_None) const);
  EXTENSION(PyObject *__getitem__(const string &key) const);
  EXTENSION(void __setitem__(const string &key, nullptr_t));
  EXTENSION(void __setitem__(const string &key, bool value));
  EXTENSION(void __setitem__(const string &key, double value));
  EXTENSION(void __setitem__(const string &key, string value));
  //EXTENSION(void __setitem__(const string &key, GltfArray<GltfValue> value));
  //EXTENSION(void __setitem__(const string &key, GltfValueDictionary value));

  EXTENSION(PyObject *__iter__() const);
  EXTENSION(PyObject *keys() const);
  EXTENSION(PyObject *values() const);
  EXTENSION(PyObject *items() const);

protected:
  virtual bool parse_property(GltfParser &parser, const string &key);
  virtual void write_properties(GltfWriter &writer) const;
};
#else
typedef GltfDictionary<GltfValue> GltfValueDictionary;

EXPORT_TEMPLATE_CLASS(EXPCL_GLTF, EXPTP_GLTF, GltfDictionary<GltfValue>);
#endif

#include "gltfDictionary.h"

#endif
