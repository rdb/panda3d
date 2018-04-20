/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfSaver.h
 * @author rdb
 * @date 2017-10-01
 */

#include "gltfSaver.h"
#include "workingNodePath.h"

/**
 * Creates a GltfSaver that writes into the given GltfRoot structure.  The
 * coordsys argument indicates which coordinate system the Panda scene graph
 * is considered to be in (so that transformations can be converted to glTF's
 * righthanded Y-up coordinate system).
 */
GltfSaver::
GltfSaver(GltfRoot &data, CoordinateSystem coordsys) :
  _data(data),
  _coordsys(coordsys)
{
}

/**
 * Adds the indicated scene root to the output.  Any children that weren't
 * already added will also be added.
 */
GltfScene *GltfSaver::
add_scene(PandaNode *scene) {
  GltfNode *gltf_scene = _data.make_scene();
  _nodes[node] = gltf_node;

  NodePath root(scene);
  for (size_t i = 0; i < scene->get_num_children(); ++i) {
    gltf_scene->add_node(add_node(node));
  }
}

/**
 * Adds the indicated node to the output.  Any children that weren't already
 * added will also be added.
 */
GltfNode *GltfSaver::
add_node(PandaNode *node) {
  if (_nodes.find() == _nodes.end()) {
    GltfNode *gltf_node = _data.make_node();
    _nodes[node] = gltf_node;
    NodePath root(node);
    convert_node(WorkingNodePath(root), gltf_node);
    return gltf_node;
  } else {
    return _nodes[node];
  }
}

/**
 * Converts the indicated node to the corresponding Egg constructs, by first
 * determining what kind of node it is.
 */
void GltfSaver::
convert_node(const WorkingNodePath &node_path, GltfNode *gltf_node) {
  const PandaNode *node = node_path.node();
  gltf_node->set_name(node->get_name());

  const TransformState *xform = node->get_transform();
  if (xform != nullptr && !xform->is_identity()) {
    if (xform->components_given() && !xform->has_nonzero_shear()) {
      // Specify as components.
      if (xform->has_pos()) {
        gltf_node->set_translation(LCAST(double, xform->get_pos()));
      }
      if (xform->has_quat()) {
        gltf_node->set_rotation(LCAST(double, xform->get_quat()));
      }
      if (xform->has_scale()) {
        gltf_node->set_scale(LCAST(double, xform->get_scale()));
      }
    } else {
      // Specify as matrix.
      gltf_node->set_matrix(LCAST(double, xform->get_matrix()));
    }
  }

  if (node->is_geom_node()) {
    GltfMesh *mesh = _data->make_mesh();
  }
}
