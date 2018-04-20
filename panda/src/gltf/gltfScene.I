/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfScene.I
 * @author rdb
 * @date 2017-09-17
 */

/**
 * Creates a new scene object.  Add it to the GltfRoot using add_scene.
 */
INLINE GltfScene::
GltfScene(const string &name) : GltfRootObject(name) {
}

/**
 * Adds the given node under the scene root, if it doesn't already exist.
 */
INLINE void GltfScene::
add_node(GltfNode *node) {
  if (_nodes.find(node) == _nodes.end()) {
    _nodes.push_back(node);
    mark_modified();
  }
}

/**
 * Returns the number of nodes directly under the scene root.
 */
INLINE size_t GltfScene::
get_num_nodes() const {
  return _nodes.size();
}

/**
 * Returns the node with the given index.
 */
INLINE GltfNode *GltfScene::
get_node(size_t i) const {
  return _nodes[i];
}

/**
 * Replaces the node with the given index.
 */
INLINE void GltfScene::
set_node(size_t i, GltfNode *node) {
  _nodes[i] = nullptr;
  nassertv(_nodes.find(node) == _nodes.end());
  _nodes[i] = node;
  mark_modified();
}

/**
 * Removes the indicated node from the scene.
 */
INLINE void GltfScene::
remove_node(size_t i) {
  nassertv(i < _nodes.size());
  _nodes.erase(_nodes.begin() + i);
  mark_modified();
}
