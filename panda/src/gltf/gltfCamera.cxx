/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfCamera.cxx
 * @author rdb
 * @date 2017-08-03
 */

#include "gltfCamera.h"
#include "gltfParser.h"
#include "gltfWriter.h"

/**
 *
 */
bool GltfCamera::Orthographic::
parse_property(GltfParser &parser, const string &key) {
  if (key == "xmag") {
    return parser.parse_value(_xmag);
  }
  if (key == "ymag") {
    return parser.parse_value(_ymag);
  }
  if (key == "zfar") {
    return parser.parse_value(_zfar);
  }
  if (key == "znear") {
    return parser.parse_value(_znear);
  }
  return GltfObject::parse_property(parser, key);
}

/**
 * Writes out the properties contained within this object to the writer.
 */
void GltfCamera::Orthographic::
write_properties(GltfWriter &writer) const {
  writer.write_property("xmag", _xmag);
  writer.write_property("ymag", _ymag);
  writer.write_property("zfar", _zfar);
  writer.write_property("znear", _znear);
  GltfObject::write_properties(writer);
}

/**
 *
 */
bool GltfCamera::Perspective::
parse_property(GltfParser &parser, const string &key) {
  if (key == "aspectRatio") {
    return parser.parse_value(_aspect_ratio) && _aspect_ratio > 0;
  }
  if (key == "yfov") {
    return parser.parse_value(_yfov) && _yfov > 0;
  }
  if (key == "zfar") {
    return parser.parse_value(_zfar) && _zfar > 0;
  }
  if (key == "znear") {
    return parser.parse_value(_znear) && _znear > 0;
  }
  return GltfObject::parse_property(parser, key);
}

/**
 * Writes out the properties contained within this object to the writer.
 */
void GltfCamera::Perspective::
write_properties(GltfWriter &writer) const {
  if (has_aspect_ratio()) {
    writer.write_property("aspectRatio", _aspect_ratio);
  }
  writer.write_property("yfov", _yfov);
  if (!cinf(_zfar)) {
    writer.write_property("zfar", _zfar);
  }
  writer.write_property("znear", _znear);
  GltfObject::write_properties(writer);
}

/**
 * It is an error to call this if is_orthographic() returns false.
 */
const GltfCamera::Orthographic &GltfCamera::
get_orthographic() const {
  nassertr(is_orthographic(), _orthographic);
  return _orthographic;
}
GltfCamera::Orthographic &GltfCamera::
get_orthographic() {
  nassertr(is_orthographic(), _orthographic);
  return _orthographic;
}

/**
 * It is an error to call this if is_perspective() returns false.
 */
const GltfCamera::Perspective &GltfCamera::
get_perspective() const {
  nassertr(is_perspective(), _perspective);
  return _perspective;
}
GltfCamera::Perspective &GltfCamera::
get_perspective() {
  nassertr(is_perspective(), _perspective);
  return _perspective;
}

/**
 * Resets this object to an initial, invalid state.
 */
void GltfCamera::
clear() {
  _type.clear();
  GltfRootObject::clear();
}

/**
 *
 */
bool GltfCamera::
parse_property(GltfParser &parser, const string &key) {
  if (key == "type") {
    return parser.parse_value(_type);
  }
  if (key == "orthographic") {
    Orthographic orthographic;
    if (!parser.parse_value(orthographic)) {
      return false;
    }
    _orthographic = orthographic;
    return true;
  }
  if (key == "perspective") {
    Perspective perspective;
    if (!parser.parse_value(perspective)) {
      return false;
    }
    _perspective = perspective;
    return true;
  }
  return GltfRootObject::parse_property(parser, key);
}

/**
 * Writes out the properties contained within this object to the writer.
 */
void GltfCamera::
write_properties(GltfWriter &writer) const {
  writer.write_property("type", _type);
  if (is_orthographic()) {
    writer.write_property("orthographic", _orthographic);
  } else if (is_perspective()) {
    writer.write_property("perspective", _perspective);
  }
  GltfRootObject::write_properties(writer);
}

/**
 * Writes out a one-line description of this object.
 */
void GltfCamera::
output(ostream &out) const {
  out << "GltfCamera #" << get_index();
  if (has_name()) {
    out << " \"" << get_name() << "\"";
  }
  out << _type << "\n";
}
