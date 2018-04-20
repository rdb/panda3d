/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfMaterial.I
 * @author rdb
 * @date 2017-10-01
 */

/**
 *
 */
INLINE GltfMaterial::TextureInfo::
TextureInfo() : _tex_coord(0) {
}

/**
 *
 */
INLINE void GltfMaterial::TextureInfo::
clear() {
  _texture.clear();
  _tex_coord = 0;
  GltfObject::clear();
}

/**
 *
 */
INLINE GltfTexture *GltfMaterial::TextureInfo::
get_texture() const {
  return _texture;
}

/**
 *
 */
INLINE void GltfMaterial::TextureInfo::
set_texture(GltfTexture *texture) {
  _texture = texture;
}

/**
 *
 */
INLINE unsigned int GltfMaterial::TextureInfo::
get_tex_coord() const {
  return _tex_coord;
}

/**
 *
 */
INLINE void GltfMaterial::TextureInfo::
set_tex_coord(int tex_coord) {
  _tex_coord = tex_coord;
}

/**
 *
 */
INLINE GltfMaterial::TextureInfo::
operator const GltfTexture *() const {
  return _texture.p();
}

/**
 *
 */
INLINE GltfMaterial::TextureInfo::
operator bool() const {
  return !_texture.is_null();
}

/**
 *
 */
INLINE const LColord &GltfMaterial::PbrMetallicRoughness::
get_base_color_factor() const {
  return _base_color_factor;
}

/**
 *
 */
INLINE void GltfMaterial::PbrMetallicRoughness::
set_base_color_factor(const LColord &factor) {
  _base_color_factor = factor;
}

/**
 *
 */
INLINE const GltfMaterial::TextureInfo &GltfMaterial::PbrMetallicRoughness::
get_base_color_texture() const {
  return _base_color_texture;
}

/**
 *
 */
INLINE GltfMaterial::TextureInfo &GltfMaterial::PbrMetallicRoughness::
get_base_color_texture() {
  return _base_color_texture;
}

/**
 *
 */
INLINE double GltfMaterial::PbrMetallicRoughness::
get_metallic_factor() const {
  return _metallic_factor;
}

/**
 * Sets the metallic factor.  This will be multiplied with the texture value
 * specified by metallic_roughness_texture.
 */
INLINE void GltfMaterial::PbrMetallicRoughness::
set_metallic_factor(double factor) {
  _metallic_factor = factor;
}

/**
 *
 */
INLINE double GltfMaterial::PbrMetallicRoughness::
get_roughness_factor() const {
  return _roughness_factor;
}

/**
 * Sets the roughness factor.  This will be multiplied with the texture value
 * specified by metallic_roughness_texture.
 */
INLINE void GltfMaterial::PbrMetallicRoughness::
set_roughness_factor(double factor) {
  _roughness_factor = factor;
}

/**
 *
 */
INLINE const GltfMaterial::TextureInfo &GltfMaterial::PbrMetallicRoughness::
get_metallic_roughness_texture() const {
  return _metallic_roughness_texture;
}

/**
 *
 */
INLINE GltfMaterial::TextureInfo &GltfMaterial::PbrMetallicRoughness::
get_metallic_roughness_texture() {
  return _metallic_roughness_texture;
}

/**
 * The pbr_metallic_roughness structure contains textures and factors to
 * describe the PBR material in terms of base_color, metallic and roughness.
 */
INLINE GltfMaterial::PbrMetallicRoughness &GltfMaterial::
get_pbr_metallic_roughness() {
  return _pbr_metallic_roughness;
}

/**
 * The pbr_metallic_roughness structure contains textures and factors to
 * describe the PBR material in terms of base_color, metallic and roughness.
 */
INLINE const GltfMaterial::PbrMetallicRoughness &GltfMaterial::
get_pbr_metallic_roughness() const {
  return _pbr_metallic_roughness;
}

/**
 *
 */
INLINE GltfMaterial::NormalTextureInfo &GltfMaterial::
get_normal_texture() {
  return _normal_texture;
}

/**
 *
 */
INLINE const GltfMaterial::NormalTextureInfo &GltfMaterial::
get_normal_texture() const {
  return _normal_texture;
}

/**
 *
 */
INLINE GltfMaterial::OcclusionTextureInfo &GltfMaterial::
get_occlusion_texture() {
  return _occlusion_texture;
}

/**
 *
 */
INLINE const GltfMaterial::OcclusionTextureInfo &GltfMaterial::
get_occlusion_texture() const {
  return _occlusion_texture;
}

/**
 *
 */
INLINE GltfMaterial::TextureInfo &GltfMaterial::
get_emissive_texture() {
  return _emissive_texture;
}

/**
 *
 */
INLINE const GltfMaterial::TextureInfo &GltfMaterial::
get_emissive_texture() const {
  return _emissive_texture;
}

/**
 *
 */
INLINE bool GltfMaterial::
has_emissive_factor() const {
  return _has_emissive_factor;
}

/**
 *
 */
INLINE const LRGBColord &GltfMaterial::
get_emissive_factor() const {
  return _emissive_factor;
}

/**
 *
 */
INLINE void GltfMaterial::
set_emissive_factor(const LRGBColord &factor) {
  _emissive_factor = factor;
  _has_emissive_factor = true;
}

/**
 * Returns the alpha mode, which should be one of "OPAQUE", "MASK" or "BLEND".
 * The default value is "OPAQUE".
 */
INLINE const string &GltfMaterial::
get_alpha_mode() const {
  return _alpha_mode;
}

/**
 * Sets the alpha mode, which should be one of "OPAQUE", "MASK" or "BLEND".
 * The default value is "OPAQUE".
 */
INLINE void GltfMaterial::
set_alpha_mode(const string &mode) {
  _alpha_mode = mode;
}

/**
 *
 */
INLINE bool GltfMaterial::
has_alpha_cutoff() const {
  return _has_alpha_cutoff;
}

/**
 *
 */
INLINE double GltfMaterial::
get_alpha_cutoff() const {
  return _alpha_cutoff;
}

/**
 *
 */
INLINE void GltfMaterial::
set_alpha_cutoff(double cutoff) {
  _alpha_cutoff = cutoff;
  _has_alpha_cutoff = true;
}

/**
 *
 */
INLINE bool GltfMaterial::
get_double_sided() const {
  return _double_sided;
}

/**
 *
 */
INLINE void GltfMaterial::
set_double_sided(bool double_sided) {
  _double_sided = double_sided;
}
