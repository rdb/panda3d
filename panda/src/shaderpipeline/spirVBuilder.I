/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file spirVBuilder.I
 * @author rdb
 * @date 2026-07-07
 */

/**
 *
 */
INLINE SpirVModule &SpirVBuilder::
get_module() {
  return _module;
}

/**
 * Returns the instruction index within the current function at which the
 * next instruction will be inserted.
 */
INLINE size_t SpirVBuilder::
get_insertion_index() const {
  return _index;
}

/**
 * Returns the id of the function the cursor is currently inside, or 0.
 */
INLINE uint32_t SpirVBuilder::
get_current_function_id() const {
  return _current_function_id;
}

/**
 * Allocates a fresh identifier, unless one was pushed with push_id().
 */
INLINE uint32_t SpirVBuilder::
allocate_id() {
  uint32_t next_id = _next_id;
  if (next_id != 0) {
    _next_id = 0;
    return next_id;
  }
  return _module.allocate_id();
}

/**
 * Sets the id that should be returned by the next allocate_id() call.  Used
 * when replacing an instruction with a new sequence whose final result must
 * assume the replaced instruction's id.
 *
 * Only valid immediately before a call that allocates exactly one result id:
 * helpers that allocate several (such as branch_if) assert against a pending
 * id, as does an op_convert that turns out to require no conversion.
 */
INLINE void SpirVBuilder::
push_id(uint32_t id) {
  nassertv(_next_id == 0);
  _next_id = id;
}

/**
 * Inserts an arbitrary instruction at the cursor.
 */
INLINE void SpirVBuilder::
insert(spv::Op opcode, std::initializer_list<uint32_t> args) {
  insert(Instruction(opcode, args));
}

/**
 *
 */
INLINE uint32_t SpirVBuilder::
define_type(const ShaderType *type) {
  return _module.define_type(type);
}

/**
 *
 */
INLINE uint32_t SpirVBuilder::
define_pointer_type(const ShaderType *type, spv::StorageClass storage_class) {
  return _module.define_pointer_type(type, storage_class);
}

/**
 *
 */
INLINE uint32_t SpirVBuilder::
define_variable(const ShaderType *type, spv::StorageClass storage_class) {
  return _module.define_variable(type, storage_class);
}

/**
 *
 */
INLINE uint32_t SpirVBuilder::
define_int_constant(int32_t constant) {
  return _module.define_int_constant(constant);
}

/**
 *
 */
INLINE uint32_t SpirVBuilder::
define_float_constant(float constant) {
  return _module.define_float_constant(constant);
}

/**
 *
 */
INLINE uint32_t SpirVBuilder::
define_constant(const ShaderType *type, uint32_t constant) {
  return _module.define_constant(type, constant);
}

/**
 *
 */
INLINE uint32_t SpirVBuilder::
define_null_constant(const ShaderType *type) {
  return _module.define_null_constant(type);
}
