/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfSampler.I
 * @author rdb
 * @date 2017-10-01
 */

/**
 *
 */
INLINE GltfSampler::
GltfSampler() :
  _mag_filter(FT_unspecified),
  _min_filter(FT_unspecified),
  _wrap_s(WM_repeat),
  _wrap_t(WM_repeat) {
}

/**
 * Returns the magnification filter.  This will never be one of the mipmapping
 * modes.
 */
INLINE GltfSampler::FilterType GltfSampler::
get_mag_filter() const {
  return _mag_filter;
}

/**
 * Sets the magnification filter.  This shouldn't be one of the mipmapping
 * modes.
 */
INLINE void GltfSampler::
set_mag_filter(FilterType filter) {
  if (filter == FT_nearest_mipmap_nearest ||
      filter == FT_nearest_mipmap_linear) {
    _mag_filter = FT_nearest;
  } else if (filter == FT_linear_mipmap_nearest ||
             filter == FT_linear_mipmap_linear) {
    _mag_filter = FT_linear;
  } else {
    _mag_filter = filter;
  }
}

/**
 * Returns the minification filter.
 */
INLINE GltfSampler::FilterType GltfSampler::
get_min_filter() const {
  return _min_filter;
}

/**
 * Sets the minification filter.
 */
INLINE void GltfSampler::
set_min_filter(FilterType filter) {
  _min_filter = filter;
}

/**
 * Returns the wrap mode in the horizontal direction.
 */
INLINE GltfSampler::WrapMode GltfSampler::
get_wrap_s() const {
  return _wrap_s;
}

/**
 * Sets the wrap mode in the horizontal direction.
 */
INLINE void GltfSampler::
set_wrap_s(WrapMode mode) {
  _wrap_s = mode;
}

/**
 * Returns the wrap mode in the vertical direction.
 */
INLINE GltfSampler::WrapMode GltfSampler::
get_wrap_t() const {
  return _wrap_t;
}

/**
 * Sets the wrap mode in the horizontal direction.
 */
INLINE void GltfSampler::
set_wrap_t(WrapMode mode) {
  _wrap_t = mode;
}
