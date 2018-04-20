/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfTexture.I
 * @author rdb
 * @date 2017-10-31
 */

/**
 * Returns true if there is a source image object used by this texture.
 */
INLINE bool GltfTexture::
has_sampler() const {
  return _sampler != nullptr;
}

/**
 * Returns a reference to the sampler object used by this texture.  This may
 * be null to indicate the default sampler.
 */
INLINE GltfSampler *GltfTexture::
get_sampler() const {
  return _sampler;
}

/**
 * Sets a reference to the sampler object used by this texture.
 */
INLINE void GltfTexture::
set_sampler(GltfSampler *sampler) {
  _sampler = sampler;
}

/**
 * Removes the sampler that is associated with this texture, which will cause
 * it to use the default sampler settings instead.
 */
INLINE void GltfTexture::
clear_sampler() {
  _sampler.clear();
}

/**
 * Returns get_sampler()->get_modified().
 */
INLINE UpdateSeq GltfTexture::
get_sampler_modified() const {
  return _sampler ? _sampler->get_modified() : UpdateSeq::initial();
}

/**
 * Returns true if there is a source image object used by this texture.
 */
INLINE bool GltfTexture::
has_source() const {
  return _source != nullptr;
}

/**
 * Gets a reference to the image data object that is sampled by this texture.
 * This may be null to indicate that the data is filled in using another way,
 * such as by dynamically generating its content via an extension.
 */
INLINE GltfImage *GltfTexture::
get_source() const {
  return _source;
}

/**
 * Sets a reference to the image data object that is sampled by this texture.
 */
INLINE void GltfTexture::
set_source(GltfImage *source) {
  _source = source;
}

/**
 * Removes the source image that is associated with this texture.
 */
INLINE void GltfTexture::
clear_source() {
  _source.clear();
}

/**
 * Returns get_source()->get_modified().
 */
INLINE UpdateSeq GltfTexture::
get_source_modified() const {
  return _source ? _source->get_modified() : UpdateSeq::initial();
}
