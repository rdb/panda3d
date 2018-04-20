/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfAnimation.I
 * @author rdb
 * @date 2017-09-03
 */

/**
 *
 */
INLINE GltfAnimation::Sampler::
Sampler(GltfAccessor *input, GltfAccessor *output) :
  input(input),
  interpolation("LINEAR"),
  output(output) {
}

/**
 *
 */
INLINE GltfAnimation::Sampler::
Sampler() :
  input(nullptr),
  interpolation("LINEAR"),
  output(nullptr) {
}

/**
 * Returns the index of this sampler in the animation.
 */
INLINE size_t GltfAnimation::Sampler::
get_index() const {
  return _index;
}

/**
 *
 */
INLINE GltfAnimation::Channel::Target::
Target() : node(nullptr), path() {
}

/**
 *
 */
INLINE GltfAnimation::Channel::
Channel() : sampler(0) {
}
