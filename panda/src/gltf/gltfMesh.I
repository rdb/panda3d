/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfMesh.I
 * @author rdb
 * @date 2017-09-17
 */

/**
 *
 */
INLINE GltfMesh::
GltfMesh(const string &name) : GltfRootObject(name) {
}

/**
 * Adds the given primitive to the mesh.
 */
INLINE void GltfMesh::
add_primitive(Primitive primitive) {
  _primitives.push_back(move(primitive));
  _primitives.back()._mesh = this;
  mark_modified();
}

/**
 * Adds the given weight value to the mesh.
 */
INLINE void GltfMesh::
add_weight(double weight) {
  _weights.push_back(weight);
  mark_modified();
}

/**
 *
 */
INLINE size_t GltfMesh::
get_num_primitives() const {
  return _primitives.size();
}

/**
 *
 */
INLINE GltfMesh::Primitive &GltfMesh::
get_primitive(size_t i) {
  return _primitives[i];
}

/**
 *
 */
INLINE const GltfMesh::Primitive &GltfMesh::
get_primitive(size_t i) const {
  return _primitives[i];
}

/**
 * Replaces the primitive at the given index with the given object.
 */
INLINE void GltfMesh::
set_primitive(size_t i, Primitive primitive) {
  nassertv(i < _primitives.size());
  _primitives[i] = move(primitive);
  _primitives[i]._mesh = this;
}

/**
 * Removes the primitive at the given index.
 */
INLINE void GltfMesh::
remove_primitive(size_t i) {
  nassertv(i < _primitives.size());
  _primitives.erase(_primitives.begin() + i);
}

/**
 * Returns the number of weights in the mesh.
 */
INLINE size_t GltfMesh::
get_num_weights() const {
  return _weights.size();
}

/**
 *
 */
INLINE double GltfMesh::
get_weight(size_t i) const {
  return _weights[i];
}

/**
 * Replaces the weight at the given index with the given object.
 */
INLINE void GltfMesh::
set_weight(size_t i, double weight) {
  nassertv(i < _weights.size());
  _weights[i] = weight;
}

/**
 * Removes the weight at the given index.
 */
INLINE void GltfMesh::
remove_weight(size_t i) {
  nassertv(i < _weights.size());
  _weights.erase(_weights.begin() + i);
}

/**
 *
 */
INLINE GltfMesh::Primitive::
Primitive(Mode mode) : _mode(mode), _mesh(nullptr) {
}

/**
 *
 */
INLINE bool GltfMesh::Primitive::
has_attribute(const string &semantic) const {
  return (_attributes.find(semantic) != _attributes.end());
}

/**
 *
 */
INLINE GltfAccessor *GltfMesh::Primitive::
get_attribute(const string &semantic) const {
  Attributes::const_iterator it = _attributes.find(semantic);
  if (it != _attributes.end()) {
    return it->second;
  } else {
    return nullptr;
  }
}

/**
 *
 */
INLINE void GltfMesh::Primitive::
set_attribute(const string &semantic, GltfAccessor *accessor) {
  _attributes[semantic] = accessor;
  if (_mesh) {
    _mesh->mark_modified();
  }
}

/**
 *
 */
INLINE void GltfMesh::Primitive::
remove_attribute(const string &semantic) {
  Attributes::const_iterator it = _attributes.find(semantic);
  if (it != _attributes.end()) {
    _attributes.erase(it);
    if (_mesh) {
      _mesh->mark_modified();
    }
  }
}

/**
 *
 */
INLINE const GltfMesh::Primitive::Attributes &GltfMesh::Primitive::
get_attributes() const {
  return _attributes;
}

/**
 *
 */
INLINE bool GltfMesh::Primitive::
has_indices() const {
  return _indices != nullptr;
}

/**
 *
 */
INLINE GltfAccessor *GltfMesh::Primitive::
get_indices() const {
  return _indices;
}

/**
 *
 */
INLINE void GltfMesh::Primitive::
set_indices(GltfAccessor *indices) {
  if (indices != _indices) {
    _indices = indices;
    if (_mesh) {
      _mesh->mark_modified();
    }
  }
}

/**
 *
 */
INLINE void GltfMesh::Primitive::
clear_indices() {
  if (_indices) {
    _indices.clear();
    if (_mesh) {
      _mesh->mark_modified();
    }
  }
}

/**
 *
 */
INLINE bool GltfMesh::Primitive::
has_material() const {
  return _material != nullptr;
}

/**
 *
 */
INLINE GltfMaterial *GltfMesh::Primitive::
get_material() const {
  return _material;
}

/**
 *
 */
INLINE void GltfMesh::Primitive::
set_material(GltfMaterial *material) {
  if (material != _material) {
    _material = material;
    if (_mesh) {
      _mesh->mark_modified();
    }
  }
}

/**
 *
 */
INLINE void GltfMesh::Primitive::
clear_material() {
  if (_material) {
    _material.clear();
    if (_mesh) {
      _mesh->mark_modified();
    }
  }
}

/**
 * Returns the primitive mode of this primitive, eg. triangles or points.
 */
INLINE GltfMesh::Primitive::Mode GltfMesh::Primitive::
get_mode() const {
  return _mode;
}

/**
 * Returns the mesh that this primitive is a part of.
 */
INLINE GltfMesh *GltfMesh::Primitive::
get_mesh() const {
  return _mesh;
}
