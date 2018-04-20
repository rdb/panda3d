/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfObject.h
 * @author rdb
 * @date 2017-07-27
 */

#ifndef GLTFOBJECT_H
#define GLTFOBJECT_H

#include "config_gltf.h"
#include "gltfArray.h"
#include "gltfDictionary.h"
#include "gltfValue.h"
#include "updateSeq.h"

class GltfParser;
class GltfWriter;

/**
 * This is the base class of all glTF objects and contains properties common
 * to all glTF objects, such as extensions and extras.
 */
class EXPCL_GLTF GltfObject {
protected:
  GltfObject() DEFAULT_CTOR;

  INLINE virtual void clear();

public:
  typedef GltfValueDictionary Extras;
  typedef GltfValueDictionary Extension;
  typedef GltfDictionary<Extension> Extensions;

  bool has_extra(const string &) const;
  const GltfValue &get_extra(const string &) const;
  void set_extra(const string &, GltfValue value);
  void clear_extra(const string &);

  //const Extras &get_extras() const { return _extras; }
  //void set_extras(Extras extras) { _extras = move(extras); }
  void clear_extras();

  bool has_extension(const string &) const;
  const Extension &get_extension(const string &) const;
  Extension &get_extension(const string &);
  void set_extension(const string &, Extension extension);
  void clear_extension(const string &);
  void clear_extensions();

PUBLISHED:
  virtual void write(ostream &out, int indent_level = 0) const;
  void write_json(ostream &out, int indent_level = 0) const;

  MAKE_MAP_PROPERTY(extras, has_extra, get_extra, set_extra, clear_extra);
  MAKE_MAP_PROPERTY(extensions, has_extension, get_extension,
                                set_extension, clear_extension);

protected:
  virtual bool parse_property(GltfParser &parser, const string &key);
  virtual void write_properties(GltfWriter &writer) const;

protected:
  Extras _extras;
  Extensions _extensions;
  UpdateSeq _modified;

  friend class GltfParser;
  friend class GltfWriter;
};

#include "gltfObject.I"

#endif
