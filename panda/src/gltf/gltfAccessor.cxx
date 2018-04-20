/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfAccessor.cxx
 * @author rdb
 * @date 2017-07-28
 */

#include "gltfAccessor.h"
#include "gltfParser.h"
#include "gltfWriter.h"

/**
 *
 */
GltfAccessor::
GltfAccessor() :
  _byte_offset(0),
  _component_type(CT_float),
  _count(0),
  _normalized(false),
  _type(T_scalar) {
}

/**
 * Returns the size in bytes of each component.
 */
size_t GltfAccessor::
get_component_size() const {
  switch (_component_type) {
  case CT_byte:
  case CT_unsigned_byte:
    return 1;

  case CT_short:
  case CT_unsigned_short:
    return 2;

  case CT_unsigned_int:
  case CT_float:
    return 4;
  }
  nassertr(false, 0);
  return 0;
}

/**
 * Returns the number of components for each element.
 */
size_t GltfAccessor::
get_num_components() const {
  nassertr(_type >= T_scalar && _type <= T_mat4, 0)
  static const unsigned int num_components[] = {1, 2, 3, 4, 4, 9, 16};
  return num_components[(size_t)_type];
}

/**
 * Returns the size in bytes of the data.
 */
size_t GltfAccessor::
get_data_size() const {
  return get_component_size() * get_num_components() * _count;
}

/**
 * Resets this object to its default state.
 */
void GltfAccessor::
clear() {
  _buffer_view.clear();
  _byte_offset = 0;
  _component_type = CT_float;
  _normalized = false;
  _count = 0;
  _type = T_scalar;
  _max.clear();
  _min.clear();
  GltfRootObject::clear();
}

/**
 *
 */
bool GltfAccessor::
parse_property(GltfParser &parser, const string &key) {
  if (key == "bufferView") {
    return parser.parse_value(_buffer_view);
  }
  if (key == "byteOffset") {
    return parser.parse_value(_byte_offset);
  }
  if (key == "componentType") {
    unsigned int type;
    if (!parser.parse_value(type)) {
      return false;
    }
    _component_type = (ComponentType)type;
    return true;
  }
  if (key == "normalized") {
    return parser.parse_value(_normalized);
  }
  if (key == "count") {
    return parser.parse_value(_count);
  }
  if (key == "max") {
    return parser.parse_value(_max) && _max.size() >= 1 && _max.size() <= 16;
  }
  if (key == "min") {
    return parser.parse_value(_min) && _min.size() >= 1 && _min.size() <= 16;
  }
  if (key == "type") {
    string type;
    if (!parser.parse_value(type)) {
      return false;
    }
    if (type == "SCALAR") {
      _type = T_scalar;
    } else if (type == "VEC2") {
      _type = T_vec2;
    } else if (type == "VEC3") {
      _type = T_vec3;
    } else if (type == "VEC4") {
      _type = T_vec4;
    } else if (type == "MAT2") {
      _type = T_mat2;
    } else if (type == "MAT3") {
      _type = T_mat3;
    } else if (type == "MAT4") {
      _type = T_mat4;
    } else {
      gltf_cat.error()
        << "invalid type \"" << type << "\" for accessor\n";
      return false;
    }
    return true;
  }
  return GltfRootObject::parse_property(parser, key);
}

/**
 * Writes out the properties contained within this object to the writer.
 */
void GltfAccessor::
write_properties(GltfWriter &writer) const {
  static const char *const types[] = {"SCALAR", "VEC2", "VEC3", "VEC4", "MAT2", "MAT3", "MAT4", ""};
  if (_buffer_view) {
    writer.write_property("bufferView", _buffer_view);
    if (_byte_offset != 0) {
      writer.write_property("byteOffset", _byte_offset);
    }
  }
  writer.write_property("componentType", (unsigned int)_component_type);
  if (_normalized) {
    writer.write_property("normalized", _normalized);
  }
  writer.write_property("count", _count);
  writer.write_property("type", string(types[(size_t)_type]));
  writer.write_property("max", _max);
  writer.write_property("min", _min);
  if (_sparse) {
    writer.write_property("sparse", _sparse);
  }
  GltfRootObject::write_properties(writer);
}

/**
 * Writes out a one-line description of this object.
 */
void GltfAccessor::
output(ostream &out) const {
  out << "GltfAccessor #" << get_index();
  if (has_name()) {
    out << " \"" << get_name() << "\"";
  }
}

/**
 *
 */
bool GltfAccessor::Sparse::
parse_property(GltfParser &parser, const string &key) {
  if (key == "count") {
    return parser.parse_value(_count) && _count > 0;
  }
  /* TODO
  if (key == "indices") {
    return parser.parse_value(_indices);
  }
  if (key == "values") {
    return parser.parse_value(_values);
  }
  */
  return GltfObject::parse_property(parser, key);
}

/**
 * Writes out the properties contained within this object to the writer.
 */
void GltfAccessor::Sparse::
write_properties(GltfWriter &writer) const {
  writer.write_property("count", _count);
  GltfObject::write_properties(writer);
}
