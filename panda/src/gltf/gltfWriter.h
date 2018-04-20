/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfWriter.h
 * @author rdb
 * @date 2017-08-18
 */

#ifndef GLTFWRITER_H
#define GLTFWRITER_H

#include "pandabase.h"
#include "luse.h"
#include "pointerToBase.h"
#include "gltfRoot.h"
#include "gltfUriReference.h"

class GltfArrayBase;
class GltfObject;

/**
 * Contains the code to write glTF files.  Not exported from this module.
 * Wraps around an output stream.
 */
class GltfWriter {
public:
  GltfWriter(ostream &out, int indent, bool use_comma=true, bool quote_keys=true);

  INLINE void write_property(const char *key, const GltfValue &value);
  INLINE void write_property(const char *key, const GltfObject &value);
  INLINE void write_property(const char *key, const GltfDictionaryBase &value);
  INLINE void write_property(const char *key, const GltfArrayBase &value);
  INLINE void write_property(const char *key, const string &value);
  INLINE void write_property(const char *key, const GltfUriReference &value);
  INLINE void write_property(const char *key, unsigned int value);
  INLINE void write_property(const char *key, double value);
  INLINE void write_property(const char *key, const LVecBase3d &value);
  INLINE void write_property(const char *key, const LVecBase4d &value);
  INLINE void write_property(const char *key, const LMatrix4d &value);
  INLINE void write_property(const char *key, bool value);
  INLINE void write_property(const char *key, const GltfRootObject *value);

  void write_value(const GltfValue &value);
  void write_value(const GltfObject &value);
  void write_value(const GltfDictionaryBase &value);
  void write_value(const GltfArrayBase &value);
  void write_value(const string &value);
  void write_value(unsigned long long value);
  void write_value(unsigned long value);
  void write_value(unsigned int value);
  void write_value(double value);
  void write_value(const LVecBase3d &value);
  void write_value(const LVecBase4d &value);
  void write_value(const LMatrix4d &value);

  INLINE void write_value(bool value);
  INLINE void write_value(nullptr_t);
  INLINE void write_value(const GltfUriReference &value);
  INLINE void write_value(const GltfRootObject &value);

  template<class T>
  INLINE void write_value(const PointerTo<T> &value) {
    if (value != nullptr) {
      write_value(*value);
    } else {
      write_value(nullptr);
    }
  }

protected:
  void write_property_key(const string &key);

private:
  ostream &_out;
  int _indent;
  int _indent_level;
  bool _use_comma;
  bool _need_comma;
  unsigned int _bytes_written;
  const char *const _key_quote;
  const char *const _key_separator;

  friend void GltfRootObject::write(ostream &out, int indent_level) const;
  friend void GltfObject::write(ostream &out, int indent_level) const;
};

#include "gltfWriter.I"

#endif
