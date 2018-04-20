/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfAsset.I
 * @author rdb
 * @date 2017-09-02
 */

/**
 *
 */
INLINE const string &GltfAsset::
get_copyright() const {
  return _copyright;
}

/**
 *
 */
INLINE void GltfAsset::
set_copyright(const string &copyright) {
  _copyright = copyright;
}

/**
 *
 */
INLINE const string &GltfAsset::
get_generator() const {
  return _generator;
}

/**
 *
 */
INLINE void GltfAsset::
set_generator(const string &generator) {
  _generator = generator;
}

/**
 *
 */
INLINE const string &GltfAsset::
get_version() const {
  return _version;
}

/**
 *
 */
INLINE void GltfAsset::
set_version(const string &version) {
  nassertv(check_version(version));
  _version = version;
}

/**
 *
 */
INLINE const string &GltfAsset::
get_min_version() const {
  return _min_version;
}

/**
 *
 */
INLINE void GltfAsset::
set_min_version(const string &min_version) {
  nassertv(min_version.empty() || check_version(min_version));
  _min_version = min_version;
}
