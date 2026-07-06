/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file spirVModule.h
 * @author rdb
 * @date 2026-07-07
 */

#ifndef SPIRVMODULE_H
#define SPIRVMODULE_H

#include "shaderModuleSpirV.h"
#include "spirVResultDatabase.h"

/**
 * A SPIR-V module, materialized into a mutable in-memory representation that
 * transformation passes operate on.
 *
 * The design trades the raw word stream for structures that make the module's
 * global invariants either trivially maintained or enforced in one place:
 *
 * - Instructions are value types (opcode plus operand vector), stored in
 *   per-section vectors; deleting one removes it immediately, inserting one
 *   inserts it immediately.  There is no deferred fixup: a mutation method
 *   returns with the module (and the result database) already consistent.
 *
 * - Ids are plain uint32_t indices into the embedded SpirVResultDatabase,
 *   which acts as the single id table.  Ids are never renumbered; the bound
 *   only grows.
 *
 * - Debug names, decorations and entry point interfaces are not kept as
 *   instruction streams but as properties: names in a map, decorations as
 *   per-id annotation records (kept opaque -- the operands are not
 *   interpreted here beyond the target id and member index), interfaces as
 *   id lists on structured EntryPoint records.  Deleting an id therefore
 *   deletes its names, decorations and interface references by construction;
 *   the orphan-sweeping machinery of the old streaming transformer does not
 *   exist anymore.
 *
 * - The globals section is kept in declaration order, but emit() performs a
 *   topological sort, so a pass may freely append declarations or redirect
 *   references (replace_type_id) without regard for ordering.  emit() also
 *   merges duplicate declarations of types that SPIR-V requires to be unique
 *   (see deduplicate_types), which a pass may create when a type replacement
 *   makes two declarations structurally identical.
 *
 * Function bodies are flat instruction vectors (including the OpFunction
 * header and OpFunctionEnd); use SpirVBuilder to insert code at a cursor.
 */
class EXPCL_PANDA_SHADERPIPELINE SpirVModule {
public:
  using Definition = SpirVResultDatabase::Definition;
  using MemberDefinition = SpirVResultDatabase::MemberDefinition;
  using InstructionStream = ShaderModuleSpirV::InstructionStream;

  /**
   * A single instruction, owning its operands.  For instructions with a
   * result id and/or result type id, these are stored in args at the
   * positions defined by the SPIR-V specification.
   */
  class EXPCL_PANDA_SHADERPIPELINE Instruction {
  public:
    Instruction() = default;
    INLINE Instruction(spv::Op opcode);
    INLINE Instruction(spv::Op opcode, std::initializer_list<uint32_t> args);
    INLINE Instruction(spv::Op opcode, const uint32_t *args, size_t nargs);

    // These are defined out-of-line because they rely on utility code that
    // is only available inside the library build.
    bool has_result() const;
    bool has_result_type() const;
    uint32_t get_result() const;
    uint32_t get_result_type() const;

    INLINE size_t size() const;
    INLINE uint32_t operator [] (size_t i) const;
    INLINE uint32_t &operator [] (size_t i);

    INLINE bool is_nop() const;

    // Extracts a null-terminated string literal starting at the given operand
    // index, and returns the operand index just past it.
    std::string get_string(size_t arg_index, size_t *end_index = nullptr) const;

    spv::Op opcode = spv::OpNop;
    pvector<uint32_t> args;
  };

  /**
   * An annotation (OpDecorate and friends) attached to an id.  The target id
   * is implicit (the key under which the annotation is stored); args holds
   * the remaining operands, uninterpreted.  For member annotations, args[0]
   * is the member index.
   */
  struct Annotation {
    spv::Op opcode;
    pvector<uint32_t> args;

    INLINE bool is_member_annotation() const;
    INLINE spv::Decoration get_decoration() const;
  };
  typedef pvector<Annotation> Annotations;

  /**
   * A structured entry point record.  The interface list holds the ids of
   * the module-scope variables the entry point references; deleting a
   * variable id removes it from all interfaces automatically.
   */
  struct EntryPoint {
    spv::ExecutionModel model;
    uint32_t function_id;
    std::string name;
    pvector<uint32_t> interface_vars;
  };

  /**
   * A function: the OpFunction header, any OpFunctionParameter instructions,
   * the body, and the terminating OpFunctionEnd, as a flat instruction list.
   */
  struct Function {
    uint32_t id;
    pvector<Instruction> instructions;
  };

  SpirVModule() = default;
  explicit SpirVModule(const InstructionStream &stream);

  bool parse(const uint32_t *words, size_t num_words);
  InstructionStream emit() const;

  INLINE uint32_t get_id_bound() const;
  INLINE uint32_t allocate_id();

  INLINE const SpirVResultDatabase &get_db() const;
  INLINE SpirVResultDatabase &modify_db();

  // Name handling.  The name stored here is what is emitted as OpName; the
  // database may deliberately carry a different (qualified) name for
  // consumers, so the two are set together but may be diverged by a pass.
  const std::string &get_name(uint32_t id) const;
  void set_name(uint32_t id, const std::string &name);
  void set_member_name(uint32_t type_id, uint32_t member_index, const std::string &name);

  // Annotation handling.  decorate() also feeds the decoration through the
  // database's decoder, so that queries (location, builtin, flags...) always
  // agree with what will be emitted.
  INLINE void decorate(uint32_t id, spv::Decoration decoration);
  INLINE void decorate(uint32_t id, spv::Decoration decoration, uint32_t value);
  void decorate(uint32_t id, const Annotation &annotation);
  void decorate_member(uint32_t type_id, uint32_t member_index, spv::Decoration decoration);
  void decorate_member(uint32_t type_id, uint32_t member_index, spv::Decoration decoration, uint32_t value);
  bool remove_decoration(uint32_t id, spv::Decoration decoration);
  void set_decoration(uint32_t id, spv::Decoration decoration, uint32_t value);
  void strip_decoration(spv::Decoration decoration);
  void transfer_annotations(uint32_t from, uint32_t to);
  INLINE const Annotations *get_annotations(uint32_t id) const;

  void set_location(uint32_t id, int location);

  // Core mutations.  All of these apply immediately; when they return, the
  // module and the database are consistent, and emit() would produce a valid
  // module (provided the pass has upheld its own obligations, such as not
  // leaving references to a deleted id in function bodies).
  void delete_id(uint32_t id);
  void replace_type_id(uint32_t before, uint32_t after);
  void delete_struct_member(uint32_t type_id, uint32_t member_index);
  void remove_function_parameters(uint32_t type_id, const pset<uint32_t> &param_indices);
  void delete_dead_code(pset<uint32_t> &dead_ids);
  void deduplicate_types();
  void reparse_db();

  // Section access.
  pvector<Instruction> &modify_globals() { return _globals; }
  const pvector<Instruction> &get_globals() const { return _globals; }
  pvector<Function> &modify_functions() { return _functions; }
  const pvector<Function> &get_functions() const { return _functions; }
  pvector<EntryPoint> &modify_entry_points() { return _entry_points; }
  const pvector<EntryPoint> &get_entry_points() const { return _entry_points; }

  Function *get_function(uint32_t function_id);
  Instruction *get_global(uint32_t result_id);

  // Module construction, for building a module from scratch (as the tests
  // and the shader generator do).
  void add_capability(spv::Capability capability);
  void add_extension(const std::string &name);
  void set_memory_model(spv::AddressingModel addressing, spv::MemoryModel memory);
  void add_entry_point(spv::ExecutionModel model, uint32_t function_id,
                       const std::string &name, pvector<uint32_t> interface_vars = {});
  void add_execution_mode(uint32_t function_id, spv::ExecutionMode mode,
                          std::initializer_list<uint32_t> args = {});
  uint32_t add_string(const std::string &str);
  uint32_t add_ext_inst_import(const std::string &name);

  // Adds the given variable to the interface of every entry point with the
  // given execution model (if not already listed).
  void add_to_interfaces(spv::ExecutionModel model, uint32_t var_id);

  // Looks for a module-scope input variable with the given builtin
  // decoration, creating one (added to all matching entry point interfaces)
  // if it does not exist.
  uint32_t ensure_builtin_input(spv::ExecutionModel model, spv::BuiltIn builtin);

  // Definition helpers; these append any needed declarations to the globals
  // section (reusing existing declarations where permitted) and record them
  // in the database.
  uint32_t define_type(const ShaderType *type);
  uint32_t define_image_type(const ShaderType *type, uint32_t depth, uint32_t sampled, spv::ImageFormat format);
  uint32_t define_pointer_type(uint32_t type_id, spv::StorageClass storage_class);
  uint32_t define_pointer_type(const ShaderType *type, spv::StorageClass storage_class);
  uint32_t define_function_type(const ShaderType *return_type);
  uint32_t define_variable(const ShaderType *type, spv::StorageClass storage_class);
  uint32_t define_int_constant(int32_t constant);
  INLINE uint32_t define_float_constant(float constant);
  uint32_t define_null_constant(const ShaderType *type);
  uint32_t define_constant(const ShaderType *type, uint32_t constant);
  uint32_t define_spec_constant(const ShaderType *type, uint32_t def_value);

  void add_global(const Instruction &op);

  INLINE std::string resolve_string(uint32_t id) const;
  INLINE uint32_t resolve_constant(uint32_t id) const;
  INLINE const ShaderType *resolve_type(uint32_t id) const;
  INLINE const ShaderType *resolve_pointer_type(uint32_t id) const;
  INLINE uint32_t get_type_id(uint32_t id) const;
  INLINE uint32_t unwrap_pointer_type(uint32_t id) const;
  INLINE void mark_used(uint32_t id);

  uint32_t error_expected(uint32_t id, const char *msg) const;

  // Returns, for a globals-section instruction, the operand indices that
  // hold ids (as opposed to literals).  Used by the emit-time topological
  // sort and by replace_type_id.
  static void get_global_id_operands(const Instruction &op, pvector<size_t> &indices);

private:
  void r_annotate_struct_layout(uint32_t type_id);
  pvector<size_t> sort_globals() const;
  void r_sort_global(size_t index, const pmap<uint32_t, size_t> &decl_index,
                     pvector<int> &state, pvector<size_t> &order) const;
  static void emit_instruction(std::vector<uint32_t> &words, spv::Op opcode,
                               const uint32_t *args, size_t nargs);
  void rewrite_type_references(uint32_t before, uint32_t after);

  // Module header state.
  uint32_t _version = 0x10000;
  uint32_t _generator = 0;
  uint32_t _id_bound = 1;
  uint32_t _schema = 0;

  // The sections, in module order.  Capabilities, extensions, imports, the
  // memory model, execution modes and non-name debug instructions are kept
  // as opaque instruction lists; they are rare and no pass interprets them
  // structurally (execution modes reference the entry function id, which is
  // checked when a function is deleted).
  pvector<Instruction> _capabilities;
  pvector<Instruction> _extensions;
  pvector<Instruction> _ext_inst_imports;
  Instruction _memory_model;
  pvector<EntryPoint> _entry_points;
  pvector<Instruction> _execution_modes;
  pvector<Instruction> _debug;  // OpString, OpSource*
  pvector<Instruction> _module_processed;
  pmap<uint32_t, std::string> _names;
  pmap<uint32_t, Annotations> _annotations;
  pvector<Instruction> _globals;
  pvector<Function> _functions;

  SpirVResultDatabase _db;

  friend class SpirVBuilder;
};

#include "spirVModule.I"

#endif
