/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfRootObject.h
 * @author rdb
 * @date 2017-08-02
 */

#ifndef GLTFROOTOBJECT_H
#define GLTFROOTOBJECT_H

#include "gltfObject.h"
#include "referenceCount.h"
#include "pointerTo.h"

/**
 * This is the base class of all glTF objects that are stored in the root, and
 * are identified by a single integer index into the corresponding array on
 * the GltfRoot object.
 */
class EXPCL_GLTF GltfRootObject : public ReferenceCount, public GltfObject {
public:
  GltfRootObject(const string &name = string()) : _name(name) {}

  INLINE bool has_name() const;
  INLINE const string &get_name() const;
  INLINE void set_name(const string &name);
  INLINE size_t get_index() const;
  INLINE UpdateSeq get_modified() const;

PUBLISHED:
  INLINE void mark_modified();

  MAKE_PROPERTY(name, get_name, set_name);
  MAKE_PROPERTY(index, get_index);

public:
  virtual void clear() override;

  virtual bool parse_property(GltfParser &parser, const string &key) override;
  virtual void write_properties(GltfWriter &writer) const override;

  virtual void output(ostream &out) const=0;
  virtual void write(ostream &out, int indent_level = 0) const override;

private:
  string _name;
  size_t _index;
  UpdateSeq _modified;

  template<class T>
  friend class GltfRootArray;
};

INLINE ostream &operator << (ostream &out, const GltfRootObject &obj) {
  obj.output(out);
  return out;
}

#include "gltfRootObject.I"

#endif
