/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfBuffer.h
 * @author rdb
 * @date 2017-07-28
 */

#ifndef GLTFBUFFER_H
#define GLTFBUFFER_H

#include "gltfRootObject.h"
#include "gltfUriReference.h"
#include "dSearchPath.h"
#include "vector_uchar.h"

class BamCacheRecord;

/**
 * A buffer points to binary geometry, animation, or skins.
 */
class EXPCL_GLTF GltfBuffer : public GltfRootObject {
public:
  GltfBuffer();

  bool load(const DSearchPath &searchpath = DSearchPath(),
            BamCacheRecord *record = nullptr);
  bool read(istream &in);

  INLINE bool has_uri() const;
  INLINE const GltfUriReference &get_uri() const;

  INLINE size_t get_byte_length() const;
  INLINE const unsigned char *get_data_pointer() const;

  virtual void clear() override;

  virtual bool parse_property(GltfParser &parser, const string &key) override;
  virtual void write_properties(GltfWriter &writer) const override;

  virtual void output(ostream &out) const override;

PUBLISHED:
  MAKE_PROPERTY2(uri, has_uri, get_uri);
  MAKE_PROPERTY(byte_length, get_byte_length);

protected:
  GltfUriReference _uri;
  unsigned int _byte_length;
  vector_uchar _data;
};

#include "gltfBuffer.I"

#endif
