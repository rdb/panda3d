/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfUriReference.h
 * @author rdb
 * @date 2017-08-18
 */

#ifndef GLTFURIREFERENCE_H
#define GLTFURIREFERENCE_H

#include "gltfObject.h"
#include "dSearchPath.h"

class BamCacheRecord;

/**
 * A glTF URI reference, as per RFC 3986, section 4.2.  Note that this need
 * not be a complete URI.
 */
class EXPCL_GLTF GltfUriReference {
PUBLISHED:
  INLINE GltfUriReference();
  GltfUriReference(const string &uri);
  explicit GltfUriReference(const Filename &fn);

  istream *open_read() const;
  string read() const;

  INLINE size_t size() const;
  INLINE char operator [](size_t i) const;
  INLINE operator const string & () const;
  INLINE operator bool () const;

public:
  bool resolve(const DSearchPath &search_path = DSearchPath(),
               BamCacheRecord *record = nullptr);

  INLINE bool has_filename() const;
  Filename get_filename() const;
  INLINE Filename get_fullpath() const;

  INLINE bool has_scheme() const;
  INLINE string get_scheme() const;

  /*
  INLINE bool has_authority() const;
  INLINE string get_authority() const;
  */

  INLINE bool has_query() const;
  INLINE string get_query() const;
  void set_query(const string &query);
  INLINE void clear_query();

  INLINE bool has_fragment() const;
  INLINE string get_fragment() const;
  void set_fragment(const string &fragment);
  INLINE void clear_fragment();

private:
  string decode_part(size_t begin, size_t end) const;
  static string encode(const string &input);

PUBLISHED:
  MAKE_PROPERTY2(filename, has_filename, get_filename);
  MAKE_PROPERTY2(scheme, has_scheme, get_scheme);
  MAKE_PROPERTY2(query, has_query, get_query, set_query, clear_query);
  MAKE_PROPERTY2(fragment, has_fragment, get_fragment, set_fragment, clear_fragment);

protected:
  string _uri;

  size_t _hier_start;
  size_t _query_start;
  size_t _fragment_start;

  Filename _fullpath;
};

EXPCL_GLTF INLINE ostream &
operator << (ostream &out, const GltfUriReference &uri) {
  const string &str = uri;
  if (str.size() > 63) {
    out << str.substr(0, 60) << "...";
  } else {
    out << str;
  }
  return out;
}

#include "gltfUriReference.I"

#endif
