/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfNode.I
 * @author rdb
 * @date 2017-09-17
 */

/**
 * Creates a new node object.  Add it to the GltfRoot using add_node.
 */
INLINE GltfNode::
GltfNode(const string &name) :
  GltfRootObject(name),
  _flags(0),
  _translation(0),
  _scale(1),
  _rotation(LQuaterniond::ident_quat()) {
}

/**
 * Adds the given node as child to this node, if it doesn't already exist.
 */
INLINE void GltfNode::
add_child(GltfNode *node) {
  if (_children.find(node) == _children.end()) {
    _children.push_back(node);
    mark_modified();
  }
}

/**
 * Adds the given weight value to the node.  It is an error to call this if
 * no mesh has been specified.
 */
INLINE void GltfNode::
add_weight(double weight) {
  nassertv(has_mesh());
  _weights.push_back(weight);
  mark_modified();
}

/**
 * Returns true if this node has an associated camera.
 */
INLINE bool GltfNode::
has_camera() const {
  return _camera != nullptr;
}

/**
 * Returns the camera associated with this node, if any.
 */
INLINE GltfCamera *GltfNode::
get_camera() const {
  return _camera;
}

/**
 * Sets the camera associated with this node.
 */
INLINE void GltfNode::
set_camera(GltfCamera *camera) {
  _camera = camera;
  mark_modified();
}

/**
 * Removes the camera associated with this node.
 */
INLINE void GltfNode::
clear_camera() {
  if (_camera) {
    _camera.clear();
    mark_modified();
  }
}

/**
 * Returns the number of children directly under this node.
 */
INLINE size_t GltfNode::
get_num_children() const {
  return _children.size();
}

/**
 * Returns the child node with the given index.
 */
INLINE GltfNode *GltfNode::
get_child(size_t i) const {
  return _children[i];
}

/**
 * Replaces the child node at the given index.
 */
INLINE void GltfNode::
set_child(size_t i, GltfNode *node) {
  _children[i] = nullptr;
  nassertv(_children.find(node) == _children.end());
  _children[i] = node;
  mark_modified();
}

/**
 * Removes the indicated child node from this node's children.
 */
INLINE void GltfNode::
remove_child(size_t i) {
  nassertv(i < _children.size());
  _children.erase(_children.begin() + i);
  mark_modified();
}

/**
 * Returns true if this node has an associated mesh.
 */
INLINE bool GltfNode::
has_mesh() const {
  return _mesh != nullptr;
}

/**
 * Returns the mesh associated with this node, if any.
 */
INLINE GltfMesh *GltfNode::
get_mesh() const {
  return _mesh;
}

/**
 * Sets the mesh associated with this node.
 */
INLINE void GltfNode::
set_mesh(GltfMesh *mesh) {
  _mesh = mesh;
  mark_modified();
}

/**
 * Removes the mesh associated with this node, as well as the skin.
 */
INLINE void GltfNode::
clear_mesh() {
  _mesh.clear();
  _skin.clear();
  _weights.clear();
  mark_modified();
}

/**
 * Returns true if this node has an associated skin.
 */
INLINE bool GltfNode::
has_skin() const {
  return _skin != nullptr;
}

/**
 * Returns the skin associated with this node, if any.
 */
INLINE GltfSkin *GltfNode::
get_skin() const {
  return (GltfSkin *)_skin.p();
}

/**
 * Sets the skin associated with this node.
 */
INLINE void GltfNode::
set_skin(GltfSkin *skin) {
  if (skin != nullptr) {
    nassertv(_mesh != nullptr);
  }
  _skin = skin;
  mark_modified();
}

/**
 * Removes the skin associated with this node.
 */
INLINE void GltfNode::
clear_skin() {
  if (_skin) {
    _skin.clear();
    mark_modified();
  }
}

/**
 * Returns the number of weights in the mesh.
 */
INLINE size_t GltfNode::
get_num_weights() const {
  return _weights.size();
}

/**
 *
 */
INLINE double GltfNode::
get_weight(size_t i) const {
  nassertr(has_mesh(), 0.0);
  nassertr_always(i < _weights.size(), 0.0);
  return _weights[i];
}

/**
 * Replaces the weight at the given index with the given object.
 */
INLINE void GltfNode::
set_weight(size_t i, double weight) {
  nassertv(has_mesh());
  nassertv(i < _weights.size());
  _weights[i] = weight;
  mark_modified();
}

/**
 * Removes the weight at the given index.
 */
INLINE void GltfNode::
remove_weight(size_t i) {
  nassertv(has_mesh());
  nassertv(i < _weights.size());
  _weights.erase(_weights.begin() + i);
}

/**
 * Returns true if this node has a matrix component.  Even if this returns
 * false, you may call get_matrix().
 * @see get_matrix()
 */
INLINE bool GltfNode::
has_matrix() const {
  return (_flags & F_has_matrix) != 0;
}

/**
 * Sets the transform matrix of this node.  This will clear all the other
 * components.
 */
INLINE void GltfNode::
set_matrix(const LMatrix4d &matrix) {
  _matrix = matrix;
  _translation.set(0, 0, 0);
  _scale.set(1, 1, 1);
  _rotation = LQuaterniond::ident_quat();
  _flags = F_has_matrix;
  mark_modified();
}

/**
 * Returns true if this node has a translation component.
 */
INLINE bool GltfNode::
has_translation() const {
  return (_flags & F_has_translation) != 0;
}

/**
 * Returns the translation of this node's transform.  It is an error to call
 * this if has_matrix() returns true.
 */
INLINE const LPoint3d &GltfNode::
get_translation() const {
  nassertr(!has_matrix(), _translation);
  return _translation;
}

/**
 * Sets the translation of this node.  If the transform is currently set as a
 * matrix, it will be broken down to its components first.
 */
INLINE void GltfNode::
set_translation(LPoint3d translation) {
  if (has_matrix()) {
    nassertv_always(decompose_matrix());
  }
  _translation = translation;
  _flags |= F_has_translation;
  mark_modified();
}

/**
 * Removes a previously specified translation component.
 */
INLINE void GltfNode::
clear_translation() {
  nassertv(!has_matrix());
  _flags &= ~F_has_translation;
  _translation.set(0, 0, 0);
  mark_modified();
}

/**
 * Returns true if this node has a scale component.
 */
INLINE bool GltfNode::
has_scale() const {
  return (_flags & F_has_scale) != 0;
}

/**
 * Returns the scale of this node's transform.  It is an error to call this if
 * has_matrix() returns true.
 */
INLINE const LVecBase3d &GltfNode::
get_scale() const {
  nassertr(!has_matrix(), _scale);
  return _scale;
}

/**
 * Sets the scale of this node.  If the transform is currently set as a
 * matrix, it will be broken down to its components first.
 */
INLINE void GltfNode::
set_scale(LVecBase3d scale) {
  if (has_matrix()) {
    nassertv_always(decompose_matrix());
  }
  _scale = scale;
  _flags |= F_has_scale;
  mark_modified();
}

/**
 * Removes a previously specified scale component.
 */
INLINE void GltfNode::
clear_scale() {
  nassertv(!has_matrix());
  _flags &= ~F_has_scale;
  _scale.set(1, 1, 1);
  mark_modified();
}

/**
 * Returns true if this node has a rotation component.
 */
INLINE bool GltfNode::
has_rotation() const {
  return (_flags & F_has_rotation) != 0;
}

/**
 * Returns the rotation of this node's transform.  It is an error to call this
 * if has_matrix() returns true.
 */
INLINE const LQuaterniond &GltfNode::
get_rotation() const {
  nassertr(!has_matrix(), _rotation);
  return _rotation;
}

/**
 * Sets the rotation of this node.  If the transform is currently set as a
 * matrix, it will be broken down to its components first.
 */
INLINE void GltfNode::
set_rotation(const LQuaterniond &rotation) {
  if (has_matrix()) {
    nassertv_always(decompose_matrix());
  }
  _rotation = rotation;
  _flags |= F_has_rotation;
  mark_modified();
}

/**
 * Removes a previously specified rotation component.
 */
INLINE void GltfNode::
clear_rotation() {
  nassertv(!has_matrix());
  _flags &= ~F_has_rotation;
  _rotation = LQuaterniond::ident_quat();
  mark_modified();
}
