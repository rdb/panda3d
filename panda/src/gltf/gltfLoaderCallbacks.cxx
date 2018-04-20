/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfLoaderCallbacks.cxx
 * @author rdb
 * @date 2017-08-12
 */

#include "gltfLoaderCallbacks.h"
#include "gltfLoader.h"

GltfLoaderCallbacks *GltfLoaderCallbacks::_first;

/**
 *
 */
GltfLoaderCallbacks::
GltfLoaderCallbacks() : _sort(0), _loader(nullptr), _registered(false) {
}

/**
 * Registers this as a global loader callback with the given sort value.
 */
void GltfLoaderCallbacks::
register_this(int sort) {
  nassertv(!_registered);
  _registered = true;

  GltfLoaderCallbacks *callbacks = _first;
  if (callbacks == nullptr || callbacks->_sort >= sort) {
    // This is the first callbacks object.
    _next = _first;
    _first = this;
    return;
  }

  while (callbacks->_next != nullptr && callbacks->_next->_sort < sort) {
    callbacks = callbacks->_next;
  }
  _next = callbacks->_next;
  callbacks->_next = this;
}

/**
 * Updates an earlier imported node.
 */
PT(PandaNode) GltfLoaderCallbacks::
update_node(PandaNode *node, const GltfNode *gltf_node) {
  if (_next == nullptr) {
    return _loader->do_update_node(node, gltf_node);
  } else {
    return _next->update_node(node, gltf_node);
  }
}

/**
 * Updates an earlier imported material.
 */
PT(Material) GltfLoaderCallbacks::
update_material(Material *material, const GltfMaterial *gltf_material) {
  if (_next == nullptr) {
    return _loader->do_update_material(material, gltf_material);
  } else {
    return _next->update_material(material, gltf_material);
  }
}

/**
 * Updates an earlier imported texture.
 */
PT(Texture) GltfLoaderCallbacks::
update_texture(Texture *texture, const GltfTexture *gltf_texture) {
  if (_next == nullptr) {
    return _loader->do_update_texture(texture, gltf_texture);
  } else {
    return _next->update_texture(texture, gltf_texture);
  }
}

/**
 * Converts the mesh primitive.  Can be overridden.
 */
PT(Geom) GltfLoaderCallbacks::
make_geom(const GltfMesh *mesh, const GltfMesh::Primitive &primitive) {
  if (_next == nullptr) {
    return _loader->make_geom(mesh, primitive);
  } else {
    return _next->make_geom(mesh, primitive);
  }
}

/**
 * Creates a RenderState suitable for rendering the given primitive.
 */
CPT(RenderState) GltfLoaderCallbacks::
make_geom_state(const GltfMesh *mesh, const GltfMesh::Primitive &primitive) {
  if (_next == nullptr) {
    return _loader->make_geom_state(mesh, primitive);
  } else {
    return _next->make_geom_state(mesh, primitive);
  }
}

/**
 * Returns the GltfRoot associated with this loader.  This is only set if the
 * loader is currently processing this callback.
 */
const GltfRoot *GltfLoaderCallbacks::
get_root() {
  return _loader ? &_loader->_root : nullptr;
}
