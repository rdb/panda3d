/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfMesh.cxx
 * @author rdb
 * @date 2017-07-27
 */

#include "gltfMesh.h"
#include "gltfParser.h"
#include "gltfWriter.h"

/**
 * Resets this object to an initial, invalid state.
 */
void GltfMesh::
clear() {
  _primitives.clear();
  _weights.clear();
  GltfRootObject::clear();
}

/**
 *
 */
bool GltfMesh::
parse_property(GltfParser &parser, const string &key) {
  if (key == "primitives") {
    bool retval = parser.parse_value(_primitives);
    // Make sure all primitives have their "mesh" pointer assigned to this.
    GltfArray<Primitive>::iterator it;
    for (it = _primitives.begin(); it != _primitives.end(); ++it) {
      (*it)._mesh = this;
    }
    return retval;
  }
  if (key == "weights") {
    return parser.parse_value(_weights);
  }
  return GltfRootObject::parse_property(parser, key);
}

/**
 * Writes out the properties contained within this object to the writer.
 */
void GltfMesh::
write_properties(GltfWriter &writer) const {
  writer.write_property("primitives", _primitives);
  writer.write_property("weights", _weights);
  GltfRootObject::write_properties(writer);
}

/**
 *
 */
bool GltfMesh::Primitive::
parse_property(GltfParser &parser, const string &key) {
if (key == "attributes") {
    return parser.parse_value(_attributes);
  }
  if (key == "indices") {
    return parser.parse_value(_indices);
  }
  if (key == "material") {
    return parser.parse_value(_material);
  }
  if (key == "mode") {
    unsigned int mode;
    if (!parser.parse_value(mode)) {
      return false;
    }
    _mode = (Mode)mode;
    return true;
  }
  return GltfObject::parse_property(parser, key);
}

/**
 * Writes out the properties contained within this object to the writer.
 */
void GltfMesh::Primitive::
write_properties(GltfWriter &writer) const {
  writer.write_property("attributes", _attributes);
  writer.write_property("indices", _indices);
  writer.write_property("material", _material);
  writer.write_property("mode", (unsigned int)_mode);
  GltfObject::write_properties(writer);
}

/**
 * Writes out a one-line description of this object.
 */
void GltfMesh::
output(ostream &out) const {
  out << "GltfMesh #" << get_index();
  if (has_name()) {
    out << " \"" << get_name() << "\"";
  }
  out << ", " << _primitives.size() << " primitives";
}
