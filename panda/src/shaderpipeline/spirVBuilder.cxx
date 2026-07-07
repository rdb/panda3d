/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file spirVBuilder.cxx
 * @author rdb
 * @date 2026-07-07
 */

#include "spirVBuilder.h"
#include "pbitops.h"

/**
 * Creates a builder without a cursor position; global definitions may be
 * created, and op_function may be called to start a new function.
 */
SpirVBuilder::
SpirVBuilder(SpirVModule &module) :
  _module(module) {
}

/**
 * Creates a builder with the cursor at the given instruction index of the
 * given function.
 */
SpirVBuilder::
SpirVBuilder(SpirVModule &module, uint32_t function_id, size_t index) :
  _module(module) {
  set_insertion_point(function_id, index);
}

/**
 * Positions the cursor at the given instruction index of the given function.
 */
void SpirVBuilder::
set_insertion_point(uint32_t function_id, size_t index) {
  const pvector<Function> &functions = _module.get_functions();
  for (size_t fi = 0; fi < functions.size(); ++fi) {
    if (functions[fi].id == function_id) {
      _function_index = fi;
      _index = index;
      _current_function_id = function_id;
      return;
    }
  }
  nassert_raise("no function with this id");
  _function_index = (size_t)-1;
  _current_function_id = 0;
}

/**
 * Returns the function the cursor is currently inside, or null.  Note that
 * the pointer is invalidated when a function is added to the module.
 */
SpirVBuilder::Function *SpirVBuilder::
get_current_function() {
  if (_function_index >= _module.get_functions().size()) {
    return nullptr;
  }
  return &_module.modify_functions()[_function_index];
}

/**
 * Inserts an arbitrary instruction at the cursor, recording its result in
 * the module's id index.
 */
void SpirVBuilder::
insert(Instruction op) {
  Function *function = get_current_function();
  nassertv(function != nullptr);
  nassertv(_index <= function->instructions.size());
  _module.record_result(op, _current_function_id);
  function->instructions.insert(function->instructions.begin() + _index,
                                std::move(op));
  ++_index;
}

/**
 * Inserts an arbitrary instruction at the cursor.
 */
void SpirVBuilder::
insert(spv::Op opcode, const uint32_t *args, size_t nargs) {
  insert(Instruction(opcode, args, nargs));
}

/**
 * Inserts an OpLoad from the given pointer id.
 */
uint32_t SpirVBuilder::
op_load(uint32_t var_id, spv::MemoryAccessMask access) {
  uint32_t type_id = _module.unwrap_pointer_type(_module.get_type_id(var_id));

  uint32_t id = allocate_id();
  if (access != spv::MemoryAccessMaskNone) {
    insert(spv::OpLoad, {type_id, id, var_id, (uint32_t)access});
  } else {
    insert(spv::OpLoad, {type_id, id, var_id});
  }
  return id;
}

/**
 * Inserts an OpStore to the given pointer id.
 */
void SpirVBuilder::
op_store(uint32_t var_id, uint32_t value) {
  insert(spv::OpStore, {var_id, value});
}

/**
 * Inserts an OpSelect.
 */
uint32_t SpirVBuilder::
op_select(uint32_t cond, uint32_t obj1, uint32_t obj2) {
  uint32_t type_id = _module.get_type_id(obj1);
  nassertr(type_id == _module.get_type_id(obj2), 0);

  uint32_t id = allocate_id();
  insert(spv::OpSelect, {type_id, id, cond, obj1, obj2});
  return id;
}

/**
 * Inserts an OpAccessChain with the given base id (which must be a pointer)
 * and constant ids containing the various member/array indices.
 */
uint32_t SpirVBuilder::
op_access_chain(uint32_t var_id, std::initializer_list<uint32_t> chain) {
  uint32_t base_pointer_type_id = _module.get_type_id(var_id);
  nassertr(_module.get_kind(base_pointer_type_id) == SpirVModule::IdKind::POINTER_TYPE, 0);

  // The chain's pointer type must use the storage class as declared on the
  // base pointer type, not the canonicalized one that get_storage_class
  // reports: chaining into a legacy-style buffer block (Uniform storage
  // class with the BufferBlock decoration) must yield Uniform-class
  // pointers, since declaring a StorageBuffer-class pointer requires
  // SPV_KHR_storage_buffer_storage_class before SPIR-V 1.3.
  const SpirVModule::Instruction *base_decl = _module.get_decl(base_pointer_type_id);
  nassertr(base_decl != nullptr && base_decl->args.size() >= 3, 0);
  spv::StorageClass storage_class = (spv::StorageClass)base_decl->args[1];

  uint32_t type_id = _module.unwrap_pointer_type(base_pointer_type_id);
  for (auto index_id : chain) {
    const Instruction *type_decl = _module.get_decl(type_id);
    nassertr(type_decl != nullptr, 0);

    if (type_decl->opcode == spv::OpTypeStruct) {
      uint32_t member_index = _module.resolve_constant(index_id);
      type_id = _module.get_composite_member_type_id(type_id, member_index);
    } else {
      // Array, matrix, or vector
      type_id = _module.get_composite_member_type_id(type_id, 0);
    }
    nassertr(type_id != 0, 0);
  }

  uint32_t id = allocate_id();
  uint32_t pointer_type_id = _module.define_pointer_type(type_id, storage_class);

  Instruction op(spv::OpAccessChain, {pointer_type_id, id, var_id});
  op.args.insert(op.args.end(), chain.begin(), chain.end());
  insert(std::move(op));
  return id;
}

/**
 * Inserts an OpVectorShuffle, like a swizzle but may source from two vectors
 * at once, with the indices continuing to number into the second vector.
 * For a regular swizzle, pass the same vector twice.
 */
uint32_t SpirVBuilder::
op_vector_shuffle(uint32_t vec1, uint32_t vec2, const pvector<uint32_t> &components) {
  const ShaderType *vec1_shader_type = _module.resolve_value_type(vec1);
  const ShaderType *vec2_shader_type = _module.resolve_value_type(vec2);
  nassertr(vec1_shader_type != nullptr && vec2_shader_type != nullptr, 0);
  const ShaderType::Vector *vec1_type = vec1_shader_type->as_vector();
  const ShaderType::Vector *vec2_type = vec2_shader_type->as_vector();
  nassertr(vec1_type != nullptr && vec2_type != nullptr, 0);
  nassertr(vec1_type->get_scalar_type() == vec2_type->get_scalar_type(), 0);

  uint32_t id = allocate_id();

  const ShaderType *result_type = ShaderType::register_type(ShaderType::Vector(vec1_type->get_scalar_type(), components.size()));
  uint32_t type_id = _module.define_type(result_type);

  Instruction op(spv::OpVectorShuffle, {type_id, id, vec1, vec2});
  op.args.insert(op.args.end(), components.begin(), components.end());
  insert(std::move(op));
  return id;
}

/**
 * Constructs a composite with the given type from the given constituents.
 */
uint32_t SpirVBuilder::
op_composite_construct(const ShaderType *type, const pvector<uint32_t> &constituents) {
  uint32_t id = allocate_id();
  uint32_t type_id = _module.define_type(type);

  Instruction op(spv::OpCompositeConstruct, {type_id, id});
  op.args.insert(op.args.end(), constituents.begin(), constituents.end());
  insert(std::move(op));
  return id;
}

/**
 * Inserts an OpCompositeExtract.
 */
uint32_t SpirVBuilder::
op_composite_extract(uint32_t obj_id, std::initializer_list<uint32_t> chain) {
  uint32_t type_id = _module.get_type_id(obj_id);
  nassertr(_module.get_kind(type_id) != SpirVModule::IdKind::POINTER_TYPE, 0);

  for (auto index : chain) {
    type_id = _module.get_composite_member_type_id(type_id, index);
    nassertr(type_id != 0, 0);
  }

  uint32_t id = allocate_id();
  Instruction op(spv::OpCompositeExtract, {type_id, id, obj_id});
  op.args.insert(op.args.end(), chain.begin(), chain.end());
  insert(std::move(op));
  return id;
}

/**
 * Inserts an OpCompositeInsert.
 */
uint32_t SpirVBuilder::
op_composite_insert(uint32_t obj_id, uint32_t composite_id, std::initializer_list<uint32_t> chain) {
  uint32_t type_id = _module.get_type_id(composite_id);

  uint32_t id = allocate_id();
  Instruction op(spv::OpCompositeInsert, {type_id, id, obj_id, composite_id});
  op.args.insert(op.args.end(), chain.begin(), chain.end());
  insert(std::move(op));
  return id;
}

/**
 * Inserts a comparison op, taking two operands and returning a bool.
 * At the moment, only works on scalars.
 */
uint32_t SpirVBuilder::
op_compare(spv::Op opcode, uint32_t obj1, uint32_t obj2) {
  uint32_t id = allocate_id();
  uint32_t type_id = _module.define_type(ShaderType::BOOL);

  insert(opcode, {type_id, id, obj1, obj2});
  return id;
}

/**
 * Insert a conversion op to the given scalar type.  May be a scalar or vector.
 * If it is already of the given scalar type, does nothing.
 */
uint32_t SpirVBuilder::
op_convert(ShaderType::ScalarType new_scalar_type, uint32_t value) {
  const ShaderType *value_type = _module.resolve_value_type(value);
  nassertr(value_type != nullptr, 0u);

  const ShaderType *new_type = nullptr;
  uint32_t new_type_id = 0;
  ShaderType::ScalarType old_scalar_type = ShaderType::ST_float;

  if (const ShaderType::Scalar *scalar = value_type->as_scalar()) {
    old_scalar_type = scalar->get_scalar_type();
    if (old_scalar_type == new_scalar_type) {
      // No conversion needed.  An id pushed for the conversion result would
      // silently transfer to the next unrelated allocation, so pushing one
      // before a conversion that turns out to be a no-op is an error.
      nassertr(_next_id == 0, value);
      return value;
    }
    new_type = ShaderType::register_type(ShaderType::Scalar(new_scalar_type));
  }
  else if (const ShaderType::Vector *vector = value_type->as_vector()) {
    old_scalar_type = vector->get_scalar_type();
    if (old_scalar_type == new_scalar_type) {
      // See above.
      nassertr(_next_id == 0, value);
      return value;
    }
    new_type = ShaderType::register_type(ShaderType::Vector(new_scalar_type, vector->get_num_components()));
  }
  else {
    nassertr_always(false, 0);
  }

  bool old_float = old_scalar_type == ShaderType::ST_float
                || old_scalar_type == ShaderType::ST_double;
  bool new_float = new_scalar_type == ShaderType::ST_float
                || new_scalar_type == ShaderType::ST_double;

  uint32_t id = allocate_id();

  if (old_scalar_type == ShaderType::ST_bool && new_scalar_type != ShaderType::ST_bool) {
    uint32_t true_value, false_value;
    if (new_float) {
      true_value = define_float_constant(1.0f);
      false_value = define_float_constant(0.0f);
    } else {
      const ShaderType *scalar_type = ShaderType::register_type(ShaderType::Scalar(new_scalar_type));
      true_value = define_constant(scalar_type, 1u);
      false_value = define_constant(scalar_type, 0u);
    }
    if (const ShaderType::Vector *vector = new_type->as_vector()) {
      true_value = op_composite_construct(new_type, pvector<uint32_t>(vector->get_num_components(), true_value));
      false_value = op_composite_construct(new_type, pvector<uint32_t>(vector->get_num_components(), false_value));
    }
    push_id(id);
    return op_select(value, true_value, false_value);
  }

  if (new_scalar_type == ShaderType::ST_bool && old_scalar_type != ShaderType::ST_bool) {
    // Conversion to bool is the same as != 0
    uint32_t null_value = define_null_constant(value_type);
    push_id(id);
    return op_compare(old_float ? spv::OpFUnordNotEqual : spv::OpINotEqual, value, null_value);
  }

  new_type_id = _module.define_type(new_type);

  // Determine which conversion instruction to use.
  spv::Op opcode;
  if (old_float && new_float) {
    opcode = spv::OpFConvert;
  }
  else if (old_float) {
    bool new_signed = new_scalar_type == ShaderType::ST_int;
    opcode = new_signed ? spv::OpConvertFToS : spv::OpConvertFToU;
  }
  else if (new_float) {
    bool old_signed = old_scalar_type == ShaderType::ST_int;
    opcode = old_signed ? spv::OpConvertSToF : spv::OpConvertUToF;
  }
  else {
    // Equal scalar types were already handled by the early returns above, so
    // this is int<->uint; assuming it's the same bit width, for now.
    opcode = spv::OpBitcast;
  }

  insert(opcode, {new_type_id, id, value});
  return id;
}

/**
 * Insert an OpBitcast.
 */
uint32_t SpirVBuilder::
op_bitcast(const ShaderType *new_type, uint32_t value) {
  uint32_t id = allocate_id();
  uint32_t new_type_id = _module.define_type(new_type);

  insert(spv::OpBitcast, {new_type_id, id, value});
  return id;
}

/**
 * Inserts a matrix transpose instruction.
 */
uint32_t SpirVBuilder::
op_transpose(uint32_t obj) {
  const ShaderType *type = _module.resolve_value_type(obj);
  uint32_t type_id = _module.get_type_id(obj);

  uint32_t id = allocate_id();

  if (const ShaderType::Matrix *matrix = type->as_matrix()) {
    if (matrix->get_num_rows() != matrix->get_num_columns()) {
      type = ShaderType::register_type(ShaderType::Matrix(matrix->get_scalar_type(), matrix->get_num_columns(), matrix->get_num_rows()));
      type_id = _module.define_type(type);
    }
  } else {
    nassert_raise("OpTranspose requires a matrix");
  }

  insert(spv::OpTranspose, {type_id, id, obj});
  return id;
}

/**
 * Inserts a scalar or vector addition instruction.
 */
uint32_t SpirVBuilder::
op_add(uint32_t left, uint32_t right) {
  nassertr(left != 0u, 0u);
  nassertr(right != 0u, 0u);
  const ShaderType *left_type = _module.resolve_value_type(left);
  nassertr(left_type != nullptr, 0u);
#ifndef NDEBUG
  const ShaderType *right_type = _module.resolve_value_type(right);
  nassertr(right_type != nullptr, 0u);
#endif

  ShaderType::ScalarType left_scalar_type;
#ifndef NDEBUG
  ShaderType::ScalarType right_scalar_type;
#endif
  if (const ShaderType::Scalar *scalar = left_type->as_scalar()) {
    left_scalar_type = scalar->get_scalar_type();

#ifndef NDEBUG
    const ShaderType::Scalar *right_scalar = right_type->as_scalar();
    nassertr(right_scalar != nullptr, 0);
    right_scalar_type = right_scalar->get_scalar_type();
#endif
  }
  else if (const ShaderType::Vector *vector = left_type->as_vector()) {
    left_scalar_type = vector->get_scalar_type();

#ifndef NDEBUG
    const ShaderType::Vector *right_vector = right_type->as_vector();
    nassertr(right_vector != nullptr, 0);
    right_scalar_type = right_vector->get_scalar_type();
#endif
  }
  else {
    nassertr(false, 0);
    return 0u;
  }

  spv::Op opcode;
  if (left_scalar_type == ShaderType::ST_float || left_scalar_type == ShaderType::ST_double) {
    opcode = spv::OpFAdd;
#ifndef NDEBUG
    nassertr(right_scalar_type == left_scalar_type, 0);
#endif
  } else {
    opcode = spv::OpIAdd;
#ifndef NDEBUG
    nassertr(right_scalar_type != ShaderType::ST_float && right_scalar_type != ShaderType::ST_double, 0);
#endif
  }

  uint32_t id = allocate_id();
  insert(opcode, {_module.get_type_id(left), id, left, right});
  return id;
}

/**
 * Inserts a scalar or vector subtraction instruction.
 */
uint32_t SpirVBuilder::
op_sub(uint32_t left, uint32_t right) {
  nassertr(left != 0u, 0u);
  nassertr(right != 0u, 0u);
  const ShaderType *left_type = _module.resolve_value_type(left);
  nassertr(left_type != nullptr, 0u);
  nassertr(left_type == _module.resolve_value_type(right), 0);

  ShaderType::ScalarType scalar_type;
  if (const ShaderType::Scalar *scalar = left_type->as_scalar()) {
    scalar_type = scalar->get_scalar_type();
  }
  else if (const ShaderType::Vector *vector = left_type->as_vector()) {
    scalar_type = vector->get_scalar_type();
  }
  else {
    nassertr(false, 0);
    return 0u;
  }

  spv::Op opcode;
  if (scalar_type == ShaderType::ST_float || scalar_type == ShaderType::ST_double) {
    opcode = spv::OpFSub;
  } else {
    opcode = spv::OpISub;
  }

  uint32_t id = allocate_id();
  insert(opcode, {_module.get_type_id(left), id, left, right});
  return id;
}

/**
 * Inserts a scalar division instruction.
 */
uint32_t SpirVBuilder::
op_div(uint32_t left, uint32_t right) {
  nassertr(left != 0u, 0u);
  nassertr(right != 0u, 0u);
  nassertr(_module.resolve_value_type(left) == _module.resolve_value_type(right), 0);

  uint32_t id = allocate_id();
  insert(spv::OpFDiv, {_module.get_type_id(left), id, left, right});
  return id;
}

/**
 * Inserts a vector dot instruction.
 */
uint32_t SpirVBuilder::
op_dot(uint32_t left, uint32_t right) {
  nassertr(left != 0u, 0u);
  nassertr(right != 0u, 0u);
  const ShaderType *left_type = _module.resolve_value_type(left);
  nassertr(left_type != nullptr, 0u);
  nassertr(left_type == _module.resolve_value_type(right), 0u);

  uint32_t id = allocate_id();

  const ShaderType::Vector *vector = left_type->as_vector();
  nassertr(vector != nullptr, 0u);

  const ShaderType *type = ShaderType::register_type(ShaderType::Scalar(vector->get_scalar_type()));
  uint32_t type_id = _module.define_type(type);

  insert(spv::OpDot, {type_id, id, left, right});
  return id;
}

/**
 * Inserts a negation instruction.
 */
uint32_t SpirVBuilder::
op_negate(uint32_t value) {
  nassertr(value != 0u, 0u);

  uint32_t id = allocate_id();
  insert(spv::OpFNegate, {_module.get_type_id(value), id, value});
  return id;
}

/**
 * Inserts a multiplication instruction.
 */
uint32_t SpirVBuilder::
op_multiply(uint32_t left, uint32_t right) {
  nassertr(left != 0u, 0u);
  nassertr(right != 0u, 0u);
  const ShaderType *left_type = _module.resolve_value_type(left);
  const ShaderType *right_type = _module.resolve_value_type(right);
  nassertr(left_type != nullptr, 0u);
  nassertr(right_type != nullptr, 0u);

  uint32_t id = allocate_id();

  uint32_t type_id;
  spv::Op opcode;

  if (const ShaderType::Scalar *lscalar = left_type->as_scalar()) {
    if (const ShaderType::Scalar *rscalar = right_type->as_scalar()) {
      nassertr(lscalar == rscalar, 0);
      opcode = spv::OpFMul;
      type_id = _module.get_type_id(left);
    }
    else if (const ShaderType::Vector *rvector = right_type->as_vector()) {
      nassertr(lscalar->get_scalar_type() == rvector->get_scalar_type(), 0);
      opcode = spv::OpVectorTimesScalar;
      type_id = _module.get_type_id(right);
      std::swap(left, right);
    }
    else if (const ShaderType::Matrix *rmatrix = right_type->as_matrix()) {
      nassertr(lscalar->get_scalar_type() == rmatrix->get_scalar_type(), 0);
      opcode = spv::OpMatrixTimesScalar;
      type_id = _module.get_type_id(right);
      std::swap(left, right);
    }
    else {
      nassert_raise("right side of multiplication is not a scalar, vector or matrix");
      return 0u;
    }
  }
  else if (const ShaderType::Vector *lvector = left_type->as_vector()) {
    if (const ShaderType::Scalar *rscalar = right_type->as_scalar()) {
      nassertr(lvector->get_scalar_type() == rscalar->get_scalar_type(), 0);
      opcode = spv::OpVectorTimesScalar;
      type_id = _module.get_type_id(left);
    }
    else if (right_type->as_vector() != nullptr) {
      nassert_raise("op_multiply for vector times vector is ambiguous");
      return 0u;
    }
    else if (const ShaderType::Matrix *rmatrix = right_type->as_matrix()) {
      nassertr(lvector->get_scalar_type() == rmatrix->get_scalar_type(), 0);
      nassertr(lvector->get_num_components() == rmatrix->get_num_columns(), 0);
      opcode = spv::OpVectorTimesMatrix;
      const ShaderType *type = ShaderType::register_type(ShaderType::Vector(lvector->get_scalar_type(), rmatrix->get_num_rows()));
      type_id = _module.define_type(type);
    }
    else {
      nassert_raise("right side of multiplication is not a scalar, vector or matrix");
      return 0u;
    }
  }
  else if (const ShaderType::Matrix *lmatrix = left_type->as_matrix()) {
    if (const ShaderType::Scalar *rscalar = right_type->as_scalar()) {
      nassertr(lmatrix->get_scalar_type() == rscalar->get_scalar_type(), 0);
      opcode = spv::OpMatrixTimesScalar;
      type_id = _module.get_type_id(left);
    }
    else if (const ShaderType::Vector *rvector = right_type->as_vector()) {
      nassertr(lmatrix->get_scalar_type() == rvector->get_scalar_type(), 0);
      nassertr(lmatrix->get_num_rows() == rvector->get_num_components(), 0);
      opcode = spv::OpMatrixTimesVector;
      const ShaderType *type = ShaderType::register_type(ShaderType::Vector(lmatrix->get_scalar_type(), lmatrix->get_num_columns()));
      type_id = _module.define_type(type);
    }
    else if (const ShaderType::Matrix *rmatrix = right_type->as_matrix()) {
      nassertr(lmatrix->get_scalar_type() == rmatrix->get_scalar_type(), 0);
      opcode = spv::OpMatrixTimesMatrix;
      const ShaderType *type = ShaderType::register_type(ShaderType::Matrix(lmatrix->get_scalar_type(), rmatrix->get_num_rows(), lmatrix->get_num_columns()));
      type_id = _module.define_type(type);
    }
    else {
      nassert_raise("right side of multiplication is not a scalar, vector or matrix");
      return 0u;
    }
  }
  else {
    nassert_raise("left side of multiplication is not a scalar, vector or matrix");
    return 0u;
  }

  insert(opcode, {type_id, id, left, right});
  return id;
}

/**
 * Inserts an OpImageSampleExplicitLod or OpImageSampleImplicitLod, depending
 * on whether a Lod is included.
 */
uint32_t SpirVBuilder::
op_image_sample(uint32_t image, uint32_t coord, uint32_t operands, const uint32_t *ids) {
  uint32_t id = allocate_id();

  const ShaderType *image_type = _module.resolve_value_type(image);
  nassertr(image_type != nullptr, 0u);
  const ShaderType::SampledImage *sampled_image = image_type->as_sampled_image();
  nassertr(sampled_image != nullptr, 0u);

  const ShaderType *type = ShaderType::register_type(ShaderType::Vector(sampled_image->get_sampled_type(), 4));
  uint32_t type_id = _module.define_type(type);

  spv::Op opcode;
  if (operands & spv::ImageOperandsLodMask) {
    opcode = spv::OpImageSampleExplicitLod;
  } else {
    opcode = spv::OpImageSampleImplicitLod;
  }

  uint32_t num_ids = ids != nullptr ? ::count_bits_in_word(operands) : 0u;
  Instruction op(opcode, {type_id, id, image, coord, operands});
  if (num_ids > 0) {
    op.args.insert(op.args.end(), ids, ids + num_ids);
  }
  insert(std::move(op));
  return id;
}

/**
 * Inserts an OpFunctionCall passing no arguments.
 */
uint32_t SpirVBuilder::
op_function_call(uint32_t func_id) {
  uint32_t id = allocate_id();

  nassertr(_module.get_kind(func_id) == SpirVModule::IdKind::FUNCTION, 0);

  uint32_t function_type_id = _module.get_type_id(func_id);
  nassertr(_module.get_kind(function_type_id) == SpirVModule::IdKind::FUNCTION_TYPE, 0);

  uint32_t return_type_id = _module.get_type_id(function_type_id);

  insert(spv::OpFunctionCall, {return_type_id, id, func_id});
  return id;
}

/**
 * Starts a new function returning void and taking no arguments, at the end
 * of the module, positioning the cursor inside it.
 */
uint32_t SpirVBuilder::
op_function(const ShaderType *return_type) {
  uint32_t id = allocate_id();

  uint32_t function_type_id = _module.define_function_type(return_type);
  uint32_t return_type_id = _module.get_type_id(function_type_id);
  nassertr(return_type_id != 0, 0);

  pvector<Function> &functions = _module.modify_functions();
  functions.push_back(Function {id, {}});
  _function_index = functions.size() - 1;
  _index = 0;
  _current_function_id = id;

  insert(spv::OpFunction, {return_type_id, id, 0, function_type_id});
  return id;
}

/**
 * Inserts an OpLabel.
 */
uint32_t SpirVBuilder::
op_label() {
  uint32_t id = allocate_id();
  insert(spv::OpLabel, {id});
  return id;
}

/**
 * Inserts an OpBranch.
 */
void SpirVBuilder::
op_branch(uint32_t target) {
  insert(spv::OpBranch, {target});
}

/**
 * Inserts an OpReturn.
 */
void SpirVBuilder::
op_return() {
  insert(spv::OpReturn, {});
}

/**
 * Inserts an OpKill.
 */
void SpirVBuilder::
op_kill() {
  insert(spv::OpKill, {});
}

/**
 * Inserts an OpFunctionEnd, ending the current function.
 */
void SpirVBuilder::
op_function_end() {
  insert(spv::OpFunctionEnd, {});
  _current_function_id = 0;
  _function_index = (size_t)-1;
  _index = 0;
}

/**
 * Begins an "if" branch.
 * The return value should be passed to branch_endif().
 */
uint32_t SpirVBuilder::
branch_if(uint32_t cond) {
  // This helper allocates two label ids, so it cannot honor a pending
  // push_id (there is no way to tell which allocation it was meant for).
  nassertr(_next_id == 0, 0);

  uint32_t true_label = allocate_id();
  uint32_t false_label = allocate_id();

  insert(spv::OpSelectionMerge, {false_label, (uint32_t)spv::SelectionControlMaskNone});
  insert(spv::OpBranchConditional, {cond, true_label, false_label});
  insert(spv::OpLabel, {true_label});
  return false_label;
}

/**
 * Ends an "if" branch.
 */
void SpirVBuilder::
branch_endif(uint32_t false_label) {
  insert(spv::OpLabel, {false_label});
}
