/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfBuffer.cxx
 * @author rdb
 * @date 2017-07-28
 */

#include "gltfBuffer.h"
#include "gltfParser.h"
#include "gltfWriter.h"
#include "virtualFileSystem.h"

/**
 *
 */
GltfBuffer::
GltfBuffer() : _byte_length(0) {
}

/**
 * Loads the data from the glTF file.
 */
bool GltfBuffer::
load(const DSearchPath &searchpath, BamCacheRecord *record) {
  if (!_uri) {
    return false;
  }

  if (_uri.has_filename()) {
    if (!_uri.resolve(searchpath, record)) {
      gltf_cat.error()
        << "Failed to find buffer " << _uri << "\n";
      return false;
    }

    VirtualFileSystem *vfs = VirtualFileSystem::get_global_ptr();
    if (!vfs->read_file(_uri.get_fullpath(), _data, true)) {
      gltf_cat.error()
        << "Failed to read buffer from " << _uri << "\n";
      return false;
    }
  } else {
    // It's a data URI or something.
    string data = _uri.read();
    _data.resize(data.size());
    memcpy(&_data[0], data.data(), data.size());
  }

  if (_data.size() != _byte_length) {
    gltf_cat.error()
      << "Number of bytes in buffer file " << _uri << " (" << _data.size()
      << ") did not match specified byteLength " << _byte_length << "\n";
  }

  return true;
}

/**
 * Reads the data from the indicated stream.  Returns false on failure.
 */
bool GltfBuffer::
read(istream &in) {
  _data.resize(_byte_length);
  in.read((char *)&_data[0], _byte_length);

  size_t bytes_read = in.gcount();
  if (bytes_read != _byte_length) {
    gltf_cat.error()
      << "Expected to read " << _byte_length << " bytes for buffer, got " << bytes_read << "\n";
    return false;
  }

  return true;
}

/**
 * Resets this object to its default state.
 */
void GltfBuffer::
clear() {
  _uri = GltfUriReference();
  _byte_length = 0;
  GltfRootObject::clear();
}

/**
 *
 */
bool GltfBuffer::
parse_property(GltfParser &parser, const string &key) {
  if (key == "uri") {
    return parser.parse_value(_uri);
  }
  if (key == "byteLength") {
    return parser.parse_value(_byte_length) && _byte_length > 0;
  }
  return GltfRootObject::parse_property(parser, key);
}

/**
 * Writes out the properties contained within this object to the writer.
 */
void GltfBuffer::
write_properties(GltfWriter &writer) const {
  if (_byte_length == 0) {
    gltf_cat.error()
      << "Buffer #" << get_index() << " has invalid byte length 0\n";
  }
  if (_uri) {
    writer.write_property("uri", _uri);
  }
  writer.write_property("byteLength", _byte_length);
  GltfRootObject::write_properties(writer);
}

/**
 * Writes out a one-line description of this object.
 */
void GltfBuffer::
output(ostream &out) const {
  out << "GltfBuffer #" << get_index();
  if (has_name()) {
    out << " \"" << get_name() << "\"";
  }
  if (_uri) {
    out << ", uri=" << _uri;
  } else {
    out << ", internal";
  }
}
