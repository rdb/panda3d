/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfScene.cxx
 * @author rdb
 * @date 2017-07-27
 */

#include "gltfScene.h"
#include "gltfParser.h"

/**
 * Resets this object to its default state.
 */
void GltfScene::
clear() {
  _nodes.clear();
  GltfRootObject::clear();
}

/**
 *
 */
bool GltfScene::
parse_property(GltfParser &parser, const string &key) {
  if (key == "nodes") {
    return parser.parse_value(_nodes);
  }
  return GltfRootObject::parse_property(parser, key);
}

/**
 * Writes out the properties contained within this object to the writer.
 */
void GltfScene::
write_properties(GltfWriter &writer) const {
  writer.write_property("nodes", _nodes);
  GltfRootObject::write_properties(writer);
}

/**
 * Writes out a one-line description of this object.
 */
void GltfScene::
output(ostream &out) const {
  out << "GltfScene #" << get_index();
  if (has_name()) {
    out << " \"" << get_name() << "\"";
  }
  out << ", " << _nodes.size() << " nodes";
}
