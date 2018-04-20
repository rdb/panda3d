/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfSkin.cxx
 * @author rdb
 * @date 2017-09-03
 */

#include "gltfSkin.h"
#include "gltfParser.h"

/**
 * Resets this object to its default state.
 */
void GltfSkin::
clear() {
  _inverse_bind_matrices.clear();
  _skeleton.clear();
  _joints.clear();
  GltfRootObject::clear();
}

/**
 *
 */
bool GltfSkin::
parse_property(GltfParser &parser, const string &key) {
  if (key == "inverseBindMatrices") {
    return parser.parse_value(_inverse_bind_matrices);
  }
  if (key == "joints") {
    return parser.parse_value(_joints);
  }
  if (key == "skeleton") {
    return parser.parse_value(_skeleton);
  }
  return GltfRootObject::parse_property(parser, key);
}

/**
 * Writes out the properties contained within this object to the writer.
 */
void GltfSkin::
write_properties(GltfWriter &writer) const {
  writer.write_property("inverseBindMatrices", _inverse_bind_matrices);
  writer.write_property("joints", _joints);
  writer.write_property("skeleton", _skeleton);
  GltfRootObject::write_properties(writer);
}

/**
 * Writes out a one-line description of this object.
 */
void GltfSkin::
output(ostream &out) const {
  out << "GltfSkin #" << get_index();
  if (has_name()) {
    out << " \"" << get_name() << "\"";
  }
}
