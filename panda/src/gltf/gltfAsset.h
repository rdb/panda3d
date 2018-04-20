/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfAsset.h
 * @author rdb
 * @date 2017-07-27
 */

#ifndef GLTFASSET_H
#define GLTFASSET_H

#include "gltfObject.h"

/**
 * Contains metadata about the glTF asset.
 */
class EXPCL_GLTF GltfAsset : public GltfObject {
public:
  GltfAsset();

  INLINE const string &get_copyright() const;
  INLINE void set_copyright(const string &copyright);
  INLINE const string &get_generator() const;
  INLINE void set_generator(const string &generator);
  INLINE const string &get_version() const;
  INLINE void set_version(const string &version);
  INLINE const string &get_min_version() const;
  INLINE void set_min_version(const string &min_version);

  virtual bool parse_property(GltfParser &parser, const string &key) override;
  virtual void write_properties(GltfWriter &writer) const override;

private:
  static bool check_version(const string &version);

PUBLISHED:
  MAKE_PROPERTY(copyright, get_copyright, set_copyright);
  MAKE_PROPERTY(generator, get_generator, set_generator);
  MAKE_PROPERTY(version, get_version, set_version);
  MAKE_PROPERTY(min_version, get_min_version, set_min_version);

protected:
  string _copyright;
  string _generator;
  string _version;
  string _min_version;
};

#include "gltfAsset.I"

#endif
