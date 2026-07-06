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
 * at a cursor position and recording their results in the module's id table
 * as it goes.  Used by the transformation passes to inject code, by the unit
 * tests to assemble input modules, and by the shader generator to build
 * modules from scratch.
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
  using Definition = SpirVResultDatabase::Definition;

  explicit SpirVBuilder(SpirVModule &module);
  SpirVBuilder(SpirVModule &module, uint32_t function_id, size_t index);

  INLINE SpirVModule &get_module();
  INLINE const SpirVResultDatabase &get_db() const;

  // Cursor control.
  void set_insertion_point(uint32_t function_id, size_t index);
  INLINE size_t get_insertion_index() const;
  Function *get_current_function();
  INLINE uint32_t get_current_function_id() const;

  INLINE uint32_t allocate_id();
  INLINE void push_id(uint32_t id);

  // Inserts an arbitrary instruction at the cursor, without recording
  // anything in the id table; use this for instructions that have no op_xyz
  // helper, and record the result via the database if it has one.
  void insert(Instruction op);
  INLINE void insert(spv::Op opcode, std::initializer_list<uint32_t> args);
  void insert(spv::Op opcode, const uint32_t *args, size_t nargs);

  // Convenience forwards to the module's definition helpers.
  INLINE uint32_t define_type(const ShaderType *type);
  INLINE uint32_t define_pointer_type(const ShaderType *type, spv::StorageClass storage_class);
  INLINE uint32_t define_variable(const ShaderType *type, spv::StorageClass storage_class);
  INLINE uint32_t define_int_constant(int32_t constant);
  INLINE uint32_t define_float_constant(float constant);
  INLINE uint32_t define_constant(const ShaderType *type, uint32_t constant);
  INLINE uint32_t define_null_constant(const ShaderType *type);

  // Functions for inserting specific instructions at the cursor.
  uint32_t op_load(uint32_t var_id, spv::MemoryAccessMask access = spv::MemoryAccessMaskNone);
  void op_store(uint32_t var_id, uint32_t value);
  uint32_t op_select(uint32_t cond, uint32_t obj1, uint32_t obj2);
  uint32_t op_access_chain(uint32_t var_id, std::initializer_list<uint32_t> chain);
  uint32_t op_vector_shuffle(uint32_t vec1, uint32_t vec2, const pvector<uint32_t> &components);
  uint32_t op_composite_construct(const ShaderType *type, const pvector<uint32_t> &constituents);
  uint32_t op_composite_extract(uint32_t obj_id, std::initializer_list<uint32_t> chain);
  uint32_t op_composite_insert(uint32_t obj_id, uint32_t composite_id, std::initializer_list<uint32_t> chain);
  uint32_t op_compare(spv::Op opcode, uint32_t obj1, uint32_t obj2);
  uint32_t op_convert(ShaderType::ScalarType to_scalar_type, uint32_t value);
  uint32_t op_bitcast(const ShaderType *type, uint32_t value);
  uint32_t op_transpose(uint32_t obj);
  uint32_t op_add(uint32_t left, uint32_t right);
  uint32_t op_sub(uint32_t left, uint32_t right);
  uint32_t op_div(uint32_t left, uint32_t right);
  uint32_t op_dot(uint32_t left, uint32_t right);
  uint32_t op_negate(uint32_t value);
  uint32_t op_multiply(uint32_t left, uint32_t right);
  uint32_t op_image_sample(uint32_t image, uint32_t coord, uint32_t operands = 0u, const uint32_t *ids = nullptr);
  uint32_t op_function_call(uint32_t func_id);

  // Starts a new function at the end of the module, positioning the cursor
  // inside it.  Terminate it with op_function_end.
  uint32_t op_function(const ShaderType *return_type);
  uint32_t op_label();
  void op_branch(uint32_t target);
  void op_return();
  void op_kill();
  void op_function_end();

  uint32_t branch_if(uint32_t cond);
  void branch_endif(uint32_t label);

private:
  SpirVModule &_module;
  SpirVResultDatabase &_db;

  // Cursor: index of the function in the module (or npos when not inside a
  // function) and instruction index within it.
  size_t _function_index = (size_t)-1;
  size_t _index = 0;
  uint32_t _current_function_id = 0;

  uint32_t _next_id = 0;
};

#include "spirVBuilder.I"

#endif
