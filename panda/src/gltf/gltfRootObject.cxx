/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfRootObject.cxx
 * @author rdb
 * @date 2017-08-02
 */

#include "gltfRootObject.h"

/**
 * Clears this object to its default state.
 */
void GltfRootObject::
clear() {
  _name.clear();
  _extras.clear();
  _extensions.clear();
  ++_modified;
}

/**
 * Writes a human-readable representation of this glTF object to the output.
 */
void GltfRootObject::
write(ostream &out, int indent_level) const {
  indent(out, indent_level) << *this << ":";
  GltfWriter writer(out, 2, false, false);
  writer._indent_level = indent_level + 2;
  write_properties(writer);
}

/**
 *
 */
bool GltfRootObject::
parse_property(GltfParser &parser, const string &key) {
  if (key == "name") {
    return parser.parse_value(_name);
  }
  return GltfObject::parse_property(parser, key);
}

/**
 *
 */
void GltfRootObject::
write_properties(GltfWriter &writer) const {
  if (!_name.empty()) {
    writer.write_property("name", _name);
  }
  GltfObject::write_properties(writer);
}
