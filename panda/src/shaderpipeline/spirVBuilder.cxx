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

#include "GLSL.std.450.h"

#ifndef NDEBUG
/**
 * Returns the scalar type of a scalar, vector or matrix ShaderType, or
 * ST_unknown.  Used by the float-only assertions below.
 */
static ShaderType::ScalarType
get_element_scalar_type(const ShaderType *type) {
  if (type != nullptr) {
    if (const ShaderType::Scalar *scalar = type->as_scalar()) {
      return scalar->get_scalar_type();
    }
    if (const ShaderType::Vector *vector = type->as_vector()) {
      return vector->get_scalar_type();
    }
    if (const ShaderType::Matrix *matrix = type->as_matrix()) {
      return matrix->get_scalar_type();
    }
  }
  return ShaderType::ST_unknown;
}

/**
 * Returns true if the given scalar type is a floating-point type.
 */
static bool
is_float_scalar_type(ShaderType::ScalarType scalar_type) {
  return scalar_type == ShaderType::ST_float ||
         scalar_type == ShaderType::ST_double;
}
#endif  // NDEBUG

/**
 * Returns true if the given opcode terminates a block, so that no further
 * instruction may follow it before the next OpLabel.
 */
static bool
is_block_terminator(spv::Op opcode) {
  switch (opcode) {
  case spv::OpBranch:
  case spv::OpBranchConditional:
  case spv::OpSwitch:
  case spv::OpReturn:
  case spv::OpReturnValue:
  case spv::OpKill:
  case spv::OpUnreachable:
  case spv::OpTerminateInvocation:
  case spv::OpIgnoreIntersectionKHR:
  case spv::OpTerminateRayKHR:
  case spv::OpEmitMeshTasksEXT:
    return true;
  default:
    return false;
  }
}

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
SpirVBuilder(SpirVModule &module, SpirVId function_id, size_t index) :
  _module(module) {
  set_insertion_point(function_id, index);
}

/**
 * Positions the cursor at the given instruction index of the given function.
 */
void SpirVBuilder::
set_insertion_point(SpirVId function_id, size_t index) {
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
  _current_function_id = SpirVId();
}

/**
 * Positions the cursor at the top of the given function's body: after the
 * OpLabel opening the entry block and past the OpVariable declarations that
 * SPIR-V requires to lead it, so that inserted code may not accidentally
 * precede a variable declaration.
 */
void SpirVBuilder::
set_insertion_point_to_body_start(SpirVId function_id) {
  set_insertion_point(function_id, 0);

  Function *function = get_current_function();
  nassertv(function != nullptr);
  const pvector<Instruction> &instructions = function->instructions;

  size_t index = 0;
  while (index < instructions.size() &&
         instructions[index].opcode != spv::OpLabel) {
    ++index;
  }
  nassertv(index < instructions.size());
  ++index;

  while (index < instructions.size()) {
    spv::Op opcode = instructions[index].opcode;
    if (opcode != spv::OpVariable && opcode != spv::OpLine &&
        opcode != spv::OpNoLine && opcode != spv::OpNop) {
      break;
    }
    ++index;
  }
  _index = index;
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
SpirVId SpirVBuilder::
op_load(SpirVId var_id, spv::MemoryAccessMask access) {
  SpirVId type_id = _module.unwrap_pointer_type(_module.get_type_id(var_id));

  SpirVId id = allocate_id();
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
op_store(SpirVId var_id, SpirVId value) {
  insert(spv::OpStore, {var_id, value});
}

/**
 * Inserts an OpSelect.
 */
SpirVId SpirVBuilder::
op_select(SpirVId cond, SpirVId obj1, SpirVId obj2) {
  SpirVId type_id = _module.get_type_id(obj1);
  nassertr(type_id == _module.get_type_id(obj2), SpirVId());

  SpirVId id = allocate_id();
  insert(spv::OpSelect, {type_id, id, cond, obj1, obj2});
  return id;
}

/**
 * Inserts an OpAccessChain with the given base id (which must be a pointer)
 * and constant ids containing the various member/array indices.
 */
SpirVId SpirVBuilder::
op_access_chain(SpirVId var_id, std::initializer_list<SpirVId> chain) {
  SpirVId base_pointer_type_id = _module.get_type_id(var_id);
  nassertr(_module.get_kind(base_pointer_type_id) == SpirVModule::IdKind::POINTER_TYPE, SpirVId());

  // The chain's pointer type must use the storage class as declared on the
  // base pointer type, not the canonicalized one that get_storage_class
  // reports: chaining into a legacy-style buffer block (Uniform storage
  // class with the BufferBlock decoration) must yield Uniform-class
  // pointers, since declaring a StorageBuffer-class pointer requires
  // SPV_KHR_storage_buffer_storage_class before SPIR-V 1.3.
  const SpirVModule::Instruction *base_decl = _module.get_decl(base_pointer_type_id);
  nassertr(base_decl != nullptr && base_decl->args.size() >= 3, SpirVId());
  spv::StorageClass storage_class = (spv::StorageClass)base_decl->args[1];

  SpirVId type_id = _module.unwrap_pointer_type(base_pointer_type_id);
  for (auto index_id : chain) {
    const Instruction *type_decl = _module.get_decl(type_id);
    nassertr(type_decl != nullptr, SpirVId());

    if (type_decl->opcode == spv::OpTypeStruct) {
      uint32_t member_index = _module.resolve_constant(index_id);
      type_id = _module.get_composite_member_type_id(type_id, member_index);
    } else {
      // Array, matrix, or vector
      type_id = _module.get_composite_member_type_id(type_id, 0);
    }
    nassertr(type_id != 0, SpirVId());
  }

  SpirVId id = allocate_id();
  SpirVId pointer_type_id = _module.define_pointer_type(type_id, storage_class);

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
SpirVId SpirVBuilder::
op_vector_shuffle(SpirVId vec1, SpirVId vec2, const pvector<uint32_t> &components) {
  const ShaderType *vec1_shader_type = _module.resolve_value_type(vec1);
  const ShaderType *vec2_shader_type = _module.resolve_value_type(vec2);
  nassertr(vec1_shader_type != nullptr && vec2_shader_type != nullptr, SpirVId());
  const ShaderType::Vector *vec1_type = vec1_shader_type->as_vector();
  const ShaderType::Vector *vec2_type = vec2_shader_type->as_vector();
  nassertr(vec1_type != nullptr && vec2_type != nullptr, SpirVId());
  nassertr(vec1_type->get_scalar_type() == vec2_type->get_scalar_type(), SpirVId());

  SpirVId id = allocate_id();

  const ShaderType *result_type = ShaderType::register_type(ShaderType::Vector(vec1_type->get_scalar_type(), components.size()));
  SpirVId type_id = _module.define_type(result_type);

  Instruction op(spv::OpVectorShuffle, {type_id, id, vec1, vec2});
  op.args.insert(op.args.end(), components.begin(), components.end());
  insert(std::move(op));
  return id;
}

/**
 * Convenience swizzle of a single vector: extracts the given components, in
 * the given order, into a new vector (or a scalar, if only one component is
 * given).
 */
SpirVId SpirVBuilder::
op_swizzle(SpirVId vec, std::initializer_list<uint32_t> components) {
  nassertr(components.size() > 0, SpirVId());
  if (components.size() == 1) {
    return op_composite_extract(vec, {*components.begin()});
  }
  return op_vector_shuffle(vec, vec,
    pvector<uint32_t>(components.begin(), components.end()));
}

/**
 * Constructs a composite with the given type from the given constituents.
 */
SpirVId SpirVBuilder::
op_composite_construct(const ShaderType *type, const pvector<SpirVId> &constituents) {
  SpirVId id = allocate_id();
  SpirVId type_id = _module.define_type(type);

  Instruction op(spv::OpCompositeConstruct, {type_id, id});
  op.args.insert(op.args.end(), constituents.begin(), constituents.end());
  insert(std::move(op));
  return id;
}

/**
 * Inserts an OpCompositeExtract.
 */
SpirVId SpirVBuilder::
op_composite_extract(SpirVId obj_id, std::initializer_list<uint32_t> chain) {
  SpirVId type_id = _module.get_type_id(obj_id);
  nassertr(_module.get_kind(type_id) != SpirVModule::IdKind::POINTER_TYPE, SpirVId());

  for (auto index : chain) {
    type_id = _module.get_composite_member_type_id(type_id, index);
    nassertr(type_id != 0, SpirVId());
  }

  SpirVId id = allocate_id();
  Instruction op(spv::OpCompositeExtract, {type_id, id, obj_id});
  op.args.insert(op.args.end(), chain.begin(), chain.end());
  insert(std::move(op));
  return id;
}

/**
 * Inserts an OpCompositeInsert.
 */
SpirVId SpirVBuilder::
op_composite_insert(SpirVId obj_id, SpirVId composite_id, std::initializer_list<uint32_t> chain) {
  SpirVId type_id = _module.get_type_id(composite_id);

  SpirVId id = allocate_id();
  Instruction op(spv::OpCompositeInsert, {type_id, id, obj_id, composite_id});
  op.args.insert(op.args.end(), chain.begin(), chain.end());
  insert(std::move(op));
  return id;
}

/**
 * Inserts a comparison op, taking two operands and returning a bool.
 * At the moment, only works on scalars.
 */
SpirVId SpirVBuilder::
op_compare(spv::Op opcode, SpirVId obj1, SpirVId obj2) {
  SpirVId id = allocate_id();
  SpirVId type_id = _module.define_type(ShaderType::BOOL);

  insert(opcode, {type_id, id, obj1, obj2});
  return id;
}

/**
 * Insert a conversion op to the given scalar type.  May be a scalar or vector.
 * If it is already of the given scalar type, does nothing.
 */
SpirVId SpirVBuilder::
op_convert(ShaderType::ScalarType new_scalar_type, SpirVId value) {
  const ShaderType *value_type = _module.resolve_value_type(value);
  nassertr(value_type != nullptr, SpirVId());

  const ShaderType *new_type = nullptr;
  ShaderType::ScalarType old_scalar_type = ShaderType::ST_float;

  if (const ShaderType::Scalar *scalar = value_type->as_scalar()) {
    old_scalar_type = scalar->get_scalar_type();
    if (old_scalar_type == new_scalar_type) {
      // No conversion needed.
      return value;
    }
    new_type = ShaderType::register_type(ShaderType::Scalar(new_scalar_type));
  }
  else if (const ShaderType::Vector *vector = value_type->as_vector()) {
    old_scalar_type = vector->get_scalar_type();
    if (old_scalar_type == new_scalar_type) {
      // No conversion needed.
      return value;
    }
    new_type = ShaderType::register_type(ShaderType::Vector(new_scalar_type, vector->get_num_components()));
  }
  else {
    nassertr_always(false, SpirVId());
  }

  bool old_float = old_scalar_type == ShaderType::ST_float
                || old_scalar_type == ShaderType::ST_double;
  bool new_float = new_scalar_type == ShaderType::ST_float
                || new_scalar_type == ShaderType::ST_double;

  if (old_scalar_type == ShaderType::ST_bool && new_scalar_type != ShaderType::ST_bool) {
    SpirVId true_value, false_value;
    if (new_float) {
      true_value = define_float_constant(1.0f);
      false_value = define_float_constant(0.0f);
    } else {
      const ShaderType *scalar_type = ShaderType::register_type(ShaderType::Scalar(new_scalar_type));
      true_value = define_constant(scalar_type, 1u);
      false_value = define_constant(scalar_type, 0u);
    }
    if (const ShaderType::Vector *vector = new_type->as_vector()) {
      true_value = op_composite_construct(new_type, pvector<SpirVId>(vector->get_num_components(), true_value));
      false_value = op_composite_construct(new_type, pvector<SpirVId>(vector->get_num_components(), false_value));
    }
    return op_select(value, true_value, false_value);
  }

  if (new_scalar_type == ShaderType::ST_bool && old_scalar_type != ShaderType::ST_bool) {
    // Conversion to bool is the same as != 0
    SpirVId null_value = define_null_constant(value_type);
    return op_compare(old_float ? spv::OpFUnordNotEqual : spv::OpINotEqual, value, null_value);
  }

  SpirVId new_type_id = _module.define_type(new_type);

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

  SpirVId id = allocate_id();
  insert(opcode, {new_type_id, id, value});
  return id;
}

/**
 * Insert an OpBitcast.
 */
SpirVId SpirVBuilder::
op_bitcast(const ShaderType *new_type, SpirVId value) {
  SpirVId id = allocate_id();
  SpirVId new_type_id = _module.define_type(new_type);

  insert(spv::OpBitcast, {new_type_id, id, value});
  return id;
}

/**
 * Inserts a matrix transpose instruction.
 */
SpirVId SpirVBuilder::
op_transpose(SpirVId obj) {
  const ShaderType *type = _module.resolve_value_type(obj);
  SpirVId type_id = _module.get_type_id(obj);

  SpirVId id = allocate_id();

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
SpirVId SpirVBuilder::
op_add(SpirVId left, SpirVId right) {
  nassertr(left != 0u, SpirVId());
  nassertr(right != 0u, SpirVId());
  const ShaderType *left_type = _module.resolve_value_type(left);
  nassertr(left_type != nullptr, SpirVId());
#ifndef NDEBUG
  const ShaderType *right_type = _module.resolve_value_type(right);
  nassertr(right_type != nullptr, SpirVId());
#endif

  ShaderType::ScalarType left_scalar_type;
#ifndef NDEBUG
  ShaderType::ScalarType right_scalar_type;
#endif
  if (const ShaderType::Scalar *scalar = left_type->as_scalar()) {
    left_scalar_type = scalar->get_scalar_type();

#ifndef NDEBUG
    const ShaderType::Scalar *right_scalar = right_type->as_scalar();
    nassertr(right_scalar != nullptr, SpirVId());
    right_scalar_type = right_scalar->get_scalar_type();
#endif
  }
  else if (const ShaderType::Vector *vector = left_type->as_vector()) {
    left_scalar_type = vector->get_scalar_type();

#ifndef NDEBUG
    const ShaderType::Vector *right_vector = right_type->as_vector();
    nassertr(right_vector != nullptr, SpirVId());
    right_scalar_type = right_vector->get_scalar_type();
#endif
  }
  else {
    nassertr(false, SpirVId());
    return SpirVId();
  }

  spv::Op opcode;
  if (left_scalar_type == ShaderType::ST_float || left_scalar_type == ShaderType::ST_double) {
    opcode = spv::OpFAdd;
#ifndef NDEBUG
    nassertr(right_scalar_type == left_scalar_type, SpirVId());
#endif
  } else {
    opcode = spv::OpIAdd;
#ifndef NDEBUG
    nassertr(right_scalar_type != ShaderType::ST_float && right_scalar_type != ShaderType::ST_double, SpirVId());
#endif
  }

  SpirVId id = allocate_id();
  insert(opcode, {_module.get_type_id(left), id, left, right});
  return id;
}

/**
 * Inserts a scalar or vector subtraction instruction.
 */
SpirVId SpirVBuilder::
op_sub(SpirVId left, SpirVId right) {
  nassertr(left != 0u, SpirVId());
  nassertr(right != 0u, SpirVId());
  const ShaderType *left_type = _module.resolve_value_type(left);
  nassertr(left_type != nullptr, SpirVId());
  nassertr(left_type == _module.resolve_value_type(right), SpirVId());

  ShaderType::ScalarType scalar_type;
  if (const ShaderType::Scalar *scalar = left_type->as_scalar()) {
    scalar_type = scalar->get_scalar_type();
  }
  else if (const ShaderType::Vector *vector = left_type->as_vector()) {
    scalar_type = vector->get_scalar_type();
  }
  else {
    nassertr(false, SpirVId());
    return SpirVId();
  }

  spv::Op opcode;
  if (scalar_type == ShaderType::ST_float || scalar_type == ShaderType::ST_double) {
    opcode = spv::OpFSub;
  } else {
    opcode = spv::OpISub;
  }

  SpirVId id = allocate_id();
  insert(opcode, {_module.get_type_id(left), id, left, right});
  return id;
}

/**
 * Inserts a scalar division instruction.  Floating-point only, for now; see
 * op_convert for producing a float operand.
 */
SpirVId SpirVBuilder::
op_div(SpirVId left, SpirVId right) {
  nassertr(left != 0u, SpirVId());
  nassertr(right != 0u, SpirVId());
#ifndef NDEBUG
  const ShaderType *left_type = _module.resolve_value_type(left);
  nassertr(left_type == _module.resolve_value_type(right), SpirVId());
  nassertr(is_float_scalar_type(get_element_scalar_type(left_type)), SpirVId());
#endif

  SpirVId id = allocate_id();
  insert(spv::OpFDiv, {_module.get_type_id(left), id, left, right});
  return id;
}

/**
 * Inserts a vector dot instruction.
 */
SpirVId SpirVBuilder::
op_dot(SpirVId left, SpirVId right) {
  nassertr(left != 0u, SpirVId());
  nassertr(right != 0u, SpirVId());
  const ShaderType *left_type = _module.resolve_value_type(left);
  nassertr(left_type != nullptr, SpirVId());
  nassertr(left_type == _module.resolve_value_type(right), SpirVId());

  const ShaderType::Vector *vector = left_type->as_vector();
  nassertr(vector != nullptr, SpirVId());
  nassertr(is_float_scalar_type(vector->get_scalar_type()), SpirVId());

  const ShaderType *type = ShaderType::register_type(ShaderType::Scalar(vector->get_scalar_type()));
  SpirVId type_id = _module.define_type(type);

  SpirVId id = allocate_id();
  insert(spv::OpDot, {type_id, id, left, right});
  return id;
}

/**
 * Inserts a negation instruction.  Floating-point only, for now.
 */
SpirVId SpirVBuilder::
op_negate(SpirVId value) {
  nassertr(value != 0u, SpirVId());
  nassertr(is_float_scalar_type(get_element_scalar_type(
    _module.resolve_value_type(value))), SpirVId());

  SpirVId id = allocate_id();
  insert(spv::OpFNegate, {_module.get_type_id(value), id, value});
  return id;
}

/**
 * Inserts a multiplication instruction.
 */
SpirVId SpirVBuilder::
op_multiply(SpirVId left, SpirVId right) {
  nassertr(left != 0u, SpirVId());
  nassertr(right != 0u, SpirVId());
  const ShaderType *left_type = _module.resolve_value_type(left);
  const ShaderType *right_type = _module.resolve_value_type(right);
  nassertr(left_type != nullptr, SpirVId());
  nassertr(right_type != nullptr, SpirVId());

  SpirVId id = allocate_id();

  SpirVId type_id;
  spv::Op opcode;

  if (const ShaderType::Scalar *lscalar = left_type->as_scalar()) {
    if (const ShaderType::Scalar *rscalar = right_type->as_scalar()) {
      nassertr(lscalar == rscalar, SpirVId());
      // Floating-point only, for now.
      nassertr(is_float_scalar_type(lscalar->get_scalar_type()), SpirVId());
      opcode = spv::OpFMul;
      type_id = _module.get_type_id(left);
    }
    else if (const ShaderType::Vector *rvector = right_type->as_vector()) {
      nassertr(lscalar->get_scalar_type() == rvector->get_scalar_type(), SpirVId());
      opcode = spv::OpVectorTimesScalar;
      type_id = _module.get_type_id(right);
      std::swap(left, right);
    }
    else if (const ShaderType::Matrix *rmatrix = right_type->as_matrix()) {
      nassertr(lscalar->get_scalar_type() == rmatrix->get_scalar_type(), SpirVId());
      opcode = spv::OpMatrixTimesScalar;
      type_id = _module.get_type_id(right);
      std::swap(left, right);
    }
    else {
      nassert_raise("right side of multiplication is not a scalar, vector or matrix");
      return SpirVId();
    }
  }
  else if (const ShaderType::Vector *lvector = left_type->as_vector()) {
    if (const ShaderType::Scalar *rscalar = right_type->as_scalar()) {
      nassertr(lvector->get_scalar_type() == rscalar->get_scalar_type(), SpirVId());
      opcode = spv::OpVectorTimesScalar;
      type_id = _module.get_type_id(left);
    }
    else if (const ShaderType::Vector *rvector = right_type->as_vector()) {
      // Component-wise multiplication, with GLSL/Cg semantics.  For a dot
      // product, use op_dot instead.
      nassertr(left_type == right_type, SpirVId());
      // Floating-point only, for now.
      nassertr(is_float_scalar_type(lvector->get_scalar_type()), SpirVId());
      (void)rvector;
      opcode = spv::OpFMul;
      type_id = _module.get_type_id(left);
    }
    else if (const ShaderType::Matrix *rmatrix = right_type->as_matrix()) {
      nassertr(lvector->get_scalar_type() == rmatrix->get_scalar_type(), SpirVId());
      nassertr(lvector->get_num_components() == rmatrix->get_num_columns(), SpirVId());
      opcode = spv::OpVectorTimesMatrix;
      const ShaderType *type = ShaderType::register_type(ShaderType::Vector(lvector->get_scalar_type(), rmatrix->get_num_rows()));
      type_id = _module.define_type(type);
    }
    else {
      nassert_raise("right side of multiplication is not a scalar, vector or matrix");
      return SpirVId();
    }
  }
  else if (const ShaderType::Matrix *lmatrix = left_type->as_matrix()) {
    if (const ShaderType::Scalar *rscalar = right_type->as_scalar()) {
      nassertr(lmatrix->get_scalar_type() == rscalar->get_scalar_type(), SpirVId());
      opcode = spv::OpMatrixTimesScalar;
      type_id = _module.get_type_id(left);
    }
    else if (const ShaderType::Vector *rvector = right_type->as_vector()) {
      nassertr(lmatrix->get_scalar_type() == rvector->get_scalar_type(), SpirVId());
      nassertr(lmatrix->get_num_rows() == rvector->get_num_components(), SpirVId());
      opcode = spv::OpMatrixTimesVector;
      const ShaderType *type = ShaderType::register_type(ShaderType::Vector(lmatrix->get_scalar_type(), lmatrix->get_num_columns()));
      type_id = _module.define_type(type);
    }
    else if (const ShaderType::Matrix *rmatrix = right_type->as_matrix()) {
      nassertr(lmatrix->get_scalar_type() == rmatrix->get_scalar_type(), SpirVId());
      opcode = spv::OpMatrixTimesMatrix;
      const ShaderType *type = ShaderType::register_type(ShaderType::Matrix(lmatrix->get_scalar_type(), rmatrix->get_num_rows(), lmatrix->get_num_columns()));
      type_id = _module.define_type(type);
    }
    else {
      nassert_raise("right side of multiplication is not a scalar, vector or matrix");
      return SpirVId();
    }
  }
  else {
    nassert_raise("left side of multiplication is not a scalar, vector or matrix");
    return SpirVId();
  }

  insert(opcode, {type_id, id, left, right});
  return id;
}

/**
 * Inserts an OpImageSampleExplicitLod or OpImageSampleImplicitLod, depending
 * on whether a Lod is included.
 */
SpirVId SpirVBuilder::
op_image_sample(SpirVId image, SpirVId coord, uint32_t operands, const uint32_t *ids) {
  SpirVId id = allocate_id();

  const ShaderType *image_type = _module.resolve_value_type(image);
  nassertr(image_type != nullptr, SpirVId());
  const ShaderType::SampledImage *sampled_image = image_type->as_sampled_image();
  nassertr(sampled_image != nullptr, SpirVId());

  const ShaderType *type = ShaderType::register_type(ShaderType::Vector(sampled_image->get_sampled_type(), 4));
  SpirVId type_id = _module.define_type(type);

  spv::Op opcode;
  if (operands & spv::ImageOperandsLodMask) {
    opcode = spv::OpImageSampleExplicitLod;
  } else {
    opcode = spv::OpImageSampleImplicitLod;
  }

  // NOTE: this miscounts image operands that carry more than one id (Grad
  // has two); to be addressed when a caller needs those.
  uint32_t num_ids = ids != nullptr ? ::count_bits_in_word(operands) : 0u;
  Instruction op(opcode, {type_id, id, image, coord, operands});
  if (num_ids > 0) {
    op.args.insert(op.args.end(), ids, ids + num_ids);
  }
  insert(std::move(op));
  return id;
}

/**
 * Inserts an OpImageSampleProjImplicitLod, which divides the coordinate by
 * its last component before sampling.
 */
SpirVId SpirVBuilder::
op_image_sample_proj(SpirVId image, SpirVId coord) {
  const ShaderType *image_type = _module.resolve_value_type(image);
  nassertr(image_type != nullptr, SpirVId());
  const ShaderType::SampledImage *sampled_image = image_type->as_sampled_image();
  nassertr(sampled_image != nullptr, SpirVId());

  const ShaderType *type = ShaderType::register_type(ShaderType::Vector(sampled_image->get_sampled_type(), 4));
  SpirVId type_id = _module.define_type(type);

  SpirVId id = allocate_id();
  insert(spv::OpImageSampleProjImplicitLod, {type_id, id, image, coord});
  return id;
}

/**
 * Inserts an OpImageSampleDrefImplicitLod, which performs a depth comparison
 * against the given reference value.  The result is a scalar.
 */
SpirVId SpirVBuilder::
op_image_sample_dref(SpirVId image, SpirVId coord, SpirVId dref) {
  const ShaderType *image_type = _module.resolve_value_type(image);
  nassertr(image_type != nullptr, SpirVId());
  const ShaderType::SampledImage *sampled_image = image_type->as_sampled_image();
  nassertr(sampled_image != nullptr, SpirVId());

  const ShaderType *type = ShaderType::register_type(ShaderType::Scalar(sampled_image->get_sampled_type()));
  SpirVId type_id = _module.define_type(type);

  SpirVId id = allocate_id();
  insert(spv::OpImageSampleDrefImplicitLod, {type_id, id, image, coord, dref});
  return id;
}

/**
 * Inserts an OpImageSampleProjDrefImplicitLod: like op_image_sample_dref,
 * but the coordinate and reference value are divided by the coordinate's
 * last component first.
 */
SpirVId SpirVBuilder::
op_image_sample_proj_dref(SpirVId image, SpirVId coord, SpirVId dref) {
  const ShaderType *image_type = _module.resolve_value_type(image);
  nassertr(image_type != nullptr, SpirVId());
  const ShaderType::SampledImage *sampled_image = image_type->as_sampled_image();
  nassertr(sampled_image != nullptr, SpirVId());

  const ShaderType *type = ShaderType::register_type(ShaderType::Scalar(sampled_image->get_sampled_type()));
  SpirVId type_id = _module.define_type(type);

  SpirVId id = allocate_id();
  insert(spv::OpImageSampleProjDrefImplicitLod, {type_id, id, image, coord, dref});
  return id;
}

/**
 * Inserts an OpImageFetch, which reads a single texel using an (integer)
 * coordinate, without filtering.  The given image may be a sampled image, in
 * which case the image is first extracted from it.
 */
SpirVId SpirVBuilder::
op_image_fetch(SpirVId image, SpirVId coord) {
  const ShaderType *image_type = _module.resolve_value_type(image);
  nassertr(image_type != nullptr, SpirVId());

  ShaderType::ScalarType sampled_type;
  if (const ShaderType::SampledImage *sampled_image = image_type->as_sampled_image()) {
    // Extract the image from the sampled image.
    const Instruction *decl = _module.get_decl(_module.get_type_id(image));
    nassertr(decl != nullptr && decl->opcode == spv::OpTypeSampledImage, SpirVId());
    SpirVId image_type_id(decl->args[1]);

    SpirVId image_id = allocate_id();
    insert(spv::OpImage, {image_type_id, image_id, image});
    image = image_id;

    sampled_type = sampled_image->get_sampled_type();
  }
  else if (const ShaderType::Image *plain_image = image_type->as_image()) {
    sampled_type = plain_image->get_sampled_type();
  }
  else {
    nassert_raise("op_image_fetch requires an image or sampled image");
    return SpirVId();
  }

  const ShaderType *type = ShaderType::register_type(ShaderType::Vector(sampled_type, 4));
  SpirVId type_id = _module.define_type(type);

  SpirVId id = allocate_id();
  insert(spv::OpImageFetch, {type_id, id, image, coord});
  return id;
}

/**
 * Inserts an OpFunctionCall passing no arguments.
 */
SpirVId SpirVBuilder::
op_function_call(SpirVId func_id) {
  SpirVId id = allocate_id();

  nassertr(_module.get_kind(func_id) == SpirVModule::IdKind::FUNCTION, SpirVId());

  SpirVId function_type_id = _module.get_type_id(func_id);
  nassertr(_module.get_kind(function_type_id) == SpirVModule::IdKind::FUNCTION_TYPE, SpirVId());

  SpirVId return_type_id = _module.get_type_id(function_type_id);

  insert(spv::OpFunctionCall, {return_type_id, id, func_id});
  return id;
}

/**
 * Starts a new function returning void and taking no arguments, at the end
 * of the module, positioning the cursor inside it.
 */
SpirVId SpirVBuilder::
op_function(const ShaderType *return_type) {
  SpirVId id = allocate_id();

  SpirVId function_type_id = _module.define_function_type(return_type);
  SpirVId return_type_id = _module.get_type_id(function_type_id);
  nassertr(return_type_id != 0, SpirVId());

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
SpirVId SpirVBuilder::
op_label() {
  SpirVId id = allocate_id();
  insert(spv::OpLabel, {id});
  return id;
}

/**
 * Inserts an OpBranch.
 */
void SpirVBuilder::
op_branch(SpirVId target) {
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
  _current_function_id = SpirVId();
  _function_index = (size_t)-1;
  _index = 0;
}

/**
 * Begins an "if" branch.
 * The return value should be passed to branch_endif().
 */
SpirVId SpirVBuilder::
branch_if(SpirVId cond) {
  SpirVId true_label = allocate_id();
  SpirVId false_label = allocate_id();

  insert(spv::OpSelectionMerge, {false_label, (uint32_t)spv::SelectionControlMaskNone});
  insert(spv::OpBranchConditional, {cond, true_label, false_label});
  insert(spv::OpLabel, {true_label});
  return false_label;
}

/**
 * Ends an "if" branch.
 */
void SpirVBuilder::
branch_endif(SpirVId false_label) {
  // The block being ended must not fall through into the merge label; if the
  // code inserted since branch_if did not end with its own terminator (such
  // as the OpKill of the alpha test), branch to the merge block explicitly.
  Function *function = get_current_function();
  nassertv(function != nullptr);

  spv::Op prev_opcode = spv::OpNop;
  for (size_t i = _index; i > 0; --i) {
    const Instruction &prev = function->instructions[i - 1];
    if (!prev.is_nop()) {
      prev_opcode = prev.opcode;
      break;
    }
  }
  if (!is_block_terminator(prev_opcode)) {
    op_branch(false_label);
  }

  insert(spv::OpLabel, {false_label});
}

/**
 * Inserts an OpExtInst invoking the given instruction of the given extended
 * instruction set, which is imported into the module if necessary.
 */
SpirVId SpirVBuilder::
op_ext_inst(SpirVId result_type_id, const char *set, uint32_t instruction,
            std::initializer_list<SpirVId> operands) {
  SpirVId set_id = _module.add_ext_inst_import(set);
  SpirVId id = allocate_id();

  Instruction op(spv::OpExtInst, {result_type_id, id, set_id, instruction});
  op.args.insert(op.args.end(), operands.begin(), operands.end());
  insert(std::move(op));
  return id;
}

/**
 * Inserts a GLSL.std.450 extended instruction whose result type equals the
 * type of its first operand.
 */
SpirVId SpirVBuilder::
op_glsl_std_450(uint32_t instruction, std::initializer_list<SpirVId> operands) {
  nassertr(operands.size() > 0, SpirVId());
#ifndef NDEBUG
  const ShaderType *type = _module.resolve_value_type(*operands.begin());
  nassertr(is_float_scalar_type(get_element_scalar_type(type)), SpirVId());
  for (SpirVId operand : operands) {
    nassertr(_module.resolve_value_type(operand) == type, SpirVId());
  }
#endif
  return op_ext_inst(_module.get_type_id(*operands.begin()), "GLSL.std.450",
                     instruction, operands);
}

/**
 * Inserts a floating-point absolute value instruction.
 */
SpirVId SpirVBuilder::
op_abs(SpirVId value) {
  return op_glsl_std_450(GLSLstd450FAbs, {value});
}

/**
 * Inserts a base-2 exponential instruction.
 */
SpirVId SpirVBuilder::
op_exp2(SpirVId value) {
  return op_glsl_std_450(GLSLstd450Exp2, {value});
}

/**
 * Inserts a power instruction.
 */
SpirVId SpirVBuilder::
op_pow(SpirVId base, SpirVId exponent) {
  return op_glsl_std_450(GLSLstd450Pow, {base, exponent});
}

/**
 * Inserts a floating-point minimum instruction.
 */
SpirVId SpirVBuilder::
op_min(SpirVId a, SpirVId b) {
  return op_glsl_std_450(GLSLstd450FMin, {a, b});
}

/**
 * Inserts a floating-point maximum instruction.
 */
SpirVId SpirVBuilder::
op_max(SpirVId a, SpirVId b) {
  return op_glsl_std_450(GLSLstd450FMax, {a, b});
}

/**
 * Inserts a floating-point clamp instruction.
 */
SpirVId SpirVBuilder::
op_clamp(SpirVId value, SpirVId min_value, SpirVId max_value) {
  return op_glsl_std_450(GLSLstd450FClamp, {value, min_value, max_value});
}

/**
 * Clamps the given float scalar or vector to the [0, 1] range.
 */
SpirVId SpirVBuilder::
op_saturate(SpirVId value) {
  const ShaderType *type = _module.resolve_value_type(value);
  nassertr(type != nullptr, SpirVId());

  SpirVId zero, one;
  if (const ShaderType::Vector *vector = type->as_vector()) {
    uint32_t n = vector->get_num_components();
    zero = _module.define_composite_constant(type,
      pvector<SpirVId>(n, define_float_constant(0.0f)));
    one = _module.define_composite_constant(type,
      pvector<SpirVId>(n, define_float_constant(1.0f)));
  } else {
    zero = define_float_constant(0.0f);
    one = define_float_constant(1.0f);
  }
  return op_clamp(value, zero, one);
}

/**
 * Inserts a linear interpolation instruction.  All three operands must have
 * the same type.
 */
SpirVId SpirVBuilder::
op_mix(SpirVId a, SpirVId b, SpirVId t) {
  return op_glsl_std_450(GLSLstd450FMix, {a, b, t});
}

/**
 * Inserts an instruction taking the length of the given vector, producing a
 * scalar.
 */
SpirVId SpirVBuilder::
op_length(SpirVId vector) {
  const ShaderType *type = _module.resolve_value_type(vector);
  nassertr(type != nullptr, SpirVId());
  const ShaderType::Vector *vector_type = type->as_vector();
  nassertr(vector_type != nullptr, SpirVId());

  const ShaderType *scalar_type = ShaderType::register_type(ShaderType::Scalar(vector_type->get_scalar_type()));
  return op_ext_inst(_module.define_type(scalar_type), "GLSL.std.450",
                     GLSLstd450Length, {vector});
}

/**
 * Inserts a vector normalization instruction.
 */
SpirVId SpirVBuilder::
op_normalize(SpirVId vector) {
  return op_glsl_std_450(GLSLstd450Normalize, {vector});
}
