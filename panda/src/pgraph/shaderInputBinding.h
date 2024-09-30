/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file shaderInputBinding.h
 * @author rdb
 * @date 2024-09-22
 */

#ifndef SHADERINPUTBINDING_H
#define SHADERINPUTBINDING_H

#include "pandabase.h"
#include "referenceCount.h"
#include "internalName.h"
#include "shaderEnums.h"

class ShaderStateTracker;

/**
 * Controls how a shader parameter's value is filled in by Panda at runtime.
 */
class EXPCL_PANDA_PGRAPH ShaderInputBinding : public ReferenceCount {
protected:
  ShaderInputBinding() = default;

public:
  static ShaderInputBinding *make(const InternalName *name, const ::ShaderType *type,
                                  ShaderEnums::ShaderLanguage language);

  template<class Callable>
  INLINE static ShaderInputBinding *make_data(int dep, Callable callable);
  template<class Callable>
  INLINE static ShaderInputBinding *make_texture(int dep, Callable callable);
  template<class Callable>
  INLINE static ShaderInputBinding *make_texture_image(int dep, Callable callable);

  static ShaderInputBinding *make_shader_input(const ShaderType *type, CPT_InternalName name);

  static ShaderInputBinding *make_matrix(const ShaderType *type,
                                         ShaderEnums::StateMatrix input,
                                         CPT_InternalName arg,
                                         bool transpose = false,
                                         size_t offset = 0);
  static ShaderInputBinding *make_matrix_compose(const ShaderType *type,
                                                 ShaderEnums::StateMatrix input0,
                                                 CPT_InternalName arg0,
                                                 ShaderEnums::StateMatrix input1,
                                                 CPT_InternalName arg1,
                                                 bool transpose = false,
                                                 size_t offset = 0);

  static ShaderInputBinding *make_transform_table(const ShaderType *type, bool transpose);
  static ShaderInputBinding *make_slider_table(const ShaderType *type);

  static ShaderInputBinding *make_frame_time(const ShaderType *type);
  static ShaderInputBinding *make_color(const ShaderType *type);
  static ShaderInputBinding *make_color_scale(const ShaderType *type);
  static ShaderInputBinding *make_texture_stage(const ShaderType *type, size_t index);
  static ShaderInputBinding *make_texture_matrix(const ShaderType *type, size_t index,
                                                 bool inverse, bool transpose);
  static ShaderInputBinding *make_fog(const ShaderType *type);
  static ShaderInputBinding *make_material(const ShaderType *type);
  static ShaderInputBinding *make_light_ambient(const ShaderType *type);

  virtual int get_state_dep() const;
  virtual void setup(Shader *shader);

  virtual void fetch_data(const ShaderStateTracker &state, void *into,
                          bool pad_rows = false) const;

  typedef uintptr_t ResourceId;
  virtual ResourceId get_resource_id(int index, const ShaderType *type) const;
  virtual PT(Texture) fetch_texture(const ShaderStateTracker &state,
                                    ResourceId resource_id,
                                    SamplerState &sampler, int &view) const;
  virtual PT(Texture) fetch_texture_image(const ShaderStateTracker &state,
                                          ResourceId resource_id,
                                          ShaderType::Access &access,
                                          int &z, int &n) const;
};

/**
 * This binds a parameter to a specific transformation matrix or part thereof.
 */
class EXPCL_PANDA_PGRAPH ShaderMatrixBinding : public ShaderInputBinding {
public:
  INLINE ShaderMatrixBinding(ShaderEnums::StateMatrix input, CPT_InternalName arg,
                             bool transpose = false, size_t offset = 0,
                             size_t num_rows = 4, size_t num_cols = 4);

  virtual int get_state_dep() const override;
  virtual void setup(Shader *shader) override;

  virtual void fetch_data(const ShaderStateTracker &state, void *into, bool pad_rows) const override;

protected:
  size_t _cache_index = 0;
  ShaderEnums::StateMatrix _input;
  CPT_InternalName _arg;
  bool _transpose = false;
  size_t _offset = 0;
  size_t _num_rows;
  size_t _num_cols;
};

/**
 * This binds a parameter to a composition of two matrices.
 */
class EXPCL_PANDA_PGRAPH ShaderMatrixComposeBinding : public ShaderInputBinding {
public:
  INLINE ShaderMatrixComposeBinding(
    ShaderEnums::StateMatrix input0, CPT_InternalName arg0,
    ShaderEnums::StateMatrix input1, CPT_InternalName arg1,
    bool transpose = false, size_t offset = 0,
    size_t num_rows = 4, size_t num_cols = 4);

  virtual int get_state_dep() const override;
  virtual void setup(Shader *shader) override;

  virtual void fetch_data(const ShaderStateTracker &state, void *into, bool pad_rows) const override;

private:
  size_t _cache_index0 = 0;
  size_t _cache_index1 = 0;
  ShaderEnums::StateMatrix _input0;
  ShaderEnums::StateMatrix _input1;
  CPT_InternalName _arg0;
  CPT_InternalName _arg1;
  bool _transpose = false;
  size_t _offset = 0;
  size_t _num_rows;
  size_t _num_cols;
};

/**
 *
 */
class EXPCL_PANDA_PGRAPH ShaderPointParamsBinding : public ShaderInputBinding {
public:
  virtual int get_state_dep() const override;
  virtual void setup(Shader *shader) override;

  virtual void fetch_data(const ShaderStateTracker &state, void *into, bool pad_rows) const override;

protected:
  size_t _cache_index = 0;
};

/**
 *
 */
class EXPCL_PANDA_PGRAPH ShaderPackedLightBinding : public ShaderInputBinding {
public:
  INLINE ShaderPackedLightBinding(int index);

  virtual int get_state_dep() const override;
  virtual void setup(Shader *shader) override;

  virtual void fetch_data(const ShaderStateTracker &state, void *into, bool pad_rows) const override;

protected:
  size_t _index;
  size_t _world_mat_cache_index;
};

/**
 *
 */
class EXPCL_PANDA_PGRAPH ShaderLegacyLightBinding : public ShaderInputBinding {
public:
  INLINE ShaderLegacyLightBinding(CPT_InternalName input,
                                  ShaderEnums::StateMatrix matrix,
                                  CPT_InternalName arg);

  virtual int get_state_dep() const override;
  virtual void setup(Shader *shader) override;

protected:
  CPT_InternalName _input;
  ShaderEnums::StateMatrix _matrix;
  CPT_InternalName _arg;
  size_t _mat_cache_index;
};

/**
 *
 */
class EXPCL_PANDA_PGRAPH ShaderLegacyDirectionalLightBinding : public ShaderLegacyLightBinding {
public:
  using ShaderLegacyLightBinding::ShaderLegacyLightBinding;

  virtual void fetch_data(const ShaderStateTracker &state, void *into, bool pad_rows) const override;
};

/**
 *
 */
class EXPCL_PANDA_PGRAPH ShaderLegacyPointLightBinding : public ShaderLegacyLightBinding {
public:
  using ShaderLegacyLightBinding::ShaderLegacyLightBinding;

  virtual void fetch_data(const ShaderStateTracker &state, void *into, bool pad_rows) const override;
};

/**
 *
 */
class EXPCL_PANDA_PGRAPH ShaderLegacySpotlightBinding : public ShaderLegacyLightBinding {
public:
  using ShaderLegacyLightBinding::ShaderLegacyLightBinding;

  virtual void fetch_data(const ShaderStateTracker &state, void *into, bool pad_rows) const override;
};

/**
 *
 */
class EXPCL_PANDA_PGRAPH ShaderLightStructBinding : public ShaderInputBinding {
public:
  ShaderLightStructBinding(const ShaderType *type,
                           const InternalName *input = nullptr);

  virtual int get_state_dep() const override;
  virtual void setup(Shader *shader) override;

  virtual void fetch_data(const ShaderStateTracker &state, void *into, bool pad_rows) const override;

  virtual ResourceId get_resource_id(int index, const ShaderType *type) const;
  virtual PT(Texture) fetch_texture(const ShaderStateTracker &state,
                                    ResourceId index,
                                    SamplerState &sampler, int &view) const;

private:
  void fetch_light(const ShaderStateTracker &state, const NodePath &np, void *into) const;
  void fetch_from_input(const ShaderAttrib *target_shader, void *into) const;

protected:
  CPT(InternalName) _input;
  size_t _count;
  size_t _stride;
  size_t _world_to_view_mat_cache_index;
  size_t _apiview_to_world_mat_cache_index;
  bool _cube_shadow_map = false;
  int _color_offset = -1;
  int _specular_offset = -1;
  int _ambient_offset = -1;
  int _diffuse_offset = -1;
  int _position_offset = -1;
  int _half_vector_offset = -1;
  int _spot_direction_offset = -1;
  int _spot_cos_cutoff_offset = -1;
  int _spot_cutoff_offset = -1;
  int _spot_exponent_offset = -1;
  int _attenuation_offset = -1;
  int _constant_attenuation_offset = -1;
  int _linear_attenuation_offset = -1;
  int _quadratic_attenuation_offset = -1;
  int _radius_offset = -1;
  int _shadow_view_matrix_offset = -1;
};

/**
 * Binds a parameter to a texture stage.
 */
class EXPCL_PANDA_PGRAPH ShaderTextureStagesBinding : public ShaderInputBinding {
public:
  INLINE ShaderTextureStagesBinding(Texture::TextureType desired_type,
                                    size_t count,
                                    Texture *default_texture = nullptr,
                                    unsigned int mode_mask = ~0);

  virtual int get_state_dep() const override;

  virtual ResourceId get_resource_id(int index, const ShaderType *type) const;
  virtual PT(Texture) fetch_texture(const ShaderStateTracker &state,
                                    ResourceId resource_id,
                                    SamplerState &sampler, int &view) const;

protected:
  size_t const _count;
  Texture *const _default_texture;
  Texture::TextureType const _desired_type;
  unsigned int _mode_mask;
  mutable bool _shown_error = false;
};

/**
 * Binds a parameter to a generic texture shader input.
 */
class EXPCL_PANDA_PGRAPH ShaderTextureBinding : public ShaderInputBinding {
public:
  INLINE ShaderTextureBinding(CPT(InternalName) input, Texture::TextureType desired_type);

  virtual int get_state_dep() const override;

  virtual ResourceId get_resource_id(int index, const ShaderType *type) const;
  virtual PT(Texture) fetch_texture(const ShaderStateTracker &state,
                                    ResourceId resource_id,
                                    SamplerState &sampler, int &view) const;
  virtual PT(Texture) fetch_texture_image(const ShaderStateTracker &state,
                                          ResourceId resource_id,
                                          ShaderType::Access &access,
                                          int &z, int &n) const;

protected:
  CPT(InternalName) const _input;
  Texture::TextureType const _desired_type;
  mutable bool _shown_error = false;
};

/**
 * This binds a parameter to a generic numeric data shader input.
 */
class EXPCL_PANDA_PGRAPH ShaderDataBinding : public ShaderInputBinding {
public:
  INLINE ShaderDataBinding(CPT_InternalName input, size_t num_elements,
                           size_t num_rows, size_t num_cols);

  virtual int get_state_dep() const override;

  virtual void fetch_data(const ShaderStateTracker &state, void *into, bool pad_rows) const override=0;

protected:
  CPT_InternalName _input;
  size_t _num_elements;
  size_t _num_rows;
  size_t _num_cols;
};

/**
 * This binds a parameter to a single-precision float shader input.
 */
class EXPCL_PANDA_PGRAPH ShaderFloatBinding : public ShaderDataBinding {
public:
  using ShaderDataBinding::ShaderDataBinding;

  virtual void fetch_data(const ShaderStateTracker &state, void *into, bool pad_rows) const override;
};

/**
 * This binds a parameter to a double-precision float shader input.
 */
class EXPCL_PANDA_PGRAPH ShaderDoubleBinding : public ShaderDataBinding {
public:
  using ShaderDataBinding::ShaderDataBinding;

  virtual void fetch_data(const ShaderStateTracker &state, void *into, bool pad_rows) const override;
};

/**
 * This binds a parameter to an integer shader input.
 */
class EXPCL_PANDA_PGRAPH ShaderIntBinding : public ShaderDataBinding {
public:
  using ShaderDataBinding::ShaderDataBinding;

  virtual void fetch_data(const ShaderStateTracker &state, void *into, bool pad_rows) const override;
};

/**
 * This binds an aggregate parameter (such as a struct or an array of structs)
 * to a set of shader inputs.
 */
class EXPCL_PANDA_PGRAPH ShaderAggregateBinding : public ShaderInputBinding {
public:
  INLINE ShaderAggregateBinding(CPT_InternalName input, const ShaderType *type);

  virtual void fetch_data(const ShaderStateTracker &state, void *into, bool pad_rows) const override;

  virtual ResourceId get_resource_id(int index, const ShaderType *type) const;
  virtual PT(Texture) fetch_texture(const ShaderStateTracker &state,
                                    ResourceId index,
                                    SamplerState &sampler, int &view) const;
  virtual PT(Texture) fetch_texture_image(const ShaderStateTracker &state,
                                          ResourceId index,
                                          ShaderType::Access &access,
                                          int &z, int &n) const;
private:
  void r_collect_members(const InternalName *name, const ShaderType *type, size_t offset = 0);

private:
  struct DataMember {
    PT(ShaderDataBinding) _binding;
    size_t _offset;
  };
  pvector<DataMember> _data_members;
  pvector<CPT_InternalName> _resources;
};

#include "shaderInputBinding.I"

#endif
