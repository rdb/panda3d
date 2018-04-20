/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfTexture.cxx
 * @author rdb
 * @date 2017-08-02
 */

#include "gltfTexture.h"
#include "gltfParser.h"
#include "gltfWriter.h"

/**
 * Resets this object to its default state.
 */
void GltfTexture::
clear() {
  _sampler.clear();
  _source.clear();
  GltfRootObject::clear();
}

/**
 *
 */
bool GltfTexture::
parse_property(GltfParser &parser, const string &key) {
  if (key == "sampler") {
    return parser.parse_value(_sampler);
  }
  if (key == "source") {
    return parser.parse_value(_source);
  }
  return GltfRootObject::parse_property(parser, key);
}

/**
 * Writes out the properties contained within this object to the writer.
 */
void GltfTexture::
write_properties(GltfWriter &writer) const {
  writer.write_property("sampler", _sampler);
  writer.write_property("source", _source);
  GltfRootObject::write_properties(writer);
}

/**
 * Writes out a one-line description of this object.
 */
void GltfTexture::
output(ostream &out) const {
  out << "GltfTexture #" << get_index();
  if (has_name()) {
    out << " \"" << get_name() << "\"";
  }
  if (_source != nullptr) {
    out << ", from <" << *_source << '>';
  }
}
