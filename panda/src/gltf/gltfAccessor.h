/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfAccessor.h
 * @author rdb
 * @date 2017-07-28
 */

#ifndef GLTFACCESSOR_H
#define GLTFACCESSOR_H

#include "gltfArray.h"
#include "gltfBufferView.h"
#include "gltfRootObject.h"

/**
 * Contains metadata about the glTF Accessor.
 */
class EXPCL_GLTF GltfAccessor : public GltfRootObject {
PUBLISHED:
  GltfAccessor();

  enum ComponentType {
    CT_byte = 5120,
    CT_unsigned_byte = 5121,
    CT_short = 5122,
    CT_unsigned_short = 5123,
    CT_unsigned_int = 5125,
    CT_float = 5126,
  };

  enum Type {
    T_scalar,
    T_vec2,
    T_vec3,
    T_vec4,
    T_mat2,
    T_mat3,
    T_mat4,
  };

  class EXPCL_GLTF Sparse FINAL : public GltfObject {
  public:
    INLINE Sparse();

    virtual bool parse_property(GltfParser &parser, const string &key) override;
    virtual void write_properties(GltfWriter &writer) const override;

  PUBLISHED:
    INLINE operator bool() const;
    INLINE void clear();

  private:
    unsigned int _count;
  };

public:
  INLINE bool has_buffer_view() const;
  INLINE GltfBufferView *get_buffer_view() const;
  INLINE void set_buffer_view(GltfBufferView *buffer_view);
  INLINE void clear_buffer_view();

  INLINE size_t get_byte_offset() const;
  INLINE void set_byte_offset(size_t byte_offset);

  INLINE ComponentType get_component_type() const;
  INLINE void set_component_type(ComponentType component_type);

  INLINE bool get_normalized() const;
  INLINE void set_normalized(bool normalized);

  INLINE size_t get_count() const;
  INLINE void set_count(size_t count);

  INLINE Type get_type() const;
  INLINE void set_type(Type type);

  INLINE double get_max(size_t component);
  INLINE double get_min(size_t component);

  //INLINE const Sparse &get_sparse() const;
  //INLINE Sparse &get_sparse();

  size_t get_num_components() const;
  size_t get_component_size() const;
  size_t get_data_size() const;

  virtual void clear() override;

  virtual bool parse_property(GltfParser &parser, const string &key) override;
  virtual void write_properties(GltfWriter &writer) const override;

  virtual void output(ostream &out) const override;

PUBLISHED:
  MAKE_PROPERTY2(buffer_view, has_buffer_view, get_buffer_view,
                              set_buffer_view, clear_buffer_view);
  MAKE_PROPERTY(byte_offset, get_byte_offset, set_byte_offset);
  MAKE_PROPERTY(component_type, get_component_type, set_component_type);
  MAKE_PROPERTY(normalized, get_normalized, set_normalized);
  MAKE_PROPERTY(count, get_count, set_count);
  MAKE_PROPERTY(type, get_type, set_type);

  MAKE_SEQ_PROPERTY(max, get_num_components, get_max);
  MAKE_SEQ_PROPERTY(min, get_num_components, get_min);

  MAKE_PROPERTY(num_components, get_num_components);
  MAKE_PROPERTY(component_size, get_component_size);
  MAKE_PROPERTY(data_size, get_data_size);

protected:
  PT(GltfBufferView) _buffer_view;
  unsigned int _byte_offset;
  ComponentType _component_type;
  bool _normalized;
  unsigned int _count;
  Type _type;
  GltfArray<double> _max;
  GltfArray<double> _min;
  Sparse _sparse;
};

#include "gltfAccessor.I"

#endif
