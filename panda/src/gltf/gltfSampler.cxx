/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfSampler.cxx
 * @author rdb
 * @date 2017-08-02
 */

#include "gltfSampler.h"
#include "gltfParser.h"

/**
 * Resets this object to its default state.
 */
void GltfSampler::
clear() {
  _mag_filter = FT_unspecified;
  _min_filter = FT_unspecified;
  _wrap_s = WM_repeat;
  _wrap_t = WM_repeat;
  GltfRootObject::clear();
}

/**
 *
 */
bool GltfSampler::
parse_property(GltfParser &parser, const string &key) {
  if (key == "magFilter") {
    unsigned int filter;
    if (!parser.parse_value(filter)) {
      return false;
    }
    set_mag_filter((FilterType)filter);
    return true;
  }
  if (key == "minFilter") {
    unsigned int filter;
    if (!parser.parse_value(filter)) {
      return false;
    }
    set_min_filter((FilterType)filter);
    return true;
  }
  if (key == "wrapS") {
    unsigned int wrap;
    if (!parser.parse_value(wrap)) {
      return false;
    }
    set_wrap_s((WrapMode)wrap);
    return true;
  }
  if (key == "wrapT") {
    unsigned int wrap;
    if (!parser.parse_value(wrap)) {
      return false;
    }
    set_wrap_t((WrapMode)wrap);
    return true;
  }
  return GltfRootObject::parse_property(parser, key);
}

/**
 * Writes out the properties contained within this object to the writer.
 */
void GltfSampler::
write_properties(GltfWriter &writer) const {
  if (_mag_filter != FT_unspecified) {
    writer.write_property("magFilter", (unsigned int)_mag_filter);
  }
  if (_min_filter != FT_unspecified) {
    writer.write_property("minFilter", (unsigned int)_min_filter);
  }
  writer.write_property("wrapS", (unsigned int)_wrap_s);
  writer.write_property("wrapT", (unsigned int)_wrap_t);
  GltfRootObject::write_properties(writer);
}

/**
 * Writes out a one-line description of this object.
 */
void GltfSampler::
output(ostream &out) const {
  out << "GltfSampler #" << get_index();
  if (has_name()) {
    out << " \"" << get_name() << "\"";
  }
}
