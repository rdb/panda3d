/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfMesh.h
 * @author rdb
 * @date 2017-07-27
 */

#ifndef GLTFMESH_H
#define GLTFMESH_H

#include "gltfAccessor.h"
#include "gltfDictionary.h"
#include "gltfMaterial.h"
#include "gltfRootObject.h"

/**
 *
 */
class EXPCL_GLTF GltfMesh : public GltfRootObject {
PUBLISHED:
  class Primitive;

  INLINE GltfMesh(const string &name = string());

  INLINE void add_primitive(Primitive primitive);
  INLINE void add_weight(double weight);

public:
  INLINE size_t get_num_primitives() const;
  INLINE Primitive &get_primitive(size_t i);
  INLINE const Primitive &get_primitive(size_t i) const;
  INLINE void set_primitive(size_t i, Primitive primitive);
  INLINE void remove_primitive(size_t i);

  INLINE size_t get_num_weights() const;
  INLINE double get_weight(size_t i) const;
  INLINE void set_weight(size_t i, double weight);
  INLINE void remove_weight(size_t i);

  virtual void clear() override;

  virtual bool parse_property(GltfParser &parser, const string &key) override;
  virtual void write_properties(GltfWriter &writer) const override;

  virtual void output(ostream &out) const override;

PUBLISHED:
  class EXPCL_GLTF Primitive : public GltfObject {
  PUBLISHED:
    enum Mode {
      M_points,
      M_lines,
      M_line_loop,
      M_line_strip,
      M_triangles,
      M_triangle_strip,
      M_triangle_fan,
    };

    INLINE Primitive(Mode mode = M_triangles);

  public:
    typedef GltfDictionary<PT(GltfAccessor)> Attributes;
    const Attributes &get_attributes() const;
    bool has_attribute(const string &semantic) const;
    GltfAccessor *get_attribute(const string &semantic) const;
    void set_attribute(const string &semantic, GltfAccessor *accessor);
    void remove_attribute(const string &semantic);

    bool has_indices() const;
    GltfAccessor *get_indices() const;
    void set_indices(GltfAccessor *indices);
    void clear_indices();

    bool has_material() const;
    GltfMaterial *get_material() const;
    void set_material(GltfMaterial *material);
    void clear_material();

    Mode get_mode() const;

    GltfMesh *get_mesh() const;

    virtual bool parse_property(GltfParser &parser, const string &key) override;
    virtual void write_properties(GltfWriter &writer) const override;

  PUBLISHED:
    MAKE_MAP_PROPERTY(attributes, has_attribute, get_attribute,
                                  set_attribute, remove_attribute);
    MAKE_PROPERTY2(indices, has_indices, get_indices, set_indices, clear_indices);
    MAKE_PROPERTY2(material, has_material, get_material, set_material, clear_material);
    MAKE_PROPERTY(mode, get_mode);

  private:
    Attributes _attributes;
    PT(GltfAccessor) _indices;
    PT(GltfMaterial) _material;
    Mode _mode;

    GltfMesh *_mesh;
    friend class GltfMesh;
  };

  MAKE_SEQ_PROPERTY(primitives, get_num_primitives, get_primitive,
                                set_primitive, remove_primitive);

  MAKE_SEQ_PROPERTY(weights, get_num_weights, get_weight, set_weight, remove_weight);

protected:
  GltfArray<Primitive> _primitives;
  GltfArray<double> _weights;
};

#include "gltfMesh.I"

#endif
