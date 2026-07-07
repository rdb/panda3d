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
INLINE SpirVId SpirVModule::
allocate_id() {
  return SpirVId(_id_bound++);
}

/**
 * Returns what kind of thing the given id refers to.
 */
INLINE SpirVModule::IdKind SpirVModule::
get_kind(SpirVId id) const {
  return get_entry(id)._kind;
}

/**
 * Returns true if the given id is a type (including pointer and function
 * types).
 */
INLINE bool SpirVModule::
is_type(SpirVId id) const {
  IdKind kind = get_entry(id)._kind;
  return kind == IdKind::TYPE || kind == IdKind::POINTER_TYPE ||
         kind == IdKind::FUNCTION_TYPE;
}

/**
 * Returns the id of the function the given id is declared in, or 0 if it is
 * declared at module scope.  For a function, returns the function's own id.
 */
INLINE SpirVId SpirVModule::
get_function_id(SpirVId id) const {
  return get_entry(id)._function_id;
}

/**
 * Returns the index entry for the given id, or a default entry if the id is
 * out of range.
 */
INLINE const SpirVModule::IdEntry &SpirVModule::
get_entry(SpirVId id) const {
  if (id >= _ids.size()) {
    static const IdEntry default_entry;
    return default_entry;
  }
  return _ids[id];
}

/**
 * Returns a mutable index entry for the given id, growing the index as
 * needed.  May invalidate existing entry references.
 */
INLINE SpirVModule::IdEntry &SpirVModule::
modify_entry(SpirVId id) {
  if (id >= _ids.size()) {
    _ids.resize(id + 1);
  }
  return _ids[id];
}

/**
 * Looks up the given string.
 */
INLINE std::string SpirVModule::
resolve_string(SpirVId id) const {
  if (get_entry(id)._kind != IdKind::STRING) {
    error_expected(id, "a string");
    return std::string();
  }
  for (const Instruction &op : _debug) {
    if (op.opcode == spv::OpString && !op.args.empty() && op.args[0] == id) {
      return op.get_string(1);
    }
  }
  return std::string();
}

/**
 * Returns the location decoration of the given id, or -1 if it has none.
 */
INLINE int SpirVModule::
get_location(SpirVId id) const {
  return has_decoration(id, spv::DecorationLocation)
    ? (int)get_decoration(id, spv::DecorationLocation)
    : -1;
}

/**
 * Returns the builtin decoration of the given id, or spv::BuiltInMax if it
 * has none.
 */
INLINE spv::BuiltIn SpirVModule::
get_builtin(SpirVId id) const {
  return (spv::BuiltIn)get_decoration(id, spv::DecorationBuiltIn,
                                      (uint32_t)spv::BuiltInMax);
}

/**
 * Returns the SpecId decoration of the given id, or 0 if it has none.
 */
INLINE uint32_t SpirVModule::
get_spec_id(SpirVId id) const {
  return get_decoration(id, spv::DecorationSpecId, 0);
}

/**
 * Returns the ArrayStride decoration of the given type id, or 0 if it has
 * none.
 */
INLINE uint32_t SpirVModule::
get_array_stride(SpirVId type_id) const {
  return get_decoration(type_id, spv::DecorationArrayStride, 0);
}

/**
 * Returns the Offset decoration of the given struct member, or -1 if it has
 * none.
 */
INLINE int SpirVModule::
get_member_offset(SpirVId type_id, uint32_t member_index) const {
  return has_member_decoration(type_id, member_index, spv::DecorationOffset)
    ? (int)get_member_decoration(type_id, member_index, spv::DecorationOffset)
    : -1;
}

/**
 * Returns the builtin decoration of the given struct member, or
 * spv::BuiltInMax if it has none.
 */
INLINE spv::BuiltIn SpirVModule::
get_member_builtin(SpirVId type_id, uint32_t member_index) const {
  return (spv::BuiltIn)get_member_decoration(type_id, member_index,
    spv::DecorationBuiltIn, (uint32_t)spv::BuiltInMax);
}

/**
 * Adds a decoration without extra operands to the given id.
 */
INLINE void SpirVModule::
decorate(SpirVId id, spv::Decoration decoration) {
  decorate(id, Annotation {spv::OpDecorate, {(uint32_t)decoration}});
}

/**
 * Adds a decoration with one extra operand to the given id.
 */
INLINE void SpirVModule::
decorate(SpirVId id, spv::Decoration decoration, uint32_t value) {
  decorate(id, Annotation {spv::OpDecorate, {(uint32_t)decoration, value}});
}

/**
 * Returns the annotations attached to the given id, or null if it has none.
 */
INLINE const SpirVModule::Annotations *SpirVModule::
get_annotations(SpirVId id) const {
  auto it = _annotations.find(id);
  return it != _annotations.end() ? &it->second : nullptr;
}

/**
 * Returns true if the given id is a type and the canonical declaration of
 * that type, ie. declared the same way define_type(...) would declare it.
 */
INLINE bool SpirVModule::
is_canonical_type(SpirVId type_id) const {
  ensure_type_map();
  const ShaderType *type = get_entry(type_id)._type;
  if (type == nullptr) {
    return false;
  }
  TypeMap::const_iterator tit = _type_map.find(type);
  return tit != _type_map.end() && tit->second == type_id;
}

/**
 * Defines a new constant of type float.
 */
INLINE SpirVId SpirVModule::
define_float_constant(float constant) {
  union {
    float f;
    uint32_t ui;
  } u;
  u.f = constant;
  return define_constant(ShaderType::FLOAT, u.ui);
}
