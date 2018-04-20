/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfArray.h
 * @author rdb
 * @date 2017-07-27
 */

#ifndef GLTFARRAY_H
#define GLTFARRAY_H

#include "config_gltf.h"
#include "pvector.h"
#include "pointerTo.h"

class GltfParser;
class GltfWriter;

/**
 * Abstract base class for GltfArray.
 */
class EXPCL_GLTF GltfArrayBase {
public:
  virtual void clear()=0;
  virtual size_t size() const=0;

protected:
  virtual bool parse_element(GltfParser &parser, size_t i)=0;
  virtual void write_element(GltfWriter &writer, size_t i) const=0;

  friend class GltfParser;
  friend class GltfWriter;
};

/**
 * Contains metadata about the glTF array.
 */
template<class T>
class EXPCL_GLTF GltfArray : public GltfArrayBase, public pvector<T> {
public:
  virtual void clear() override;
  virtual size_t size() const FINAL override { return pvector<T>::size(); }

  INLINE iterator find(const T &val);
  INLINE const_iterator find(const T &val) const;

protected:
  virtual bool parse_element(GltfParser &parser, size_t i) override;
  virtual void write_element(GltfWriter &writer, size_t i) const override;
};

/**
 * Same as GltfArray except allocates as pointers.
 */
template<class T>
class EXPCL_GLTF GltfRootArray : public GltfArrayBase, public pvector<PointerTo<T> > {
public:
  virtual void clear() override;
  virtual size_t size() const FINAL override { return pvector<PointerTo<T> >::size(); }

  T *make(const string &name = string());
  T *resolve_id(size_t i);

  size_t garbage_collect();

protected:
  virtual bool parse_element(GltfParser &parser, size_t i) override;
  virtual void write_element(GltfWriter &writer, size_t i) const override;
};

#include "gltfArray.I"

#endif
