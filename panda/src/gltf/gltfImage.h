/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfImage.h
 * @author rdb
 * @date 2017-08-02
 */

#ifndef GLTFIMAGE_H
#define GLTFIMAGE_H

#include "gltfRootObject.h"
#include "gltfBufferView.h"
#include "gltfUriReference.h"

/**
 * Image data used to create a texture.  Image can be referenced by URI or
 * `bufferView` index. `mimeType` is required in the latter case.
 */
class EXPCL_GLTF GltfImage : public GltfRootObject {
PUBLISHED:
  GltfImage();

  istream *open_read() const;

public:
  INLINE GltfUriReference &get_uri();
  INLINE const GltfUriReference &get_uri() const;
  INLINE void set_uri(const GltfUriReference &uri);
  INLINE const string &get_mime_type() const;
  INLINE void set_mime_type(const string &mime_type);
  INLINE GltfBufferView *get_buffer_view() const;
  INLINE void set_buffer_view(GltfBufferView *view);

  virtual void clear() override;

  virtual bool parse_property(GltfParser &parser, const string &key) override;
  virtual void write_properties(GltfWriter &writer) const override;

  virtual void output(ostream &out) const override;

PUBLISHED:
  MAKE_PROPERTY(uri, get_uri, set_uri);
  MAKE_PROPERTY(mime_type, get_mime_type, set_mime_type);
  MAKE_PROPERTY(buffer_view, get_buffer_view, set_buffer_view);

protected:
  GltfUriReference _uri;
  string _mime_type;
  PT(GltfBufferView) _buffer_view;
};

#include "gltfImage.I"

#endif
