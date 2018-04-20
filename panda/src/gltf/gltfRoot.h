/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfRoot.h
 * @author rdb
 * @date 2017-07-26
 */

#ifndef GLTFROOT_H
#define GLTFROOT_H

#include "gltfObject.h"
#include "gltfAccessor.h"
#include "gltfAnimation.h"
#include "gltfArray.h"
#include "gltfAsset.h"
#include "gltfBuffer.h"
#include "gltfBufferView.h"
#include "gltfCamera.h"
#include "gltfImage.h"
#include "gltfMaterial.h"
#include "gltfNode.h"
#include "gltfSampler.h"
#include "gltfScene.h"
#include "gltfSkin.h"
#include "gltfTexture.h"
#include "bamCacheRecord.h"

/**
 * This is the root class of the glTF hierarchy.  It contains collections of
 * "root objects" (textures, nodes, scenes, cameras, etc.) as well as a
 * pointer to the main scene object.
 *
 * @since 1.10.0
 */
class EXPCL_GLTF GltfRoot : public GltfObject {
PUBLISHED:
  GltfRoot();

  void clear() override;
  bool read(Filename filename, BamCacheRecord *record = nullptr);
  bool read(istream &in);
  bool load_externals(const DSearchPath &searchpath = DSearchPath(),
                      BamCacheRecord *record = nullptr);

  bool write_gltf(Filename filename, int indent=4);
  bool write_gltf(ostream &out, int indent=4);
  bool write_glb(Filename filename);
  bool write_glb(ostream &out);

  // These are used to construct various objects under the glTF root.
  INLINE GltfScene *make_scene(const string &name);
  INLINE GltfNode *make_node(const string &name);
  INLINE GltfBuffer *make_buffer(const string &name);
  INLINE GltfBufferView *make_buffer_view(const string &name, GltfBuffer *buffer);
  INLINE GltfCamera *make_orthographic_camera(const string &name, double xmag, double ymag, double znear, double zfar);
  INLINE GltfCamera *make_perspective_camera(const string &name, double yfov, double znear, double zfar=make_inf(0.0));
  INLINE GltfImage *make_image(const string &name, const GltfUriReference &uri);
  INLINE GltfImage *make_image(const string &name, GltfBufferView *buffer_view, const string &mime_type);
  INLINE GltfMaterial *make_material(const string &name);
  INLINE GltfMesh *make_mesh(const string &name);
  INLINE GltfSampler *make_sampler(const string &name);
  INLINE GltfSkin *make_skin(const string &name);
  INLINE GltfTexture *make_texture(const string &name);

  size_t garbage_collect();

public:
  //virtual void write(ostream &out, int indent_level = 0) const;

  INLINE Filename get_gltf_filename() const;
  INLINE time_t get_gltf_timestamp() const;

  INLINE GltfAsset &get_asset();
  INLINE const GltfAsset &get_asset() const;

  INLINE GltfScene *get_scene();
  INLINE const GltfScene *get_scene() const;
  INLINE void set_scene(GltfScene *scene);

  INLINE size_t get_num_accessors() const;
  INLINE GltfAccessor *get_accessor(size_t i);
  INLINE const GltfAccessor *get_accessor(size_t i) const;

  INLINE size_t get_num_animations() const;
  INLINE GltfAnimation *get_animation(size_t i);
  INLINE const GltfAnimation *get_animation(size_t i) const;

  INLINE size_t get_num_buffers() const;
  INLINE GltfBuffer *get_buffer(size_t i);
  INLINE const GltfBuffer *get_buffer(size_t i) const;

  INLINE size_t get_num_buffer_views() const;
  INLINE GltfBufferView *get_buffer_view(size_t i);
  INLINE const GltfBufferView *get_buffer_view(size_t i) const;

  INLINE size_t get_num_cameras() const;
  INLINE GltfCamera *get_camera(size_t i);
  INLINE const GltfCamera *get_camera(size_t i) const;

  INLINE size_t get_num_images() const;
  INLINE GltfImage *get_image(size_t i);
  INLINE const GltfImage *get_image(size_t i) const;

  INLINE size_t get_num_materials() const;
  INLINE GltfMaterial *get_material(size_t i);
  INLINE const GltfMaterial *get_material(size_t i) const;

  INLINE size_t get_num_meshes() const;
  INLINE GltfMesh *get_mesh(size_t i);
  INLINE const GltfMesh *get_mesh(size_t i) const;

  INLINE size_t get_num_nodes() const;
  INLINE GltfNode *get_node(size_t i);
  INLINE const GltfNode *get_node(size_t i) const;

  INLINE size_t get_num_samplers() const;
  INLINE GltfSampler *get_sampler(size_t i);
  INLINE const GltfSampler *get_sampler(size_t i) const;

  INLINE size_t get_num_scenes() const;
  INLINE GltfScene *get_scene(size_t i);
  INLINE const GltfScene *get_scene(size_t i) const;

  INLINE size_t get_num_skins() const;
  INLINE GltfSkin *get_skin(size_t i);
  INLINE const GltfSkin *get_skin(size_t i) const;

  INLINE size_t get_num_textures() const;
  INLINE GltfTexture *get_texture(size_t i);
  INLINE const GltfTexture *get_texture(size_t i) const;

  virtual bool parse_property(GltfParser &parser, const string &key) override;
  virtual void write_properties(GltfWriter &writer) const override;

  template<class T>
  INLINE T *resolve_id(size_t id) {
    static_assert(false, "cannot call resolve_id for this type");
    return nullptr;
  }

PUBLISHED:
  MAKE_PROPERTY(asset, get_asset);
  MAKE_PROPERTY(scene, get_scene);

  MAKE_SEQ_PROPERTY(accessors, get_num_accessors, get_accessor);
  MAKE_SEQ_PROPERTY(animations, get_num_animations, get_animation);
  MAKE_SEQ_PROPERTY(buffers, get_num_buffers, get_buffer);
  MAKE_SEQ_PROPERTY(buffer_views, get_num_buffer_views, get_buffer_view);
  MAKE_SEQ_PROPERTY(cameras, get_num_cameras, get_camera);
  MAKE_SEQ_PROPERTY(images, get_num_images, get_image);
  MAKE_SEQ_PROPERTY(materials, get_num_materials, get_material);
  MAKE_SEQ_PROPERTY(meshes, get_num_meshes, get_mesh);
  MAKE_SEQ_PROPERTY(nodes, get_num_nodes, get_node);
  MAKE_SEQ_PROPERTY(samplers, get_num_samplers, get_sampler);
  MAKE_SEQ_PROPERTY(scenes, get_num_scenes, get_scene);
  MAKE_SEQ_PROPERTY(skins, get_num_skins, get_skin);
  MAKE_SEQ_PROPERTY(textures, get_num_textures, get_texture);

protected:
  GltfAsset _asset;
  GltfArray<string> _extensions_used;
  GltfArray<string> _extensions_required;
  PT(GltfScene) _scene;

  GltfRootArray<GltfAccessor> _accessors;
  GltfRootArray<GltfAnimation> _animations;
  GltfRootArray<GltfBuffer> _buffers;
  GltfRootArray<GltfBufferView> _buffer_views;
  GltfRootArray<GltfCamera> _cameras;
  GltfRootArray<GltfImage> _images;
  GltfRootArray<GltfMaterial> _materials;
  GltfRootArray<GltfMesh> _meshes;
  GltfRootArray<GltfNode> _nodes;
  GltfRootArray<GltfSampler> _samplers;
  GltfRootArray<GltfScene> _scenes;
  GltfRootArray<GltfSkin> _skins;
  GltfRootArray<GltfTexture> _textures;

  Filename _gltf_filename;
  time_t _gltf_timestamp;
};

#include "gltfRoot.I"

#endif
