/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfAccessor.I
 * @author rdb
 * @date 2017-09-02
 */

/**
 * Returns true if this accessor references a buffer view.
 */
INLINE bool GltfAccessor::
has_buffer_view() const {
  return _buffer_view != nullptr;
}

/**
 * Returns the buffer view that this accessor references, or nullptr if none.
 * @see get_byte_offset
 */
INLINE GltfBufferView *GltfAccessor::
get_buffer_view() const {
  return _buffer_view;
}

/**
 * Sets the buffer view that this accessor references.
 * @see set_byte_offset
 */
INLINE void GltfAccessor::
set_buffer_view(GltfBufferView *buffer_view) {
  _buffer_view = buffer_view;
}

/**
 * Clears the associated buffer view.
 */
INLINE void GltfAccessor::
clear_buffer_view() {
  _buffer_view = nullptr;
}

/**
 * Sets the offset into the buffer view specified by set_buffer_view.
 * Has no effect if no buffer view was specified.
 * @see set_buffer_view
 */
INLINE size_t GltfAccessor::
get_byte_offset() const {
  return _byte_offset;
}

/**
 * Returns the offset into the buffer view specified by set_buffer_view.
 * Has no effect if no buffer view was specified.
 * @see get_buffer_view
 */
INLINE void GltfAccessor::
set_byte_offset(size_t byte_offset) {
  _byte_offset = byte_offset;
}

/**
 * Returns the numeric data type of individual components in the attribute.
 */
INLINE GltfAccessor::ComponentType GltfAccessor::
get_component_type() const {
  return _component_type;
}

/**
 * Sets the numeric data type of individual components in the attribute.
 */
INLINE void GltfAccessor::
set_component_type(ComponentType component_type) {
  _component_type = component_type;
}

/**
 * Returns true if integer values in this attribute are normalized.
 */
INLINE bool GltfAccessor::
get_normalized() const {
  return _normalized;
}

/**
 * Sets whether integer values in this attribute are normalized.
 */
INLINE void GltfAccessor::
set_normalized(bool normalized) {
  _normalized = normalized;
}

/**
 * Returns the number of attribute rows referenced by this accessor.
 */
INLINE size_t GltfAccessor::
get_count() const {
  return _count;
}

/**
 * Sets the number of attribute rows referenced by this accessor.
 */
INLINE void GltfAccessor::
set_count(size_t count) {
  _count = count;
}

/**
 * Returns the type, which specifies whether the attribute is a scalar,
 * vector, or matrix, and how many components it has.
 */
INLINE GltfAccessor::Type GltfAccessor::
get_type() const {
  return _type;
}

/**
 * Sets whether the attribute is a scalar, vector, or matrix, and how many
 * components it has.
 */
INLINE void GltfAccessor::
set_type(Type type) {
  _type = type;
}

/**
 * Returns the maximum value for the given component.
 */
INLINE double GltfAccessor::
get_max(size_t component) {
  nassertr(component < _max.size(), 0.0);
  return _max[component];
}

/**
 * Returns the minimum value for the given component.
 */
INLINE double GltfAccessor::
get_min(size_t component) {
  nassertr(component < _min.size(), 0.0);
  return _min[component];
}

/**
 * Initializes the sparse data of the accessor.
 */
INLINE GltfAccessor::Sparse::
Sparse() : _count(0) {
}

/**
 * Returns true if the sparse data on this accessor is present.
 */
INLINE GltfAccessor::Sparse::
operator bool() const {
  return _count > 0;
}

/**
 * Clears the sparse data on the accessor.
 */
INLINE void GltfAccessor::Sparse::
clear() {
  _count = 0;
  GltfObject::clear();
}
