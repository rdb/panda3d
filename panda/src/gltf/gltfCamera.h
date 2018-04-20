/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfCamera.h
 * @author rdb
 * @date 2017-08-03
 */

#ifndef GLTFCAMERA_H
#define GLTFCAMERA_H

#include "gltfRootObject.h"
#include "gltfBufferView.h"

/**
 * A camera's projection.  A node can reference a camera to apply a transform
 * to place the camera in the scene.
 */
class EXPCL_GLTF GltfCamera : public GltfRootObject {
PUBLISHED:
  class EXPCL_GLTF Orthographic : public GltfObject {
  PUBLISHED:
    INLINE Orthographic(double xmag, double ymag, double znear, double zfar);

  public:
    Orthographic() DEFAULT_CTOR;

    INLINE double get_xmag() const;
    INLINE void set_xmag(double xmag);
    INLINE double get_ymag() const;
    INLINE void set_ymag(double ymag);
    INLINE double get_znear() const;
    INLINE void set_znear(double znear);
    INLINE double get_zfar() const;
    INLINE void set_zfar(double zfar);

    virtual bool parse_property(GltfParser &parser, const string &key) override;
    virtual void write_properties(GltfWriter &writer) const override;

  PUBLISHED:
    MAKE_PROPERTY(xmag, get_xmag, set_xmag);
    MAKE_PROPERTY(ymag, get_ymag, set_ymag);
    MAKE_PROPERTY(znear, get_znear, set_znear);
    MAKE_PROPERTY(zfar, get_zfar, set_zfar);

  protected:
    double _xmag, _ymag;
    double _zfar, _znear;
  };

  class EXPCL_GLTF Perspective : public GltfObject {
  PUBLISHED:
    INLINE Perspective(double yfov, double znear, double zfar=make_inf(0.0));

  public:
    INLINE Perspective();

    INLINE bool has_aspect_ratio() const;
    INLINE double get_aspect_ratio() const;
    INLINE void set_aspect_ratio(double aspect_ratio);
    INLINE void clear_aspect_ratio();

    INLINE double get_yfov() const;
    INLINE void set_yfov(double yfov);
    INLINE double get_znear() const;
    INLINE void set_znear(double znear);
    INLINE double get_zfar() const;
    INLINE void set_zfar(double zfar);

    virtual bool parse_property(GltfParser &parser, const string &key) override;
    virtual void write_properties(GltfWriter &writer) const override;

  PUBLISHED:
    MAKE_PROPERTY2(aspect_ratio, has_aspect_ratio, get_aspect_ratio,
                                 set_aspect_ratio, clear_aspect_ratio);
    MAKE_PROPERTY(yfov, get_yfov, set_yfov);
    MAKE_PROPERTY(znear, get_znear, set_znear);
    MAKE_PROPERTY(zfar, get_zfar, set_zfar);

  protected:
    double _aspect_ratio;
    double _yfov;
    double _zfar, _znear;
  };

public:
  GltfCamera() DEFAULT_CTOR;

  INLINE const string &get_type() const;
  INLINE bool is_perspective() const;
  INLINE bool is_orthographic() const;

  const Orthographic &get_orthographic() const;
  Orthographic &get_orthographic();

  const Perspective &get_perspective() const;
  Perspective &get_perspective();

  virtual void clear() override;

  virtual bool parse_property(GltfParser &parser, const string &key) override;
  virtual void write_properties(GltfWriter &writer) const override;

  virtual void output(ostream &out) const override;

PUBLISHED:
  INLINE void set_orthographic(Orthographic orthographic);
  INLINE void set_perspective(Perspective perspective);

  MAKE_PROPERTY(type, get_type);
  MAKE_PROPERTY2(orthographic, is_orthographic, get_orthographic);
  MAKE_PROPERTY2(perspective, is_perspective, get_perspective);

protected:
  string _type;
  Orthographic _orthographic;
  Perspective _perspective;
};

#include "gltfCamera.I"

#endif
