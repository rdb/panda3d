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
 *   returns with the module already consistent.
 *
 * - Ids are plain uint32_t indices into the module's own id index, which
 *   stores only what cannot be cheaply re-derived: what kind of thing the id
 *   is, its result type id, the function it lives in, and (for module-scope
 *   ids) where its declaration lives.  Everything else - storage classes,
 *   constant values, member types, locations, builtins, offsets, strides -
 *   is read from the declaring instruction or the annotations on demand, so
 *   a query can never disagree with what emit() writes out.  Ids are never
 *   renumbered; the bound only grows.
 *
 * - The mapping to Panda's ShaderType system is a derivation (resolve_type
 *   and friends), memoized per id.  Mutations that change what a type means
 *   (deleting a struct member, replacing a type id, adding a layout or block
 *   decoration) simply invalidate the memoization; the next query re-derives
 *   from the current declarations.  There is no reconciliation step, because
 *   there is no second copy of the truth to reconcile.
 *
 * - Debug names, decorations and entry point interfaces are not kept as
 *   instruction streams but as properties: names in a map, decorations as
 *   per-id annotation records (kept opaque -- the operands are not
 *   interpreted here beyond the target id and member index), interfaces as
 *   id lists on structured EntryPoint records.  Deleting an id therefore
 *   deletes its names, decorations and interface references by construction.
 *
 * - The globals section is kept in declaration order, but emit() performs a
 *   topological sort, so a pass may freely append declarations or redirect
 *   references (replace_type_id) without regard for ordering.  emit() also
 *   merges duplicate declarations of types that SPIR-V requires to be unique
 *   (see deduplicate_types), which a pass may create when a type replacement
 *   makes two declarations structurally identical.
 *
 * Usage information (whether a variable is loaded or stored, sampled with
 * depth comparison, dynamically indexed, and so on) is deliberately not
 * tracked here; it is a property of the function bodies, computed on demand
 * by SpirVUsageAnalysis.
 *
 * Function bodies are flat instruction vectors (including the OpFunction
 * header and OpFunctionEnd); use SpirVBuilder to insert code at a cursor.
 */
class EXPCL_PANDA_SHADERPIPELINE SpirVModule {
public:
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

  /**
   * Classifies what an id refers to.  Derived from the declaring opcode
   * (see record_result); TEMPORARY covers every instruction result that is
   * not classified more specifically.
   */
  enum class IdKind : uint8_t {
    NONE,
    TYPE,
    POINTER_TYPE,
    FUNCTION_TYPE,
    VARIABLE,
    CONSTANT,
    SPEC_CONSTANT,
    FUNCTION,
    PARAMETER,
    TEMPORARY,
    EXT_INST,
    STRING,
  };

  /**
   * The per-id index entry.  This is the only per-id state the module keeps;
   * it holds what cannot be cheaply re-derived from the instructions.  The
   * resolved ShaderType is a memoization of resolve_type and friends, thrown
   * away wholesale whenever a mutation changes what a type means.
   */
  struct IdEntry {
    IdKind _kind = IdKind::NONE;

    // The result type operand of the declaring instruction, verbatim (so for
    // a variable, the id of its pointer type).  0 for type declarations and
    // untyped results.  The one exception is FUNCTION, which stores the
    // function type id (OpFunction stores the return type in the result type
    // slot, which is available through the function type).
    uint32_t _type_id = 0;

    // The function the id is declared in, or 0 for module scope.  For a
    // FUNCTION, this is the function's own id.
    uint32_t _function_id = 0;

    // Index of the declaring instruction in the globals section, or -1 if
    // the id is not declared there.  Stable: deleted globals are tombstoned
    // in place, never erased.
    int32_t _global_index = -1;

    // Memoized result of resolve_type / resolve_value_type.  Null means "not
    // resolved yet"; OpTypeVoid is special-cased and never cached.
    mutable const ShaderType *_type = nullptr;
  };

  SpirVModule() = default;
  explicit SpirVModule(const InstructionStream &stream);

  bool parse(const uint32_t *words, size_t num_words);
  InstructionStream emit() const;

  INLINE uint32_t get_id_bound() const;
  INLINE uint32_t allocate_id();

  // Id index queries.
  INLINE IdKind get_kind(uint32_t id) const;
  INLINE bool is_type(uint32_t id) const;
  INLINE uint32_t get_function_id(uint32_t id) const;
  uint32_t get_type_id(uint32_t id) const;
  const Instruction *get_decl(uint32_t id) const;

  // (Re)derives the index entry for the given instruction's result id.  The
  // instructions are the single source of truth; call this after editing an
  // instruction's result type in place, so that the index follows.  For
  // module-scope instructions the recorded declaration index is kept.
  void record_result(const Instruction &op, uint32_t function_id);

  // Declaration-derived queries.
  spv::StorageClass get_storage_class(uint32_t id) const;
  INLINE std::string resolve_string(uint32_t id) const;
  uint32_t resolve_constant(uint32_t id) const;
  uint32_t unwrap_pointer_type(uint32_t id) const;
  uint32_t get_num_members(uint32_t type_id) const;
  uint32_t get_member_type_id(uint32_t type_id, uint32_t member_index) const;
  uint32_t get_composite_member_type_id(uint32_t type_id, uint32_t member_index) const;
  pvector<uint32_t> get_function_parameters(uint32_t function_id) const;

  // Annotation-derived queries.  These read the annotation map, so they can
  // never disagree with what emit() writes out.
  bool has_decoration(uint32_t id, spv::Decoration decoration) const;
  uint32_t get_decoration(uint32_t id, spv::Decoration decoration, uint32_t default_value = 0) const;
  bool has_member_decoration(uint32_t type_id, uint32_t member_index, spv::Decoration decoration) const;
  uint32_t get_member_decoration(uint32_t type_id, uint32_t member_index, spv::Decoration decoration, uint32_t default_value = 0) const;
  INLINE int get_location(uint32_t id) const;
  INLINE spv::BuiltIn get_builtin(uint32_t id) const;
  INLINE uint32_t get_spec_id(uint32_t id) const;
  INLINE uint32_t get_array_stride(uint32_t type_id) const;
  INLINE int get_member_offset(uint32_t type_id, uint32_t member_index) const;
  INLINE spv::BuiltIn get_member_builtin(uint32_t type_id, uint32_t member_index) const;

  // ShaderType derivation.  Memoized; see invalidate_types.
  const ShaderType *resolve_type(uint32_t type_id) const;
  const ShaderType *resolve_pointer_type(uint32_t pointer_type_id) const;
  const ShaderType *resolve_value_type(uint32_t id) const;

  // Throws away all memoized ShaderType resolutions and the type reuse map.
  // Called automatically by the mutations that change what a type means; a
  // pass that edits type declaration operands directly must call it itself.
  void invalidate_types();

  // Returns the id of a live, canonical declaration of the given type, or 0.
  uint32_t find_type(const ShaderType *type) const;
  uint32_t find_named(const std::string &name) const;

  // Returns true if the given id is a type declared exactly the way
  // define_type() would declare it, ie. whether find_type may return it.
  INLINE bool is_canonical_type(uint32_t type_id) const;

  void collect_nested_structs(pmap<uint32_t, const ShaderType::Struct *> &result, uint32_t type_id) const;

  // Name handling.  The name stored here is what is emitted as OpName; the
  // reflection name is what parameter extraction reports for the id, for the
  // few passes that give a synthetic variable a qualified name (such as
  // "block.member" after SpirVFlattenStructPass).
  const std::string &get_name(uint32_t id) const;
  void set_name(uint32_t id, const std::string &name);
  const std::string &get_member_name(uint32_t type_id, uint32_t member_index) const;
  void set_member_name(uint32_t type_id, uint32_t member_index, const std::string &name);
  const std::string &get_reflection_name(uint32_t id) const;
  void set_reflection_name(uint32_t id, const std::string &name);

  // Annotation handling.
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
  // module is consistent, and emit() would produce a valid module (provided
  // the pass has upheld its own obligations, such as not leaving references
  // to a deleted id in function bodies).
  void delete_id(uint32_t id);
  void replace_type_id(uint32_t before, uint32_t after);
  void delete_struct_member(uint32_t type_id, uint32_t member_index);
  void remove_function_parameters(uint32_t type_id, const pset<uint32_t> &param_indices);
  void delete_dead_code(pset<uint32_t> &dead_ids);
  void deduplicate_types();

  // Reassigns the result id of the given module-scope declaration, moving
  // its index entry along.  Names and annotations stay with the old id; the
  // caller transfers or reassigns those as appropriate.
  void reassign_global_id(uint32_t old_id, uint32_t new_id);

  // Section access.
  pvector<Instruction> &modify_globals() { return _globals; }
  const pvector<Instruction> &get_globals() const { return _globals; }
  pvector<Function> &modify_functions() { return _functions; }
  const pvector<Function> &get_functions() const { return _functions; }
  pvector<EntryPoint> &modify_entry_points() { return _entry_points; }
  const pvector<EntryPoint> &get_entry_points() const { return _entry_points; }
  const pvector<Instruction> &get_ext_inst_imports() const { return _ext_inst_imports; }
  const pvector<Instruction> &get_execution_modes() const { return _execution_modes; }

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
  // in the id index.
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

  // Adds the given instruction to the globals section, recording its result
  // (if any) in the id index.
  void add_global(Instruction op);

  void output_id(std::ostream &out, uint32_t id) const;
  uint32_t error_expected(uint32_t id, const char *msg) const;

  // Returns true for type-declaring opcodes whose declarations SPIR-V
  // forbids a module from containing twice with identical operands; see
  // deduplicate_types.
  static bool is_unique_type_opcode(spv::Op opcode);

  // Returns, for a globals-section instruction, the operand indices that
  // hold ids (as opposed to literals).  Used by the emit-time topological
  // sort and by replace_type_id.
  static void get_global_id_operands(const Instruction &op, pvector<size_t> &indices);

private:
  INLINE const IdEntry &get_entry(uint32_t id) const;
  INLINE IdEntry &modify_entry(uint32_t id);
  void record_result_at(const Instruction &op, uint32_t function_id, int32_t global_index);
  void clear_id_state(uint32_t id);

  const ShaderType *r_resolve_type(uint32_t id) const;
  const ShaderType *apply_member_decorations(uint32_t type_id, uint32_t member_index, const ShaderType *type) const;
  void get_aggregate_access_flags(uint32_t struct_id, bool &non_writable, bool &non_readable) const;
  spv::StorageClass canonicalize_storage_class(spv::StorageClass storage_class, uint32_t pointee_type_id) const;

  void ensure_type_map() const;
  bool is_canonical_type_decl(const Instruction &op) const;
  bool has_builtin_members(uint32_t type_id) const;

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
  pmap<uint32_t, pvector<std::string> > _member_names;
  pmap<uint32_t, std::string> _reflection_names;
  pmap<uint32_t, Annotations> _annotations;
  pvector<Instruction> _globals;
  pvector<Function> _functions;

  // The id index.
  pvector<IdEntry> _ids;

  // Reverse mapping from ShaderType to the id of a canonical declaration.
  // This is a lookup structure for reusing existing declarations; it is not
  // what SPIR-V's type uniqueness rules rest on (if it misses and a duplicate
  // declaration gets emitted, the finalization sweep run by the
  // SpirVTransformer merges the two afterwards).  Rebuilt lazily whenever
  // the type resolutions are invalidated.
  typedef pmap<const ShaderType *, uint32_t> TypeMap;
  mutable TypeMap _type_map;
  mutable bool _type_map_valid = false;
};

#include "spirVModule.I"

#endif
