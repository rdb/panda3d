/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfMaterial.h
 * @author rdb
 * @date 2017-07-28
 */

#ifndef GLTFMATERIAL_H
#define GLTFMATERIAL_H

#include "gltfRootObject.h"
#include "gltfTexture.h"
#include "luse.h"

/**
 *
 */
class EXPCL_GLTF GltfMaterial : public GltfRootObject {
PUBLISHED:
  GltfMaterial();

  /**
   * A texture/texcoord pair.
   */
  class EXPCL_GLTF TextureInfo : public GltfObject {
  PUBLISHED:
    INLINE TextureInfo();
    INLINE void clear();

  public:
    INLINE GltfTexture *get_texture() const;
    INLINE void set_texture(GltfTexture *texture);
    INLINE unsigned int get_tex_coord() const;
    INLINE void set_tex_coord(int tex_coord);

    INLINE operator const GltfTexture *() const;
    INLINE operator bool() const;

    virtual bool parse_property(GltfParser &parser, const string &key) override;
    virtual void write_properties(GltfWriter &writer) const override;

  PUBLISHED:
    MAKE_PROPERTY(texture, get_texture, set_texture);
    MAKE_PROPERTY(tex_coord, get_tex_coord, set_tex_coord);

  protected:
    PT(GltfTexture) _texture;
    unsigned int _tex_coord;
  };

  class EXPCL_GLTF NormalTextureInfo : public TextureInfo {
  };

  class EXPCL_GLTF OcclusionTextureInfo : public TextureInfo {
  };

  /**
   * Stores PBR material properties and textures.
   */
  class EXPCL_GLTF PbrMetallicRoughness : public GltfObject {
  PUBLISHED:
    PbrMetallicRoughness();
    void clear();

  public:
    INLINE const LColord &get_base_color_factor() const;
    INLINE void set_base_color_factor(const LColord &factor);
    INLINE const TextureInfo &get_base_color_texture() const;
    INLINE TextureInfo &get_base_color_texture();

    INLINE double get_metallic_factor() const;
    INLINE void set_metallic_factor(double factor);
    INLINE double get_roughness_factor() const;
    INLINE void set_roughness_factor(double factor);
    INLINE const TextureInfo &get_metallic_roughness_texture() const;
    INLINE TextureInfo &get_metallic_roughness_texture();

    virtual bool parse_property(GltfParser &parser, const string &key) override;
    virtual void write_properties(GltfWriter &writer) const override;

  PUBLISHED:
    MAKE_PROPERTY(base_color_factor, get_base_color_factor, set_base_color_factor);
    MAKE_PROPERTY(base_color_texture, get_base_color_texture);
    MAKE_PROPERTY(metallic_factor, get_metallic_factor, set_metallic_factor);
    MAKE_PROPERTY(roughness_factor, get_roughness_factor, set_roughness_factor);
    MAKE_PROPERTY(metallic_roughness_texture, get_metallic_roughness_texture);

  protected:
    LColord _base_color_factor;
    TextureInfo _base_color_texture;
    double _metallic_factor;
    double _roughness_factor;
    TextureInfo _metallic_roughness_texture;

    friend class GltfMaterial;
  };

public:
  INLINE PbrMetallicRoughness &get_pbr_metallic_roughness();
  INLINE const PbrMetallicRoughness &get_pbr_metallic_roughness() const;
  INLINE NormalTextureInfo &get_normal_texture();
  INLINE const NormalTextureInfo &get_normal_texture() const;
  INLINE OcclusionTextureInfo &get_occlusion_texture();
  INLINE const OcclusionTextureInfo &get_occlusion_texture() const;
  INLINE TextureInfo &get_emissive_texture();
  INLINE const TextureInfo &get_emissive_texture() const;
  INLINE bool has_emissive_factor() const;
  INLINE const LRGBColord &get_emissive_factor() const;
  INLINE void set_emissive_factor(const LRGBColord &factor);

  INLINE const string &get_alpha_mode() const;
  INLINE void set_alpha_mode(const string &mode);
  INLINE bool has_alpha_cutoff() const;
  INLINE double get_alpha_cutoff() const;
  INLINE void set_alpha_cutoff(double cutoff);

  INLINE bool get_double_sided() const;
  INLINE void set_double_sided(bool double_sided);

  virtual void clear() override;

  virtual bool parse_property(GltfParser &parser, const string &key) override;
  virtual void write_properties(GltfWriter &writer) const override;

  virtual void output(ostream &out) const override;

PUBLISHED:
  MAKE_PROPERTY(pbr_metallic_roughness, get_pbr_metallic_roughness);
  MAKE_PROPERTY(normal_texture, get_normal_texture);
  MAKE_PROPERTY(occlusion_texture, get_occlusion_texture);
  MAKE_PROPERTY(emissive_texture, get_emissive_texture);
  MAKE_PROPERTY(emissive_factor, get_emissive_factor, set_emissive_factor);

  MAKE_PROPERTY(alpha_mode, get_alpha_mode, set_alpha_mode);
  MAKE_PROPERTY(alpha_cutoff, get_alpha_cutoff, set_alpha_cutoff);
  MAKE_PROPERTY(double_sided, get_double_sided, set_double_sided);

protected:
  PbrMetallicRoughness _pbr_metallic_roughness;
  NormalTextureInfo _normal_texture;
  OcclusionTextureInfo _occlusion_texture;
  TextureInfo _emissive_texture;
  LRGBColord _emissive_factor;
  string _alpha_mode;
  double _alpha_cutoff;
  bool _double_sided;
  bool _has_alpha_cutoff;
  bool _has_emissive_factor;
};

#include "gltfMaterial.I"

#endif
