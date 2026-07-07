/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file spirVUsageAnalysis.I
 * @author rdb
 * @date 2026-07-07
 */

/**
 * Returns the variable or function parameter the given value originates
 * from, or 0.
 */
INLINE uint32_t SpirVUsageAnalysis::
get_origin(uint32_t id) const {
  return (id < _origins.size()) ? _origins[id] : 0;
}

/**
 * Returns true if the given variable, parameter or function is used.
 */
INLINE bool SpirVUsageAnalysis::
is_used(uint32_t id) const {
  return has_flag(id, UF_used);
}

/**
 * Returns true if a depth-comparison sample traces back to this variable.
 */
INLINE bool SpirVUsageAnalysis::
is_dref_sampled(uint32_t var_id) const {
  return has_flag(var_id, UF_dref_sampled);
}

/**
 * Returns true if a regular (non-depth-comparison) sample traces back to
 * this variable.
 */
INLINE bool SpirVUsageAnalysis::
is_non_dref_sampled(uint32_t var_id) const {
  return has_flag(var_id, UF_non_dref_sampled);
}

/**
 * Returns true if this variable is indexed with a non-constant index.
 */
INLINE bool SpirVUsageAnalysis::
is_dynamically_indexed(uint32_t var_id) const {
  return has_flag(var_id, UF_dynamically_indexed);
}

/**
 * Returns true if the size or level count of an image loaded from this
 * variable was queried.
 */
INLINE bool SpirVUsageAnalysis::
was_size_or_levels_queried(uint32_t var_id) const {
  return has_flag(var_id, UF_queried_size_levels);
}

/**
 * Returns true if the given value was extracted from a sampled image.
 */
INLINE bool SpirVUsageAnalysis::
is_sampled_image_value(uint32_t id) const {
  return has_flag(id, UF_sampled_image);
}

/**
 * Returns true if the given value can be evaluated at compile time.
 */
INLINE bool SpirVUsageAnalysis::
is_constant_expression(uint32_t id) const {
  return has_flag(id, UF_constant_expression);
}

/**
 *
 */
INLINE bool SpirVUsageAnalysis::
has_flag(uint32_t id, int flag) const {
  return id < _flags.size() && (_flags[id] & flag) != 0;
}

/**
 *
 */
INLINE void SpirVUsageAnalysis::
set_flag(uint32_t id, int flag) {
  if (id < _flags.size()) {
    _flags[id] |= (uint8_t)flag;
  }
}

/**
 *
 */
INLINE void SpirVUsageAnalysis::
set_origin(uint32_t id, uint32_t origin) {
  if (id < _origins.size()) {
    _origins[id] = origin;
  }
}

/**
 * Marks the variable or parameter the given value originates from as used.
 */
INLINE void SpirVUsageAnalysis::
mark_used(uint32_t id) {
  uint32_t origin = get_origin(id);
  if (origin != 0) {
    set_flag(origin, UF_used);
  }
}

/**
 * Sets the given flag on the variable the given value originates from.
 */
INLINE void SpirVUsageAnalysis::
set_origin_flag(uint32_t id, int flag) {
  uint32_t origin = get_origin(id);
  if (origin != 0) {
    set_flag(origin, flag);
  }
}
