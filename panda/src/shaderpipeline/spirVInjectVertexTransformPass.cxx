/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file spirVInjectVertexTransformPass.cxx
 * @author rdb
 * @date 2026-02-17
 */

#include "spirVInjectVertexTransformPass.h"

/**
 *
 */
SpirVInjectVertexTransformPass::
SpirVInjectVertexTransformPass(bool make_new_entry_points, bool use_ssbo,
                               uint32_t buffer_binding, uint32_t buffer_set) :
  _make_new_entry_points(make_new_entry_points),
  _transform_block_binding(buffer_binding),
  _transform_block_set(buffer_set),
  _use_ssbo(use_ssbo) {
}

/**
 * Sets up animation support. locations is a bitmask of all vertex input
 * locations that should be transformed, point_locations the subset that is
 * transformed as a point.  The given index_location and weight_location
 * are for the index/weight vertex columns, which will be created with the given
 * locations if they don't yet exist.
 */
void SpirVInjectVertexTransformPass::
setup_animation(uint32_t locations, uint32_t point_locations,
                int index_location, int weight_location) {
  _anim_locations = locations;
  _anim_point_locations = point_locations;
  _transform_index_location = index_location;
  _transform_weight_location = weight_location;
}

/**
 * Sets up instancing via a vertex attribute.
 */
void SpirVInjectVertexTransformPass::
setup_instancing_attrib(int instance_mat_location) {
  _instance_mat_location = instance_mat_location;
}

/**
 *
 */
void SpirVInjectVertexTransformPass::
mark_model_matrix(uint32_t id, bool inverse, bool transpose) {
  _matrix_vars[id] = MatrixVar {0u, inverse, transpose};
}

/**
 *
 */
void SpirVInjectVertexTransformPass::
run(SpirVModule &module) {
  _module = &module;
  SpirVResultDatabase &db = module.modify_db();

  // Identify the vertex inputs of interest.
  for (uint32_t id = 0; id < module.get_id_bound(); ++id) {
    const Definition &def = db.get_definition(id);

    if (def.is_variable() && def._storage_class == spv::StorageClassInput) {
      if (def._location >= 0) {
        if (_transform_index_location == def._location) {
          const ShaderType::Vector *vector = def._type->as_vector();
          if (vector != nullptr &&
              vector->get_num_components() >= 4 &&
              (vector->get_scalar_type() == ShaderType::ST_uint || vector->get_scalar_type() == ShaderType::ST_int)) {
            _transform_index_var_id = id;
          }
        }
        else if (_transform_weight_location == def._location) {
          const ShaderType::Vector *vector = def._type->as_vector();
          if (vector != nullptr &&
              vector->get_num_components() >= 4 &&
              vector->get_scalar_type() == ShaderType::ST_float) {
            _transform_weight_var_id = id;
          }
        }
        else if (_instance_mat_location == def._location) {
          _instance_mat_var_id = id;
        }
        else if (_anim_locations & (1u << def._location)) {
          // This will be transformed.
          _vertex_input_ids[id] = module.allocate_id();
        }
      }
      else if (def._builtin == spv::BuiltInInstanceIndex) {
        _instance_index_var_id = id;
      }
    }
  }

  // Capture the vertex entry points with their original interfaces, before
  // the variable swap below.
  for (const SpirVModule::EntryPoint &ep : module.get_entry_points()) {
    if (ep.model == spv::ExecutionModelVertex) {
      _todo_entry_points.insert({ep.function_id, EntryPoint {ep.name, ep.interface_vars}});
    }
  }

  // Turn each affected variable into a private variable, and recreate the
  // input variable under a new id.  We swap the ids so that the private
  // variable gets the id that the input used to have, so that all loads from
  // this point on read the private variable.  The decorations (and the
  // database record, with the location and type) move to the new input; the
  // name stays with the private variable.
  pvector<std::pair<uint32_t, uint32_t> > swaps;
  for (const auto &item : _vertex_input_ids) {
    swaps.push_back(std::make_pair(item.first, item.second));
  }
  for (auto &item : _matrix_vars) {
    item.second._id = module.allocate_id();
    swaps.push_back(std::make_pair(item.first, item.second._id));
  }

  for (const auto &swap : swaps) {
    uint32_t var_id = swap.first;
    uint32_t new_id = swap.second;

    Instruction *decl = module.get_global(var_id);
    nassertd(decl != nullptr && decl->opcode == spv::OpVariable) continue;

    if (shader_cat.is_spam()) {
      shader_cat.spam()
        << "Changing storage class for variable " << var_id
        << " to Private and creating new variable " << new_id << "\n";
    }

    const ShaderType *type = module.resolve_pointer_type(decl->args[0]);

    // The original declaration becomes the new input variable.
    decl->args[1] = new_id;

    Definition def = std::move(db.modify_definition(var_id));
    db.modify_definition(var_id).clear();
    db.modify_definition(new_id) = std::move(def);

    module.transfer_annotations(var_id, new_id);

    // Declare the private variable under the old id.  The emit-time
    // topological sort takes care of the pointer type being declared after
    // the variable.
    uint32_t pointer_type_id = module.define_pointer_type(type, spv::StorageClassPrivate);
    module.add_global(Instruction(spv::OpVariable, {
      pointer_type_id, var_id, (uint32_t)spv::StorageClassPrivate,
    }));
    db.record_variable(var_id, pointer_type_id, spv::StorageClassPrivate);

    _make_private_pointers.insert(var_id);
  }

  // We changed the storage class of these variables, so we must change the
  // storage class of the result type of any access chain formed from them.
  for (Function &function : module.modify_functions()) {
    for (Instruction &op : function.instructions) {
      if ((op.opcode == spv::OpAccessChain ||
           op.opcode == spv::OpInBoundsAccessChain ||
           op.opcode == spv::OpCopyObject) &&
          op.args.size() >= 3 && _make_private_pointers.count(op.args[2])) {
        uint32_t type_id = module.unwrap_pointer_type(op.args[0]);
        uint32_t type_pointer_id = module.define_pointer_type(type_id, spv::StorageClassPrivate);
        db.modify_definition(op.args[1])._type_id = type_pointer_id;
        op.args[0] = type_pointer_id;

        _make_private_pointers.insert(op.args[1]);
      }
    }
  }

  if (!_make_new_entry_points) {
    // Modify the existing entry points in place: inject the transformations
    // at the top of each entry point function.
    for (const auto &item : _todo_entry_points) {
      uint32_t function_id = item.first;
      const EntryPoint &entry_point = item.second;

      Function *function = module.get_function(function_id);
      nassertd(function != nullptr) continue;

      // Find the first label, and inject after it.
      size_t label_index = 0;
      while (label_index < function->instructions.size() &&
             function->instructions[label_index].opcode != spv::OpLabel) {
        ++label_index;
      }
      nassertd(label_index < function->instructions.size()) continue;

      SpirVBuilder builder(module, function_id, label_index + 1);
      inject_animation(builder, entry_point._vars);
      inject_instancing(builder);

      // Update the entry point interface.
      for (SpirVModule::EntryPoint &ep : module.modify_entry_points()) {
        if (ep.model == spv::ExecutionModelVertex && ep.function_id == function_id) {
          ep.interface_vars = make_interface(entry_point._vars,
                                        _anim_locations != 0u,
                                        !_matrix_vars.empty());
        }
      }
    }
    _todo_entry_points.clear();
    return;
  }

  // Create new entry points, in all the required variants, that wrap the
  // original entry point functions.
  {
    // The original entry points are replaced by the wrappers.
    pvector<SpirVModule::EntryPoint> &entry_points = module.modify_entry_points();
    for (size_t i = entry_points.size(); i-- > 0;) {
      if (entry_points[i].model == spv::ExecutionModelVertex) {
        entry_points.erase(entry_points.begin() + i);
      }
    }
  }

  for (const auto &item : _todo_entry_points) {
    uint32_t orig_func_id = item.first;
    const EntryPoint &entry_point = item.second;

    {
      SpirVBuilder builder(module);
      uint32_t func_id = builder.op_function(nullptr);
      builder.op_label();
      inject_instancing_noop(builder);
      inject_animation_noop(builder, entry_point._vars);
      builder.op_function_call(orig_func_id);
      builder.op_return();
      builder.op_function_end();
      module.add_entry_point(spv::ExecutionModelVertex, func_id, entry_point._name,
                             make_interface(entry_point._vars, false, false));
    }

    if (_anim_locations != 0u) {
      SpirVBuilder builder(module);
      uint32_t func_id = builder.op_function(nullptr);
      builder.op_label();
      inject_instancing_noop(builder);
      inject_animation(builder, entry_point._vars);
      builder.op_function_call(orig_func_id);
      builder.op_return();
      builder.op_function_end();
      module.add_entry_point(spv::ExecutionModelVertex, func_id, entry_point._name + "_anim",
                             make_interface(entry_point._vars, true, false));
    }

    if (!_matrix_vars.empty()) {
      SpirVBuilder builder(module);
      uint32_t func_id = builder.op_function(nullptr);
      builder.op_label();
      inject_instancing(builder);
      inject_animation_noop(builder, entry_point._vars);
      builder.op_function_call(orig_func_id);
      builder.op_return();
      builder.op_function_end();
      module.add_entry_point(spv::ExecutionModelVertex, func_id, entry_point._name + "_inst",
                             make_interface(entry_point._vars, false, true));
    }

    if (!_matrix_vars.empty() && _anim_locations != 0u) {
      SpirVBuilder builder(module);
      uint32_t func_id = builder.op_function(nullptr);
      builder.op_label();
      inject_instancing(builder);
      inject_animation(builder, entry_point._vars);
      builder.op_function_call(orig_func_id);
      builder.op_return();
      builder.op_function_end();
      module.add_entry_point(spv::ExecutionModelVertex, func_id, entry_point._name + "_anim_inst",
                             make_interface(entry_point._vars, true, true));
    }
  }
  _todo_entry_points.clear();
}

/**
 * Builds an entry point interface list from the given original interface,
 * mapping the swapped input variables and appending the extra inputs used by
 * the animation and/or instancing preambles.
 */
pvector<uint32_t> SpirVInjectVertexTransformPass::
make_interface(const pvector<uint32_t> &vars,
               bool with_animation, bool with_instancing) const {
  pvector<uint32_t> result;
  bool has_transform_index = false;
  bool has_transform_weight = false;
  bool has_instance_matrix = false;
  bool has_instance_index = false;

  for (uint32_t var_id : vars) {
    auto it = _vertex_input_ids.find(var_id);
    if (it != _vertex_input_ids.end()) {
      result.push_back(it->second);
    } else {
      result.push_back(var_id);
    }

    if (var_id == _transform_index_var_id) {
      has_transform_index = true;
    }
    if (var_id == _transform_weight_var_id) {
      has_transform_weight = true;
    }
    if (var_id == _instance_mat_var_id) {
      has_instance_matrix = true;
    }
    if (var_id == _instance_index_var_id) {
      has_instance_index = true;
    }
  }

  if (with_animation && _anim_locations != 0u) {
    if (!has_transform_index && _transform_index_var_id != 0u) {
      result.push_back(_transform_index_var_id);
    }
    if (!has_transform_weight && _transform_weight_var_id != 0u) {
      result.push_back(_transform_weight_var_id);
    }
  }
  if (with_instancing && !_matrix_vars.empty()) {
    if (_instance_mat_var_id != 0u) {
      if (!has_instance_matrix) {
        result.push_back(_instance_mat_var_id);
      }
    } else {
      if (!has_instance_index && _instance_index_var_id != 0u) {
        result.push_back(_instance_index_var_id);
      }
    }
  }
  return result;
}

/**
 * Injects the function preamble that applies the animation transformations
 * at the given builder's cursor.
 */
void SpirVInjectVertexTransformPass::
inject_animation(SpirVBuilder &builder, const pvector<uint32_t> &vars) {
  if (_anim_locations == 0u) {
    return;
  }

  SpirVModule &module = *_module;
  const SpirVResultDatabase &db = module.get_db();

  // Make sure the skinning matrix buffer is defined.
  if (_transform_block_var_id == 0) {
    define_transform_block();
  }

  if (_transform_index_var_id == 0u) {
    const ShaderType *uvec4_type = ShaderType::register_type(ShaderType::Vector(ShaderType::ST_uint, 4));
    _transform_index_var_id = module.define_variable(uvec4_type, spv::StorageClassInput);
    module.decorate(_transform_index_var_id, spv::DecorationLocation, _transform_index_location);
    module.set_name(_transform_index_var_id, "transform_index");
  }

  const ShaderType *vec4_type = ShaderType::register_type(ShaderType::Vector(ShaderType::ST_float, 4));
  if (_transform_weight_var_id == 0u) {
    _transform_weight_var_id = module.define_variable(vec4_type, spv::StorageClassInput);
    module.decorate(_transform_weight_var_id, spv::DecorationLocation, _transform_weight_location);
    module.set_name(_transform_weight_var_id, "transform_weight");
  }

  uint32_t transform_index = builder.op_load(_transform_index_var_id);
  uint32_t transform_weight = builder.op_load(_transform_weight_var_id);

  // How many components do we have?
  const ShaderType::Vector *transform_index_type = db.get_definition(_transform_index_var_id)._type->as_vector();
  const ShaderType::Vector *transform_weight_type = db.get_definition(_transform_weight_var_id)._type->as_vector();
  nassertv(transform_index_type != nullptr && transform_weight_type != nullptr);
  uint32_t num_components = std::min(transform_index_type->get_num_components(), transform_weight_type->get_num_components());

  if (_use_ssbo) {
    // Create an offset variable, add it to the indices.
    if (_transform_offset_var_id == 0u) {
      _transform_offset_var_id = module.define_variable(ShaderType::UINT, spv::StorageClassUniformConstant);
    }

    // Must broadcast to a vector before we can add it.
    uint32_t offset = builder.op_load(_transform_offset_var_id);
    uint32_t vec_offset = builder.op_composite_construct(transform_index_type,
      pvector<uint32_t>(transform_index_type->get_num_components(), offset));
    transform_index = builder.op_add(transform_index, vec_offset);
  }

  // We can't add matrices together, we have to add them per vector.
  uint32_t accum_matrix_row0 = 0;
  uint32_t accum_matrix_row1 = 0;
  uint32_t accum_matrix_row2 = 0;

  uint32_t zero = module.define_int_constant(0);

  for (uint32_t i = 0; i < num_components; ++i) {
    // matrix = ubo.m[transform_index[i]]
    uint32_t matrix = builder.op_load(builder.op_access_chain(_transform_block_var_id, {zero, builder.op_composite_extract(transform_index, {i})}));

    // matrix *= transform_weight[i]
    matrix = builder.op_multiply(matrix, builder.op_composite_extract(transform_weight, {i}));

    // Split into rows
    uint32_t matrix_row0 = builder.op_composite_extract(matrix, {0});
    uint32_t matrix_row1 = builder.op_composite_extract(matrix, {1});
    uint32_t matrix_row2 = builder.op_composite_extract(matrix, {2});
    if (i == 0) {
      accum_matrix_row0 = matrix_row0;
      accum_matrix_row1 = matrix_row1;
      accum_matrix_row2 = matrix_row2;
    } else {
      accum_matrix_row0 = builder.op_add(accum_matrix_row0, matrix_row0);
      accum_matrix_row1 = builder.op_add(accum_matrix_row1, matrix_row1);
      accum_matrix_row2 = builder.op_add(accum_matrix_row2, matrix_row2);
    }
  }

  const ShaderType *mat3x4_type = ShaderType::register_type(ShaderType::Matrix(ShaderType::ST_float, 3, 4));
  uint32_t mat3x4 = builder.op_composite_construct(mat3x4_type, {accum_matrix_row0, accum_matrix_row1, accum_matrix_row2});

  for (uint32_t private_vec_ptr : vars) {
    auto it = _vertex_input_ids.find(private_vec_ptr);
    if (it != _vertex_input_ids.end()) {
      uint32_t input_vec_ptr = it->second;

      const Definition &vec_def = db.get_definition(input_vec_ptr);
      const ShaderType::Vector *vec_type = vec_def._type->as_vector();
      if (vec_type == nullptr || vec_type->get_num_components() < 3) {
        continue;
      }

      bool is_point = (_anim_point_locations & (1u << vec_def._location)) != 0;
      uint32_t w = module.define_float_constant(is_point);

      uint32_t orig_vec = builder.op_load(input_vec_ptr);
      if (vec_type->get_num_components() == 3) {
        // Expand to vec4.
        orig_vec = builder.op_composite_construct(vec4_type, {orig_vec, w});
      }

      uint32_t result = builder.op_multiply(orig_vec, mat3x4);
      if (vec_type->get_num_components() == 4) {
        // It comes out as a vec3, so expand to vec4 again.
        result = builder.op_composite_construct(vec4_type, {result, w});
      }

      builder.op_store(private_vec_ptr, result);
    }
  }
}

/**
 * Injects the function preamble that does not apply the transformations.
 */
void SpirVInjectVertexTransformPass::
inject_animation_noop(SpirVBuilder &builder, const pvector<uint32_t> &vars) {
  for (uint32_t private_vec_ptr : vars) {
    auto it = _vertex_input_ids.find(private_vec_ptr);
    if (it != _vertex_input_ids.end()) {
      uint32_t input_vec_ptr = it->second;
      builder.op_store(private_vec_ptr, builder.op_load(input_vec_ptr));
    }
  }
}

/**
 *
 */
void SpirVInjectVertexTransformPass::
inject_instancing(SpirVBuilder &builder) {
  if (_matrix_vars.empty()) {
    return;
  }

  SpirVModule &module = *_module;
  const SpirVResultDatabase &db = module.get_db();

  uint32_t inst_mat3x4;

  if (_instance_mat_location >= 0) {
    // Make sure the instance matrix vertex input is defined.
    if (_instance_mat_var_id == 0u) {
      _instance_mat_var_id = module.define_variable(ShaderType::register_type(ShaderType::Matrix(ShaderType::ST_float, 3, 4)), spv::StorageClassInput);
      module.decorate(_instance_mat_var_id, spv::DecorationLocation, (uint32_t)_instance_mat_location);
    }

    inst_mat3x4 = builder.op_load(_instance_mat_var_id);
  } else {
    // Make sure the transform SSBO is defined.
    if (_transform_block_var_id == 0) {
      define_transform_block();
    }
    if (_instance_index_var_id == 0) {
      _instance_index_var_id = module.define_variable(ShaderType::INT, spv::StorageClassInput);
      module.decorate(_instance_index_var_id, spv::DecorationBuiltIn, spv::BuiltInInstanceIndex);
    }

    uint32_t inst_index = builder.op_load(_instance_index_var_id);
    uint32_t zero = module.define_int_constant(0);
    inst_mat3x4 = builder.op_load(builder.op_access_chain(_transform_block_var_id, {zero, inst_index}));
  }

  const ShaderType *mat3_type = ShaderType::register_type(ShaderType::Matrix(ShaderType::ST_float, 3, 3));
  const ShaderType *vec4_type = ShaderType::register_type(ShaderType::Vector(ShaderType::ST_float, 4));

  // Do we need to invert the matrix?  We know that it is affine, so we
  // only have to invert the upper 3x3, which is more efficient.  We write
  // out the math for transpose(inverse(transpose(inst_mat))).
  bool need_inverse = false;
  bool need_inverse_translation = false;
  for (auto &pair : _matrix_vars) {
    if (pair.second._inverse) {
      need_inverse = true;

      const ShaderType::Matrix *matrix = db.get_definition(pair.first)._type->as_matrix();
      if (pair.second._transpose ? matrix->get_num_columns() == 4 : matrix->get_num_rows() == 4) {
        need_inverse_translation = true;
      }
    }
  }
  uint32_t inv_inst_mat3 = 0u;
  uint32_t inv_inst_mat4 = 0u;
  uint32_t inv_inst_mat3x4 = 0u;
  if (need_inverse) {
    const ShaderType *mat3x4_type = ShaderType::register_type(ShaderType::Matrix(ShaderType::ST_float, 3, 4));
    const ShaderType *vec3_type = ShaderType::register_type(ShaderType::Vector(ShaderType::ST_float, 3));

    uint32_t r0 = builder.op_composite_construct(vec3_type, {
      builder.op_sub(builder.op_multiply(builder.op_composite_extract(inst_mat3x4, {1, 1}), builder.op_composite_extract(inst_mat3x4, {2, 2})),
                     builder.op_multiply(builder.op_composite_extract(inst_mat3x4, {2, 1}), builder.op_composite_extract(inst_mat3x4, {1, 2}))),
      builder.op_sub(builder.op_multiply(builder.op_composite_extract(inst_mat3x4, {2, 1}), builder.op_composite_extract(inst_mat3x4, {0, 2})),
                     builder.op_multiply(builder.op_composite_extract(inst_mat3x4, {0, 1}), builder.op_composite_extract(inst_mat3x4, {2, 2}))),
      builder.op_sub(builder.op_multiply(builder.op_composite_extract(inst_mat3x4, {0, 1}), builder.op_composite_extract(inst_mat3x4, {1, 2})),
                     builder.op_multiply(builder.op_composite_extract(inst_mat3x4, {1, 1}), builder.op_composite_extract(inst_mat3x4, {0, 2}))),
    });
    uint32_t r1 = builder.op_composite_construct(vec3_type, {
      builder.op_sub(builder.op_multiply(builder.op_composite_extract(inst_mat3x4, {1, 2}), builder.op_composite_extract(inst_mat3x4, {2, 0})),
                     builder.op_multiply(builder.op_composite_extract(inst_mat3x4, {2, 2}), builder.op_composite_extract(inst_mat3x4, {1, 0}))),
      builder.op_sub(builder.op_multiply(builder.op_composite_extract(inst_mat3x4, {2, 2}), builder.op_composite_extract(inst_mat3x4, {0, 0})),
                     builder.op_multiply(builder.op_composite_extract(inst_mat3x4, {0, 2}), builder.op_composite_extract(inst_mat3x4, {2, 0}))),
      builder.op_sub(builder.op_multiply(builder.op_composite_extract(inst_mat3x4, {0, 2}), builder.op_composite_extract(inst_mat3x4, {1, 0})),
                     builder.op_multiply(builder.op_composite_extract(inst_mat3x4, {1, 2}), builder.op_composite_extract(inst_mat3x4, {0, 0}))),
    });
    uint32_t r2 = builder.op_composite_construct(vec3_type, {
      builder.op_sub(builder.op_multiply(builder.op_composite_extract(inst_mat3x4, {1, 0}), builder.op_composite_extract(inst_mat3x4, {2, 1})),
                     builder.op_multiply(builder.op_composite_extract(inst_mat3x4, {2, 0}), builder.op_composite_extract(inst_mat3x4, {1, 1}))),
      builder.op_sub(builder.op_multiply(builder.op_composite_extract(inst_mat3x4, {2, 0}), builder.op_composite_extract(inst_mat3x4, {0, 1})),
                     builder.op_multiply(builder.op_composite_extract(inst_mat3x4, {0, 0}), builder.op_composite_extract(inst_mat3x4, {2, 1}))),
      builder.op_sub(builder.op_multiply(builder.op_composite_extract(inst_mat3x4, {0, 0}), builder.op_composite_extract(inst_mat3x4, {1, 1})),
                     builder.op_multiply(builder.op_composite_extract(inst_mat3x4, {1, 0}), builder.op_composite_extract(inst_mat3x4, {0, 1}))),
    });
    uint32_t one = module.define_float_constant(1);
    uint32_t inv_det = builder.op_div(one, builder.op_dot(r0, builder.op_composite_construct(vec3_type, {
      builder.op_composite_extract(inst_mat3x4, {0, 0}),
      builder.op_composite_extract(inst_mat3x4, {1, 0}),
      builder.op_composite_extract(inst_mat3x4, {2, 0}),
    })));

    if (need_inverse_translation) {
      // Handle the translation.
      uint32_t t = builder.op_composite_construct(vec3_type, {
        builder.op_composite_extract(inst_mat3x4, {0, 3}),
        builder.op_composite_extract(inst_mat3x4, {1, 3}),
        builder.op_composite_extract(inst_mat3x4, {2, 3}),
      });

      inv_inst_mat3x4 = builder.op_multiply(builder.op_composite_construct(mat3x4_type, {
        builder.op_composite_construct(vec4_type, {r0, builder.op_negate(builder.op_dot(r0, t))}),
        builder.op_composite_construct(vec4_type, {r1, builder.op_negate(builder.op_dot(r1, t))}),
        builder.op_composite_construct(vec4_type, {r2, builder.op_negate(builder.op_dot(r2, t))}),
      }), inv_det);
    }
    else {
      // Don't bother constructing the last column.
      inv_inst_mat3 = builder.op_multiply(builder.op_composite_construct(mat3_type, {r0, r1, r2}), inv_det);
    }
  }
  uint32_t inst_mat3 = 0u;
  uint32_t inst_mat4 = 0u;

  for (auto &pair : _matrix_vars) {
    bool inverse = pair.second._inverse;
    bool transpose = pair.second._transpose;
    uint32_t orig_mat_ptr = pair.second._id;
    uint32_t result_mat_ptr = pair.first;

    uint32_t orig_mat = builder.op_load(orig_mat_ptr);
    uint32_t inst_mat = 0u;

    // Compose the right matrix for multiplying into the target matrix.
    const ShaderType::Matrix *matrix = db.get_definition(orig_mat_ptr)._type->as_matrix();
    uint32_t mat_size = transpose ? matrix->get_num_columns() : matrix->get_num_rows();

    if (matrix->get_num_rows() == 3 && matrix->get_num_columns() == 4 && transpose) {
      // Special case of affine mat * affine mat.
      pvector<uint32_t> rows {
        builder.op_composite_extract(orig_mat, {0}),
        builder.op_composite_extract(orig_mat, {1}),
        builder.op_composite_extract(orig_mat, {2}),
      };

      uint32_t orig_mat3 = builder.op_composite_construct(mat3_type, {
        builder.op_vector_shuffle(rows[0], rows[0], {0, 1, 2}),
        builder.op_vector_shuffle(rows[1], rows[1], {0, 1, 2}),
        builder.op_vector_shuffle(rows[2], rows[2], {0, 1, 2}),
      });

      // mat3x4 * mat3 -> mat3x4
      uint32_t tmp_mat = builder.op_multiply(inverse ? inv_inst_mat3x4 : inst_mat3x4, orig_mat3);

      // Now handle w (we assume that the matrix we modify isn't affine)
      pvector<uint32_t> result_rows;
      for (uint32_t i = 0; i < matrix->get_num_rows(); ++i) {
        uint32_t tmp_row = builder.op_composite_extract(tmp_mat, {i});
        uint32_t new_w = builder.op_add(builder.op_composite_extract(tmp_mat, {i, 3}), builder.op_composite_extract(rows[i], {3}));
        rows[i] = builder.op_composite_construct(vec4_type, {
          builder.op_vector_shuffle(tmp_row, tmp_row, {0, 1, 2}),
          new_w,
        });
      }

      builder.op_store(result_mat_ptr, builder.op_composite_construct(matrix, rows));
      continue;
    }
    else if (mat_size == 4) {
      // Construct a 4x4 matrix.
      uint32_t &use_inst_mat4 = inverse ? inv_inst_mat4 : inst_mat4;
      if (use_inst_mat4 == 0) {
        const ShaderType *mat4_type = ShaderType::register_type(ShaderType::Matrix(ShaderType::ST_float, 4, 4));
        uint32_t zero = module.define_float_constant(0);
        uint32_t one = module.define_float_constant(1);

        uint32_t input = inverse ? inv_inst_mat3x4 : inst_mat3x4;
        uint32_t row0 = builder.op_composite_extract(input, {0});
        uint32_t row1 = builder.op_composite_extract(input, {1});
        uint32_t row2 = builder.op_composite_extract(input, {2});
        uint32_t row3 = builder.op_composite_construct(vec4_type, {zero, zero, zero, one});
        use_inst_mat4 = builder.op_composite_construct(mat4_type, {row0, row1, row2, row3});
      }
      inst_mat = use_inst_mat4;
    }
    else {
      // Construct a 3x3 matrix without translation.
      uint32_t &use_inst_mat3 = inverse ? inv_inst_mat3 : inst_mat3;
      if (use_inst_mat3 == 0) {
        uint32_t input = inverse ? inv_inst_mat3x4 : inst_mat3x4;

        pvector<uint32_t> rows;
        for (uint32_t i = 0; i < mat_size; ++i) {
          uint32_t row = builder.op_composite_extract(input, {i});
          rows.push_back(builder.op_vector_shuffle(row, row, {0, 1, 2}));
        }
        use_inst_mat3 = builder.op_composite_construct(mat3_type, rows);
      }
      inst_mat = use_inst_mat3;
    }

    uint32_t left, right;
    if (transpose) {
      // The incoming instance matrix is already transposed.
      left = inst_mat;
      right = orig_mat;
    } else {
      left = orig_mat;
      right = builder.op_transpose(inst_mat);
    }

    builder.op_store(result_mat_ptr, builder.op_multiply(left, right));
  }
}

/**
 *
 */
void SpirVInjectVertexTransformPass::
inject_instancing_noop(SpirVBuilder &builder) {
  for (auto &pair : _matrix_vars) {
    uint32_t orig_mat_ptr = pair.second._id;
    uint32_t result_mat_ptr = pair.first;

    builder.op_store(result_mat_ptr, builder.op_load(orig_mat_ptr));
  }
}

/**
 * Makes sure the transform block type is defined.
 */
void SpirVInjectVertexTransformPass::
define_transform_block() {
  SpirVModule &module = *_module;

  const ShaderType *block_type;
  if (_use_ssbo) {
    static const ShaderType *ssbo_block_type = make_transform_block_type(0);
    block_type = ssbo_block_type;
  } else {
    static const ShaderType *ubo_block_type = make_transform_block_type(256);
    block_type = ubo_block_type;
  }
  uint32_t block_type_id = module.define_type(block_type);
  uint32_t block_var_id = module.define_variable(block_type, spv::StorageClassUniform);

  module.decorate(block_type_id, _use_ssbo ? spv::DecorationBufferBlock : spv::DecorationBlock);
  module.decorate(block_var_id, spv::DecorationBinding, _transform_block_binding);
  module.decorate(block_var_id, spv::DecorationDescriptorSet, _transform_block_set);
  if (_use_ssbo) {
    module.decorate(block_var_id, spv::DecorationNonWritable);
  }
  _transform_block_var_id = block_var_id;
}

/**
 *
 */
const ShaderType *SpirVInjectVertexTransformPass::
make_transform_block_type(uint32_t num_elements) {
  // Because of alignment rules making it more efficient, we store the matrix
  // as a transposed 3x4 matrix.
  const ShaderType *mat3x4_type = ShaderType::register_type(ShaderType::Matrix(ShaderType::ST_float, 3, 4));
  const ShaderType *array_type = ShaderType::register_type(ShaderType::Array(mat3x4_type, num_elements));
  ShaderType::Struct struct_type;
  struct_type.add_member(array_type, "m");
  return ShaderType::register_type(std::move(struct_type));
}
