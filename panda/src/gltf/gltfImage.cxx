/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfImage.cxx
 * @author rdb
 * @date 2017-08-02
 */

#include "gltfImage.h"
#include "gltfParser.h"

/**
 *
 */
GltfImage::
GltfImage() {
}

/**
 * Returns an istream suitable for reading the image data.
 */
istream *GltfImage::
open_read() const {
  if (_uri) {
    return _uri.open_read();

  } else if (_buffer_view) {
    const unsigned char *ptr = _buffer_view->get_data_pointer();
    if (ptr != nullptr) {
      string data;
      data.resize(_buffer_view->get_byte_length());
      memcpy((char *)data.data(), ptr, data.size());

      return new istringstream(MOVE(data));
    } else {
      return nullptr;
    }
  }

  gltf_cat.error()
    << "image should define either uri or bufferView\n";
  return nullptr;
}

/**
 * Resets this object to an initial, invalid state.
 */
void GltfImage::
clear() {
  _uri = GltfUriReference();
  _mime_type.clear();
  _buffer_view.clear();
  GltfRootObject::clear();
}

/**
 *
 */
bool GltfImage::
parse_property(GltfParser &parser, const string &key) {
  if (key == "uri") {
    return parser.parse_value(_uri);
  }
  if (key == "mimeType") {
    return parser.parse_value(_mime_type);
  }
  if (key == "bufferView") {
    return parser.parse_value(_buffer_view);
  }
  return GltfRootObject::parse_property(parser, key);
}

/**
 * Writes out the properties contained within this object to the writer.
 */
void GltfImage::
write_properties(GltfWriter &writer) const {
  if (_uri) {
    writer.write_property("uri", _uri);
  }
  writer.write_property("bufferView", _buffer_view);
  if (!_mime_type.empty()) {
    writer.write_property("mimeType", _mime_type);
  }
  GltfRootObject::write_properties(writer);
}

/**
 * Writes out a one-line description of this object.
 */
void GltfImage::
output(ostream &out) const {
  out << "GltfImage #" << get_index();
  if (has_name()) {
    out << " \"" << get_name() << "\"";
  }
  if (_uri) {
    out << ", from " << _uri;
  } else if (_buffer_view) {
    out << ", from buffer";
  }
  if (!_mime_type.empty()) {
    out << ' [' << _mime_type << ']';
  }
}
