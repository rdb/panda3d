/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfLoaderCallbacks.h
 * @author rdb
 * @date 2017-08-12
 */

#ifndef GLTFLOADERCALLBACKS_H
#define GLTFLOADERCALLBACKS_H

#include "config_gltf.h"
#include "pandaNode.h"
#include "renderState.h"
#include "texture.h"
#include "lens.h"
#include "modelRoot.h"

class GltfRoot;
class GltfLoader;

/**
 * Contains callbacks for overriding loader functions for a specific
 * extension.
 *
 * When the loader encounters a node, it will first call this:
 * @code
 *   panda_node = ext.update_node(panda_node, gltf_node)
 * @endcode
 *
 * The panda_node argument will initially be null.  You should upcall in order
 * to let the normal conversion code do its job, which will return a node for
 * you to make further modifications to.
 *
 * If you wish your extension to override the node type, you must check the
 * type of the panda_node passed in (which may be null) and if it doesn't
 * match the required type, create your own node before upcalling.  Otherwise,
 * you should upcall first and then deal with the node type you are given.
 *
 * RenderStates are immutable.  When a glTF material is updated, the following
 * sequence is called instead:
 * @code
 *   panda_mat = update_material(panda_mat, )
 *   state = make_state(mat)
 * @endcode
 *
 */
class EXPCL_GLTF GltfLoaderCallbacks {
PUBLISHED:
  GltfLoaderCallbacks();

  void register_this(int sort);

  virtual PT(PandaNode) update_node(PandaNode *node, const GltfNode *gltf_node);
  virtual PT(Material) update_material(Material *material, const GltfMaterial *gltf_material);
  virtual PT(Texture) update_texture(Texture *texture, const GltfTexture *gltf_texture);

  virtual PT(Geom) make_geom(const GltfMesh *mesh, const GltfMesh::Primitive &primitive);
  virtual CPT(RenderState) make_geom_state(const GltfMesh *mesh, const GltfMesh::Primitive &primitive);

public:
  bool is_registered() const { return _registered; }
  int get_sort() const { return _sort; }
  GltfLoader *get_loader() { return _loader; }
  const GltfRoot *get_root();

PUBLISHED:
  MAKE_PROPERTY(registered, is_registered);
  MAKE_PROPERTY2(sort, is_registered, get_sort);
  MAKE_PROPERTY(loader, get_loader);
  MAKE_PROPERTY(root, get_root);

private:
  static GltfLoaderCallbacks *_first;
  GltfLoaderCallbacks *_next;
  GltfLoader *_loader;
  bool _registered;
  int _sort;

  friend class GltfLoader;
};

#endif
