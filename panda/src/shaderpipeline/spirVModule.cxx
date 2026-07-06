/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file spirVModule.cxx
 * @author rdb
 * @date 2026-07-07
 */

#include "spirVModule.h"

/**
 * Returns true if this instruction produces a result id.
 */
bool SpirVModule::Instruction::
has_result() const {
  bool has_result, has_type;
  spv::HasResultAndType(opcode, &has_result, &has_type);
  return has_result;
}

/**
 * Returns true if this instruction has a result type operand.
 */
bool SpirVModule::Instruction::
has_result_type() const {
  bool has_result, has_type;
  spv::HasResultAndType(opcode, &has_result, &has_type);
  return has_type;
}

/**
 * Returns the result id of this instruction, or 0 if it has none.
 */
uint32_t SpirVModule::Instruction::
get_result() const {
  bool has_result, has_type;
  spv::HasResultAndType(opcode, &has_result, &has_type);
  if (!has_result || args.size() <= (size_t)has_type) {
    return 0;
  }
  return args[has_type];
}

/**
 * Returns the result type id of this instruction, or 0 if it has none.
 */
uint32_t SpirVModule::Instruction::
get_result_type() const {
  bool has_result, has_type;
  spv::HasResultAndType(opcode, &has_result, &has_type);
  if (!has_type || args.empty()) {
    return 0;
  }
  return args[0];
}

/**
 * Extracts a null-terminated string literal starting at the given operand
 * index.  If end_index is not null, it receives the operand index just past
 * the string.
 */
std::string SpirVModule::Instruction::
get_string(size_t arg_index, size_t *end_index) const {
  std::string result;
  size_t i = arg_index;
  while (i < args.size()) {
    uint32_t word = args[i];
    ++i;
    for (int b = 0; b < 4; ++b) {
      char c = (char)((word >> (b * 8)) & 0xff);
      if (c == 0) {
        if (end_index != nullptr) {
          *end_index = i;
        }
        return result;
      }
      result += c;
    }
  }
  if (end_index != nullptr) {
    *end_index = i;
  }
  return result;
}

/**
 * Materializes the module from the given instruction stream.
 */
SpirVModule::
SpirVModule(const InstructionStream &stream) {
  parse(stream.get_data(), stream.get_data_size());
}

/**
 * Parses the given words into this module.  Returns false if the stream is
 * malformed; the module contents are undefined in that case.
 */
bool SpirVModule::
parse(const uint32_t *words, size_t num_words) {
  if (num_words < 5 || words[0] != spv::MagicNumber) {
    shader_cat.error()
      << "Invalid SPIR-V file header\n";
    return false;
  }
  _version = words[1];
  _generator = words[2];
  _id_bound = words[3];
  _schema = words[4];

  // Presize the definitions array.
  if (_id_bound > 0) {
    _db.modify_definition(_id_bound - 1);
  }

  uint32_t current_function_id = 0;
  Function *function = nullptr;

  size_t i = 5;
  while (i < num_words) {
    spv::Op opcode = (spv::Op)(words[i] & spv::OpCodeMask);
    uint32_t wcount = words[i] >> spv::WordCountShift;
    if (wcount == 0 || i + wcount > num_words) {
      shader_cat.error()
        << "Encountered malformed instruction during SPIR-V parse\n";
      return false;
    }
    const uint32_t *args = words + i + 1;
    uint32_t nargs = wcount - 1;
    i += wcount;

    Instruction op(opcode, args, nargs);

    switch (opcode) {
    case spv::OpNop:
      continue;

    case spv::OpCapability:
      _capabilities.push_back(std::move(op));
      continue;

    case spv::OpExtension:
      _extensions.push_back(std::move(op));
      continue;

    case spv::OpExtInstImport:
      _db.parse_instruction(opcode, args, nargs, current_function_id);
      _ext_inst_imports.push_back(std::move(op));
      continue;

    case spv::OpMemoryModel:
      _memory_model = std::move(op);
      continue;

    case spv::OpEntryPoint:
      {
        EntryPoint ep;
        ep.model = (spv::ExecutionModel)args[0];
        ep.function_id = args[1];
        size_t end = 2;
        ep.name = op.get_string(2, &end);
        ep.interface_vars.assign(op.args.begin() + end, op.args.end());
        _entry_points.push_back(std::move(ep));
      }
      continue;

    case spv::OpExecutionMode:
    case spv::OpExecutionModeId:
      _execution_modes.push_back(std::move(op));
      continue;

    case spv::OpString:
      _db.parse_instruction(opcode, args, nargs, current_function_id);
      _debug.push_back(std::move(op));
      continue;

    case spv::OpSource:
    case spv::OpSourceContinued:
    case spv::OpSourceExtension:
      _debug.push_back(std::move(op));
      continue;

    case spv::OpModuleProcessed:
      _module_processed.push_back(std::move(op));
      continue;

    case spv::OpName:
      if (nargs >= 2) {
        std::string name = op.get_string(1);
        if (!name.empty()) {
          _names[args[0]] = std::move(name);
        }
      }
      _db.parse_instruction(opcode, args, nargs, current_function_id);
      continue;

    case spv::OpMemberName:
      // Member names are kept on the member definitions in the database.
      _db.parse_instruction(opcode, args, nargs, current_function_id);
      continue;

    case spv::OpLine:
    case spv::OpNoLine:
      // Kept inside function bodies, stripped elsewhere.
      if (function != nullptr) {
        function->instructions.push_back(std::move(op));
      }
      continue;

    case spv::OpDecorate:
    case spv::OpMemberDecorate:
    case spv::OpDecorateId:
    case spv::OpDecorateString:
    case spv::OpMemberDecorateString:
    case spv::OpDecorationGroup:
    case spv::OpGroupDecorate:
    case spv::OpGroupMemberDecorate:
      if (nargs >= 1) {
        Annotation annotation;
        annotation.opcode = opcode;
        annotation.args.assign(op.args.begin() + 1, op.args.end());
        _annotations[args[0]].push_back(std::move(annotation));
      }
      _db.parse_instruction(opcode, args, nargs, current_function_id);
      continue;

    case spv::OpFunction:
      if (function != nullptr) {
        shader_cat.error()
          << "OpFunction may not occur within another function!\n";
        return false;
      }
      _db.parse_instruction(opcode, args, nargs, current_function_id);
      _functions.push_back(Function {nargs >= 2 ? args[1] : 0u, {}});
      function = &_functions.back();
      function->instructions.push_back(std::move(op));
      continue;

    case spv::OpFunctionEnd:
      if (function == nullptr) {
        shader_cat.error()
          << "OpFunctionEnd may only occur within a function!\n";
        return false;
      }
      _db.parse_instruction(opcode, args, nargs, current_function_id);
      function->instructions.push_back(std::move(op));
      function = nullptr;
      continue;

    default:
      _db.parse_instruction(opcode, args, nargs, current_function_id);
      if (function != nullptr) {
        function->instructions.push_back(std::move(op));
      } else {
        _globals.push_back(std::move(op));
      }
      continue;
    }
  }

  if (function != nullptr) {
    shader_cat.error()
      << "Encountered end of stream before function end\n";
    return false;
  }
  return true;
}

/**
 * Appends the given instruction to the given word vector.
 */
void SpirVModule::
emit_instruction(std::vector<uint32_t> &words, spv::Op opcode,
                 const uint32_t *args, size_t nargs) {
  words.push_back(((uint32_t)(nargs + 1) << spv::WordCountShift) | opcode);
  words.insert(words.end(), args, args + nargs);
}

/**
 * Appends a string literal (padded with at least one zero byte) to the given
 * argument vector.
 */
static void
pack_string(pvector<uint32_t> &args, const std::string &str) {
  size_t offset = args.size();
  args.resize(offset + str.size() / 4 + 1, 0u);
  memcpy((char *)(args.data() + offset), str.data(), str.size());
}

/**
 * Serializes the module back into an instruction stream.  This is where the
 * module-wide invariants are enforced: globals are emitted in topological
 * order, tombstones are skipped, and names, decorations and entry point
 * interfaces are generated from the id table, so ids that have been deleted
 * leave no trace.
 *
 * Call deduplicate_types() first if a pass may have created duplicate
 * declarations of unique types (SpirVTransformer::run does this after every
 * pass).
 */
SpirVModule::InstructionStream SpirVModule::
emit() const {
  std::vector<uint32_t> words {
    spv::MagicNumber, _version, _generator, _id_bound, _schema,
  };

  for (const Instruction &op : _capabilities) {
    emit_instruction(words, op.opcode, op.args.data(), op.args.size());
  }
  for (const Instruction &op : _extensions) {
    emit_instruction(words, op.opcode, op.args.data(), op.args.size());
  }
  for (const Instruction &op : _ext_inst_imports) {
    emit_instruction(words, op.opcode, op.args.data(), op.args.size());
  }
  if (_memory_model.opcode == spv::OpMemoryModel) {
    emit_instruction(words, _memory_model.opcode, _memory_model.args.data(),
                     _memory_model.args.size());
  }

  for (const EntryPoint &ep : _entry_points) {
    pvector<uint32_t> args({(uint32_t)ep.model, ep.function_id});
    pack_string(args, ep.name);
    args.insert(args.end(), ep.interface_vars.begin(), ep.interface_vars.end());
    emit_instruction(words, spv::OpEntryPoint, args.data(), args.size());
  }
  for (const Instruction &op : _execution_modes) {
    emit_instruction(words, op.opcode, op.args.data(), op.args.size());
  }

  // Debug section: strings and sources, then names, then OpModuleProcessed.
  for (const Instruction &op : _debug) {
    emit_instruction(words, op.opcode, op.args.data(), op.args.size());
  }
  for (const auto &item : _names) {
    if (item.second.empty()) {
      continue;
    }
    pvector<uint32_t> args({item.first});
    pack_string(args, item.second);
    emit_instruction(words, spv::OpName, args.data(), args.size());
  }
  for (uint32_t id = 0; id < _id_bound; ++id) {
    const Definition &def = _db.get_definition(id);
    for (size_t mi = 0; mi < def._members.size(); ++mi) {
      const MemberDefinition &member = def._members[mi];
      if (!member._name.empty()) {
        pvector<uint32_t> args({id, (uint32_t)mi});
        pack_string(args, member._name);
        emit_instruction(words, spv::OpMemberName, args.data(), args.size());
      }
    }
  }
  for (const Instruction &op : _module_processed) {
    emit_instruction(words, op.opcode, op.args.data(), op.args.size());
  }

  // Annotations, in id order.
  for (const auto &item : _annotations) {
    for (const Annotation &annotation : item.second) {
      pvector<uint32_t> args({item.first});
      args.insert(args.end(), annotation.args.begin(), annotation.args.end());
      emit_instruction(words, annotation.opcode, args.data(), args.size());
    }
  }

  // Globals, in topological order.  A pass may have appended a declaration
  // that an earlier declaration was redirected to (via replace_type_id), so
  // dependency order is restored here rather than maintained incrementally.
  for (size_t index : sort_globals()) {
    const Instruction &op = _globals[index];
    emit_instruction(words, op.opcode, op.args.data(), op.args.size());
  }

  for (const Function &function : _functions) {
    for (const Instruction &op : function.instructions) {
      if (!op.is_nop()) {
        emit_instruction(words, op.opcode, op.args.data(), op.args.size());
      }
    }
  }

  return InstructionStream(std::move(words));
}

/**
 * Returns the indices of the (non-tombstoned) globals in topological order:
 * every declaration is preceded by the declarations it references.  This is
 * the order in which emit() writes them out, and the order in which
 * deduplicate_types() visits them (so that merges cascade in one pass).
 */
pvector<size_t> SpirVModule::
sort_globals() const {
  pmap<uint32_t, size_t> decl_index;
  for (size_t i = 0; i < _globals.size(); ++i) {
    uint32_t result_id = _globals[i].get_result();
    if (result_id != 0) {
      decl_index[result_id] = i;
    }
  }
  pvector<size_t> order;
  order.reserve(_globals.size());
  pvector<int> state(_globals.size(), 0);
  for (size_t i = 0; i < _globals.size(); ++i) {
    r_sort_global(i, decl_index, state, order);
  }
  return order;
}

/**
 * Appends the global with the given index to the order, after first
 * appending any not-yet-visited globals it references.
 */
void SpirVModule::
r_sort_global(size_t index, const pmap<uint32_t, size_t> &decl_index,
              pvector<int> &state, pvector<size_t> &order) const {
  if (state[index] != 0) {
    // Done, or currently being visited further up the stack (which would be
    // a reference cycle; SPIR-V forbids those for the types supported here,
    // so this is only hit if a literal was misidentified as an id, in which
    // case original order is the best we can do).
    return;
  }
  state[index] = 1;

  const Instruction &op = _globals[index];
  if (!op.is_nop()) {
    pvector<size_t> id_operands;
    get_global_id_operands(op, id_operands);
    for (size_t arg_index : id_operands) {
      auto it = decl_index.find(op.args[arg_index]);
      if (it != decl_index.end() && it->second != index) {
        r_sort_global(it->second, decl_index, state, order);
      }
    }
    order.push_back(index);
  }
  state[index] = 2;
}

/**
 * Returns the operand indices of the given globals-section instruction that
 * hold ids rather than literals.  The result id itself is not included.
 */
void SpirVModule::
get_global_id_operands(const Instruction &op, pvector<size_t> &indices) {
  indices.clear();
  switch (op.opcode) {
  case spv::OpTypeVector:
  case spv::OpTypeMatrix:
  case spv::OpTypeImage:
  case spv::OpTypeSampledImage:
  case spv::OpTypeRuntimeArray:
    // Component / sampled / element type.
    indices.push_back(1);
    break;

  case spv::OpTypeArray:
    // Element type and length constant.
    indices.push_back(1);
    indices.push_back(2);
    break;

  case spv::OpTypeStruct:
  case spv::OpTypeFunction:
  case spv::OpTypeCooperativeMatrixKHR:
  case spv::OpTypeCooperativeMatrixNV:
    // All operands after the result id.
    for (size_t i = 1; i < op.args.size(); ++i) {
      indices.push_back(i);
    }
    break;

  case spv::OpTypePointer:
    // Pointee type.
    indices.push_back(2);
    break;

  case spv::OpConstant:
  case spv::OpSpecConstant:
  case spv::OpConstantTrue:
  case spv::OpConstantFalse:
  case spv::OpConstantNull:
  case spv::OpSpecConstantTrue:
  case spv::OpSpecConstantFalse:
  case spv::OpUndef:
    // Result type only; the rest is a literal payload.
    indices.push_back(0);
    break;

  case spv::OpConstantComposite:
  case spv::OpSpecConstantComposite:
    // Result type and constituents.
    indices.push_back(0);
    for (size_t i = 2; i < op.args.size(); ++i) {
      indices.push_back(i);
    }
    break;

  case spv::OpSpecConstantOp:
    // Result type, then (after the opcode literal) the operands.  Note that
    // a few SpecConstantOp opcodes (such as CompositeExtract) take literal
    // indices; those may be misidentified as ids here, which at worst
    // perturbs the emit order (the cycle guard prevents anything worse).
    indices.push_back(0);
    for (size_t i = 3; i < op.args.size(); ++i) {
      indices.push_back(i);
    }
    break;

  case spv::OpVariable:
    // Result type and optional initializer.
    indices.push_back(0);
    if (op.args.size() > 3) {
      indices.push_back(3);
    }
    break;

  default:
    // Type declarations without operands (OpTypeVoid, OpTypeInt, ...) and
    // unrecognized instructions, which keep their original relative order.
    break;
  }
}

/**
 * Returns the function with the given id, or null if it does not exist.
 */
SpirVModule::Function *SpirVModule::
get_function(uint32_t function_id) {
  for (Function &function : _functions) {
    if (function.id == function_id) {
      return &function;
    }
  }
  return nullptr;
}

/**
 * Returns the globals-section instruction with the given result id, or null
 * if there is none.
 */
SpirVModule::Instruction *SpirVModule::
get_global(uint32_t result_id) {
  for (Instruction &op : _globals) {
    if (!op.is_nop() && op.get_result() == result_id) {
      return &op;
    }
  }
  return nullptr;
}

/**
 * Returns the name that will be emitted for the given id.
 */
const std::string &SpirVModule::
get_name(uint32_t id) const {
  static const std::string empty;
  auto it = _names.find(id);
  return it != _names.end() ? it->second : empty;
}

/**
 * Sets the name emitted for the given id, and mirrors it into the database.
 * (A pass may subsequently store a different, qualified name in the database
 * for consumers; only the name set here is emitted.)
 */
void SpirVModule::
set_name(uint32_t id, const std::string &name) {
  _names[id] = name;
  _db.modify_definition(id)._name = name;
}

/**
 * Sets the name of the given struct member.
 */
void SpirVModule::
set_member_name(uint32_t type_id, uint32_t member_index, const std::string &name) {
  _db.modify_definition(type_id).modify_member(member_index)._name = name;
}

/**
 * Attaches the given annotation to the given id, and feeds it through the
 * database's decoration decoder so that queries stay consistent.
 */
void SpirVModule::
decorate(uint32_t id, const Annotation &annotation) {
  _annotations[id].push_back(annotation);

  pvector<uint32_t> args({id});
  args.insert(args.end(), annotation.args.begin(), annotation.args.end());
  uint32_t current_function_id = 0;
  _db.parse_instruction(annotation.opcode, args.data(), (uint32_t)args.size(),
                        current_function_id);
}

/**
 * Adds a member decoration without extra operands.
 */
void SpirVModule::
decorate_member(uint32_t type_id, uint32_t member_index, spv::Decoration decoration) {
  decorate(type_id, Annotation {spv::OpMemberDecorate,
    {member_index, (uint32_t)decoration}});
}

/**
 * Adds a member decoration with one extra operand.
 */
void SpirVModule::
decorate_member(uint32_t type_id, uint32_t member_index, spv::Decoration decoration, uint32_t value) {
  decorate(type_id, Annotation {spv::OpMemberDecorate,
    {member_index, (uint32_t)decoration, value}});
}

/**
 * Removes all non-member annotations with the given decoration from the
 * given id.  Returns true if any was removed.  Note that this does not reset
 * whatever state the database derived from the decoration; the caller does
 * that, since the correct "absent" value depends on the decoration.
 */
bool SpirVModule::
remove_decoration(uint32_t id, spv::Decoration decoration) {
  auto it = _annotations.find(id);
  if (it == _annotations.end()) {
    return false;
  }
  bool any = false;
  Annotations &annotations = it->second;
  for (size_t i = annotations.size(); i-- > 0;) {
    if (!annotations[i].is_member_annotation() &&
        annotations[i].get_decoration() == decoration) {
      annotations.erase(annotations.begin() + i);
      any = true;
    }
  }
  return any;
}

/**
 * Sets or replaces a single-operand decoration on the given id.
 */
void SpirVModule::
set_decoration(uint32_t id, spv::Decoration decoration, uint32_t value) {
  remove_decoration(id, decoration);
  decorate(id, decoration, value);
}

/**
 * Removes all non-member annotations with the given decoration from every
 * id in the module.  As with remove_decoration, the database state derived
 * from the decoration is the caller's to reset.
 */
void SpirVModule::
strip_decoration(spv::Decoration decoration) {
  for (auto &item : _annotations) {
    Annotations &annotations = item.second;
    for (size_t i = annotations.size(); i-- > 0;) {
      if (!annotations[i].is_member_annotation() &&
          annotations[i].get_decoration() == decoration) {
        annotations.erase(annotations.begin() + i);
      }
    }
  }
}

/**
 * Moves all annotations from one id to another, appending them to any the
 * target already has.  The decoration-derived state in the database is not
 * transferred; the caller does that (typically by moving the definition).
 */
void SpirVModule::
transfer_annotations(uint32_t from, uint32_t to) {
  auto it = _annotations.find(from);
  if (it == _annotations.end()) {
    return;
  }
  Annotations &target = _annotations[to];
  target.insert(target.end(),
                std::make_move_iterator(it->second.begin()),
                std::make_move_iterator(it->second.end()));
  _annotations.erase(it);
}

/**
 * Sets or replaces the location decoration of the given id.
 */
void SpirVModule::
set_location(uint32_t id, int location) {
  remove_decoration(id, spv::DecorationLocation);
  decorate(id, spv::DecorationLocation, (uint32_t)location);
}

/**
 * Deletes the given id: removes its declaration, name, annotations and entry
 * point interface references, and clears its definition.  Takes effect
 * immediately.
 *
 * References to the id in function bodies are the pass's responsibility
 * (see delete_dead_code for the common cascade); deletions do not cascade to
 * dependent declarations automatically.
 */
void SpirVModule::
delete_id(uint32_t id) {
  Definition &def = _db.modify_definition(id);

  if (def.is_function()) {
    // Remove the entire function, clearing the definitions of everything
    // declared inside it.
    for (size_t fi = 0; fi < _functions.size(); ++fi) {
      if (_functions[fi].id != id) {
        continue;
      }
      for (const Instruction &op : _functions[fi].instructions) {
        uint32_t result_id = op.get_result();
        if (result_id != 0 && result_id != id) {
          _names.erase(result_id);
          _annotations.erase(result_id);
          _db.modify_definition(result_id).clear();
        }
      }
      _functions.erase(_functions.begin() + fi);
      break;
    }

    // Also drop any execution modes and entry points that refer to it.
    for (size_t i = _execution_modes.size(); i-- > 0;) {
      if (!_execution_modes[i].args.empty() && _execution_modes[i].args[0] == id) {
        _execution_modes.erase(_execution_modes.begin() + i);
      }
    }
    for (size_t i = _entry_points.size(); i-- > 0;) {
      if (_entry_points[i].function_id == id) {
        _entry_points.erase(_entry_points.begin() + i);
      }
    }
  }
  else if (def._function_id != 0) {
    // Defined inside a function body.
    Function *function = get_function(def._function_id);
    if (function != nullptr) {
      for (Instruction &op : function->instructions) {
        if (!op.is_nop() && op.get_result() == id) {
          op = Instruction(spv::OpNop);
          break;
        }
      }
    }
  }
  else if (def.is_ext_inst()) {
    for (size_t i = _ext_inst_imports.size(); i-- > 0;) {
      if (!_ext_inst_imports[i].args.empty() && _ext_inst_imports[i].args[0] == id) {
        _ext_inst_imports.erase(_ext_inst_imports.begin() + i);
      }
    }
  }
  else if (def.is_string()) {
    for (size_t i = _debug.size(); i-- > 0;) {
      if (_debug[i].opcode == spv::OpString &&
          !_debug[i].args.empty() && _debug[i].args[0] == id) {
        _debug.erase(_debug.begin() + i);
      }
    }
  }
  else {
    Instruction *decl = get_global(id);
    if (decl != nullptr) {
      *decl = Instruction(spv::OpNop);
    }
  }

  // Remove it from all entry point interfaces.
  for (EntryPoint &ep : _entry_points) {
    for (size_t i = ep.interface_vars.size(); i-- > 0;) {
      if (ep.interface_vars[i] == id) {
        ep.interface_vars.erase(ep.interface_vars.begin() + i);
      }
    }
  }

  _names.erase(id);
  _annotations.erase(id);
  _db.forget_type(id);
  def.clear();
}

/**
 * Redirects all references to the given type id to the given other type id,
 * and removes the old declaration.  Takes effect immediately: subsequent
 * reads of the module see the new id everywhere.
 *
 * May only be called for type ids (including pointer and function types).
 * The literal payload of a constant is encoded according to its type, so a
 * pass that replaces a type with a differently laid out one must rewrite the
 * constants of that type itself.
 *
 * There is deliberately no general replace_id for value ids: those can be
 * forward-referenced (eg. by OpPhi) and appear in operand positions that
 * cannot be distinguished from literals without a full grammar; a pass that
 * replaces values rewrites the affected instructions itself.
 *
 * Note that the resolved ShaderType recorded for downstream definitions
 * (such as a variable of a sampled image type whose image type was replaced)
 * is not re-derived here; SpirVTransformer::run reconciles the database
 * after the pass has finished (see reparse_db).
 */
void SpirVModule::
replace_type_id(uint32_t before, uint32_t after) {
  nassertv(before != 0u && after != 0u);
  if (before == after) {
    return;
  }
  nassertv(_db.get_definition(before).is_type());
  nassertv(_db.get_definition(after).is_type());

  rewrite_type_references(before, after);

  Instruction *decl = get_global(before);
  if (decl != nullptr) {
    *decl = Instruction(spv::OpNop);
  }

  _names.erase(before);
  _annotations.erase(before);
  _db.forget_type(before);
  _db.modify_definition(before).clear();
}

/**
 * Rewrites all references to the given type id, in both the instructions and
 * the database's recorded type ids.
 */
void SpirVModule::
rewrite_type_references(uint32_t before, uint32_t after) {
  pvector<size_t> id_operands;
  for (Instruction &op : _globals) {
    if (op.is_nop()) {
      continue;
    }
    get_global_id_operands(op, id_operands);
    for (size_t arg_index : id_operands) {
      if (op.args[arg_index] == before) {
        op.args[arg_index] = after;
      }
    }
  }

  for (Function &function : _functions) {
    for (Instruction &op : function.instructions) {
      if (op.is_nop()) {
        continue;
      }
      // Type ids can occur as the result type, as the function type operand
      // of OpFunction, and (uniquely) as a regular operand of the
      // cooperative matrix length instructions.
      if (op.has_result_type() && !op.args.empty() && op.args[0] == before) {
        op.args[0] = after;
      }
      if (op.opcode == spv::OpFunction && op.args.size() >= 4 &&
          op.args[3] == before) {
        op.args[3] = after;
      }
      else if ((op.opcode == spv::OpCooperativeMatrixLengthKHR ||
                op.opcode == spv::OpCooperativeMatrixLengthNV) &&
               op.args.size() >= 3 && op.args[2] == before) {
        op.args[2] = after;
      }
    }
  }

  // Update the recorded type ids in the database, so that queries made
  // during the rest of the pass resolve to the replacement.
  for (uint32_t id = 0; id < _id_bound; ++id) {
    Definition &def = _db.modify_definition(id);
    if (def._type_id == before) {
      def._type_id = after;
    }
    for (MemberDefinition &member : def._members) {
      if (member._type_id == before) {
        member._type_id = after;
      }
    }
    if (def.is_function_type()) {
      for (uint32_t &param : def._parameters) {
        if (param == before) {
          param = after;
        }
      }
    }
  }
}

/**
 * Deletes the given member (by current index) from the given struct type
 * declaration: the OpTypeStruct operand, the recorded member definition
 * (along with its name) and its member decorations are all removed, and
 * higher member indices are renumbered.
 *
 * Access chains indexing into the struct are the pass's responsibility, as
 * is re-registering the struct's ShaderType if it needs to remain accurate
 * during the pass (the post-pass reconciliation re-derives it regardless).
 */
void SpirVModule::
delete_struct_member(uint32_t type_id, uint32_t member_index) {
  Instruction *decl = get_global(type_id);
  nassertv(decl != nullptr && decl->opcode == spv::OpTypeStruct);
  nassertv(member_index + 1 < decl->args.size());
  decl->args.erase(decl->args.begin() + 1 + member_index);

  Definition &def = _db.modify_definition(type_id);
  if (member_index < def._members.size()) {
    def._members.erase(def._members.begin() + member_index);
    for (size_t i = 0; i < def._members.size(); ++i) {
      def._members[i]._new_index = (int)i;
    }
  }

  auto it = _annotations.find(type_id);
  if (it != _annotations.end()) {
    Annotations &annotations = it->second;
    for (size_t i = annotations.size(); i-- > 0;) {
      Annotation &annotation = annotations[i];
      if (annotation.is_member_annotation() && !annotation.args.empty()) {
        if (annotation.args[0] == member_index) {
          annotations.erase(annotations.begin() + i);
        }
        else if (annotation.args[0] > member_index) {
          --annotation.args[0];
        }
      }
    }
  }
}

/**
 * Removes the given parameters (by index) from the given function type: the
 * OpTypeFunction declaration is rebuilt without them, the matching
 * OpFunctionParameter instructions of every function of this type are
 * removed (and their ids deleted), and the matching arguments are stripped
 * from every call to such a function.
 *
 * If this leaves the declaration identical to another function type, the two
 * are merged by deduplicate_types() afterwards.
 */
void SpirVModule::
remove_function_parameters(uint32_t type_id, const pset<uint32_t> &param_indices) {
  if (param_indices.empty()) {
    return;
  }

  Instruction *decl = get_global(type_id);
  nassertv(decl != nullptr && decl->opcode == spv::OpTypeFunction);

  Definition &type_def = _db.modify_definition(type_id);
  for (auto it = param_indices.rbegin(); it != param_indices.rend(); ++it) {
    uint32_t index = *it;
    nassertd(index + 2 < decl->args.size()) continue;
    decl->args.erase(decl->args.begin() + 2 + index);
    if (index < type_def._parameters.size()) {
      type_def._parameters.erase(type_def._parameters.begin() + index);
    }
  }

  // Remove the matching OpFunctionParameter instructions of every function
  // of this type, and strip the matching arguments from every call to such
  // a function.
  pset<uint32_t> affected_functions;
  for (uint32_t id = 0; id < _id_bound; ++id) {
    const Definition &def = _db.get_definition(id);
    if (def.is_function() && def._type_id == type_id) {
      affected_functions.insert(id);
    }
  }

  for (Function &function : _functions) {
    if (affected_functions.count(function.id)) {
      uint32_t param_index = 0;
      for (Instruction &op : function.instructions) {
        if (op.opcode == spv::OpFunctionParameter) {
          if (param_indices.count(param_index)) {
            uint32_t param_id = op.get_result();
            op = Instruction(spv::OpNop);
            _names.erase(param_id);
            _annotations.erase(param_id);
            _db.modify_definition(param_id).clear();
          }
          ++param_index;
        }
      }
    }

    for (Instruction &op : function.instructions) {
      if (op.opcode == spv::OpFunctionCall && op.args.size() >= 3 &&
          affected_functions.count(op.args[2])) {
        for (auto it = param_indices.rbegin(); it != param_indices.rend(); ++it) {
          size_t arg_index = 3 + *it;
          nassertd(arg_index < op.args.size()) continue;
          op.args.erase(op.args.begin() + arg_index);
        }
      }
    }
  }

  // Update the recorded parameter lists of the affected functions.
  for (uint32_t function_id : affected_functions) {
    Definition &def = _db.modify_definition(function_id);
    for (auto it = param_indices.rbegin(); it != param_indices.rend(); ++it) {
      if (*it < def._parameters.size()) {
        def._parameters.erase(def._parameters.begin() + *it);
      }
    }
  }
}

/**
 * Removes function-body instructions that reference the given dead pointer
 * ids: access chains, pointer copies and casts based on a dead id are
 * removed and their results added to the dead set (cascading forward), while
 * a load from or store to a dead id triggers an assertion, since the id
 * evidently was not dead.  Instructions whose own result id is in the set
 * are removed as well.
 */
void SpirVModule::
delete_dead_code(pset<uint32_t> &dead_ids) {
  if (dead_ids.empty()) {
    return;
  }

  for (Function &function : _functions) {
    for (Instruction &op : function.instructions) {
      if (op.is_nop()) {
        continue;
      }

      bool remove = false;
      switch (op.opcode) {
      case spv::OpAccessChain:
      case spv::OpInBoundsAccessChain:
      case spv::OpPtrAccessChain:
      case spv::OpInBoundsPtrAccessChain:
      case spv::OpImageTexelPointer:
      case spv::OpCopyObject:
      case spv::OpExpectKHR:
      case spv::OpBitcast:
      case spv::OpCopyLogical:
        remove = op.args.size() >= 3 && dead_ids.count(op.args[2]) != 0;
        break;

      case spv::OpLoad:
      case spv::OpAtomicLoad:
        nassertd(op.args.size() < 3 || !dead_ids.count(op.args[2])) {}
        break;

      case spv::OpStore:
      case spv::OpAtomicStore:
        nassertd(op.args.empty() || !dead_ids.count(op.args[0])) {}
        break;

      case spv::OpCopyMemory:
      case spv::OpCopyMemorySized:
        nassertd(op.args.size() < 2 || (!dead_ids.count(op.args[0]) &&
                                        !dead_ids.count(op.args[1]))) {}
        break;

      default:
        break;
      }

      uint32_t result_id = op.get_result();
      if (!remove && result_id != 0 && dead_ids.count(result_id)) {
        remove = true;
      }

      if (remove) {
        if (result_id != 0) {
          dead_ids.insert(result_id);
          _names.erase(result_id);
          _annotations.erase(result_id);
          _db.modify_definition(result_id).clear();
        }
        op = Instruction(spv::OpNop);
      }
    }
  }
}

/**
 * Merges duplicate declarations of types that SPIR-V requires to be unique
 * within a module (see SpirVResultDatabase::is_unique_type_opcode), plus
 * function types.  A later declaration is merged onto the earlier identical
 * one; all references are rewritten.  Aggregate types (structs and arrays)
 * are exempt, since SPIR-V permits and gives meaning to their duplication.
 *
 * SpirVTransformer::run calls this after every pass; passes that run outside
 * the transformer (or tests) may call it directly before emit().
 */
void SpirVModule::
deduplicate_types() {
  pmap<pvector<uint32_t>, uint32_t> seen;

  // Visit the declarations in topological order rather than section order:
  // a replacement type appended at the end of the section is then visited
  // before the declarations that reference it, so that merges cascade
  // through dependent types (sampled image, pointer, function types) in a
  // single pass.
  for (size_t index : sort_globals()) {
    Instruction &op = _globals[index];
    if (op.is_nop()) {
      // Tombstoned by a merge earlier in this scan.
      continue;
    }
    if (!SpirVResultDatabase::is_unique_type_opcode(op.opcode) &&
        op.opcode != spv::OpTypeFunction) {
      continue;
    }

    pvector<uint32_t> key;
    key.reserve(op.args.size());
    key.push_back((uint32_t)op.opcode);
    key.insert(key.end(), op.args.begin() + 1, op.args.end());

    auto result = seen.insert(std::make_pair(std::move(key), op.args[0]));
    if (!result.second && result.first->second != op.args[0]) {
      // An identical declaration already exists; merge this one onto it.
      // This may in turn make downstream declarations identical, which the
      // rest of this scan picks up, since the rewrite has already been
      // applied by the time they are visited.
      replace_type_id(op.args[0], result.first->second);
    }
  }
}

/**
 * Re-derives the database from the module's current definitions and
 * function bodies, replaying them through the parser.  This makes the
 * database identical to what a fresh parse of the emitted module would
 * produce (names and usage flags excepted, which are carried over), which
 * releases the individual mutation methods from having to re-derive
 * resolved ShaderTypes of downstream definitions.  SpirVTransformer::run
 * calls this after every pass.
 */
void SpirVModule::
reparse_db() {
  // Function parameter lists are appended to by the parser (unlike all other
  // recorded state, which overwrites), so clear them first.
  for (uint32_t id = 0; id < _id_bound; ++id) {
    Definition &def = _db.modify_definition(id);
    if (def.is_function()) {
      def._parameters.clear();
    }
  }

  uint32_t current_function_id = 0;
  for (const Instruction &op : _globals) {
    if (!op.is_nop()) {
      _db.parse_instruction(op.opcode, op.args.data(), (uint32_t)op.args.size(),
                            current_function_id);
    }
  }
  for (const Function &function : _functions) {
    for (const Instruction &op : function.instructions) {
      if (!op.is_nop()) {
        _db.parse_instruction(op.opcode, op.args.data(), (uint32_t)op.args.size(),
                              current_function_id);
      }
    }
  }
}

/**
 * Adds the given instruction to the globals section.
 */
void SpirVModule::
add_global(const Instruction &op) {
  _globals.push_back(op);
}

/**
 * Adds the given capability to the module, if it is not already declared.
 */
void SpirVModule::
add_capability(spv::Capability capability) {
  for (const Instruction &op : _capabilities) {
    if (!op.args.empty() && op.args[0] == (uint32_t)capability) {
      return;
    }
  }
  _capabilities.push_back(Instruction(spv::OpCapability, {(uint32_t)capability}));
}

/**
 * Adds the given extension to the module, if it is not already declared.
 */
void SpirVModule::
add_extension(const std::string &name) {
  for (const Instruction &op : _extensions) {
    if (op.get_string(0) == name) {
      return;
    }
  }
  Instruction op(spv::OpExtension);
  pack_string(op.args, name);
  _extensions.push_back(std::move(op));
}

/**
 * Sets the addressing and memory model of the module.
 */
void SpirVModule::
set_memory_model(spv::AddressingModel addressing, spv::MemoryModel memory) {
  _memory_model = Instruction(spv::OpMemoryModel,
    {(uint32_t)addressing, (uint32_t)memory});
}

/**
 * Adds an entry point for the given function.
 */
void SpirVModule::
add_entry_point(spv::ExecutionModel model, uint32_t function_id,
                const std::string &name, pvector<uint32_t> interface_vars) {
  EntryPoint ep;
  ep.model = model;
  ep.function_id = function_id;
  ep.name = name;
  ep.interface_vars = std::move(interface_vars);
  _entry_points.push_back(std::move(ep));
}

/**
 * Adds an execution mode for the given entry point function.
 */
void SpirVModule::
add_execution_mode(uint32_t function_id, spv::ExecutionMode mode,
                   std::initializer_list<uint32_t> args) {
  Instruction op(spv::OpExecutionMode, {function_id, (uint32_t)mode});
  op.args.insert(op.args.end(), args.begin(), args.end());
  _execution_modes.push_back(std::move(op));
}

/**
 * Adds an OpString with the given contents and returns its id.
 */
uint32_t SpirVModule::
add_string(const std::string &str) {
  uint32_t id = allocate_id();
  Instruction op(spv::OpString, {id});
  pack_string(op.args, str);
  uint32_t current_function_id = 0;
  _db.parse_instruction(op.opcode, op.args.data(), (uint32_t)op.args.size(),
                        current_function_id);
  _debug.push_back(std::move(op));
  return id;
}

/**
 * Adds an OpExtInstImport with the given set name and returns its id.
 */
uint32_t SpirVModule::
add_ext_inst_import(const std::string &name) {
  for (const Instruction &op : _ext_inst_imports) {
    const Definition &def = _db.get_definition(op.args[0]);
    if (def._name == name) {
      return op.args[0];
    }
  }
  uint32_t id = allocate_id();
  Instruction op(spv::OpExtInstImport, {id});
  pack_string(op.args, name);
  uint32_t current_function_id = 0;
  _db.parse_instruction(op.opcode, op.args.data(), (uint32_t)op.args.size(),
                        current_function_id);
  _ext_inst_imports.push_back(std::move(op));
  return id;
}

/**
 * Adds the given variable to the interface of every entry point with the
 * given execution model, if it is not already listed.
 */
void SpirVModule::
add_to_interfaces(spv::ExecutionModel model, uint32_t var_id) {
  for (EntryPoint &ep : _entry_points) {
    if (ep.model == model &&
        std::find(ep.interface_vars.begin(), ep.interface_vars.end(), var_id) == ep.interface_vars.end()) {
      ep.interface_vars.push_back(var_id);
    }
  }
}

/**
 * Makes sure that a module-scope input variable with the given builtin
 * exists and is part of the interface of all entry points with the given
 * execution model, and returns its id.
 */
uint32_t SpirVModule::
ensure_builtin_input(spv::ExecutionModel model, spv::BuiltIn builtin) {
  uint32_t var_id = 0;
  for (uint32_t id = 0; id < _id_bound; ++id) {
    const Definition &def = _db.get_definition(id);
    if (def.is_variable() && def._storage_class == spv::StorageClassInput &&
        def._builtin == builtin) {
      var_id = id;
      break;
    }
  }

  if (var_id == 0) {
    const ShaderType *type;
    switch (builtin) {
    case spv::BuiltInVertexId:
    case spv::BuiltInInstanceId:
    case spv::BuiltInPrimitiveId:
    case spv::BuiltInInvocationId:
    case spv::BuiltInLayer:
    case spv::BuiltInViewportIndex:
    case spv::BuiltInPatchVertices:
    case spv::BuiltInSampleId:
    case spv::BuiltInVertexIndex:
    case spv::BuiltInInstanceIndex:
      type = ShaderType::INT;
      break;

    case spv::BuiltInTessLevelOuter:
    case spv::BuiltInTessLevelInner:
      type = ShaderType::FLOAT;
      break;

    case spv::BuiltInTessCoord:
      type = ShaderType::register_type(ShaderType::Vector(ShaderType::ST_float, 3));
      break;

    case spv::BuiltInFragCoord:
      type = ShaderType::register_type(ShaderType::Vector(ShaderType::ST_float, 4));
      break;

    case spv::BuiltInPointCoord:
    case spv::BuiltInSamplePosition:
      type = ShaderType::register_type(ShaderType::Vector(ShaderType::ST_float, 2));
      break;

    case spv::BuiltInFrontFacing:
    case spv::BuiltInHelperInvocation:
      type = ShaderType::BOOL;
      break;

    case spv::BuiltInNumWorkgroups:
    case spv::BuiltInWorkgroupSize:
    case spv::BuiltInWorkgroupId:
    case spv::BuiltInLocalInvocationId:
    case spv::BuiltInGlobalInvocationId:
      type = ShaderType::register_type(ShaderType::Vector(ShaderType::ST_uint, 3));
      break;

    case spv::BuiltInLocalInvocationIndex:
      type = ShaderType::UINT;
      break;

    default:
      // Unhandled / invalid input
      nassertr(false, 0);
      return 0;
    }

    var_id = define_variable(type, spv::StorageClassInput);
    decorate(var_id, spv::DecorationBuiltIn, builtin);
  }

  add_to_interfaces(model, var_id);
  return var_id;
}

/**
 * Ensures that the given type is declared, adding declarations to the
 * globals section as necessary, and returns its id.
 */
uint32_t SpirVModule::
define_type(const ShaderType *type) {
  uint32_t id = _db.find_type(type);
  if (id != 0) {
    return id;
  }

  if (type != nullptr) {
    if (const ShaderType::Image *image_type = type->as_image()) {
      // Unspecified depthness, storage image.
      return define_image_type(image_type, 2, 2, spv::ImageFormatUnknown);
    }
  }

  id = allocate_id();
  _db.record_type(id, type);

  if (type == nullptr) {
    add_global(Instruction(spv::OpTypeVoid, {id}));
  }
  else if (const ShaderType::Scalar *scalar_type = type->as_scalar()) {
    switch (scalar_type->get_scalar_type()) {
    case ShaderType::ST_float:
      add_global(Instruction(spv::OpTypeFloat, {id, 32}));
      break;
    case ShaderType::ST_double:
      add_global(Instruction(spv::OpTypeFloat, {id, 64}));
      break;
    case ShaderType::ST_int:
      add_global(Instruction(spv::OpTypeInt, {id, 32, 1}));
      break;
    case ShaderType::ST_uint:
      add_global(Instruction(spv::OpTypeInt, {id, 32, 0}));
      break;
    case ShaderType::ST_bool:
      add_global(Instruction(spv::OpTypeBool, {id}));
      break;
    default:
      add_global(Instruction(spv::OpTypeVoid, {id}));
      break;
    }
  }
  else if (const ShaderType::Vector *vector_type = type->as_vector()) {
    uint32_t component_type = define_type(
      ShaderType::register_type(ShaderType::Scalar(vector_type->get_scalar_type())));

    add_global(Instruction(spv::OpTypeVector,
      {id, component_type, vector_type->get_num_components()}));

    _db.modify_definition(id)._type_id = component_type;
  }
  else if (const ShaderType::Matrix *matrix_type = type->as_matrix()) {
    uint32_t row_type = define_type(
      ShaderType::register_type(ShaderType::Vector(matrix_type->get_scalar_type(), matrix_type->get_num_columns())));

    add_global(Instruction(spv::OpTypeMatrix,
      {id, row_type, matrix_type->get_num_rows()}));

    _db.modify_definition(id)._type_id = row_type;
  }
  else if (const ShaderType::Struct *struct_type = type->as_struct()) {
    size_t num_members = struct_type->get_num_members();
    Instruction op(spv::OpTypeStruct);
    op.args.push_back(id);

    for (size_t i = 0; i < num_members; ++i) {
      const ShaderType::Struct::Member &member = struct_type->get_member(i);
      uint32_t member_type_id = define_type(member.type);
      op.args.push_back(member_type_id);

      MemberDefinition &member_def = _db.modify_definition(id).modify_member(i);
      member_def._new_index = (int)i;
      member_def._type_id = member_type_id;
      member_def._name = member.name;
    }

    add_global(std::move(op));
  }
  else if (const ShaderType::Array *array_type = type->as_array()) {
    uint32_t element_type = define_type(array_type->get_element_type());

    auto size = array_type->get_num_elements();
    if (size != 0) {
      uint32_t constant_id = define_int_constant(size);
      add_global(Instruction(spv::OpTypeArray, {id, element_type, constant_id}));
    } else {
      add_global(Instruction(spv::OpTypeRuntimeArray, {id, element_type}));
    }
    _db.modify_definition(id)._type_id = element_type;
  }
  else if (type->as_sampler() != nullptr) {
    add_global(Instruction(spv::OpTypeSampler, {id}));
  }
  else if (const ShaderType::SampledImage *sampled_image_type = type->as_sampled_image()) {
    const ShaderType *image_type = ShaderType::register_type(ShaderType::Image(
      sampled_image_type->get_texture_type(),
      sampled_image_type->get_sampled_type(),
      ShaderType::Access::NONE));

    // Sampled image, depthness 1 if shadow sampler
    uint32_t depth = sampled_image_type->is_shadow() ? (uint32_t)1 : (uint32_t)0;
    uint32_t image_id = define_image_type(image_type, depth, 1, spv::ImageFormatUnknown);

    // If the module already declares a sampled image type for this image
    // type (which is possible even on a type map miss, since multiple
    // sampled image types may map to the same ShaderType), this creates a
    // duplicate, which deduplicate_types() merges before the module is
    // emitted.
    add_global(Instruction(spv::OpTypeSampledImage, {id, image_id}));
    _db.modify_definition(id)._type_id = image_id;
  }
  else {
    add_global(Instruction(spv::OpTypeVoid, {id}));
  }

  return id;
}

/**
 * Declares a new image type with the given extra properties (which are not
 * stored on ShaderType).
 */
uint32_t SpirVModule::
define_image_type(const ShaderType *type, uint32_t depth, uint32_t sampled, spv::ImageFormat format) {
  const ShaderType::Image *image_type = type->as_image();
  nassertr(image_type != nullptr, 0u);

  const ShaderType *sampled_scalar_type =
    ShaderType::register_type(ShaderType::Scalar(image_type->get_sampled_type()));

  uint32_t args[9] = {
    0, // Result id, assigned below
    0, // Sampled type, assigned below
    0, // Dimensionality, see below
    depth,
    0, // Arrayness, see below
    0, // Multisample not supported
    sampled,
    (uint32_t)format,
    0, // Access qualifier
  };

  switch (image_type->get_texture_type()) {
  case Texture::TT_1d_texture:
    args[2] = spv::Dim1D;
    args[4] = 0;
    break;
  case Texture::TT_2d_texture:
    args[2] = spv::Dim2D;
    args[4] = 0;
    break;
  case Texture::TT_3d_texture:
    args[2] = spv::Dim3D;
    args[4] = 0;
    break;
  case Texture::TT_2d_texture_array:
    args[2] = spv::Dim2D;
    args[4] = 1;
    break;
  case Texture::TT_cube_map:
    args[2] = spv::DimCube;
    args[4] = 0;
    break;
  case Texture::TT_buffer_texture:
    args[2] = spv::DimBuffer;
    args[4] = 0;
    break;
  case Texture::TT_cube_map_array:
    args[2] = spv::DimCube;
    args[4] = 1;
    break;
  case Texture::TT_1d_texture_array:
    args[2] = spv::Dim1D;
    args[4] = 1;
    break;
  }

  uint32_t nargs = 8;
  if (sampled != 1) {
    switch (image_type->get_access()) {
    case ShaderType::Access::NONE:
      break;
    case ShaderType::Access::READ_ONLY:
      args[8] = spv::AccessQualifierReadOnly;
      ++nargs;
      break;
    case ShaderType::Access::WRITE_ONLY:
      args[8] = spv::AccessQualifierWriteOnly;
      ++nargs;
      break;
    case ShaderType::Access::READ_WRITE:
      args[8] = spv::AccessQualifierReadWrite;
      ++nargs;
      break;
    }
  }

  // If the module already declares a type with these exact operands, this
  // creates a duplicate, which deduplicate_types() merges before the module
  // is emitted.
  uint32_t id = allocate_id();

  // This only becomes the canonical declaration for this ShaderType if it
  // is declared exactly the way define_type() would declare it.
  bool canonical = (depth == 2 && sampled == 2 && format == spv::ImageFormatUnknown);
  _db.record_type(id, type, canonical);

  args[0] = id;
  args[1] = define_type(sampled_scalar_type);

  add_global(Instruction(spv::OpTypeImage, args, nargs));

  Definition &def = _db.modify_definition(id);
  if (depth == 0) {
    def._flags |= SpirVResultDatabase::DF_non_depth_image;
  }
  if (depth == 1) {
    def._flags |= SpirVResultDatabase::DF_depth_image;
  }
  if (sampled == 1) {
    def._flags |= SpirVResultDatabase::DF_sampled_image;
  }
  if (sampled == 2) {
    def._flags |= SpirVResultDatabase::DF_storage_image;
  }
  def._image_format = format;
  return id;
}

/**
 * Ensures that a pointer type to the given (already declared) type with the
 * given storage class is declared, and returns its id.
 */
uint32_t SpirVModule::
define_pointer_type(uint32_t type_id, spv::StorageClass storage_class) {
  nassertr_always(type_id != 0u, 0u);

  uint32_t pointer_type_id = _db.find_pointer_type(type_id, storage_class);
  if (pointer_type_id != 0) {
    return pointer_type_id;
  }

  pointer_type_id = allocate_id();
  _db.record_pointer_type(pointer_type_id, storage_class, type_id);
  add_global(Instruction(spv::OpTypePointer,
    {pointer_type_id, (uint32_t)storage_class, type_id}));
  return pointer_type_id;
}

/**
 * Ensures that a pointer type to the given type with the given storage class
 * is declared, and returns its id.
 */
uint32_t SpirVModule::
define_pointer_type(const ShaderType *type, spv::StorageClass storage_class) {
  uint32_t pointer_type_id = _db.find_pointer_type(type, storage_class);
  if (pointer_type_id != 0) {
    return pointer_type_id;
  }
  return define_pointer_type(define_type(type), storage_class);
}

/**
 * Ensures that a function type with the given return type is declared, and
 * returns its id.
 */
uint32_t SpirVModule::
define_function_type(const ShaderType *return_type) {
  //TODO: right now we only handle the case of void with no parameters.
  nassertr(return_type == nullptr, 0);

  uint32_t function_type_id = _db.find_function_type();
  if (function_type_id != 0) {
    return function_type_id;
  }

  uint32_t return_type_id = define_type(return_type);
  function_type_id = allocate_id();
  _db.record_function_type(function_type_id, return_type_id, nullptr, 0);
  add_global(Instruction(spv::OpTypeFunction, {function_type_id, return_type_id}));
  return function_type_id;
}

/**
 * Adds a new module-scope variable declaration, along with any type
 * declarations and layout annotations that may be necessary, and returns its
 * id.
 */
uint32_t SpirVModule::
define_variable(const ShaderType *type, spv::StorageClass storage_class) {
  uint32_t pointer_type_id = define_pointer_type(type, storage_class);
  uint32_t variable_id = allocate_id();

  add_global(Instruction(spv::OpVariable, {
    pointer_type_id,
    variable_id,
    (uint32_t)storage_class,
  }));

  _db.record_variable(variable_id, pointer_type_id, storage_class);

  // Depending on the storage class, we may need to make sure it is laid out.
  if (storage_class == spv::StorageClassStorageBuffer ||
      storage_class == spv::StorageClassPhysicalStorageBuffer ||
      storage_class == spv::StorageClassUniform ||
      storage_class == spv::StorageClassPushConstant) {
    r_annotate_struct_layout(unwrap_pointer_type(pointer_type_id));
  }

  return variable_id;
}

/**
 * Makes sure that the given type has all its structure members correctly laid
 * out using offsets and strides.
 */
void SpirVModule::
r_annotate_struct_layout(uint32_t type_id) {
  Definition &def = _db.modify_definition(type_id);

  const ShaderType *type = def._type;
  nassertv(type != nullptr);

  const ShaderType::Struct *struct_type = type->as_struct();
  if (struct_type == nullptr) {
    // If this is an array of structs, recurse.
    if (const ShaderType::Array *array_type = type->as_array()) {
      // Also make sure there's an ArrayStride decoration for this array.
      if (def._array_stride == 0) {
        decorate(type_id, spv::DecorationArrayStride, array_type->get_stride_bytes());
      }

      uint32_t element_type_id = _db.find_type(array_type->get_element_type());
      r_annotate_struct_layout(element_type_id);
    }
    return;
  }

  uint32_t num_members = struct_type->get_num_members();

  for (uint32_t i = 0; i < num_members; ++i) {
    const ShaderType::Struct::Member &member = struct_type->get_member(i);

    if (def.get_member(i)._offset < 0) {
      decorate_member(type_id, i, spv::DecorationOffset, member.offset);
    }

    // Unwrap array to see if there's a matrix here.
    const ShaderType *base_type = member.type;
    while (const ShaderType::Array *array_type = base_type->as_array()) {
      base_type = array_type->get_element_type();

      // Also make sure there's an ArrayStride decoration for this array.
      uint32_t array_type_id = _db.find_type(array_type);

      if (_db.get_definition(array_type_id)._array_stride == 0) {
        decorate(array_type_id, spv::DecorationArrayStride, array_type->get_stride_bytes());
      }
    }

    if (const ShaderType::Matrix *matrix_type = base_type->as_matrix()) {
      // Matrix types need to be explicitly laid out.
      if (def.get_member(i)._matrix_stride == 0) {
        decorate_member(type_id, i, spv::DecorationMatrixStride, matrix_type->get_row_stride_bytes());
        decorate_member(type_id, i, spv::DecorationColMajor);
      }
    } else {
      r_annotate_struct_layout(def.get_member(i)._type_id);
    }
  }
}

/**
 * Defines a new integral constant, either of type uint or int, reusing an
 * existing one if one is already defined (except for OpConstantNull, which
 * can't be used to index structure members).
 */
uint32_t SpirVModule::
define_int_constant(int32_t constant) {
  for (uint32_t id = 0; id < _id_bound; ++id) {
    const Definition &def = _db.get_definition(id);
    if (def.is_constant() && !def.is_null_constant() &&
        def._constant == (uint32_t)constant &&
        (def._type == ShaderType::INT || (constant >= 0 && def._type == ShaderType::UINT))) {
      // Skip constants of a non-canonical type declaration (eg. a 16-bit
      // int): ShaderType does not record the width, so the check above
      // cannot tell those apart from a plain int constant.
      if (_db.is_canonical_type(def._type_id)) {
        return id;
      }
    }
  }

  uint32_t type_id = define_type(ShaderType::INT);
  uint32_t constant_id = allocate_id();

  add_global(Instruction(spv::OpConstant, {type_id, constant_id, (uint32_t)constant}));

  _db.record_constant(constant_id, type_id, (uint32_t *)&constant, 1);
  return constant_id;
}

/**
 * Defines a new null constant of the given type, reusing an existing one if
 * one is already defined.
 */
uint32_t SpirVModule::
define_null_constant(const ShaderType *type) {
  uint32_t type_id = define_type(type);

  uint32_t constant_id = _db.find_null_constant(type_id);
  if (constant_id != 0) {
    return constant_id;
  }

  constant_id = allocate_id();
  add_global(Instruction(spv::OpConstantNull, {type_id, constant_id}));

  _db.record_constant(constant_id, type_id, nullptr, 0);
  return constant_id;
}

/**
 * Defines a new constant.  Does not attempt to reuse constants.
 */
uint32_t SpirVModule::
define_constant(const ShaderType *type, uint32_t constant) {
  uint32_t type_id = define_type(type);

  uint32_t constant_id = allocate_id();
  if (type == ShaderType::BOOL) {
    add_global(Instruction(constant ? spv::OpConstantTrue : spv::OpConstantFalse,
                           {type_id, constant_id}));
    constant = (constant != 0);
  } else {
    add_global(Instruction(spv::OpConstant, {type_id, constant_id, constant}));
  }

  _db.record_constant(constant_id, type_id, &constant, 1);
  return constant_id;
}

/**
 * Defines a new spec constant.  Follow up with a call to decorate() to set
 * the SpecId.
 */
uint32_t SpirVModule::
define_spec_constant(const ShaderType *type, uint32_t def_value) {
  uint32_t type_id = define_type(type);

  uint32_t id = allocate_id();
  if (type == ShaderType::BOOL) {
    add_global(Instruction(def_value ? spv::OpSpecConstantTrue : spv::OpSpecConstantFalse, {type_id, id}));
  } else {
    add_global(Instruction(spv::OpSpecConstant, {type_id, id, def_value}));
  }

  _db.record_spec_constant(id, type_id);
  return id;
}

/**
 * Issues an error about the given id.  Returns 0.
 */
uint32_t SpirVModule::
error_expected(uint32_t id, const char *msg) const {
  std::ostringstream str;
  str << id << " (";

  const Definition &def = _db.get_definition(id);
  def.output(str);

  str << ") was expected to be " << msg << "\n";
  nassert_raise(str.str());
  return 0u;
}
