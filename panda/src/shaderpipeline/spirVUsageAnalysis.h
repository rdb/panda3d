/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file spirVUsageAnalysis.h
 * @author rdb
 * @date 2026-07-07
 */

#ifndef SPIRVUSAGEANALYSIS_H
#define SPIRVUSAGEANALYSIS_H

#include "spirVModule.h"

/**
 * Answers questions about how a module's variables are used: whether a
 * variable or function parameter is loaded, stored or otherwise consumed,
 * which variable a pointer or image value (indirectly) originates from,
 * whether a variable was sampled with depth comparison, dynamically indexed,
 * or had its image size or level count queried.
 *
 * This is computed by a single sweep over the function bodies at
 * construction time, not tracked incrementally: construct one when the
 * answers are needed, and construct a fresh one after transforming the
 * module.  A pass therefore never has to remember to mark anything as used;
 * the analysis sees the instructions as they are.
 */
class EXPCL_PANDA_SHADERPIPELINE SpirVUsageAnalysis {
public:
  using Id = SpirVId;

  explicit SpirVUsageAnalysis(const SpirVModule &module);

  // Returns the id of the variable or function parameter that the given
  // (pointer or image) value originates from, chased through loads, access
  // chains, copies and casts.  A variable originates from itself.  Returns 0
  // if the value does not trace back to one.
  INLINE SpirVId get_origin(SpirVId id) const;

  // For a variable or function parameter: true if its value is loaded,
  // stored, passed to a function, or otherwise consumed.  For a function:
  // true if it is called at least once.
  INLINE bool is_used(SpirVId id) const;

  // For a variable: true if a sampling operation with (respectively without)
  // depth comparison traces back to it.
  INLINE bool is_dref_sampled(SpirVId var_id) const;
  INLINE bool is_non_dref_sampled(SpirVId var_id) const;

  // For a variable: true if an access chain that traces back to it uses an
  // index that is not a constant expression.
  INLINE bool is_dynamically_indexed(SpirVId var_id) const;

  // For a variable: true if the size or level count of an image loaded from
  // it was queried.
  INLINE bool was_size_or_levels_queried(SpirVId var_id) const;

  // For a value: true if it was extracted from a sampled image (via OpImage),
  // as opposed to being loaded from a plain (storage) image.
  INLINE bool is_sampled_image_value(SpirVId id) const;

  // For a value: true if it can be evaluated at compile time.
  INLINE bool is_constant_expression(SpirVId id) const;

private:
  enum UsageFlags {
    UF_used = 1 << 0,
    UF_constant_expression = 1 << 1,
    UF_sampled_image = 1 << 2,
    UF_dref_sampled = 1 << 3,
    UF_non_dref_sampled = 1 << 4,
    UF_dynamically_indexed = 1 << 5,
    UF_queried_size_levels = 1 << 6,
  };

  INLINE bool has_flag(SpirVId id, int flag) const;
  INLINE void set_flag(SpirVId id, int flag);
  INLINE void set_origin(SpirVId id, SpirVId origin);
  INLINE void mark_used(SpirVId id);
  INLINE void set_origin_flag(SpirVId id, int flag);

  pvector<uint8_t> _flags;
  pvector<SpirVId> _origins;
};

#include "spirVUsageAnalysis.I"

#endif
