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

#ifndef GLTFSAVER_H
#define GLTFSAVER_H

#include "pandabase.h"
#include "coordinateSystem.h"
#include "pandaNode.h"

#include "gltfNode.h"

class GltfRoot;
class WorkingNodePath;

/**
 * Converts the scene graph beginning at the indicated node into a GltfRoot
 * structure, for writing to a glTF file.  The conversion is not necessarily
 * complete, since many Panda3D features are not supported by the glTF format.
 */
class GltfSaver {
public:
  GltfSaver(GltfRoot &data, CoordinateSystem coordsys = CS_default);

  GltfScene *add_scene(PandaNode *scene);
  GltfNode *add_node(PandaNode *node);

private:
  void convert_node(const WorkingNodePath &node_path, GltfNode *gltf_node);

  pmap<PT(PandaNode), PT(GltfNode)> _nodes;
};

#endif
