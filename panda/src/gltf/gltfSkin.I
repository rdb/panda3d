/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfSkin.I
 * @author rdb
 * @date 2017-09-17
 */

/**
 *
 */
INLINE GltfSkin::
GltfSkin(const string &name) : GltfRootObject(name) {
}

/**
 * Adds the indicated joint to this skin, if it hasn't already been added.
 * @return true if added, false if this joint already existed in this skin.
 */
INLINE bool GltfSkin::
add_joint(GltfNode *joint) {
  if (_joints.find(joint) == _joints.end()) {
    _joints.push_back(joint);
    return true;
  } else {
    return false;
  }
}

/**
 *
 */
INLINE bool GltfSkin::
has_inverse_bind_matrices() const {
  return _inverse_bind_matrices != nullptr;
}

/**
 *
 */
INLINE GltfAccessor *GltfSkin::
get_inverse_bind_matrices() const {
  return _inverse_bind_matrices;
}

/**
 *
 */
INLINE void GltfSkin::
set_inverse_bind_matrices(GltfAccessor *inverse_bind_matrices) {
  _inverse_bind_matrices = inverse_bind_matrices;
}

/**
 *
 */
INLINE void GltfSkin::
clear_inverse_bind_matrices() {
  _inverse_bind_matrices = nullptr;
}

/**
 *
 */
INLINE bool GltfSkin::
has_skeleton() const {
  return _skeleton != nullptr;
}

/**
 *
 */
INLINE GltfNode *GltfSkin::
get_skeleton() const {
  return _skeleton;
}

/**
 *
 */
INLINE void GltfSkin::
set_skeleton(GltfNode *skeleton) {
  _skeleton = skeleton;
}

/**
 *
 */
INLINE void GltfSkin::
clear_skeleton() {
  _skeleton = nullptr;
}

/**
 *
 */
INLINE size_t GltfSkin::
get_num_joints() const {
  return _joints.size();
}

/**
 *
 */
INLINE GltfNode *GltfSkin::
get_joint(size_t i) const {
  return _joints[i];
}

/**
 *
 */
INLINE void GltfSkin::
set_joint(size_t i, GltfNode *node) {
  _joints[i] = node;
}

/**
 *
 */
INLINE void GltfSkin::
remove_joint(size_t i) {
  _joints.erase(_joints.begin() + i);
}
