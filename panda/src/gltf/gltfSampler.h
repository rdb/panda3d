/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfSampler.h
 * @author rdb
 * @date 2017-08-02
 */

#ifndef GLTFSAMPLER_H
#define GLTFSAMPLER_H

#include "gltfRootObject.h"

/**
 * Texture sampler properties for filtering and wrapping modes.
 */
class EXPCL_GLTF GltfSampler : public GltfRootObject {
PUBLISHED:
  enum FilterType {
    FT_unspecified = 0,
    FT_nearest = 9728,
    FT_linear = 9729,
    FT_nearest_mipmap_nearest = 9984,
    FT_linear_mipmap_nearest = 9985,
    FT_nearest_mipmap_linear = 9986,
    FT_linear_mipmap_linear = 9987,
  };

  enum WrapMode {
    WM_repeat = 10497,
    WM_clamp_to_edge = 33071,
    WM_mirrored_repeat = 33648,
  };

public:
  INLINE GltfSampler();

  INLINE FilterType get_mag_filter() const;
  INLINE void set_mag_filter(FilterType filter);
  INLINE FilterType get_min_filter() const;
  INLINE void set_min_filter(FilterType filter);

  INLINE WrapMode get_wrap_s() const;
  INLINE void set_wrap_s(WrapMode mode);
  INLINE WrapMode get_wrap_t() const;
  INLINE void set_wrap_t(WrapMode mode);

  virtual void clear() override;

  virtual bool parse_property(GltfParser &parser, const string &key) override;
  virtual void write_properties(GltfWriter &writer) const override;

  virtual void output(ostream &out) const override;

PUBLISHED:
  MAKE_PROPERTY(mag_filter, get_mag_filter, set_mag_filter);
  MAKE_PROPERTY(min_filter, get_min_filter, set_min_filter);

  MAKE_PROPERTY(wrap_s, get_wrap_s, set_wrap_s);
  MAKE_PROPERTY(wrap_t, get_wrap_t, set_wrap_t);

protected:
  FilterType _mag_filter;
  FilterType _min_filter;
  WrapMode _wrap_s;
  WrapMode _wrap_t;
};

#include "gltfSampler.I"

#endif
