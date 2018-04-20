/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfNode.cxx
 * @author rdb
 * @date 2017-07-27
 */

#include "gltfNode.h"
#include "gltfParser.h"
#include "gltfWriter.h"

/**
 * Returns the transformation matrix.  This will return a valid matrix even if
 * has_matrix() returns false.
 */
const LMatrix4d &GltfNode::
get_matrix() const {
  if (!has_matrix()) {
    LVecBase3d hpr = _rotation.get_hpr(CS_yup_right);
    compose_matrix(_matrix, _scale, hpr, _translation, CS_yup_right);
  }
  return _matrix;
}

/**
 * If the transform was specified as a matrix, it will instead be broken up
 * into its constituent components.  Returns true if the transform could be
 * completely represented via the three components, false otherwise.
 */
bool GltfNode::
decompose_matrix() {
  if (_flags == 0) {
    // No transform, this is easy.
    return true;
  }

  nassertr_always(has_matrix(), false);

  _flags = 0;
  LVecBase3d hpr;
  bool result = ::decompose_matrix(_matrix, _scale, hpr, _translation, CS_yup_right);

  if (_scale != LVecBase3d::zero()) {
    _flags |= F_has_scale;
  }

  if (hpr != LVecBase3d::zero()) {
    _flags |= F_has_rotation;
    _rotation.set_hpr(hpr, CS_yup_right);
  }

  if (_translation != LPoint3d::zero()) {
    _flags |= F_has_translation;
  }

  _matrix = LMatrix4d::ident_mat();
  _flags &= ~F_has_matrix;
  return result;
}

/**
 * Resets this object to its default state.
 */
void GltfNode::
clear() {
  _children.clear();
  _camera.clear();
  _mesh.clear();
  _skin.clear();
  _weights.clear();

  _flags = 0;
  _translation.fill(0);
  _scale.fill(1);
  _rotation = LQuaterniond::ident_quat();

  GltfRootObject::clear();
}

/**
 *
 */
bool GltfNode::
parse_property(GltfParser &parser, const string &key) {
  if (key == "camera") {
    return parser.parse_value(_camera);
  }
  if (key == "children") {
    return parser.parse_value(_children);
  }
  if (key == "matrix") {
    _flags |= F_has_matrix;
    return parser.parse_value(_matrix);
  }
  if (key == "mesh") {
    return parser.parse_value(_mesh);
  }
  if (key == "skin") {
    return parser.parse_value((PointerTo<GltfSkin> &)_skin);
  }
  if (key == "weights") {
    return parser.parse_value(_weights);
  }
  if (key == "rotation") {
    _flags |= F_has_rotation;
    return parser.parse_value(_rotation);
  }
  if (key == "scale") {
    _flags |= F_has_scale;
    return parser.parse_value(_scale);
  }
  if (key == "translation") {
    _flags |= F_has_translation;
    return parser.parse_value(_translation);
  }
  return GltfRootObject::parse_property(parser, key);
}

/**
 * Writes out the properties contained within this object to the writer.
 */
void GltfNode::
write_properties(GltfWriter &writer) const {
  writer.write_property("camera", _camera);
  writer.write_property("children", _children);
  if (has_mesh()) {
    writer.write_property("mesh", _mesh);
    writer.write_property("skin", _skin);
    writer.write_property("weights", _weights);
  }
  if (has_matrix()) {
    writer.write_property("matrix", _matrix);
  } else {
    if (has_translation()) {
      writer.write_property("translation", _translation);
    }
    if (has_scale()) {
      writer.write_property("scale", _scale);
    }
    if (has_rotation()) {
      writer.write_property("rotation", _rotation);
    }
  }
  GltfRootObject::write_properties(writer);
}

/**
 * Writes out a one-line description of this object.
 */
void GltfNode::
output(ostream &out) const {
  out << "GltfNode #" << get_index();
  if (has_name()) {
    out << " \"" << get_name() << "\"";
  }
  if (has_matrix()) {
    out << ", matrix";
  } else if (has_translation() || has_scale() || has_rotation()) {
    out << ", TRS";
  }
  out << ", " << _children.size() << " children";
}
