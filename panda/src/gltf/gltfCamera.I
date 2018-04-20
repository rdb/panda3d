/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfCamera.I
 * @author rdb
 * @date 2017-09-09
 */

/**
 * Creates an orthographic lens with the given parameters.
 */
INLINE GltfCamera::Orthographic::
Orthographic(double xmag, double ymag, double znear, double zfar) :
  _xmag(xmag), _ymag(ymag), _znear(znear), _zfar(zfar) {
  nassertv(xmag != 0);
  nassertv(ymag != 0);
  nassertv(znear >= 0);
  nassertv(zfar > 0);
}

/**
 * The floating-point horizontal magnification of the view.  Must not be zero.
 */
INLINE double GltfCamera::Orthographic::
get_xmag() const {
  return _xmag;
}

/**
 * The floating-point horizontal magnification of the view.  Must not be zero.
 */
INLINE void GltfCamera::Orthographic::
set_xmag(double xmag) {
  nassertv(xmag != 0.0);
  _xmag = xmag;
}

/**
 * The floating-point horizontal magnification of the view.  Must not be zero.
 */
INLINE double GltfCamera::Orthographic::
get_ymag() const {
  return _ymag;
}

/**
 * The floating-point horizontal magnification of the view.  Must not be zero.
 */
INLINE void GltfCamera::Orthographic::
set_ymag(double ymag) {
  nassertv(ymag != 0.0);
  _ymag = ymag;
}

/**
 * Returns the near distance of this orthographic lens.
 */
INLINE double GltfCamera::Orthographic::
get_znear() const {
  return _znear;
}

/**
 * Sets the near distance of this orthographic lens.
 */
INLINE void GltfCamera::Orthographic::
set_znear(double znear) {
  nassertv(!cinf(znear));
  nassertv(znear >= 0);
  _znear = znear;
}

/**
 * Returns the far distance of this orthographic lens.  May not be infinity.
 */
INLINE double GltfCamera::Orthographic::
get_zfar() const {
  return _zfar;
}

/**
 * Sets the far distance of this orthographic lens.  May not be infinity.
 */
INLINE void GltfCamera::Orthographic::
set_zfar(double zfar) {
  nassertv(!cinf(zfar));
  nassertv(zfar > 0);
  _zfar = zfar;
}

/**
 * Creates a perspective lens with the given parameters.
 */
INLINE GltfCamera::Perspective::
Perspective(double yfov, double znear, double zfar) :
  _yfov(yfov), _znear(znear), _zfar(zfar) {
}

/**
 * Creates a perspective lens in the initial state.
 */
INLINE GltfCamera::Perspective::
Perspective() :
  _aspect_ratio(0.0), _yfov(0.0), _znear(0.0), _zfar(make_inf(0.0)) {
}

/**
 * Returns true if this perspective lens has an explicit aspect ratio setting,
 * false if should automatically be computed from the viewport.
 */
INLINE bool GltfCamera::Perspective::
has_aspect_ratio() const {
  return _aspect_ratio != 0.0;
}

/**
 * Returns the aspect ratio of this camera.  Only meaningful if
 * has_aspect_ratio() returns true.
 */
INLINE double GltfCamera::Perspective::
get_aspect_ratio() const {
  nassertr(has_aspect_ratio(), 0.0);
  return _aspect_ratio;
}

/**
 * Sets the aspect ratio of this camera.  If not specified, it will be
 * automatically computed from the viewport.
 */
INLINE void GltfCamera::Perspective::
set_aspect_ratio(double aspect_ratio) {
  nassertv(aspect_ratio > 0.0);
  _aspect_ratio = aspect_ratio;
}

/**
 * Removes the aspect ratio specification from this perspective lens.
 */
INLINE void GltfCamera::Perspective::
clear_aspect_ratio() {
  _aspect_ratio = 0.0;
}

/**
 * Returns the vertical field-of-view of this perspective lens.
 */
INLINE double GltfCamera::Perspective::
get_yfov() const {
  return _yfov;
}

/**
 * Sets the vertical field-of-view of this perspective lens.
 */
INLINE void GltfCamera::Perspective::
set_yfov(double yfov) {
  nassertv(yfov > 0.0);
  _yfov = yfov;
}

/**
 * Returns the near distance of this perspective lens.
 */
INLINE double GltfCamera::Perspective::
get_znear() const {
  return _znear;
}

/**
 * Sets the near distance of this perspective lens.  May not be infinity.
 */
INLINE void GltfCamera::Perspective::
set_znear(double znear) {
  nassertv(!cinf(znear));
  nassertv(znear > 0.0);
  _znear = znear;
}

/**
 * Returns the far distance of this perspective lens.  May be infinity.
 */
INLINE double GltfCamera::Perspective::
get_zfar() const {
  return _zfar;
}

/**
 * Sets the far distance of this perspective lens.  May be infinity.
 */
INLINE void GltfCamera::Perspective::
set_zfar(double zfar) {
  nassertv(zfar > 0.0);
  _zfar = zfar;
}

/**
 * Returns the type of this camera, "orthographic" or "perspective".
 */
INLINE const string &GltfCamera::
get_type() const {
  return _type;
}

/**
 * Returns true if this is an orthographic camera.
 * @see get_orthographic()
 */
INLINE bool GltfCamera::
is_orthographic() const {
  return _type == "orthographic";
}

/**
 * Returns true if this is a perspective camera.
 * @see get_perspective()
 */
INLINE bool GltfCamera::
is_perspective() const {
  return _type == "perspective";
}

/**
 * Sets this to be an orthographic camera using the given properties.
 */
INLINE void GltfCamera::
set_orthographic(Orthographic orthographic) {
  _type = "orthographic";
  _orthographic = move(orthographic);
}

/**
 * Sets this to be a perspective camera using the given properties.
 */
INLINE void GltfCamera::
set_perspective(Perspective perspective) {
  _type = "perspective";
  _perspective = move(perspective);
}
