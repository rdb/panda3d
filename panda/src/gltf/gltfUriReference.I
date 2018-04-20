/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfUriReference.I
 * @author rdb
 * @date 2017-08-18
 */

/**
 * Creates an empty URI reference.
 */
INLINE GltfUriReference::
GltfUriReference() : _hier_start(0), _query_start(0), _fragment_start(0) {}

/**
 * Returns the number of characters in the URI.
 */
INLINE size_t GltfUriReference::
size() const {
  return _uri.size();
}

/**
 * Returns the nth character in the URI.
 */
INLINE char GltfUriReference::
operator [](size_t i) const {
  return _uri[i];
}

/**
 * Returns the URI reference as a string.
 */
INLINE GltfUriReference::
operator const string & () const {
  return _uri;
}

/**
 * Returns true if this URI reference is not empty.
 */
INLINE GltfUriReference::
operator bool () const {
  return !_uri.empty();
}

/**
 * Returns true if this URI reference could refer to a file.
 */
INLINE bool GltfUriReference::
has_filename() const {
  return !_uri.empty() &&
    (_hier_start == 0 || _uri.compare(0, _hier_start, "file:") == 0);
}

/**
 * Returns the resolved fullpath, if available, otherwise this is the same as
 * get_filename().
 */
INLINE Filename GltfUriReference::
get_fullpath() const {
  if (!_fullpath.empty()) {
    return _fullpath;
  } else {
    return get_filename();
  }
}

/**
 *
 */
INLINE bool GltfUriReference::
has_scheme() const {
  return _hier_start > 0;
}

/**
 *
 */
INLINE string GltfUriReference::
get_scheme() const {
  return _uri.substr(0, _hier_start - 1);
}

/**
 * Returns true if there is a query string in this URI reference.
 */
INLINE bool GltfUriReference::
has_query() const {
  return _query_start < _fragment_start;
}

/**
 * Return the query string part of the URI reference, without the '?'.
 */
INLINE string GltfUriReference::
get_query() const {
  return decode_part(_query_start + 1, _uri.size());
}

/**
 * Removes the query string part of the URI reference.
 */
INLINE void GltfUriReference::
clear_query() {
  _uri = _uri.substr(0, _query_start) + _uri.substr(_fragment_start);
  _fragment_start = _query_start;
}

/**
 * Returns true if the URI reference has a fragment part, starting with '#'.
 */
INLINE bool GltfUriReference::
has_fragment() const {
  return _fragment_start < _uri.size();
}

/**
 * Returns the fragment part of the URI reference, without the '#'.
 */
INLINE string GltfUriReference::
get_fragment() const {
  return decode_part(_fragment_start + 1, _uri.size());
}

/**
 * Removes the fragment part of the URI reference.
 */
INLINE void GltfUriReference::
clear_fragment() {
  _uri.resize(_fragment_start);
}
