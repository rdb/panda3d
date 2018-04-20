/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfValue.h
 * @author rdb
 * @date 2017-09-05
 */

#ifndef GLTFVALUE_H
#define GLTFVALUE_H

#include "config_gltf.h"

template<class T>
class GltfArray;
template<class T>
class GltfDictionary;

/**
 * This is a variant type that is used for storing any kind of JSON value.
 * It does not need to be exposed to Python.
 */
struct GltfValue {
  explicit GltfValue() : _type(T_undefined) {}
  GltfValue(nullptr_t) : _type(T_null) {
    _value.as_null = nullptr;
  }
  GltfValue(bool value) : _type(T_boolean) {
    _value.as_boolean = value;
  }
  GltfValue(double value) : _type(T_number) {
    _value.as_number = value;
  }
  GltfValue(string value) : _type(T_string) {
    _value.as_string = new string(move(value));
  }
  GltfValue(GltfArray<GltfValue> value);
  GltfValue(GltfDictionary<GltfValue> value);

  enum Type {
    T_undefined,
    T_null,
    T_boolean,
    T_number,
    T_string,
    T_array,
    T_object,
  };

  Type _type;
  union {
    nullptr_t as_null;
    bool as_boolean;
    double as_number;
    string *as_string;
    GltfArray<GltfValue> *as_array;
    GltfDictionary<GltfValue> *as_object;
  } _value;
};

#endif
