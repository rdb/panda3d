/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfScene.h
 * @author rdb
 * @date 2017-07-27
 */

#ifndef GLTFSCENE_H
#define GLTFSCENE_H

#include "gltfArray.h"
#include "gltfNode.h"
#include "gltfRootObject.h"

/**
 * Contains metadata about the glTF asset.
 */
class EXPCL_GLTF GltfScene : public GltfRootObject {
PUBLISHED:
  INLINE GltfScene(const string &name = string());

  INLINE void add_node(GltfNode *node);

public:
  INLINE size_t get_num_nodes() const;
  INLINE GltfNode *get_node(size_t i) const;
  INLINE void set_node(size_t i, GltfNode *node);
  INLINE void remove_node(size_t i);

  virtual void clear() override;

  virtual bool parse_property(GltfParser &parser, const string &key) override;
  virtual void write_properties(GltfWriter &writer) const override;

  virtual void output(ostream &out) const override;

PUBLISHED:
  MAKE_SEQ_PROPERTY(nodes, get_num_nodes, get_node, set_node, remove_node);

protected:
  GltfArray<PT(GltfNode)> _nodes;
};

#include "gltfScene.I"

#endif
