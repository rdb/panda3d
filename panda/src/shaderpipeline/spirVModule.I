/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file spirVModule.I
 * @author rdb
 * @date 2026-07-07
 */

/**
 *
 */
INLINE SpirVModule::Instruction::
Instruction(spv::Op opcode) : opcode(opcode) {
}

/**
 *
 */
INLINE SpirVModule::Instruction::
Instruction(spv::Op opcode, std::initializer_list<uint32_t> args) :
  opcode(opcode), args(args) {
}

/**
 *
 */
INLINE SpirVModule::Instruction::
Instruction(spv::Op opcode, const uint32_t *args, size_t nargs) :
  opcode(opcode), args(args, args + nargs) {
}

/**
 *
 */
INLINE size_t SpirVModule::Instruction::
size() const {
  return args.size();
}

/**
 *
 */
INLINE uint32_t SpirVModule::Instruction::
operator [] (size_t i) const {
  return args[i];
}

/**
 *
 */
INLINE uint32_t &SpirVModule::Instruction::
operator [] (size_t i) {
  return args[i];
}

/**
 * Returns true if this is an OpNop, which is used as a tombstone for deleted
 * globals (emit() skips them).
 */
INLINE bool SpirVModule::Instruction::
is_nop() const {
  return opcode == spv::OpNop;
}

/**
 * Returns true if this annotation applies to a struct member (in which case
 * args[0] is the member index).
 */
INLINE bool SpirVModule::Annotation::
is_member_annotation() const {
  return opcode == spv::OpMemberDecorate ||
         opcode == spv::OpMemberDecorateString;
}

/**
 * Returns the decoration of this annotation.
 */
INLINE spv::Decoration SpirVModule::Annotation::
get_decoration() const {
  size_t i = is_member_annotation() ? 1 : 0;
  return (i < args.size()) ? (spv::Decoration)args[i] : spv::DecorationMax;
}

/**
 * Returns the current id bound; all ids in the module are lower than this.
 */
INLINE uint32_t SpirVModule::
get_id_bound() const {
  return _id_bound;
}

/**
 * Allocates a fresh identifier and returns it.  Ids are never reused.
 */
INLINE uint32_t SpirVModule::
allocate_id() {
  return _id_bound++;
}

/**
 * Returns the id table.
 */
INLINE const SpirVResultDatabase &SpirVModule::
get_db() const {
  return _db;
}

/**
 * Returns the id table for modification.
 */
INLINE SpirVResultDatabase &SpirVModule::
modify_db() {
  return _db;
}

/**
 * Adds a decoration without extra operands to the given id.
 */
INLINE void SpirVModule::
decorate(uint32_t id, spv::Decoration decoration) {
  decorate(id, Annotation {spv::OpDecorate, {(uint32_t)decoration}});
}

/**
 * Adds a decoration with one extra operand to the given id.
 */
INLINE void SpirVModule::
decorate(uint32_t id, spv::Decoration decoration, uint32_t value) {
  decorate(id, Annotation {spv::OpDecorate, {(uint32_t)decoration, value}});
}

/**
 * Returns the annotations attached to the given id, or null if it has none.
 */
INLINE const SpirVModule::Annotations *SpirVModule::
get_annotations(uint32_t id) const {
  auto it = _annotations.find(id);
  return it != _annotations.end() ? &it->second : nullptr;
}

/**
 * Looks up the given string.
 */
INLINE std::string SpirVModule::
resolve_string(uint32_t id) const {
  const Definition &def = _db.get_definition(id);
  if (!def.is_string()) {
    error_expected(id, "a string");
    return std::string();
  }
  return def._name;
}

/**
 * Looks up the given constant's (first word) value.
 */
INLINE uint32_t SpirVModule::
resolve_constant(uint32_t id) const {
  const Definition &def = _db.get_definition(id);
  if (!def.is_constant()) {
    return error_expected(id, "a constant");
  }
  return def._constant;
}

/**
 * Looks up the given non-pointer type.
 */
INLINE const ShaderType *SpirVModule::
resolve_type(uint32_t id) const {
  const Definition &def = _db.get_definition(id);
  if (!def.is_type() || def.is_pointer_type()) {
    error_expected(id, "a non-pointer type");
    return nullptr;
  }
  return def._type;
}

/**
 * Looks up the type underneath the given pointer type.
 */
INLINE const ShaderType *SpirVModule::
resolve_pointer_type(uint32_t id) const {
  const Definition &def = _db.get_definition(id);
  if (!def.is_pointer_type()) {
    error_expected(id, "a pointer type");
    return nullptr;
  }
  return def._type;
}

/**
 * Returns the type id of the given id.
 */
INLINE uint32_t SpirVModule::
get_type_id(uint32_t id) const {
  const Definition &def = _db.get_definition(id);
  nassertr(def._type_id != 0, 0);
  return def._type_id;
}

/**
 * Returns the pointee type id of the given pointer type id.
 */
INLINE uint32_t SpirVModule::
unwrap_pointer_type(uint32_t id) const {
  const Definition &def = _db.get_definition(id);
  if (!def.is_pointer_type()) {
    return error_expected(id, "a pointer type");
  }
  return def._type_id;
}

/**
 * Marks the variable that the given id (indirectly) originates from as used.
 */
INLINE void SpirVModule::
mark_used(uint32_t id) {
  _db.mark_used(id);
}

/**
 * Defines a new constant of type float.
 */
INLINE uint32_t SpirVModule::
define_float_constant(float constant) {
  union {
    float f;
    uint32_t ui;
  } u;
  u.f = constant;
  return define_constant(ShaderType::FLOAT, u.ui);
}
