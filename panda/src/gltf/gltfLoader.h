/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfLoader.h
 * @author rdb
 * @date 2017-07-28
 */

#ifndef GLTFLOADER_H
#define GLTFLOADER_H

#include "pandabase.h"
#include "modelRoot.h"
#include "geomEnums.h"
#include "renderState.h"
#include "texture.h"
#include "gltfRoot.h"
#include "geomNode.h"
#include "material.h"

class GeomVertexFormat;
class GeomVertexArrayFormat;

/**
 * Converts a glTF data structure, possibly read from an glTF file but not
 * necessarily, into a scene graph suitable for rendering.
 *
 * It has the ability to keep the converted scene graph in sync with the
 * glTF data structures when they are modified.
 *
 * The main two ways to use this class are to access the default scene and use
 * update_all() to make sure it is up-to-date, or to instead use load_scene to
 * copy the converted nodes of a given scene to a preexisting root node.
 *
 * @since 1.10.0
 */
class EXPCL_GLTF GltfLoader {
PUBLISHED:
  GltfLoader(const GltfRoot &root, CoordinateSystem coordsys = CS_default);

  bool update_all();
  PandaNode *update_node(const GltfNode *node);
  const GeomNode *update_mesh(const GltfMesh *mesh);
  Material *update_material(const GltfMaterial *material);
  Texture *update_texture(const GltfTexture *texture);

  void load_scene(PandaNode *root, const GltfScene *scene);

public:
  ModelRoot *get_root_node() { return _default_scene; }

PUBLISHED:
  MAKE_PROPERTY(root_node, get_root_node);

private:
  PT(Geom) make_geom(const GltfMesh *mesh, const GltfMesh::Primitive &prim);

  bool make_column(GeomVertexArrayFormat *format, const GltfMesh::Primitive &prim,
                   const string &attrib, const InternalName *name, GeomEnums::Contents contents);
  void load_column(GeomVertexData *vdata, const GltfMesh::Primitive &prim,
                   const string &attrib, const InternalName *name);
  void load_accessor(GeomVertexArrayDataHandle *handle, size_t offset, const GltfAccessor *accessor);

  PT(PandaNode) do_update_node(PandaNode *panda_node, const GltfNode *node);
  PT(Material) do_update_material(Material *panda_mat, const GltfMaterial *material);
  PT(Texture) do_update_texture(PT(Texture) panda_tex, const GltfTexture *texture);

  CPT(RenderState) make_geom_state(const GltfMesh *mesh, const GltfMesh::Primitive &primitive);

  LMatrix4 _cs_matrix;
  LMatrix4 _inv_cs_matrix;
  const GltfRoot &_root;
  PT(ModelRoot) _default_scene;
  CoordinateSystem _coordsys;
  bool _any_updated;

  //TODO: these are vectors, which does not handle garbage collect properly.
  struct ConvertedNode {
    ConvertedNode() : _updating(false) {}

    PT(PandaNode) _node;
    UpdateSeq _updated;
    bool _updating;
  };
  typedef pvector<ConvertedNode> ConvertedNodes;
  ConvertedNodes _nodes;

  struct ConvertedMesh {
    PT(GeomNode) _node;
    UpdateSeq _updated;
  };
  typedef pvector<ConvertedMesh> ConvertedMeshs;
  pvector<ConvertedMesh> _meshes;

  struct ConvertedMaterial {
    PT(Material) _material;
    UpdateSeq _updated;
  };
  typedef pvector<ConvertedMaterial> ConvertedMaterials;
  ConvertedMaterials _materials;

  struct ConvertedTexture {
    PT(Texture) _texture;
    UpdateSeq _updated;
    UpdateSeq _source_updated;
    UpdateSeq _sampler_updated;
  };
  typedef pvector<ConvertedTexture> ConvertedTextures;
  pvector<ConvertedTexture> _textures;

  pvector<CPT(RenderState)> _states;

  friend class GltfLoaderCallbacks;
};

//#include "gltfLoader.I"

#endif
