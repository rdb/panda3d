/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfBuffer.I
 * @author rdb
 * @date 2017-10-31
 */

/**
 * Returns true if this buffer has a URI.
 */
INLINE bool GltfBuffer::
has_uri() const {
  return !!_uri;
}

/**
 * This may be empty.
 */
INLINE const GltfUriReference &GltfBuffer::
get_uri() const {
  return _uri;
}

/**
 * Returns the size in bytes of this buffer.
 */
INLINE size_t GltfBuffer::
get_byte_length() const {
  return _byte_length;
}

INLINE const unsigned char *GltfBuffer::
get_data_pointer() const {
  return &_data[0];
}
