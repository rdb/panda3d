/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfBufferView.cxx
 * @author rdb
 * @date 2017-07-28
 */

#include "gltfBufferView.h"
#include "gltfParser.h"
#include "gltfWriter.h"

/**
 * Resets this object to its default state.
 */
void GltfBufferView::
clear() {
  _buffer.clear();
  _byte_offset = 0;
  _byte_length = 0;
  _byte_stride = 0;
  _target = T_unspecified;
  GltfRootObject::clear();
}

/**
 *
 */
bool GltfBufferView::
parse_property(GltfParser &parser, const string &key) {
  if (key == "buffer") {
    return parser.parse_value(_buffer);
  }
  if (key == "byteOffset") {
    return parser.parse_value(_byte_offset);
  }
  if (key == "byteLength") {
    return parser.parse_value(_byte_length) && _byte_length >= 1;
  }
  if (key == "byteStride") {
    return parser.parse_value(_byte_stride);
  }
  if (key == "target") {
    unsigned int value;
    if (!parser.parse_value(value)) {
      return false;
    }
    _target = (Target)value;
    if (_target != T_element_array_buffer && _target != T_array_buffer) {
      gltf_cat.error()
        << "invalid target " << value << " for buffer view\n";
      return false;
    }
    return true;
  }
  return GltfRootObject::parse_property(parser, key);
}

/**
 * Writes out the properties contained within this object to the writer.
 */
void GltfBufferView::
write_properties(GltfWriter &writer) const {
  writer.write_property("buffer", _buffer);
  if (_byte_offset != 0) {
    writer.write_property("byteOffset", _byte_offset);
  }
  writer.write_property("byteLength", (unsigned int)get_byte_length());
  if (has_byte_stride()) {
    writer.write_property("byteStride", _byte_stride);
  }
  GltfRootObject::write_properties(writer);
}

/**
 * Writes out a one-line description of this object.
 */
void GltfBufferView::
output(ostream &out) const {
  out << "GltfBufferView #" << get_index();
  if (has_name()) {
    out << " \"" << get_name() << "\"";
  }
  if (_buffer) {
    out << " <" << *_buffer << ">[";
    if (_byte_offset != 0) {
      out << _byte_offset;
    }
    out << ':' << _byte_length;
    if (has_byte_stride()) {
      out << ":" << _byte_stride;
    }
    out << ']';
  }
}
