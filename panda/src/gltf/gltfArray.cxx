/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfArray.cxx
 * @author rdb
 * @date 2017-07-27
 */

#include "gltfArray.h"

/**
 * Clears the array.
 */
template<class T>
void GltfArray<T>::
clear() {
  pvector<T>::clear();
}

/**
 * Parses a single element and adds it to the array.
 */
template<class T>
bool GltfArray<T>::
parse_element(GltfParser &parser, size_t i) {
  if (i == 0) {
    pvector<T>::clear();
  }
  T value;
  if (!parser.parse_value(value)) {
    return false;
  }
  push_back(value);
  return true;
}

/**
 * Writes the nth element in the array to the writer.
 */
template<class T>
void GltfArray<T>::
write_element(GltfWriter &writer, size_t i) const {
  writer.write_value((*this)[i]);
}

/**
 * Clears the array.
 */
template<class T>
void GltfRootArray<T>::
clear() {
  pvector<PointerTo<T> >::clear();
}

/**
 * Constructs a new element and adds it to the array.
 */
template<class T>
T *GltfRootArray<T>::
make(const string &name) {
  T *t = new T;
  t->_name = name;
  t->_index = size();
  push_back(t);
  return t;
}

/**
 * Returns the i'th element in the array, resizing it to the given size if not
 * already large enough.
 */
template<class T>
T *GltfRootArray<T>::
resolve_id(size_t i) {
  while (i >= size()) {
    PT(T) t = new T;
    t->_index = size();
    push_back(move(t));
  }
  return (*this)[i];
}

/**
 * Removes all objects not externally referenced.
 */
template<class T>
size_t GltfRootArray<T>::
garbage_collect() {
  size_t old_size = size();
  size_t new_size = old_size;
  for (size_t i = 0; i < new_size; ++i) {
    if ((*this)[i]->get_ref_count() == 1) {
      // Move it to the end.
      --new_size;
      ::swap((*this)[i], (*this)[new_size]);
    }
  }
  resize(new_size);
  return old_size - new_size;
}

/**
 * Parses a single element and adds it to the array.
 */
template<class T>
bool GltfRootArray<T>::
parse_element(GltfParser &parser, size_t i) {
  T *object = resolve_id(i);
  nassertr(object != nullptr, false);
  object->clear();
  return parser.parse_value(*object);
}

/**
 * Writes the nth element in the array to the writer.
 */
template<class T>
void GltfRootArray<T>::
write_element(GltfWriter &writer, size_t i) const {
  if (!(*this)[i].is_null()) {
    writer.write_value((const T &)*(*this)[i]);
  } else {
    writer.write_value(nullptr);
  }
}

/*template<>
void GltfRootArray<GltfImage>::
write_element(GltfWriter &writer, size_t i) const {
  if (!(*this)[i].is_null()) {
    writer.write_value((const GltfImage &)*(*this)[i]);
  } else {
    writer.write_value(nullptr);
  }
}*/
