/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfAsset.cxx
 * @author rdb
 * @date 2017-07-27
 */

#include "gltfAsset.h"
#include "gltfParser.h"
#include "gltfWriter.h"

/**
 *
 */
GltfAsset::
GltfAsset() : _version("2.0") {
}

/**
 *
 */
bool GltfAsset::
parse_property(GltfParser &parser, const string &key) {
  if (key == "copyright") {
    return parser.parse_value(_copyright);
  }
  if (key == "generator") {
    return parser.parse_value(_generator);
  }
  if (key == "version") {
    return parser.parse_value(_version) && check_version(_version);
  }
  if (key == "minVersion") {
    return parser.parse_value(_min_version) && check_version(_min_version);
  }
  return GltfObject::parse_property(parser, key);
}

/**
 * Writes out the properties contained within this object to the writer.
 */
void GltfAsset::
write_properties(GltfWriter &writer) const {
  writer.write_property("version", _version);
  if (!_copyright.empty()) {
    writer.write_property("copyright", _copyright);
  }
  if (!_generator.empty()) {
    writer.write_property("generator", _generator);
  }
  if (!_min_version.empty()) {
    writer.write_property("minVersion", _min_version);
  }
  GltfObject::write_properties(writer);
}

/**
 * Checks that the given version string is formatted correctly.
 */
bool GltfAsset::
check_version(const string &version) {
  if (version.empty()) {
    gltf_cat.error() << "version may not be empty\n";
    return false;
  }
  const char *c = version.data();
  const char *end = c + version.size();
  while (c < end && isdigit(*c)) {
    ++c;
  }
  if (c == end || *c != '.') {
    gltf_cat.error()
      << "expected dot in version string \"" << version << "\"\n";
    return false;
  }
  ++c;
  while (c < end && isdigit(*c)) {
    ++c;
  }
  if (c != end) {
    gltf_cat.error()
      << "expected only digits after dot in version string \"" << version << "\"\n";
    return false;
  }
  return true;
}
