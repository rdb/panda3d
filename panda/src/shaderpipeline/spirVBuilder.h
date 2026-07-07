/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file spirVBuilder.h
 * @author rdb
 * @date 2026-07-07
 */

#ifndef SPIRVBUILDER_H
#define SPIRVBUILDER_H

#include "spirVModule.h"

/**
 * Builds SPIR-V function bodies, inserting instructions into a SpirVModule
 * at a cursor position; every inserted instruction is recorded in the
 * module's id index automatically.  Used by the transformation passes to
 * inject code, by the unit tests to assemble input modules, and by the
 * shader generator to build modules from scratch.
 *
 * The cursor addresses a function by index (not pointer, so that creating a
 * function does not invalidate an outstanding builder) and an instruction
 * index within it.  Each op_xyz call inserts before the instruction at the
 * cursor and advances the cursor past the inserted code.
 *
 * Global definitions (types, constants, variables) are forwarded to the
 * module, which appends them to the globals section; they may be created at
 * any time regardless of the cursor position.
 */
class EXPCL_PANDA_SHADERPIPELINE SpirVBuilder {
public:
  using Instruction = SpirVModule::Instruction;
  using Function = SpirVModule::Function;
  using Id = SpirVId;

  explicit SpirVBuilder(SpirVModule &module);
  SpirVBuilder(SpirVModule &module, SpirVId function_id, size_t index);

  INLINE SpirVModule &get_module();

  // Cursor control.
  void set_insertion_point(SpirVId function_id, size_t index);
  void set_insertion_point_to_body_start(SpirVId function_id);
  INLINE size_t get_insertion_index() const;
  Function *get_current_function();
  INLINE SpirVId get_current_function_id() const;

  INLINE SpirVId allocate_id();

  // Inserts an arbitrary instruction at the cursor, recording its result (if
  // any) in the module's id index; use this for instructions that have no
  // op_xyz helper.
  void insert(Instruction op);
  INLINE void insert(spv::Op opcode, std::initializer_list<uint32_t> args);
  void insert(spv::Op opcode, const uint32_t *args, size_t nargs);

  // Convenience forwards to the module's definition helpers.
  INLINE SpirVId define_type(const ShaderType *type);
  INLINE SpirVId define_pointer_type(const ShaderType *type, spv::StorageClass storage_class);
  INLINE SpirVId define_variable(const ShaderType *type, spv::StorageClass storage_class);
  INLINE SpirVId define_int_constant(int32_t constant);
  INLINE SpirVId define_float_constant(float constant);
  INLINE SpirVId define_constant(const ShaderType *type, uint32_t constant);
  INLINE SpirVId define_null_constant(const ShaderType *type);

  // Functions for inserting specific instructions at the cursor.  Note that
  // op_access_chain takes the ids of constants, while op_composite_extract
  // and op_composite_insert take literal indices.
  SpirVId op_load(SpirVId var_id, spv::MemoryAccessMask access = spv::MemoryAccessMaskNone);
  void op_store(SpirVId var_id, SpirVId value);
  SpirVId op_select(SpirVId cond, SpirVId obj1, SpirVId obj2);
  SpirVId op_access_chain(SpirVId var_id, std::initializer_list<SpirVId> chain);
  SpirVId op_vector_shuffle(SpirVId vec1, SpirVId vec2, const pvector<uint32_t> &components);
  SpirVId op_composite_construct(const ShaderType *type, const pvector<SpirVId> &constituents);
  SpirVId op_composite_extract(SpirVId obj_id, std::initializer_list<uint32_t> chain);
  SpirVId op_composite_insert(SpirVId obj_id, SpirVId composite_id, std::initializer_list<uint32_t> chain);
  SpirVId op_compare(spv::Op opcode, SpirVId obj1, SpirVId obj2);
  SpirVId op_convert(ShaderType::ScalarType to_scalar_type, SpirVId value);
  SpirVId op_bitcast(const ShaderType *type, SpirVId value);
  SpirVId op_transpose(SpirVId obj);
  SpirVId op_add(SpirVId left, SpirVId right);
  SpirVId op_sub(SpirVId left, SpirVId right);
  SpirVId op_div(SpirVId left, SpirVId right);
  SpirVId op_dot(SpirVId left, SpirVId right);
  SpirVId op_negate(SpirVId value);
  SpirVId op_multiply(SpirVId left, SpirVId right);
  SpirVId op_image_sample(SpirVId image, SpirVId coord, uint32_t operands = 0u, const uint32_t *ids = nullptr);
  SpirVId op_function_call(SpirVId func_id);

  // Starts a new function at the end of the module, positioning the cursor
  // inside it.  Terminate it with op_function_end.
  SpirVId op_function(const ShaderType *return_type);
  SpirVId op_label();
  void op_branch(SpirVId target);
  void op_return();
  void op_kill();
  void op_function_end();

  SpirVId branch_if(SpirVId cond);
  void branch_endif(SpirVId label);

private:
  SpirVModule &_module;

  // Cursor: index of the function in the module (or npos when not inside a
  // function) and instruction index within it.
  size_t _function_index = (size_t)-1;
  size_t _index = 0;
  SpirVId _current_function_id;
};

#include "spirVBuilder.I"

#endif
