/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfDictionary.cxx
 * @author rdb
 * @date 2017-07-29
 */

#include "gltfDictionary.h"

/**
 * Parses a single element and adds it to the dictionary.
 */
template<class T>
bool GltfDictionary<T>::
parse_property(GltfParser &parser, const string &key) {
  T value;
  if (!parser.parse_value(value)) {
    return false;
  }
  (*this)[key] = value;
  return true;
}

/**
 * Writes out the properties contained within this object to the writer.
 */
template<class T>
void GltfDictionary<T>::
write_properties(GltfWriter &writer) const {
  for (const_iterator it = begin(); it != end(); ++it) {
    writer.write_property(it->first.c_str(), it->second);
  }
}
