/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file shaderStateTracker.h
 * @author rdb
 * @date 2024-09-22
 */

#ifndef SHADERSTATETRACKER_H
#define SHADERSTATETRACKER_H

#include "pandabase.h"
#include "weakPointerTo.h"
#include "transformState.h"
#include "colorAttrib.h"
#include "shaderAttrib.h"
#include "shaderEnums.h"

class GraphicsStateGuardianBase;

/**
 * Keeps track of how the graphics state that a shader depends on changes over
 * time.
 */
class EXPCL_PANDA_PGRAPH ShaderStateTracker {
public:
  enum Dependency {
    D_none           = 0x00000,
    D_frame          = 0x00001,
    D_scene          = 0x00002,
    D_state          = 0x00004,
    D_vertex_data    = 0x00008,
    D_transform      = 0x00030,
    D_view_transform = 0x00020,
    D_projection     = 0x00040,
    D_color          = 0x00080 | D_state,
    D_colorscale     = 0x00100 | D_state,
    D_material       = 0x00200 | D_state,
    D_shader_inputs  = 0x00400 | D_state,
    D_fog            = 0x00800 | D_state,
    D_light          = 0x01000 | D_state,
    D_clip_planes    = 0x02000 | D_state,
    D_tex_matrix     = 0x04000 | D_state,
    D_texture        = 0x08000 | D_state,
    D_tex_gen        = 0x10000 | D_state,
    D_render_mode    = 0x20000 | D_state,
  };

  void setup(GraphicsStateGuardianBase *gsg, Shader *shader);

  int update(const RenderState *state, const ShaderAttrib *target_shader,
             const TransformState *modelview_transform,
             const TransformState *camera_transform,
             const TransformState *projection_transform);

  INLINE void update_vertex_data(const GeomVertexDataPipelineReader *data_reader);

  static int get_matrix_dep(ShaderEnums::StateMatrix input);

  GraphicsStateGuardianBase *_gsg;
  WCPT(RenderState) _state_rs;
  CPT(TransformState) _modelview_transform;
  CPT(TransformState) _camera_transform;
  CPT(TransformState) _projection_transform;
  CPT(ColorAttrib) _color_attrib;
  WCPT(ShaderAttrib) _shader_attrib;
  const DisplayRegion *_display_region = nullptr;
  const GeomVertexDataPipelineReader *_data_reader = nullptr;
  LVecBase2i _pixel_size;
  int _frame_number = -1;

  pvector<LMatrix4> _matrix_cache;
  int _matrix_cache_deps = D_none;
};

#include "shaderStateTracker.I"

#endif
