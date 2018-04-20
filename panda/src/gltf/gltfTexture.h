/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfTexture.h
 * @author rdb
 * @date 2017-08-02
 */

#ifndef GLTFTEXTURE_H
#define GLTFTEXTURE_H

#include "gltfRootObject.h"
#include "gltfImage.h"
#include "gltfSampler.h"

/**
 * A texture is defined by a reference to an image and an optional sampler.
 */
class EXPCL_GLTF GltfTexture : public GltfRootObject {
public:
  GltfTexture() DEFAULT_CTOR;

  INLINE bool has_sampler() const;
  INLINE GltfSampler *get_sampler() const;
  INLINE void set_sampler(GltfSampler *sampler);
  INLINE void clear_sampler();
  INLINE UpdateSeq get_sampler_modified() const;

  INLINE bool has_source() const;
  INLINE GltfImage *get_source() const;
  INLINE void set_source(GltfImage *source);
  INLINE void clear_source();
  INLINE UpdateSeq get_source_modified() const;

  virtual void clear() override;

  virtual bool parse_property(GltfParser &parser, const string &key) override;
  virtual void write_properties(GltfWriter &writer) const override;

  virtual void output(ostream &out) const override;

PUBLISHED:
  MAKE_PROPERTY2(sampler, has_sampler, get_sampler, set_sampler, clear_sampler);
  MAKE_PROPERTY2(source, has_source, get_source, set_source, clear_source);

protected:
  PT(GltfSampler) _sampler;
  PT(GltfImage) _source;
};

#include "gltfTexture.I"

#endif
