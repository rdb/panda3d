/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfNode.h
 * @author rdb
 * @date 2017-07-27
 */

#ifndef GLTFNODE_H
#define GLTFNODE_H

#include "gltfArray.h"
#include "gltfCamera.h"
#include "gltfMesh.h"
#include "gltfObject.h"

#include "transformState.h"

class GltfSkin;

/**
 *
 */
class EXPCL_GLTF GltfNode : public GltfRootObject {
PUBLISHED:
  INLINE GltfNode(const string &name = string());

  INLINE void add_child(GltfNode *node);
  INLINE void add_weight(double weight);

  bool decompose_matrix();

public:
  INLINE bool has_camera() const;
  INLINE GltfCamera *get_camera() const;
  INLINE void set_camera(GltfCamera *camera);
  INLINE void clear_camera();

  INLINE size_t get_num_children() const;
  INLINE GltfNode *get_child(size_t i) const;
  INLINE void set_child(size_t i, GltfNode *node);
  INLINE void remove_child(size_t i);

  INLINE bool has_mesh() const;
  INLINE GltfMesh *get_mesh() const;
  INLINE void set_mesh(GltfMesh *mesh);
  INLINE void clear_mesh();

  INLINE bool has_skin() const;
  INLINE GltfSkin *get_skin() const;
  INLINE void set_skin(GltfSkin *skin);
  INLINE void clear_skin();

  INLINE size_t get_num_weights() const;
  INLINE double get_weight(size_t i) const;
  INLINE void set_weight(size_t i, double weight);
  INLINE void remove_weight(size_t i);

  INLINE bool has_matrix() const;
  const LMatrix4d &get_matrix() const;
  INLINE void set_matrix(const LMatrix4d &matrix);

  INLINE bool has_translation() const;
  INLINE const LPoint3d &get_translation() const;
  INLINE void set_translation(LPoint3d translation);
  INLINE void clear_translation();

  INLINE bool has_scale() const;
  INLINE const LVecBase3d &get_scale() const;
  INLINE void set_scale(LVecBase3d scale);
  INLINE void clear_scale();

  INLINE bool has_rotation() const;
  INLINE const LQuaterniond &get_rotation() const;
  INLINE void set_rotation(const LQuaterniond &rotation);
  INLINE void clear_rotation();

  virtual void clear() override;

  virtual bool parse_property(GltfParser &parser, const string &key) override;
  virtual void write_properties(GltfWriter &writer) const override;

  virtual void output(ostream &out) const override;

PUBLISHED:
  MAKE_PROPERTY2(camera, has_camera, get_camera, set_camera, clear_camera);
  MAKE_SEQ_PROPERTY(children, get_num_children, get_child, set_child, remove_child);
  MAKE_PROPERTY2(mesh, has_mesh, get_mesh, set_mesh, clear_mesh);
  MAKE_PROPERTY2(skin, has_skin, get_skin, set_skin, clear_skin);
  MAKE_SEQ_PROPERTY(weights, get_num_weights, get_weight, set_weight, remove_weight);

  MAKE_PROPERTY(matrix, get_matrix, set_matrix);
  MAKE_PROPERTY2(translation, has_translation, get_translation,
                              set_translation, clear_translation);
  MAKE_PROPERTY2(scale, has_scale, get_scale, set_scale, clear_scale);
  MAKE_PROPERTY2(rotation, has_rotation, get_rotation,
                           set_rotation, clear_rotation);

protected:
  GltfArray<PT(GltfNode)> _children;
  PT(GltfCamera) _camera;
  PT(GltfMesh) _mesh;
  PT(GltfRootObject) _skin;
  GltfArray<double> _weights;

  enum TransformFlags {
    F_has_matrix = 0x01,
    F_has_translation = 0x02,
    F_has_scale = 0x04,
    F_has_rotation = 0x08,
  };

  mutable LMatrix4d _matrix;
  LPoint3d _translation;
  LVecBase3d _scale;
  LQuaterniond _rotation;
  int _flags;
};

// We can't include this at the top due to a circular reference.
#include "gltfSkin.h"

#include "gltfNode.I"

#endif
