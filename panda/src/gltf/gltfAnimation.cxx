/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfAnimation.cxx
 * @author rdb
 * @date 2017-09-02
 */

#include "gltfAnimation.h"
#include "gltfParser.h"
#include "gltfWriter.h"

/**
 *
 */
GltfAnimation::
GltfAnimation() {
}

/**
 *
 */
bool GltfAnimation::Sampler::
parse_property(GltfParser &parser, const string &key) {
  if (key == "input") {
    return parser.parse_value(input);
  }
  if (key == "interpolation") {
    return parser.parse_value(interpolation);
  }
  if (key == "output") {
    return parser.parse_value(output);
  }
  return GltfObject::parse_property(parser, key);
}

/**
 * Writes out the properties contained within this object to the writer.
 */
void GltfAnimation::Sampler::
write_properties(GltfWriter &writer) const {
  writer.write_property("input", input);
  writer.write_property("interpolation", interpolation);
  writer.write_property("output", output);
  GltfObject::write_properties(writer);
}

/**
 *
 */
bool GltfAnimation::Channel::Target::
parse_property(GltfParser &parser, const string &key) {
  if (key == "node") {
    return parser.parse_value(node);
  }
  if (key == "path") {
    return parser.parse_value(path);
  }
  return GltfObject::parse_property(parser, key);
}

/**
 * Writes out the properties contained within this object to the writer.
 */
void GltfAnimation::Channel::Target::
write_properties(GltfWriter &writer) const {
  writer.write_property("node", node);
  writer.write_property("path", path);
  GltfObject::write_properties(writer);
}

/**
 *
 */
bool GltfAnimation::Channel::
parse_property(GltfParser &parser, const string &key) {
  if (key == "sampler") {
    unsigned int index;
    if (!parser.parse_value(index)) {
      return false;
    }
    GltfAnimation *animation = (GltfAnimation *)parser.get_parent();
    nassertr(animation != nullptr, false);
    sampler = animation->_samplers.resolve_id(index);
    return true;
  }
  if (key == "target") {
    return parser.parse_value(target);
  }
  return GltfObject::parse_property(parser, key);
}

/**
 * Writes out the properties contained within this object to the writer.
 */
void GltfAnimation::Channel::
write_properties(GltfWriter &writer) const {
  writer.write_property("sampler", (unsigned int)sampler->get_index());
  writer.write_property("target", target);
  GltfObject::write_properties(writer);
}

/**
 * Resets this object to its default state.
 */
void GltfAnimation::
clear() {
  _channels.clear();
  _samplers.clear();
  GltfRootObject::clear();
}

/**
 *
 */
bool GltfAnimation::
parse_property(GltfParser &parser, const string &key) {
  if (key == "channels") {
    return parser.parse_value(_channels);
  }
  if (key == "samplers") {
    return parser.parse_value(_samplers);
  }
  return GltfRootObject::parse_property(parser, key);
}

/**
 * Writes out the properties contained within this object to the writer.
 */
void GltfAnimation::
write_properties(GltfWriter &writer) const {
  writer.write_property("channels", _channels);
  writer.write_property("samplers", _samplers);
  GltfRootObject::write_properties(writer);
}

/**
 * Writes out a one-line description of this object.
 */
void GltfAnimation::
output(ostream &out) const {
  out << "GltfAnimation #" << get_index();
  if (has_name()) {
    out << " \"" << get_name() << "\"";
  }
  out << ", " << _channels.size() << " channels, "
              << _samplers.size() << " samplers";
}
