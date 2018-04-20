/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfRootObject.I
 * @author rdb
 * @date 2017-10-31
 */

/**
 * Returns true if this object has a name.
 */
INLINE bool GltfRootObject::
has_name() const {
  return !_name.empty();
}

/**
 * Returns the name of this object.  This is optional, and need not be unique.
 * An empty string implies the absence of a name.
 */
INLINE const string &GltfRootObject::
get_name() const {
  return _name;
}

/**
 * Sets the name of this object.  This is optional, and need not be unique.
 * An empty string implies the absence of a name.
 */
INLINE void GltfRootObject::
set_name(const string &name) {
  _name = name;
  ++_modified;
}

/**
 * Returns the numerical index of this object in the GltfRoot.  This value
 * will only change when the GltfRoot is garbage collected.
 */
INLINE size_t GltfRootObject::
get_index() const {
  return _index;
}

/**
 * Returns an UpdateSeq that is incremented whenever this object is modified.
 */
INLINE UpdateSeq GltfRootObject::
get_modified() const {
  return _modified;
}

/**
 * Marks this object as having been modified.  This is normally done
 * automatically.
 */
INLINE void GltfRootObject::
mark_modified() {
  ++_modified;
}
