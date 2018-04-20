/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfMaterial.cxx
 * @author rdb
 * @date 2017-07-28
 */

#include "gltfMaterial.h"
#include "gltfParser.h"
#include "gltfWriter.h"

/**
 *
 */
GltfMaterial::
GltfMaterial() :
  _emissive_factor(0, 0, 0),
  _alpha_mode("OPAQUE"),
  _alpha_cutoff(0.5),
  _double_sided(false),
  _has_alpha_cutoff(false),
  _has_emissive_factor(false) {
}

/**
 *
 */
bool GltfMaterial::TextureInfo::
parse_property(GltfParser &parser, const string &key) {
  if (key == "index") {
    return parser.parse_value(_texture);
  }
  if (key == "texCoord") {
    return parser.parse_value(_tex_coord);
  }
  return GltfObject::parse_property(parser, key);
}

/**
 * Writes out the properties contained within this object to the writer.
 */
void GltfMaterial::TextureInfo::
write_properties(GltfWriter &writer) const {
  // Shouldn't write this object if it evaluates to false.
  nassertv(_texture != nullptr);

  writer.write_property("index", _texture);
  writer.write_property("texCoord", _tex_coord);
  GltfObject::write_properties(writer);
}

/**
 * Initializes the PBR settings of this material.
 */
GltfMaterial::PbrMetallicRoughness::
PbrMetallicRoughness() :
  _base_color_factor(1, 1, 1, 1),
  _metallic_factor(1.0),
  _roughness_factor(1.0) {
}

/**
 * Resets the PBR settings of this material.
 */
void GltfMaterial::PbrMetallicRoughness::
clear() {
  _base_color_factor.set(1, 1, 1, 1);
  _base_color_texture.clear();
  _metallic_factor = 1.0;
  _roughness_factor = 1.0;
  _metallic_roughness_texture.clear();
  GltfObject::clear();
}

/**
 *
 */
bool GltfMaterial::PbrMetallicRoughness::
parse_property(GltfParser &parser, const string &key) {
  if (key == "baseColorFactor") {
    return parser.parse_value(_base_color_factor);
  }
  if (key == "baseColorTexture") {
    return parser.parse_value(_base_color_texture);
  }
  if (key == "metallicFactor") {
    return parser.parse_value(_metallic_factor);
  }
  if (key == "roughnessFactor") {
    return parser.parse_value(_roughness_factor);
  }
  if (key == "metallicRoughnessTexture") {
    return parser.parse_value(_metallic_roughness_texture);
  }
  return GltfObject::parse_property(parser, key);
}

/**
 * Writes out the properties contained within this object to the writer.
 */
void GltfMaterial::PbrMetallicRoughness::
write_properties(GltfWriter &writer) const {
  writer.write_property("baseColorFactor", _base_color_factor);
  if (_base_color_texture) {
    writer.write_property("baseColorTexture", _base_color_texture);
  }
  writer.write_property("metallicFactor", _metallic_factor);
  writer.write_property("roughnessFactor", _roughness_factor);
  if (_metallic_roughness_texture) {
    writer.write_property("metallicRoughnessTexture", _metallic_roughness_texture);
  }
  GltfObject::write_properties(writer);
}

/**
 * Resets this object to its default state.
 */
void GltfMaterial::
clear() {
  _pbr_metallic_roughness.clear();
  _normal_texture.clear();
  _occlusion_texture.clear();
  _emissive_texture.clear();
  _emissive_factor.set(0, 0, 0);
  _alpha_mode = "OPAQUE";
  _alpha_cutoff = 0.5;
  _double_sided = false;
  _has_alpha_cutoff = false;
  _has_emissive_factor = false;
  GltfRootObject::clear();
}

/**
 *
 */
bool GltfMaterial::
parse_property(GltfParser &parser, const string &key) {
  if (key == "pbrMetallicRoughness") {
    return parser.parse_value(_pbr_metallic_roughness);
  }
  if (key == "normalTexture") {
    return parser.parse_value(_normal_texture);
  }
  if (key == "occlusionTexture") {
    return parser.parse_value(_occlusion_texture);
  }
  if (key == "emissiveTexture") {
    return parser.parse_value(_emissive_texture);
  }
  if (key == "emissiveFactor") {
    _has_emissive_factor = true;
    return parser.parse_value(_emissive_factor);
  }
  if (key == "alphaMode") {
    return parser.parse_value(_alpha_mode);
  }
  if (key == "alphaCutoff") {
    _has_alpha_cutoff = true;
    return parser.parse_value(_alpha_cutoff);
  }
  if (key == "doubleSided") {
    return parser.parse_value(_double_sided);
  }
  return GltfRootObject::parse_property(parser, key);
}

/**
 * Writes out the properties contained within this object to the writer.
 */
void GltfMaterial::
write_properties(GltfWriter &writer) const {
  if (_pbr_metallic_roughness._base_color_factor != LColord(1) ||
      _pbr_metallic_roughness._base_color_texture ||
      _pbr_metallic_roughness._metallic_factor != 1.0 ||
      _pbr_metallic_roughness._roughness_factor != 1.0 ||
      _pbr_metallic_roughness._metallic_roughness_texture ||
      !_pbr_metallic_roughness._extras.empty() ||
      !_pbr_metallic_roughness._extensions.empty()) {
    writer.write_property("pbrMetallicRoughness", _pbr_metallic_roughness);
  }
  if (_normal_texture) {
    writer.write_property("normalTexture", _normal_texture);
  }
  if (_occlusion_texture) {
    writer.write_property("occlusionTexture", _occlusion_texture);
  }
  if (_emissive_texture) {
    writer.write_property("emissiveTexture", _emissive_texture);
  }
  if (_has_emissive_factor) {
    writer.write_property("emissiveFactor", _emissive_factor);
  }
  if (!_alpha_mode.empty()) {
    writer.write_property("alphaMode", _alpha_mode);
    if (_has_alpha_cutoff) {
      writer.write_property("alphaCutoff", _alpha_cutoff);
    }
  }
  if (_double_sided) {
    writer.write_property("doubleSided", _double_sided);
  }
  GltfObject::write_properties(writer);
}

/**
 * Writes out a one-line description of this object.
 */
void GltfMaterial::
output(ostream &out) const {
  out << "GltfMaterial #" << get_index();
  if (has_name()) {
    out << " \"" << get_name() << "\"";
  }
}
