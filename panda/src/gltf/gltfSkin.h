/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfSkin.h
 * @author rdb
 * @date 2017-09-03
 */

#ifndef GLTFSKIN_H
#define GLTFSKIN_H

#include "gltfArray.h"
#include "gltfAccessor.h"
#include "gltfNode.h"
#include "gltfRootObject.h"

/**
 * Joints and matrices defining a skin.
 */
class EXPCL_GLTF GltfSkin : public GltfRootObject {
PUBLISHED:
  INLINE GltfSkin(const string &name = string());

  INLINE bool add_joint(GltfNode *node);

public:
  INLINE bool has_inverse_bind_matrices() const;
  INLINE GltfAccessor *get_inverse_bind_matrices() const;
  INLINE void set_inverse_bind_matrices(GltfAccessor *inverse_bind_matrices);
  INLINE void clear_inverse_bind_matrices();

  INLINE bool has_skeleton() const;
  INLINE GltfNode *get_skeleton() const;
  INLINE void set_skeleton(GltfNode *skeleton);
  INLINE void clear_skeleton();

  INLINE size_t get_num_joints() const;
  INLINE GltfNode *get_joint(size_t i) const;
  INLINE void set_joint(size_t i, GltfNode *node);
  INLINE void remove_joint(size_t i);

  virtual void clear() override;

  virtual bool parse_property(GltfParser &parser, const string &key) override;
  virtual void write_properties(GltfWriter &writer) const override;

  virtual void output(ostream &out) const override;

PUBLISHED:
  MAKE_PROPERTY2(inverse_bind_matrices,
                 has_inverse_bind_matrices, get_inverse_bind_matrices,
                 set_inverse_bind_matrices, clear_inverse_bind_matrices);
  MAKE_PROPERTY2(skeleton, has_skeleton, get_skeleton,
                           set_skeleton, clear_skeleton);
  MAKE_SEQ_PROPERTY(joints, get_num_joints, get_joint, set_joint, remove_joint);

protected:
  PT(GltfAccessor) _inverse_bind_matrices;
  PT(GltfNode) _skeleton;
  GltfArray<PT(GltfNode)> _joints;
};

#include "gltfSkin.I"

#endif
