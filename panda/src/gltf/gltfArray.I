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
 * @date 2018-01-26
 */

/**
 * Finds the given value in the array.
 */
template<class T>
INLINE typename GltfArray<T>::iterator GltfArray<T>::
find(const T &val) {
  return std::find(this->begin(), this->end(), val);
}

/**
 * Finds the given value in the array.
 */
template<class T>
INLINE typename GltfArray<T>::const_iterator GltfArray<T>::
find(const T &val) const {
  return std::find(this->begin(), this->end(), val);
}
