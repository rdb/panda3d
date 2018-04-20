/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfAnimation.h
 * @author rdb
 * @date 2017-09-02
 */

#ifndef GLTFANIMATION_H
#define GLTFANIMATION_H

#include "gltfRootObject.h"
#include "gltfAccessor.h"
#include "gltfArray.h"
#include "gltfNode.h"

/**
 * A keyframe animation.
 */
class EXPCL_GLTF GltfAnimation : public GltfRootObject {
PUBLISHED:
  GltfAnimation();

  /**
   * Combines input and output accessors with an interpolation algorithm to
   * define a keyframe graph (but not its target).
   */
  class EXPCL_GLTF Sampler : public GltfObject, public ReferenceCount {
  PUBLISHED:
    INLINE explicit Sampler(GltfAccessor *input, GltfAccessor *output);

    PT(GltfAccessor) input, output;
    string interpolation;

  public:
    INLINE Sampler();
    INLINE size_t get_index() const;

    virtual bool parse_property(GltfParser &parser, const string &key) override;
    virtual void write_properties(GltfWriter &writer) const override;

  private:
    unsigned int _index;

    template<class T>
    friend class GltfRootArray;
  };

  /**
   * Targets an animation's sampler at a node's property.
   */
  class EXPCL_GLTF Channel : public GltfObject {
  PUBLISHED:
    class EXPCL_GLTF Target : public GltfObject {
    PUBLISHED:
      INLINE Target();

      PT(GltfNode) node;
      string path;

    public:
      virtual bool parse_property(GltfParser &parser, const string &key) override;
      virtual void write_properties(GltfWriter &writer) const override;
    };

    PT(Sampler) sampler;
    Target target;

  public:
    INLINE Channel();

    virtual bool parse_property(GltfParser &parser, const string &key) override;
    virtual void write_properties(GltfWriter &writer) const override;
  };

  //void add_channel(Sampler *sampler, Channel::Target target);

public:
  virtual void clear() override;

  virtual bool parse_property(GltfParser &parser, const string &key) override;
  virtual void write_properties(GltfWriter &writer) const override;

  virtual void output(ostream &out) const override;

protected:
  GltfArray<Channel> _channels;
  GltfRootArray<Sampler> _samplers;

  friend class Channel;
};

#include "gltfAnimation.I"

#endif
