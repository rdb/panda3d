/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file shaderStateTracker.cxx
 * @author rdb
 * @date 2024-09-22
 */

#include "shaderStateTracker.h"
#include "colorAttrib.h"
#include "lightAttrib.h"
#include "materialAttrib.h"

/**
 * Sets up the state tracker to work for the given shader.
 */
void ShaderStateTracker::
setup(GraphicsStateGuardianBase *gsg, Shader *shader) {
  _gsg = gsg;
  _matrix_cache = pvector<LMatrix4>(shader->_matrix_cache_desc.size(), LMatrix4::ident_mat());
  _matrix_cache_deps = shader->_matrix_cache_deps;
}

/**
 * Updates the state and returns a mask of which attributes have changed.
 */
int ShaderStateTracker::
update(const RenderState *target_rs,
       const ShaderAttrib *target_shader,
       const TransformState *modelview_transform,
       const TransformState *camera_transform,
       const TransformState *projection_transform) {

  // Find out which state properties have changed.
  int altered = 0;

  if (_modelview_transform != modelview_transform) {
    _modelview_transform = modelview_transform;
    altered |= D_transform;
  }
  if (_camera_transform != camera_transform) {
    _camera_transform = camera_transform;
    altered |= D_view_transform;
  }
  if (_projection_transform != projection_transform) {
    _projection_transform = projection_transform;
    altered |= D_projection;
  }

  CPT(RenderState) state_rs = _state_rs.lock();
  if (state_rs == nullptr) {
    // Reset all of the state.
    altered |= D_state;
    _state_rs = target_rs;
    target_rs->get_attrib_def(_color_attrib);
  }
  else if (state_rs != target_rs) {
    // The state has changed since last time.
    if (state_rs->get_attrib(ColorAttrib::get_class_slot()) !=
        target_rs->get_attrib(ColorAttrib::get_class_slot())) {
      altered |= D_color;
      target_rs->get_attrib_def(_color_attrib);
    }
    if (state_rs->get_attrib(ColorScaleAttrib::get_class_slot()) !=
        target_rs->get_attrib(ColorScaleAttrib::get_class_slot())) {
      altered |= D_colorscale;
    }
    if (state_rs->get_attrib(MaterialAttrib::get_class_slot()) !=
        target_rs->get_attrib(MaterialAttrib::get_class_slot())) {
      altered |= D_material;
    }
    if (state_rs->get_attrib(FogAttrib::get_class_slot()) !=
        target_rs->get_attrib(FogAttrib::get_class_slot())) {
      altered |= D_fog;
    }
    if (state_rs->get_attrib(LightAttrib::get_class_slot()) !=
        target_rs->get_attrib(LightAttrib::get_class_slot())) {
      altered |= D_light;
    }
    if (state_rs->get_attrib(ClipPlaneAttrib::get_class_slot()) !=
        target_rs->get_attrib(ClipPlaneAttrib::get_class_slot())) {
      altered |= D_clip_planes;
    }
    if (state_rs->get_attrib(TexMatrixAttrib::get_class_slot()) !=
        target_rs->get_attrib(TexMatrixAttrib::get_class_slot())) {
      altered |= D_tex_matrix;
    }
    if (state_rs->get_attrib(TextureAttrib::get_class_slot()) !=
        target_rs->get_attrib(TextureAttrib::get_class_slot())) {
      altered |= D_texture;
    }
    if (state_rs->get_attrib(TexGenAttrib::get_class_slot()) !=
        target_rs->get_attrib(TexGenAttrib::get_class_slot())) {
      altered |= D_tex_gen;
    }
    if (state_rs->get_attrib(RenderModeAttrib::get_class_slot()) !=
        target_rs->get_attrib(RenderModeAttrib::get_class_slot())) {
      altered |= D_render_mode;
    }
    _state_rs = target_rs;
  }

  if (_shader_attrib.get_orig() != target_shader || _shader_attrib.was_deleted()) {
    altered |= D_shader_inputs;
    _shader_attrib = target_shader;
  }

  // Is this the first time this shader is used this frame?
  int frame_number = ClockObject::get_global_clock()->get_frame_count();
  if (frame_number != _frame_number) {
     altered |= D_frame;
    _frame_number = frame_number;
  }

  return altered;
}

/**
 *
 */
int ShaderStateTracker::
get_matrix_dep(ShaderEnums::StateMatrix inp) {
  int dep = D_none;
  if (inp == Shader::SM_model_to_view ||
      inp == Shader::SM_view_to_model ||
      inp == Shader::SM_model_to_apiview ||
      inp == Shader::SM_apiview_to_model) {
    dep |= D_transform & ~D_view_transform;
  }
  if (inp == Shader::SM_view_to_world ||
      inp == Shader::SM_world_to_view ||
      inp == Shader::SM_apiview_to_world ||
      inp == Shader::SM_world_to_apiview ||
      inp == Shader::SM_view_x_to_view ||
      inp == Shader::SM_view_to_view_x ||
      inp == Shader::SM_apiview_x_to_view ||
      inp == Shader::SM_view_to_apiview_x ||
      inp == Shader::SM_clip_x_to_view ||
      inp == Shader::SM_view_to_clip_x ||
      inp == Shader::SM_apiclip_x_to_view ||
      inp == Shader::SM_view_to_apiclip_x) {
    dep |= D_view_transform;
  }
  if (inp == Shader::SM_mat_constant_x ||
      inp == Shader::SM_vec_constant_x ||
      inp == Shader::SM_view_x_to_view ||
      inp == Shader::SM_view_to_view_x ||
      inp == Shader::SM_apiview_x_to_view ||
      inp == Shader::SM_view_to_apiview_x ||
      inp == Shader::SM_clip_x_to_view ||
      inp == Shader::SM_view_to_clip_x ||
      inp == Shader::SM_apiclip_x_to_view ||
      inp == Shader::SM_view_to_apiclip_x) {
    dep |= D_shader_inputs;

    if (inp == Shader::SM_mat_constant_x ||
        inp == Shader::SM_view_x_to_view ||
        inp == Shader::SM_view_to_view_x ||
        inp == Shader::SM_apiview_x_to_view ||
        inp == Shader::SM_view_to_apiview_x ||
        inp == Shader::SM_clip_x_to_view ||
        inp == Shader::SM_view_to_clip_x ||
        inp == Shader::SM_apiclip_x_to_view ||
        inp == Shader::SM_view_to_apiclip_x ||
        inp == Shader::SM_world_to_apiclip_light_i ||
        inp == Shader::SM_point_attenuation) {
      // We can't track changes to these yet, so we have to assume that they
      // are modified every frame.
      dep |= D_frame;
    }
  }
  if (inp == Shader::SM_clipplane_x) {
    dep |= D_clip_planes;
  }
  if (inp == Shader::SM_clip_to_view ||
      inp == Shader::SM_view_to_clip ||
      inp == Shader::SM_apiclip_to_view ||
      inp == Shader::SM_view_to_apiclip ||
      inp == Shader::SM_apiview_to_apiclip ||
      inp == Shader::SM_apiclip_to_apiview ||
      inp == Shader::SM_point_attenuation) {
    dep |= D_projection;
  }
  if (inp == Shader::SM_point_attenuation) {
    dep |= D_scene;
  }
  if (inp == Shader::SM_world_to_apiclip_light_i) {
    dep |= D_light;
  }
  return dep;
}
