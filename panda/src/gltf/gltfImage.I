/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfImage.I
 * @author rdb
 * @date 2017-09-03
 */

/**
 * Returns the URI referenced by this image.
 */
INLINE GltfUriReference &GltfImage::
get_uri() {
  return _uri;
}

/**
 * Returns the URI referenced by this image.
 */
INLINE const GltfUriReference &GltfImage::
get_uri() const {
  return _uri;
}

/**
 * Sets the URI that the image data should be read from.
 */
INLINE void GltfImage::
set_uri(const GltfUriReference &uri) {
  _uri = uri;
}

/**
 * Returns a MIME type if it has been specified, or the empty string.
 */
INLINE const string &GltfImage::
get_mime_type() const {
  return _mime_type;
}

/**
 * Sets the MIME type.  Required if the image is specified by a buffer view.
 * Otherwise, it can be left blank to infer from the file.
 */
INLINE void GltfImage::
set_mime_type(const string &mime_type) {
  _mime_type = mime_type;
}

/**
 * Returns the buffer view that the image data is read from, or nullptr if
 * none has been specified (in which case get_uri should return something)
 */
INLINE GltfBufferView *GltfImage::
get_buffer_view() const {
  return _buffer_view;
}

/**
 * Sets the buffer view the image data should be read from.  A MIME type
 * should also be specified.
 * @see set_mime_type
 */
INLINE void GltfImage::
set_buffer_view(GltfBufferView *view) {
  _buffer_view = view;
}
