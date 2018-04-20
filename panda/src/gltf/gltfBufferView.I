/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfBufferView.I
 * @author rdb
 * @date 2017-09-03
 */

/**
 *
 */
INLINE GltfBufferView::
GltfBufferView() :
  _byte_offset(0),
  _byte_length(0),
  _byte_stride(0),
  _target(T_unspecified) {
}

/**
 * Returns the buffer that this buffer view is accessing.
 */
INLINE GltfBuffer *GltfBufferView::
get_buffer() const {
  return _buffer;
}

/**
 * Sets the buffer this buffer view is accessing.
 */
INLINE void GltfBufferView::
set_buffer(GltfBuffer *buffer) {
  nassertv(buffer != nullptr);
  _buffer = buffer;
}

/**
 * Returns the hyte offset into the buffer specified by get_buffer/set_buffer.
 */
INLINE size_t GltfBufferView::
get_byte_offset() const {
  return _byte_offset;
}

/**
 * Sets the byte offset into the buffer specified by get_buffer/set_buffer.
 */
INLINE void GltfBufferView::
set_byte_offset(size_t offset) {
  _byte_offset = offset;
}

/**
 * Returns the size, in bytes, of the segment of the buffer that is accessed.
 */
INLINE size_t GltfBufferView::
get_byte_length() const {
  if (_byte_length >= 1) {
    return _byte_length;
  } else {
    // Use entire buffer.
    return _buffer->get_byte_length() - _byte_offset;
  }
}

/**
 * Sets the size, in bytes, of the segment of the buffer that is accessed.
 */
INLINE void GltfBufferView::
set_byte_length(size_t length) {
  nassertv(length > 0);
  _byte_length = length;
}

/**
 * Returns true if a byte stride has been specified.
 */
INLINE bool GltfBufferView::
has_byte_stride() const {
  return _byte_stride != 0;
}

/**
 * Returns the byte stride.
 */
INLINE size_t GltfBufferView::
get_byte_stride() const {
  return _byte_stride;
}

/**
 * Sets the byte stride.  Must be between 4 and 252, and a multiple of 4.
 */
INLINE void GltfBufferView::
set_byte_stride(size_t stride) {
  nassertv(stride >= 4 && stride <= 252);
  nassertv((stride & 3) == 0);
  _byte_stride = stride;
}

/**
 * Clears an explicitly specified byte stride.
 */
INLINE void GltfBufferView::
clear_byte_stride() {
  _byte_stride = 0;
}

/**
 * Returns the target, or T_unspecified if none has been specified.
 */
INLINE GltfBufferView::Target GltfBufferView::
get_target() const {
  return _target;
}

/**
 * Sets the target.
 */
INLINE void GltfBufferView::
set_target(Target target) {
  nassertv(target == T_array_buffer || target == T_element_array_buffer ||
           target == T_unspecified);
  _target = target;
}

/**
 *
 */
INLINE const unsigned char *GltfBufferView::
get_data_pointer() const {
  if (_buffer != nullptr) {
    const unsigned char *ptr = _buffer->get_data_pointer();
    if (ptr != nullptr) {
      return ptr + _byte_offset;
    }
  }
  return nullptr;
}
