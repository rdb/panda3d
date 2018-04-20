/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfBufferView.h
 * @author rdb
 * @date 2017-07-28
 */

#ifndef GLTFBUFFERVIEW_H
#define GLTFBUFFERVIEW_H

#include "gltfBuffer.h"
#include "gltfRootObject.h"

/**
 * A view into a buffer generally representing a subset of the buffer.
 */
class EXPCL_GLTF GltfBufferView : public GltfRootObject {
public:
  enum Target {
    T_unspecified = 0,
    T_array_buffer = 34962,
    T_element_array_buffer = 34963,
  };

  INLINE GltfBufferView();
  INLINE GltfBuffer *get_buffer() const;
  INLINE void set_buffer(GltfBuffer *buffer);
  INLINE size_t get_byte_offset() const;
  INLINE void set_byte_offset(size_t offset);
  INLINE size_t get_byte_length() const;
  INLINE void set_byte_length(size_t length);
  INLINE bool has_byte_stride() const;
  INLINE size_t get_byte_stride() const;
  INLINE void set_byte_stride(size_t stride);
  INLINE void clear_byte_stride();
  INLINE Target get_target() const;
  INLINE void set_target(Target target);
  INLINE const unsigned char *get_data_pointer() const;

  virtual void clear() override;

  virtual bool parse_property(GltfParser &parser, const string &key) override;
  virtual void write_properties(GltfWriter &writer) const override;

  virtual void output(ostream &out) const override;

PUBLISHED:
  MAKE_PROPERTY(buffer, get_buffer, set_buffer);
  MAKE_PROPERTY(byte_offset, get_byte_offset, set_byte_offset);
  MAKE_PROPERTY(byte_length, get_byte_length, set_byte_length);
  MAKE_PROPERTY2(byte_stride, has_byte_stride, get_byte_stride,
                              set_byte_stride, clear_byte_stride);
  MAKE_PROPERTY(target, get_target, set_target);

protected:
  PT(GltfBuffer) _buffer;
  unsigned int _byte_offset;
  unsigned int _byte_length;
  unsigned int _byte_stride;
  Target _target;
};

#include "gltfBufferView.I"

#endif
