/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfObject.cxx
 * @author rdb
 * @date 2017-07-27
 */

#include "gltfObject.h"
#include "gltfParser.h"
#include "gltfWriter.h"

/**
 * Returns true if the object has this key in the extras dictionary.
 */
bool GltfObject::
has_extra(const string &key) const {
  return _extras.find(key) != _extras.end();
}

/**
 * Returns field in the extras dictionary.  Can only call this if has_extra
 * returns true for this key.
 */
const GltfValue &GltfObject::
get_extra(const string &key) const {
  static GltfValue invalid;
  Extras::const_iterator it = _extras.find(key);
  nassertr(it != _extras.end(), invalid);
  return it->second;
}

/**
 * Sets the extra field in the extras dictionary.
 */
void GltfObject::
set_extra(const string &key, GltfValue value) {
  if (value._type != GltfValue::T_undefined) {
    _extras[key] = value;
  } else {
    _extras.erase(key);
  }
  ++_modified;
}

/**
 * Clears the extra field in the extras dictionary.
 */
void GltfObject::
clear_extra(const string &key) {
  _extras.erase(key);
  ++_modified;
}

/**
 * Removes all extras from this object.
 */
void GltfObject::
clear_extras() {
  _extras.clear();
  ++_modified;
}

/**
 * Returns true if the object has an extension with the given name.
 */
bool GltfObject::
has_extension(const string &key) const {
  return _extensions.find(key) != _extensions.end();
}

/**
 * Returns the extension object associated with this object.  Can only call
 * this if has_extension returns true for this key.
 */
const GltfObject::Extension &GltfObject::
get_extension(const string &key) const {
  static const Extension invalid;
  Extensions::const_iterator it = _extensions.find(key);
  nassertr(it != _extensions.end(), invalid);
  return it->second;
}

/**
 * Returns the extension object associated with this object.  Can only call
 * this if has_extension returns true for this key.
 */
GltfObject::Extension &GltfObject::
get_extension(const string &key) {
  static Extension invalid;
  Extensions::iterator it = _extensions.find(key);
  nassertr(it != _extensions.end(), invalid);
  return it->second;
}

/**
 * Sets the extension field in the extensions dictionary.
 */
void GltfObject::
set_extension(const string &key, Extension extension) {
  _extensions[key] = move(extension);
  ++_modified;
}

/**
 * Removes the given extension, if it is present on the object.
 */
void GltfObject::
clear_extension(const string &key) {
  _extensions.erase(key);
  ++_modified;
}

/**
 * Removes all extensions from this object.
 */
void GltfObject::
clear_extensions() {
  if (!_extensions.empty()) {
    _extensions.clear();
    ++_modified;
  }
}

/**
 * Writes a human-readable representation of this glTF object to the output.
 */
void GltfObject::
write(ostream &out, int indent_level) const {
  indent(out, indent_level) << "GltfObject:";
  GltfWriter writer(out, 2, false, false);
  writer._indent_level = indent_level + 2;
  write_properties(writer);
}

/**
 * Writes a human-readable representation of this glTF object to the output.
 */
void GltfObject::
write_json(ostream &out, int indent_level) const {
  GltfWriter writer(out, 4);
  writer.write_value(*this);
}

/**
 *
 */
bool GltfObject::
parse_property(GltfParser &parser, const string &key) {
  if (key == "extras") {
    ++_modified;
    return parser.parse_value(_extras);
  }
  if (key == "extensions") {
    ++_modified;
    return parser.parse_value(_extensions);
  }
  parser.warning()
    << "Ignoring unrecognized property \"" << key << "\"\n";
  return parser.skip_value();
}

/**
 * Writes the properties using the indicated writer.
 */
void GltfObject::
write_properties(GltfWriter &writer) const {
  if (!_extras.empty()) {
    writer.write_property("extras", _extras);
  }
  if (!_extensions.empty()) {
    writer.write_property("extensions", _extensions);
  }
}

