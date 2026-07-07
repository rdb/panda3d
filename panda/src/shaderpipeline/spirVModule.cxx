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
 * Returns the minimum operand count (excluding the opcode word) that this
 * module representation relies on for the given opcode: parse() rejects a
 * shorter instruction, which is what makes the declaration-reading accessors
 * and the passes safe to index operands without checking each time.  The
 * counts follow the SPIR-V specification; opcodes whose operands are never
 * interpreted here return 0 and pass through unchecked.
 */
static uint32_t
get_min_operand_count(spv::Op opcode) {
  switch (opcode) {
  case spv::OpCapability:
  case spv::OpExtension:
  case spv::OpReturnValue:
  case spv::OpLabel:
  case spv::OpBranch:
  case spv::OpDecorationGroup:
  case spv::OpGroupDecorate:
    return 1;

  case spv::OpExtInstImport:
  case spv::OpMemoryModel:
  case spv::OpExecutionMode:
  case spv::OpExecutionModeId:
  case spv::OpString:
  case spv::OpName:
  case spv::OpDecorate:
  case spv::OpTypeFloat:
  case spv::OpTypeSampledImage:
  case spv::OpTypeRuntimeArray:
  case spv::OpTypeFunction:
  case spv::OpConstantTrue:
  case spv::OpConstantFalse:
  case spv::OpConstantNull:
  case spv::OpConstantComposite:
  case spv::OpSpecConstantTrue:
  case spv::OpSpecConstantFalse:
  case spv::OpSpecConstantComposite:
  case spv::OpUndef:
  case spv::OpFunctionParameter:
  case spv::OpStore:
  case spv::OpCopyMemory:
  case spv::OpCompositeConstruct:
  case spv::OpSelectionMerge:
    return 2;

  case spv::OpEntryPoint:
  case spv::OpMemberName:
  case spv::OpMemberDecorate:
  case spv::OpDecorateId:
  case spv::OpLine:
  case spv::OpTypeInt:
  case spv::OpTypeVector:
  case spv::OpTypeMatrix:
  case spv::OpTypeArray:
  case spv::OpTypePointer:
  case spv::OpConstant:
  case spv::OpSpecConstant:
  case spv::OpSpecConstantOp:
  case spv::OpVariable:
  case spv::OpLoad:
  case spv::OpAccessChain:
  case spv::OpInBoundsAccessChain:
  case spv::OpFunctionCall:
  case spv::OpCopyObject:
  case spv::OpCopyLogical:
  case spv::OpBitcast:
  case spv::OpConvertPtrToU:
  case spv::OpConvertUToPtr:
  case spv::OpCopyMemorySized:
  case spv::OpImage:
  case spv::OpImageQuerySize:
  case spv::OpImageQueryLevels:
  case spv::OpCompositeExtract:
  case spv::OpAtomicFlagClear:
  case spv::OpBranchConditional:
  // The unary operations whose constant-expression flag the usage analysis
  // propagates.
  case spv::OpConvertFToU:
  case spv::OpConvertFToS:
  case spv::OpConvertSToF:
  case spv::OpConvertUToF:
  case spv::OpUConvert:
  case spv::OpSConvert:
  case spv::OpFConvert:
  case spv::OpQuantizeToF16:
  case spv::OpSNegate:
  case spv::OpFNegate:
  case spv::OpNot:
  case spv::OpAny:
  case spv::OpAll:
  case spv::OpIsNan:
  case spv::OpIsInf:
  case spv::OpTranspose:
    return 3;

  case spv::OpFunction:
  case spv::OpPtrAccessChain:
  case spv::OpInBoundsPtrAccessChain:
  case spv::OpSampledImage:
  case spv::OpImageQuerySizeLod:
  case spv::OpImageSampleImplicitLod:
  case spv::OpImageSampleProjImplicitLod:
  case spv::OpImageFetch:
  case spv::OpExtInst:
  case spv::OpCompositeInsert:
  case spv::OpVectorShuffle:
  case spv::OpArrayLength:
  case spv::OpExpectKHR:
  case spv::OpAtomicStore:
  case spv::OpPtrEqual:
  case spv::OpPtrNotEqual:
  case spv::OpPtrDiff:
  // Binary operations, for the constant-expression propagation.
  case spv::OpIAdd:
  case spv::OpFAdd:
  case spv::OpISub:
  case spv::OpFSub:
  case spv::OpIMul:
  case spv::OpFMul:
  case spv::OpUDiv:
  case spv::OpSDiv:
  case spv::OpFDiv:
  case spv::OpUMod:
  case spv::OpSRem:
  case spv::OpSMod:
  case spv::OpFRem:
  case spv::OpFMod:
  case spv::OpVectorTimesScalar:
  case spv::OpMatrixTimesScalar:
  case spv::OpVectorTimesMatrix:
  case spv::OpMatrixTimesVector:
  case spv::OpMatrixTimesMatrix:
  case spv::OpOuterProduct:
  case spv::OpDot:
  case spv::OpShiftRightLogical:
  case spv::OpShiftRightArithmetic:
  case spv::OpShiftLeftLogical:
  case spv::OpBitwiseOr:
  case spv::OpBitwiseXor:
  case spv::OpBitwiseAnd:
    return 4;

  case spv::OpImageSampleExplicitLod:
  case spv::OpImageSampleProjExplicitLod:
  case spv::OpImageSampleDrefImplicitLod:
  case spv::OpImageSampleDrefExplicitLod:
  case spv::OpImageSampleProjDrefImplicitLod:
  case spv::OpImageSampleProjDrefExplicitLod:
  case spv::OpImageGather:
  case spv::OpImageDrefGather:
  case spv::OpImageTexelPointer:
  case spv::OpAtomicLoad:
  case spv::OpAtomicIIncrement:
  case spv::OpAtomicIDecrement:
  case spv::OpAtomicFlagTestAndSet:
  case spv::OpSelect:
    return 5;

  case spv::OpAtomicExchange:
  case spv::OpAtomicIAdd:
  case spv::OpAtomicISub:
  case spv::OpAtomicSMin:
  case spv::OpAtomicUMin:
  case spv::OpAtomicSMax:
  case spv::OpAtomicUMax:
  case spv::OpAtomicAnd:
  case spv::OpAtomicOr:
  case spv::OpAtomicXor:
  case spv::OpAtomicFMinEXT:
  case spv::OpAtomicFMaxEXT:
  case spv::OpAtomicFAddEXT:
    return 6;

  case spv::OpTypeImage:
    return 8;

  case spv::OpAtomicCompareExchange:
  case spv::OpAtomicCompareExchangeWeak:
    return 8;

  default:
    return 0;
  }
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

  _ids.resize(_id_bound);

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

    if (nargs < get_min_operand_count(opcode)) {
      shader_cat.error()
        << "Truncated instruction (opcode " << (uint32_t)opcode << ", "
        << nargs << " operands) during SPIR-V parse\n";
      return false;
    }

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
      record_result(op, 0);
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
      record_result(op, 0);
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
      continue;

    case spv::OpMemberName:
      if (nargs >= 3) {
        std::string name = op.get_string(2);
        if (!name.empty()) {
          pvector<std::string> &names = _member_names[args[0]];
          if (args[1] >= names.size()) {
            names.resize(args[1] + 1);
          }
          names[args[1]] = std::move(name);
        }
      }
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
      continue;

    case spv::OpFunction:
      if (function != nullptr) {
        shader_cat.error()
          << "OpFunction may not occur within another function!\n";
        return false;
      }
      record_result(op, 0);
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
      function->instructions.push_back(std::move(op));
      function = nullptr;
      continue;

    default:
      if (function != nullptr) {
        record_result(op, function->id);
        function->instructions.push_back(std::move(op));
      } else {
        record_result_at(op, 0, (int32_t)_globals.size());
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
 * (Re)derives the index entry for the given instruction's result id from the
 * instruction itself.  Any previously recorded declaration index is kept for
 * module-scope instructions (so a pass may edit a declaration in place and
 * call this to make the index follow).
 */
void SpirVModule::
record_result(const Instruction &op, uint32_t function_id) {
  int32_t global_index = -1;
  uint32_t result_id = op.get_result();
  if (function_id == 0 && result_id != 0 && result_id < _ids.size()) {
    global_index = _ids[result_id]._global_index;
  }
  record_result_at(op, function_id, global_index);
}

/**
 * Implementation of record_result, with an explicit declaration index.
 */
void SpirVModule::
record_result_at(const Instruction &op, uint32_t function_id, int32_t global_index) {
  uint32_t result_id = op.get_result();
  if (result_id == 0) {
    return;
  }

  IdEntry &entry = modify_entry(result_id);
  entry = IdEntry();
  entry._function_id = function_id;
  entry._global_index = global_index;

  switch (op.opcode) {
  case spv::OpTypeVoid:
  case spv::OpTypeBool:
  case spv::OpTypeInt:
  case spv::OpTypeFloat:
  case spv::OpTypeVector:
  case spv::OpTypeMatrix:
  case spv::OpTypeImage:
  case spv::OpTypeSampler:
  case spv::OpTypeSampledImage:
  case spv::OpTypeArray:
  case spv::OpTypeRuntimeArray:
  case spv::OpTypeStruct:
  case spv::OpTypeOpaque:
  case spv::OpTypeEvent:
  case spv::OpTypeDeviceEvent:
  case spv::OpTypeReserveId:
  case spv::OpTypeQueue:
  case spv::OpTypePipe:
  case spv::OpTypePipeStorage:
  case spv::OpTypeNamedBarrier:
  case spv::OpTypeCooperativeMatrixKHR:
  case spv::OpTypeCooperativeMatrixNV:
    entry._kind = IdKind::TYPE;
    break;

  case spv::OpTypePointer:
    entry._kind = IdKind::POINTER_TYPE;
    break;

  case spv::OpTypeFunction:
    entry._kind = IdKind::FUNCTION_TYPE;
    break;

  case spv::OpVariable:
    entry._kind = IdKind::VARIABLE;
    entry._type_id = op.args[0];
    break;

  case spv::OpConstant:
  case spv::OpConstantTrue:
  case spv::OpConstantFalse:
  case spv::OpConstantNull:
  case spv::OpConstantComposite:
  case spv::OpSpecConstantComposite:
    entry._kind = IdKind::CONSTANT;
    entry._type_id = op.args[0];
    break;

  case spv::OpSpecConstant:
  case spv::OpSpecConstantTrue:
  case spv::OpSpecConstantFalse:
    entry._kind = IdKind::SPEC_CONSTANT;
    entry._type_id = op.args[0];
    break;

  case spv::OpFunction:
    entry._kind = IdKind::FUNCTION;
    entry._type_id = (op.args.size() >= 4) ? op.args[3] : 0;
    entry._function_id = result_id;
    break;

  case spv::OpFunctionParameter:
    entry._kind = IdKind::PARAMETER;
    entry._type_id = op.args[0];
    break;

  case spv::OpExtInstImport:
    entry._kind = IdKind::EXT_INST;
    break;

  case spv::OpString:
    entry._kind = IdKind::STRING;
    break;

  default:
    if (op.has_result_type()) {
      entry._kind = IdKind::TEMPORARY;
      entry._type_id = op.args[0];
    }
    // Results without a type (labels, decoration groups) stay NONE.
    break;
  }
}

/**
 * Clears every trace of the given id from the index, names and annotations.
 * The declaring instruction itself is the caller's to remove.
 */
void SpirVModule::
clear_id_state(uint32_t id) {
  _names.erase(id);
  _reflection_names.erase(id);
  _member_names.erase(id);
  _annotations.erase(id);
  modify_entry(id) = IdEntry();
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
 * interfaces are generated from the module's own maps, so ids that have been
 * deleted leave no trace.
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
  for (const auto &item : _member_names) {
    for (size_t mi = 0; mi < item.second.size(); ++mi) {
      if (!item.second[mi].empty()) {
        pvector<uint32_t> args({item.first, (uint32_t)mi});
        pack_string(args, item.second[mi]);
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
    // Result type, then (after the opcode literal) the operands.  Most of
    // the permitted nested opcodes take only id operands; the three that
    // take trailing literal indices are special-cased, so that a literal is
    // never mistaken for an id (replace_type_id would corrupt it if it
    // happened to equal the replaced id).
    indices.push_back(0);
    if (op.args.size() > 3) {
      size_t num_ids;
      switch ((spv::Op)op.args[2]) {
      case spv::OpCompositeExtract:
        // The composite, then literal indexes.
        num_ids = 1;
        break;
      case spv::OpCompositeInsert:
        // The object and the composite, then literal indexes.
        num_ids = 2;
        break;
      case spv::OpVectorShuffle:
        // The two vectors, then literal components.
        num_ids = 2;
        break;
      default:
        // All operands are ids.
        num_ids = op.args.size();
        break;
      }
      for (size_t i = 3; i < op.args.size() && i - 3 < num_ids; ++i) {
        indices.push_back(i);
      }
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
  const IdEntry &entry = get_entry(result_id);
  if (entry._global_index < 0 || (size_t)entry._global_index >= _globals.size()) {
    return nullptr;
  }
  Instruction &op = _globals[entry._global_index];
  return op.is_nop() ? nullptr : &op;
}

/**
 * Returns the declaring instruction of the given module-scope id, or null if
 * the id is not declared in the globals section.
 */
const SpirVModule::Instruction *SpirVModule::
get_decl(uint32_t id) const {
  const IdEntry &entry = get_entry(id);
  if (entry._global_index < 0 || (size_t)entry._global_index >= _globals.size()) {
    return nullptr;
  }
  const Instruction &op = _globals[entry._global_index];
  return op.is_nop() ? nullptr : &op;
}

/**
 * Returns the type id of the given id.  For a value, this is the result type
 * operand of its declaring instruction (so for a variable, its pointer type);
 * for a function, its function type.  For a composite type declaration, this
 * is the element, component, pointee, image or return type, read from the
 * declaration.
 */
uint32_t SpirVModule::
get_type_id(uint32_t id) const {
  const IdEntry &entry = get_entry(id);
  if (entry._kind == IdKind::TYPE || entry._kind == IdKind::POINTER_TYPE ||
      entry._kind == IdKind::FUNCTION_TYPE) {
    const Instruction *decl = get_decl(id);
    if (decl != nullptr && decl->args.size() >= 2) {
      switch (decl->opcode) {
      case spv::OpTypeVector:
      case spv::OpTypeMatrix:
      case spv::OpTypeArray:
      case spv::OpTypeRuntimeArray:
      case spv::OpTypeSampledImage:
        return decl->args[1];

      case spv::OpTypeFunction:
        // The return type.
        return decl->args[1];

      case spv::OpTypePointer:
        return (decl->args.size() >= 3) ? decl->args[2] : 0;

      default:
        break;
      }
    }
    nassertr(false, 0);
    return 0;
  }
  nassertr(entry._type_id != 0, 0);
  return entry._type_id;
}

/**
 * Returns the storage class of the given variable or pointer type.  A legacy
 * buffer block (Uniform storage class, BufferBlock decoration) is reported
 * as StorageBuffer, matching how the type is resolved.
 */
spv::StorageClass SpirVModule::
get_storage_class(uint32_t id) const {
  const IdEntry &entry = get_entry(id);
  if (entry._kind == IdKind::VARIABLE) {
    if (entry._function_id != 0) {
      return spv::StorageClassFunction;
    }
    const Instruction *decl = get_decl(id);
    if (decl != nullptr && decl->args.size() >= 3) {
      const Instruction *ptr_decl = get_decl(decl->args[0]);
      uint32_t pointee = (ptr_decl != nullptr && ptr_decl->args.size() >= 3)
        ? ptr_decl->args[2] : 0;
      return canonicalize_storage_class((spv::StorageClass)decl->args[2], pointee);
    }
  }
  else if (entry._kind == IdKind::POINTER_TYPE) {
    const Instruction *decl = get_decl(id);
    if (decl != nullptr && decl->args.size() >= 3) {
      return canonicalize_storage_class((spv::StorageClass)decl->args[1], decl->args[2]);
    }
  }
  return spv::StorageClassMax;
}

/**
 * Maps the Uniform storage class of a legacy-style buffer block (a struct,
 * possibly wrapped in arrays, carrying the BufferBlock decoration) to
 * StorageBuffer, the way newer SPIR-V declares it.
 */
spv::StorageClass SpirVModule::
canonicalize_storage_class(spv::StorageClass storage_class, uint32_t pointee_type_id) const {
  if (storage_class != spv::StorageClassUniform || pointee_type_id == 0) {
    return storage_class;
  }
  const Instruction *decl = get_decl(pointee_type_id);
  while (decl != nullptr && (decl->opcode == spv::OpTypeArray ||
                             decl->opcode == spv::OpTypeRuntimeArray)) {
    decl = get_decl(decl->args[1]);
  }
  if (decl != nullptr && decl->opcode == spv::OpTypeStruct &&
      has_decoration(decl->args[0], spv::DecorationBufferBlock)) {
    return spv::StorageClassStorageBuffer;
  }
  return storage_class;
}

/**
 * Looks up the given constant's (first word) value.  Also accepts a spec
 * constant, whose default value is returned.
 */
uint32_t SpirVModule::
resolve_constant(uint32_t id) const {
  const IdEntry &entry = get_entry(id);
  if (entry._kind != IdKind::CONSTANT && entry._kind != IdKind::SPEC_CONSTANT) {
    return error_expected(id, "a constant");
  }
  const Instruction *decl = get_decl(id);
  if (decl == nullptr) {
    return error_expected(id, "a constant");
  }
  switch (decl->opcode) {
  case spv::OpConstantTrue:
  case spv::OpSpecConstantTrue:
    return 1;
  case spv::OpConstant:
  case spv::OpSpecConstant:
    return (decl->args.size() >= 3) ? decl->args[2] : 0;
  default:
    // Null and composite constants have no meaningful first word.
    return 0;
  }
}

/**
 * Returns the pointee type id of the given pointer type id.
 */
uint32_t SpirVModule::
unwrap_pointer_type(uint32_t id) const {
  if (get_entry(id)._kind != IdKind::POINTER_TYPE) {
    return error_expected(id, "a pointer type");
  }
  const Instruction *decl = get_decl(id);
  nassertr(decl != nullptr && decl->args.size() >= 3, 0);
  return decl->args[2];
}

/**
 * Returns the number of members of the given struct type, or 0 if the id is
 * not a struct type.
 */
uint32_t SpirVModule::
get_num_members(uint32_t type_id) const {
  const Instruction *decl = get_decl(type_id);
  if (decl == nullptr || decl->opcode != spv::OpTypeStruct) {
    return 0;
  }
  return (uint32_t)decl->args.size() - 1;
}

/**
 * Returns the type id of the given member of the given struct type.
 */
uint32_t SpirVModule::
get_member_type_id(uint32_t type_id, uint32_t member_index) const {
  const Instruction *decl = get_decl(type_id);
  nassertr(decl != nullptr && decl->opcode == spv::OpTypeStruct, 0);
  nassertr((size_t)member_index + 1 < decl->args.size(), 0);
  return decl->args[member_index + 1];
}

/**
 * Returns the type id obtained by indexing into the given composite type
 * with the given (literal) index: the member type for a struct, the element
 * or component type for arrays, vectors and matrices.
 */
uint32_t SpirVModule::
get_composite_member_type_id(uint32_t type_id, uint32_t member_index) const {
  const Instruction *decl = get_decl(type_id);
  nassertr(decl != nullptr, 0);
  switch (decl->opcode) {
  case spv::OpTypeStruct:
    nassertr((size_t)member_index + 1 < decl->args.size(), 0);
    return decl->args[member_index + 1];

  case spv::OpTypeVector:
  case spv::OpTypeMatrix:
  case spv::OpTypeArray:
  case spv::OpTypeRuntimeArray:
    return decl->args[1];

  default:
    return error_expected(type_id, "a composite type");
  }
}

/**
 * Returns the parameter ids of the given function, in order, by scanning the
 * OpFunctionParameter instructions at the top of its body.
 */
pvector<uint32_t> SpirVModule::
get_function_parameters(uint32_t function_id) const {
  pvector<uint32_t> result;
  for (const Function &function : _functions) {
    if (function.id != function_id) {
      continue;
    }
    for (const Instruction &op : function.instructions) {
      if (op.opcode == spv::OpFunctionParameter) {
        if (op.args.size() >= 2) {
          result.push_back(op.args[1]);
        }
      }
      else if (op.opcode != spv::OpFunction && op.opcode != spv::OpLine &&
               op.opcode != spv::OpNoLine && !op.is_nop()) {
        break;
      }
    }
    break;
  }
  return result;
}

/**
 * Returns true if the given id carries the given (non-member) decoration.
 */
bool SpirVModule::
has_decoration(uint32_t id, spv::Decoration decoration) const {
  const Annotations *annotations = get_annotations(id);
  if (annotations == nullptr) {
    return false;
  }
  for (const Annotation &annotation : *annotations) {
    if (!annotation.is_member_annotation() &&
        annotation.get_decoration() == decoration) {
      return true;
    }
  }
  return false;
}

/**
 * Returns the first extra operand of the given (non-member) decoration on
 * the given id, or the given default if the id does not carry it.
 */
uint32_t SpirVModule::
get_decoration(uint32_t id, spv::Decoration decoration, uint32_t default_value) const {
  const Annotations *annotations = get_annotations(id);
  if (annotations == nullptr) {
    return default_value;
  }
  for (const Annotation &annotation : *annotations) {
    if (!annotation.is_member_annotation() &&
        annotation.get_decoration() == decoration) {
      return (annotation.args.size() >= 2) ? annotation.args[1] : default_value;
    }
  }
  return default_value;
}

/**
 * Returns true if the given struct member carries the given decoration.
 */
bool SpirVModule::
has_member_decoration(uint32_t type_id, uint32_t member_index, spv::Decoration decoration) const {
  const Annotations *annotations = get_annotations(type_id);
  if (annotations == nullptr) {
    return false;
  }
  for (const Annotation &annotation : *annotations) {
    if (annotation.is_member_annotation() && annotation.args.size() >= 2 &&
        annotation.args[0] == member_index &&
        annotation.get_decoration() == decoration) {
      return true;
    }
  }
  return false;
}

/**
 * Returns the first extra operand of the given decoration on the given
 * struct member, or the given default if the member does not carry it.
 */
uint32_t SpirVModule::
get_member_decoration(uint32_t type_id, uint32_t member_index, spv::Decoration decoration, uint32_t default_value) const {
  const Annotations *annotations = get_annotations(type_id);
  if (annotations == nullptr) {
    return default_value;
  }
  for (const Annotation &annotation : *annotations) {
    if (annotation.is_member_annotation() && annotation.args.size() >= 2 &&
        annotation.args[0] == member_index &&
        annotation.get_decoration() == decoration) {
      return (annotation.args.size() >= 3) ? annotation.args[2] : default_value;
    }
  }
  return default_value;
}

/**
 * Resolves the given non-pointer type id to Panda's type system.  The result
 * is memoized; see invalidate_types.
 */
const ShaderType *SpirVModule::
resolve_type(uint32_t id) const {
  const IdEntry &entry = get_entry(id);
  if (entry._kind != IdKind::TYPE) {
    error_expected(id, "a non-pointer type");
    return nullptr;
  }
  return r_resolve_type(id);
}

/**
 * Internal implementation of resolve_type, without the kind check.
 */
const ShaderType *SpirVModule::
r_resolve_type(uint32_t id) const {
  const IdEntry &entry = get_entry(id);
  if (entry._type != nullptr) {
    return entry._type;
  }
  const Instruction *decl = get_decl(id);
  if (decl == nullptr) {
    return nullptr;
  }

  const ShaderType *type = nullptr;
  switch (decl->opcode) {
  case spv::OpTypeVoid:
    // Never cached; a null cache slot means "not resolved yet".
    return nullptr;

  case spv::OpTypeBool:
    type = ShaderType::BOOL;
    break;

  case spv::OpTypeInt:
    type = decl->args[2] ? ShaderType::INT : ShaderType::UINT;
    break;

  case spv::OpTypeFloat:
    type = (decl->args.size() >= 2 && decl->args[1] >= 64)
      ? ShaderType::DOUBLE : ShaderType::FLOAT;
    break;

  case spv::OpTypeVector:
    {
      const ShaderType *component = r_resolve_type(decl->args[1]);
      const ShaderType::Scalar *scalar = (component != nullptr) ? component->as_scalar() : nullptr;
      nassertr(scalar != nullptr, nullptr);
      type = ShaderType::register_type(
        ShaderType::Vector(scalar->get_scalar_type(), decl->args[2]));
    }
    break;

  case spv::OpTypeMatrix:
    {
      // SPIR-V uses GLSL parlance, in which a column is a row and a row is a
      // column, compared to Panda conventions.  We flip it around here.
      const ShaderType *row = r_resolve_type(decl->args[1]);
      const ShaderType::Vector *row_type = (row != nullptr) ? row->as_vector() : nullptr;
      nassertr(row_type != nullptr, nullptr);
      type = ShaderType::register_type(
        ShaderType::Matrix(row_type->get_scalar_type(), decl->args[2],
                           row_type->get_num_components()));
    }
    break;

  case spv::OpTypeSampler:
    type = ShaderType::SAMPLER;
    break;

  case spv::OpTypeImage:
    {
      const ShaderType *sampled = r_resolve_type(decl->args[1]);
      const ShaderType::Scalar *scalar = (sampled != nullptr) ? sampled->as_scalar() : nullptr;
      nassertr(scalar != nullptr, nullptr);

      Texture::TextureType texture_type;
      switch ((spv::Dim)decl->args[2]) {
      case spv::Dim1D:
        texture_type = decl->args[4] ? Texture::TT_1d_texture_array
                                     : Texture::TT_1d_texture;
        break;
      case spv::Dim2D:
        texture_type = decl->args[4] ? Texture::TT_2d_texture_array
                                     : Texture::TT_2d_texture;
        break;
      case spv::Dim3D:
        texture_type = Texture::TT_3d_texture;
        break;
      case spv::DimCube:
        texture_type = decl->args[4] ? Texture::TT_cube_map_array
                                     : Texture::TT_cube_map;
        break;
      case spv::DimBuffer:
        texture_type = Texture::TT_buffer_texture;
        break;
      case spv::DimRect:
        shader_cat.error()
          << "imageRect shader inputs are not supported.\n";
        return nullptr;
      case spv::DimSubpassData:
        shader_cat.error()
          << "subpassInput shader inputs are not supported.\n";
        return nullptr;
      default:
        shader_cat.error()
          << "Unknown image dimensionality in OpTypeImage instruction.\n";
        return nullptr;
      }

      ShaderType::Access access = ShaderType::Access::READ_WRITE;
      if (decl->args.size() > 8) {
        switch ((spv::AccessQualifier)decl->args[8]) {
        case spv::AccessQualifierReadOnly:
          access = ShaderType::Access::READ_ONLY;
          break;
        case spv::AccessQualifierWriteOnly:
          access = ShaderType::Access::WRITE_ONLY;
          break;
        case spv::AccessQualifierReadWrite:
          access = ShaderType::Access::READ_WRITE;
          break;
        default:
          shader_cat.error()
            << "Invalid access qualifier in OpTypeImage instruction.\n";
          break;
        }
      }
      if (has_decoration(id, spv::DecorationNonWritable)) {
        access = (access & ShaderType::Access::READ_ONLY);
      }
      if (has_decoration(id, spv::DecorationNonReadable)) {
        access = (access & ShaderType::Access::WRITE_ONLY);
      }

      type = ShaderType::register_type(
        ShaderType::Image(texture_type, scalar->get_scalar_type(), access));
    }
    break;

  case spv::OpTypeSampledImage:
    {
      const ShaderType *image_shader_type = r_resolve_type(decl->args[1]);
      const ShaderType::Image *image = (image_shader_type != nullptr)
        ? image_shader_type->as_image() : nullptr;
      if (image == nullptr) {
        shader_cat.error()
          << "OpTypeSampledImage must refer to an image type!\n";
        return nullptr;
      }
      // The image's depth flag determines shadow sampler status.
      const Instruction *image_decl = get_decl(decl->args[1]);
      bool shadow = (image_decl != nullptr && image_decl->args.size() > 3 &&
                     image_decl->args[3] == 1);
      type = ShaderType::register_type(
        ShaderType::SampledImage(image->get_texture_type(),
                                 image->get_sampled_type(), shadow));
    }
    break;

  case spv::OpTypeArray:
    {
      const ShaderType *element = r_resolve_type(decl->args[1]);
      if (element == nullptr) {
        return nullptr;
      }
      type = ShaderType::register_type(ShaderType::Array(
        element, resolve_constant(decl->args[2]), get_array_stride(id)));
    }
    break;

  case spv::OpTypeRuntimeArray:
    {
      const ShaderType *element = r_resolve_type(decl->args[1]);
      if (element == nullptr) {
        return nullptr;
      }
      type = ShaderType::register_type(ShaderType::Array(
        element, 0, get_array_stride(id)));
    }
    break;

  case spv::OpTypeStruct:
    {
      ShaderType::Struct struct_type;
      for (size_t ai = 1; ai < decl->args.size(); ++ai) {
        uint32_t member_index = (uint32_t)(ai - 1);
        uint32_t member_type_id = decl->args[ai];

        if (get_member_builtin(id, member_index) != spv::BuiltInMax) {
          // Built-in members are not part of the Panda type.
          continue;
        }

        const ShaderType *member_type = r_resolve_type(member_type_id);
        if (member_type == nullptr) {
          shader_cat.error()
            << "Struct type with id " << id
            << " contains invalid member type " << member_type_id << "\n";
          return nullptr;
        }
        member_type = apply_member_decorations(id, member_index, member_type);

        const std::string &name = get_member_name(id, member_index);
        int offset = get_member_offset(id, member_index);
        if (offset >= 0) {
          struct_type.add_member(member_type, name, (uint32_t)offset);
        } else {
          struct_type.add_member(member_type, name);
        }
      }
      type = ShaderType::register_type(std::move(struct_type));
    }
    break;

  default:
    // Function types, pointer types and unsupported types do not resolve.
    return nullptr;
  }

  entry._type = type;
  return type;
}

/**
 * Applies the readonly/writeonly and matrix stride decorations of the given
 * struct member to its resolved type, recursing into arrays.
 */
const ShaderType *SpirVModule::
apply_member_decorations(uint32_t type_id, uint32_t member_index, const ShaderType *type) const {
  bool non_writable = has_member_decoration(type_id, member_index, spv::DecorationNonWritable);
  bool non_readable = has_member_decoration(type_id, member_index, spv::DecorationNonReadable);
  if (non_writable || non_readable) {
    if (const ShaderType::Image *image = type->as_image()) {
      ShaderType::Access access = image->get_access();
      if (non_writable) {
        access = (access & ShaderType::Access::READ_ONLY);
      }
      if (non_readable) {
        access = (access & ShaderType::Access::WRITE_ONLY);
      }
      if (access != image->get_access()) {
        return ShaderType::register_type(ShaderType::Image(
          image->get_texture_type(),
          image->get_sampled_type(),
          access));
      }
    }
  }

  uint32_t matrix_stride = get_member_decoration(type_id, member_index, spv::DecorationMatrixStride, 0);
  if (matrix_stride > 0) {
    // Inject matrix stride into the matrix type.
    if (const ShaderType::Matrix *matrix = type->as_matrix()) {
      if (matrix->get_row_stride_bytes() != matrix_stride) {
        return ShaderType::register_type(ShaderType::Matrix(
          matrix->get_scalar_type(), matrix->get_num_rows(),
          matrix->get_num_columns(), matrix_stride));
      }
    }
  }

  if (const ShaderType::Array *array = type->as_array()) {
    const ShaderType *element_type = array->get_element_type();
    const ShaderType *new_element_type = apply_member_decorations(type_id, member_index, element_type);
    if (element_type != new_element_type) {
      return ShaderType::register_type(ShaderType::Array(
        new_element_type, array->get_num_elements(), array->get_stride_bytes()));
    }
  }

  return type;
}

/**
 * Computes whether the given struct type is effectively readonly and/or
 * writeonly: either decorated so itself, or with every non-builtin member so
 * decorated (glslang decorates the members of an SSBO rather than the block).
 */
void SpirVModule::
get_aggregate_access_flags(uint32_t struct_id, bool &non_writable, bool &non_readable) const {
  non_writable = has_decoration(struct_id, spv::DecorationNonWritable);
  non_readable = has_decoration(struct_id, spv::DecorationNonReadable);
  if (non_writable && non_readable) {
    return;
  }

  bool members_non_writable = true;
  bool members_non_readable = true;
  uint32_t num_members = get_num_members(struct_id);
  for (uint32_t i = 0; i < num_members; ++i) {
    if (get_member_builtin(struct_id, i) != spv::BuiltInMax) {
      continue;
    }
    members_non_writable = members_non_writable &&
      has_member_decoration(struct_id, i, spv::DecorationNonWritable);
    members_non_readable = members_non_readable &&
      has_member_decoration(struct_id, i, spv::DecorationNonReadable);
  }
  non_writable = non_writable || members_non_writable;
  non_readable = non_readable || members_non_readable;
}

/**
 * Resolves the type pointed to by the given pointer type id.  A pointer to a
 * (legacy or modern) storage buffer block resolves to a StorageBuffer type
 * wrapping the struct.  The result is memoized; see invalidate_types.
 */
const ShaderType *SpirVModule::
resolve_pointer_type(uint32_t id) const {
  const IdEntry &entry = get_entry(id);
  if (entry._kind != IdKind::POINTER_TYPE) {
    error_expected(id, "a pointer type");
    return nullptr;
  }
  if (entry._type != nullptr) {
    return entry._type;
  }
  const Instruction *decl = get_decl(id);
  nassertr(decl != nullptr && decl->args.size() >= 3, nullptr);

  uint32_t pointee_id = decl->args[2];
  const ShaderType *type;
  if (get_entry(pointee_id)._kind == IdKind::POINTER_TYPE) {
    type = resolve_pointer_type(pointee_id);
  } else {
    type = r_resolve_type(pointee_id);
  }

  spv::StorageClass storage_class = (spv::StorageClass)decl->args[1];
  if (type != nullptr && (storage_class == spv::StorageClassUniform ||
                          storage_class == spv::StorageClassStorageBuffer)) {
    // Unwrap arrays to get to the underlying block type.
    const Instruction *block_decl = get_decl(pointee_id);
    while (block_decl != nullptr && (block_decl->opcode == spv::OpTypeArray ||
                                     block_decl->opcode == spv::OpTypeRuntimeArray)) {
      block_decl = get_decl(block_decl->args[1]);
    }
    if (block_decl != nullptr && block_decl->opcode == spv::OpTypeStruct) {
      uint32_t block_type_id = block_decl->args[0];
      bool buffer_block = has_decoration(block_type_id, spv::DecorationBufferBlock);
      bool block = has_decoration(block_type_id, spv::DecorationBlock);
      if (buffer_block || block) {
        if (buffer_block) {
          // Old SPIR-V way of declaring an SSBO.
          storage_class = spv::StorageClassStorageBuffer;
        }
        if (storage_class == spv::StorageClassStorageBuffer) {
          bool non_writable, non_readable;
          get_aggregate_access_flags(block_type_id, non_writable, non_readable);
          ShaderType::Access access = ShaderType::Access::READ_WRITE;
          if (non_writable) {
            access = (access & ShaderType::Access::READ_ONLY);
          }
          if (non_readable) {
            access = (access & ShaderType::Access::WRITE_ONLY);
          }
          const ShaderType *block_type = r_resolve_type(block_type_id);
          const ShaderType *new_type = ShaderType::register_type(
            ShaderType::StorageBuffer(block_type, access));
          type = type->replace_type(block_type, new_type);
        }
      }
    }
  }

  entry._type = type;
  return type;
}

/**
 * Resolves the type of the given value id (a variable, constant, parameter
 * or instruction result).  For pointers - variables, access chains - this is
 * the pointed-to type.  For convenience, a type id resolves to the type
 * itself.  The result is memoized; see invalidate_types.
 */
const ShaderType *SpirVModule::
resolve_value_type(uint32_t id) const {
  const IdEntry &entry = get_entry(id);
  switch (entry._kind) {
  case IdKind::TYPE:
    return r_resolve_type(id);

  case IdKind::POINTER_TYPE:
    return resolve_pointer_type(id);

  case IdKind::VARIABLE:
    {
      if (entry._type != nullptr) {
        return entry._type;
      }
      const ShaderType *type = resolve_pointer_type(entry._type_id);
      if (type == nullptr) {
        return nullptr;
      }

      // If the variable itself has the readonly/writeonly qualifiers, then
      // we inject those into the (image or storage buffer) type.
      bool non_writable = has_decoration(id, spv::DecorationNonWritable);
      bool non_readable = has_decoration(id, spv::DecorationNonReadable);
      if (non_writable || non_readable) {
        const ShaderType *unwrapped_type = type;
        while (const ShaderType::Array *array_type = unwrapped_type->as_array()) {
          unwrapped_type = array_type->get_element_type();
        }

        const ShaderType *new_type = unwrapped_type;
        if (const ShaderType::Image *image = unwrapped_type->as_image()) {
          ShaderType::Access access = image->get_access();
          if (non_writable) {
            access = (access & ShaderType::Access::READ_ONLY);
          }
          if (non_readable) {
            access = (access & ShaderType::Access::WRITE_ONLY);
          }
          if (access != image->get_access()) {
            new_type = ShaderType::register_type(ShaderType::Image(
              image->get_texture_type(),
              image->get_sampled_type(),
              access));
          }
        }
        else if (const ShaderType::StorageBuffer *buffer = unwrapped_type->as_storage_buffer()) {
          ShaderType::Access access = buffer->get_access();
          if (non_writable) {
            access = (access & ShaderType::Access::READ_ONLY);
          }
          if (non_readable) {
            access = (access & ShaderType::Access::WRITE_ONLY);
          }
          if (access != buffer->get_access()) {
            new_type = ShaderType::register_type(ShaderType::StorageBuffer(
              buffer->get_contained_type(), access));
          }
        }

        if (new_type != unwrapped_type) {
          type = type->replace_type(unwrapped_type, new_type);
        }
      }

      entry._type = type;
      return type;
    }

  case IdKind::CONSTANT:
  case IdKind::SPEC_CONSTANT:
  case IdKind::PARAMETER:
  case IdKind::TEMPORARY:
    {
      if (entry._type != nullptr) {
        return entry._type;
      }
      uint32_t type_id = entry._type_id;
      if (type_id == 0) {
        return nullptr;
      }
      const ShaderType *type;
      if (get_entry(type_id)._kind == IdKind::POINTER_TYPE) {
        type = resolve_pointer_type(type_id);
      } else {
        type = r_resolve_type(type_id);
      }
      entry._type = type;
      return type;
    }

  default:
    return nullptr;
  }
}

/**
 * Throws away all memoized ShaderType resolutions and the type reuse map.
 * The next query re-derives from the current declarations and annotations.
 */
void SpirVModule::
invalidate_types() {
  for (IdEntry &entry : _ids) {
    entry._type = nullptr;
  }
  _type_map.clear();
  _type_map_valid = false;
}

/**
 * Returns true if the given decoration feeds into the type resolution, so
 * that adding or removing it must invalidate the memoized resolutions.
 */
static bool
is_structural_decoration(spv::Decoration decoration) {
  switch (decoration) {
  case spv::DecorationBlock:
  case spv::DecorationBufferBlock:
  case spv::DecorationBuiltIn:
  case spv::DecorationNonWritable:
  case spv::DecorationNonReadable:
  case spv::DecorationOffset:
  case spv::DecorationArrayStride:
  case spv::DecorationMatrixStride:
  case spv::DecorationRowMajor:
  case spv::DecorationColMajor:
    return true;
  default:
    return false;
  }
}

/**
 * Rebuilds the type reuse map by visiting all type declarations in
 * topological order (so that a component type is classified before the types
 * that reference it) and entering those that are declared exactly the way
 * define_type() would declare them.
 */
void SpirVModule::
ensure_type_map() const {
  if (_type_map_valid) {
    return;
  }
  _type_map_valid = true;
  _type_map.clear();

  for (size_t index : sort_globals()) {
    const Instruction &op = _globals[index];
    switch (op.opcode) {
    case spv::OpTypeVoid:
    case spv::OpTypeBool:
    case spv::OpTypeInt:
    case spv::OpTypeFloat:
    case spv::OpTypeVector:
    case spv::OpTypeMatrix:
    case spv::OpTypeImage:
    case spv::OpTypeSampler:
    case spv::OpTypeSampledImage:
    case spv::OpTypeArray:
    case spv::OpTypeRuntimeArray:
    case spv::OpTypeStruct:
      break;
    default:
      continue;
    }

    uint32_t id = op.args[0];
    const ShaderType *type = r_resolve_type(id);
    if (type == nullptr && op.opcode != spv::OpTypeVoid) {
      continue;
    }
    if (has_builtin_members(id)) {
      // Only put types we can fully round-trip in the type map.
      continue;
    }
    if (is_canonical_type_decl(op)) {
      _type_map[type] = id;
    }
  }
}

/**
 * Returns true if the given type declaration is exactly what define_type()
 * would emit for its resolved ShaderType (eg. not a 16-bit float, not a
 * multisampled image).  Only such declarations enter the type reuse map.
 * Assumes the map has been seeded up to this declaration in topological
 * order, so that component canonicality can be checked through the map.
 */
bool SpirVModule::
is_canonical_type_decl(const Instruction &op) const {
  // Checks whether the given (already visited) component type declaration is
  // the canonical declaration for its ShaderType.
  auto canonical_component = [this](uint32_t component_id) {
    const ShaderType *component = r_resolve_type(component_id);
    if (component == nullptr) {
      return false;
    }
    TypeMap::const_iterator tit = _type_map.find(component);
    return tit != _type_map.end() && tit->second == component_id;
  };

  switch (op.opcode) {
  case spv::OpTypeVoid:
  case spv::OpTypeBool:
  case spv::OpTypeSampler:
    return op.args.size() == 1;

  case spv::OpTypeInt:
    return op.args.size() == 3 && op.args[1] == 32;

  case spv::OpTypeFloat:
    return op.args.size() == 2 && (op.args[1] == 32 || op.args[1] == 64);

  case spv::OpTypeVector:
  case spv::OpTypeMatrix:
  case spv::OpTypeRuntimeArray:
    return canonical_component(op.args[1]);

  case spv::OpTypeArray:
    return canonical_component(op.args[1]);

  case spv::OpTypeImage:
    // Unspecified depth, storage image, unknown format, as define_type()
    // declares images (the access qualifier operand is part of the resolved
    // type, so its presence does not affect canonicality).
    return op.args.size() >= 8 &&
           op.args[3] == 2 && op.args[5] == 0 && op.args[6] == 2 &&
           op.args[7] == (uint32_t)spv::ImageFormatUnknown &&
           canonical_component(op.args[1]);

  case spv::OpTypeSampledImage:
    {
      // Canonical only if the image is declared exactly the way define_type()
      // would declare it for this sampled image type: sampled, single-sample,
      // known depthness, unknown format.
      const Instruction *image_decl = get_decl(op.args[1]);
      return image_decl != nullptr && image_decl->opcode == spv::OpTypeImage &&
             image_decl->args.size() >= 8 &&
             image_decl->args[6] == 1 && image_decl->args[5] == 0 &&
             (image_decl->args[3] == 0 || image_decl->args[3] == 1) &&
             image_decl->args[7] == (uint32_t)spv::ImageFormatUnknown;
    }

  case spv::OpTypeStruct:
    for (size_t ai = 1; ai < op.args.size(); ++ai) {
      if (get_member_builtin(op.args[0], (uint32_t)(ai - 1)) != spv::BuiltInMax) {
        continue;
      }
      if (!canonical_component(op.args[ai])) {
        return false;
      }
    }
    return true;

  default:
    return false;
  }
}

/**
 * Returns true if the given id or any of its struct members carry the
 * BuiltIn decoration.
 */
bool SpirVModule::
has_builtin_members(uint32_t type_id) const {
  const Annotations *annotations = get_annotations(type_id);
  if (annotations == nullptr) {
    return false;
  }
  for (const Annotation &annotation : *annotations) {
    if (annotation.get_decoration() == spv::DecorationBuiltIn) {
      return true;
    }
  }
  return false;
}

/**
 * Returns the id of a live, canonical declaration of the given type, or 0 if
 * there is none.
 */
uint32_t SpirVModule::
find_type(const ShaderType *type) const {
  ensure_type_map();
  TypeMap::const_iterator tit = _type_map.find(type);
  if (tit != _type_map.end()) {
    return tit->second;
  }
  // A matrix type with an explicit stride is declared as the plain matrix
  // type; the stride lives in a decoration on the containing struct member.
  if (type != nullptr) {
    if (const ShaderType::Matrix *matrix = type->as_matrix()) {
      const ShaderType *plain = ShaderType::register_type(ShaderType::Matrix(
        matrix->get_scalar_type(), matrix->get_num_rows(), matrix->get_num_columns()));
      if (plain != type) {
        tit = _type_map.find(plain);
        if (tit != _type_map.end()) {
          return tit->second;
        }
      }
    }
  }
  return 0;
}

/**
 * Returns the first id carrying the given OpName, or 0 if there is none.
 */
uint32_t SpirVModule::
find_named(const std::string &name) const {
  for (const auto &item : _names) {
    if (item.second == name) {
      return item.first;
    }
  }
  return 0;
}

/**
 * For a given type id, recursively collects all struct types nested therein
 * (chasing pointer, array and member types) and writes them to the given
 * map.
 */
void SpirVModule::
collect_nested_structs(pmap<uint32_t, const ShaderType::Struct *> &result, uint32_t type_id) const {
  const Instruction *decl = get_decl(type_id);
  if (decl == nullptr) {
    return;
  }
  switch (decl->opcode) {
  case spv::OpTypePointer:
    collect_nested_structs(result, decl->args[2]);
    break;

  case spv::OpTypeArray:
  case spv::OpTypeRuntimeArray:
  case spv::OpTypeVector:
  case spv::OpTypeMatrix:
    collect_nested_structs(result, decl->args[1]);
    break;

  case spv::OpTypeStruct:
    {
      const ShaderType *type = r_resolve_type(type_id);
      const ShaderType::Struct *struct_type = (type != nullptr) ? type->as_struct() : nullptr;
      if (struct_type != nullptr) {
        result[type_id] = struct_type;
      }
      for (size_t ai = 1; ai < decl->args.size(); ++ai) {
        collect_nested_structs(result, decl->args[ai]);
      }
    }
    break;

  default:
    break;
  }
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
 * Sets the name emitted for the given id.
 */
void SpirVModule::
set_name(uint32_t id, const std::string &name) {
  _names[id] = name;
}

/**
 * Returns the name of the given struct member, or an empty string.
 */
const std::string &SpirVModule::
get_member_name(uint32_t type_id, uint32_t member_index) const {
  static const std::string empty;
  auto it = _member_names.find(type_id);
  if (it == _member_names.end() || member_index >= it->second.size()) {
    return empty;
  }
  return it->second[member_index];
}

/**
 * Sets the name of the given struct member.
 */
void SpirVModule::
set_member_name(uint32_t type_id, uint32_t member_index, const std::string &name) {
  pvector<std::string> &names = _member_names[type_id];
  if (member_index >= names.size()) {
    names.resize(member_index + 1);
  }
  names[member_index] = name;

  // The member names are baked into the resolved struct type.
  invalidate_types();
}

/**
 * Returns the name that parameter extraction reports for the given id: the
 * reflection name if one was set (see set_reflection_name), the emitted name
 * otherwise.
 */
const std::string &SpirVModule::
get_reflection_name(uint32_t id) const {
  auto it = _reflection_names.find(id);
  if (it != _reflection_names.end()) {
    return it->second;
  }
  return get_name(id);
}

/**
 * Overrides the name that parameter extraction reports for the given id,
 * without changing the emitted OpName.  Used by passes that give a synthetic
 * variable a qualified name, such as "block.member" after flattening.
 */
void SpirVModule::
set_reflection_name(uint32_t id, const std::string &name) {
  _reflection_names[id] = name;
}

/**
 * Attaches the given annotation to the given id.  If the decoration feeds
 * into the type resolution, the memoized resolutions are invalidated.
 */
void SpirVModule::
decorate(uint32_t id, const Annotation &annotation) {
  _annotations[id].push_back(annotation);

  if (is_structural_decoration(annotation.get_decoration())) {
    invalidate_types();
  }
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
 * given id.  Returns true if any was removed.
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
  if (any && is_structural_decoration(decoration)) {
    invalidate_types();
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
 * id in the module.
 */
void SpirVModule::
strip_decoration(spv::Decoration decoration) {
  bool any = false;
  for (auto &item : _annotations) {
    Annotations &annotations = item.second;
    for (size_t i = annotations.size(); i-- > 0;) {
      if (!annotations[i].is_member_annotation() &&
          annotations[i].get_decoration() == decoration) {
        annotations.erase(annotations.begin() + i);
        any = true;
      }
    }
  }
  if (any && is_structural_decoration(decoration)) {
    invalidate_types();
  }
}

/**
 * Moves all annotations from one id to another, appending them to any the
 * target already has.
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

  // The moved annotations may include structural ones (BuiltIn and access
  // qualifiers move along with a variable swap).
  invalidate_types();
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
 * point interface references, and clears its index entry.  Takes effect
 * immediately.
 *
 * References to the id in function bodies are the pass's responsibility
 * (see delete_dead_code for the common cascade); deletions do not cascade to
 * dependent declarations automatically.
 */
void SpirVModule::
delete_id(uint32_t id) {
  // Copied, since clearing the state of contained ids may grow the index.
  const IdEntry entry = get_entry(id);

  if (entry._kind == IdKind::FUNCTION) {
    // Remove the entire function, clearing the state of everything declared
    // inside it.
    for (size_t fi = 0; fi < _functions.size(); ++fi) {
      if (_functions[fi].id != id) {
        continue;
      }
      for (const Instruction &op : _functions[fi].instructions) {
        uint32_t result_id = op.get_result();
        if (result_id != 0 && result_id != id) {
          clear_id_state(result_id);
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
  else if (entry._function_id != 0) {
    // Defined inside a function body.
    Function *function = get_function(entry._function_id);
    if (function != nullptr) {
      for (Instruction &op : function->instructions) {
        if (!op.is_nop() && op.get_result() == id) {
          op = Instruction(spv::OpNop);
          break;
        }
      }
    }
  }
  else if (entry._kind == IdKind::EXT_INST) {
    for (size_t i = _ext_inst_imports.size(); i-- > 0;) {
      if (!_ext_inst_imports[i].args.empty() && _ext_inst_imports[i].args[0] == id) {
        _ext_inst_imports.erase(_ext_inst_imports.begin() + i);
      }
    }
  }
  else if (entry._kind == IdKind::STRING) {
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

  if (is_type(id)) {
    // The reuse map may hand out the deleted id; rebuild it on next lookup.
    // Resolutions memoized on other ids stay valid: a live declaration may
    // not reference a deleted one.
    _type_map.clear();
    _type_map_valid = false;
  }

  clear_id_state(id);
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
 */
void SpirVModule::
replace_type_id(uint32_t before, uint32_t after) {
  nassertv(before != 0u && after != 0u);
  if (before == after) {
    return;
  }
  nassertv(is_type(before));
  nassertv(is_type(after));

  rewrite_type_references(before, after);

  Instruction *decl = get_global(before);
  if (decl != nullptr) {
    *decl = Instruction(spv::OpNop);
  }

  clear_id_state(before);

  // The replacement may resolve differently (eg. a different depth flag), so
  // every memoized resolution that could embed it is suspect.
  invalidate_types();
}

/**
 * Rewrites all references to the given type id, in both the instructions and
 * the recorded result types in the id index.
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

  // The index mirrors the result type of every value id, so follow suit.
  for (IdEntry &entry : _ids) {
    if (entry._type_id == before) {
      entry._type_id = after;
    }
  }
}

/**
 * Deletes the given member (by current index) from the given struct type
 * declaration: the OpTypeStruct operand, the member's name and its member
 * decorations are all removed, and higher member indices are renumbered.
 *
 * Access chains indexing into the struct are the pass's responsibility.
 */
void SpirVModule::
delete_struct_member(uint32_t type_id, uint32_t member_index) {
  Instruction *decl = get_global(type_id);
  nassertv(decl != nullptr && decl->opcode == spv::OpTypeStruct);
  nassertv(member_index + 1 < decl->args.size());
  decl->args.erase(decl->args.begin() + 1 + member_index);

  auto nit = _member_names.find(type_id);
  if (nit != _member_names.end() && member_index < nit->second.size()) {
    nit->second.erase(nit->second.begin() + member_index);
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

  invalidate_types();
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

  for (auto it = param_indices.rbegin(); it != param_indices.rend(); ++it) {
    uint32_t index = *it;
    nassertd(index + 2 < decl->args.size()) continue;
    decl->args.erase(decl->args.begin() + 2 + index);
  }

  // Remove the matching OpFunctionParameter instructions of every function
  // of this type, and strip the matching arguments from every call to such
  // a function.
  pset<uint32_t> affected_functions;
  for (uint32_t id = 0; id < _ids.size(); ++id) {
    const IdEntry &entry = _ids[id];
    if (entry._kind == IdKind::FUNCTION && entry._type_id == type_id) {
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
            clear_id_state(param_id);
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
          clear_id_state(result_id);
        }
        op = Instruction(spv::OpNop);
      }
    }
  }
}

/**
 * Merges duplicate declarations of types that SPIR-V requires to be unique
 * within a module (see is_unique_type_opcode), plus function types.  A later
 * declaration is merged onto the earlier identical one; all references are
 * rewritten.  Aggregate types (structs and arrays) are exempt, since SPIR-V
 * permits and gives meaning to their duplication.
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
    if (!is_unique_type_opcode(op.opcode) &&
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
 * Reassigns the result id of the given module-scope declaration, moving its
 * index entry along.  Names and annotations stay with the old id; the caller
 * transfers or reassigns those as appropriate.
 */
void SpirVModule::
reassign_global_id(uint32_t old_id, uint32_t new_id) {
  IdEntry entry = get_entry(old_id);
  nassertv(entry._global_index >= 0 && (size_t)entry._global_index < _globals.size());
  nassertv(new_id != 0 && new_id < _id_bound);

  Instruction &op = _globals[entry._global_index];
  nassertv(op.get_result() == old_id);
  op.args[op.has_result_type() ? 1 : 0] = new_id;

  modify_entry(new_id) = entry;
  modify_entry(old_id) = IdEntry();

  if (is_type(new_id)) {
    _type_map.clear();
    _type_map_valid = false;
  }
}

/**
 * Adds the given instruction to the globals section, recording its result
 * (if any) in the id index.
 */
void SpirVModule::
add_global(Instruction op) {
  int32_t index = (int32_t)_globals.size();
  _globals.push_back(std::move(op));
  record_result_at(_globals.back(), 0, index);
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
  record_result(op, 0);
  _debug.push_back(std::move(op));
  return id;
}

/**
 * Adds an OpExtInstImport with the given set name and returns its id.
 */
uint32_t SpirVModule::
add_ext_inst_import(const std::string &name) {
  for (const Instruction &op : _ext_inst_imports) {
    if (op.get_string(1) == name) {
      return op.args[0];
    }
  }
  uint32_t id = allocate_id();
  Instruction op(spv::OpExtInstImport, {id});
  pack_string(op.args, name);
  record_result(op, 0);
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
  for (uint32_t id = 0; id < _ids.size(); ++id) {
    const IdEntry &entry = _ids[id];
    if (entry._kind == IdKind::VARIABLE && entry._function_id == 0 &&
        get_storage_class(id) == spv::StorageClassInput &&
        get_builtin(id) == builtin) {
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
  uint32_t id = find_type(type);
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
  }
  else if (const ShaderType::Matrix *matrix_type = type->as_matrix()) {
    uint32_t row_type = define_type(
      ShaderType::register_type(ShaderType::Vector(matrix_type->get_scalar_type(), matrix_type->get_num_columns())));

    add_global(Instruction(spv::OpTypeMatrix,
      {id, row_type, matrix_type->get_num_rows()}));
  }
  else if (const ShaderType::Struct *struct_type = type->as_struct()) {
    size_t num_members = struct_type->get_num_members();
    Instruction op(spv::OpTypeStruct);
    op.args.push_back(id);

    for (size_t i = 0; i < num_members; ++i) {
      const ShaderType::Struct::Member &member = struct_type->get_member(i);
      uint32_t member_type_id = define_type(member.type);
      op.args.push_back(member_type_id);
    }

    add_global(std::move(op));

    // Setting the member names invalidates the memoized resolutions; do it
    // before caching this type below.
    for (size_t i = 0; i < num_members; ++i) {
      const ShaderType::Struct::Member &member = struct_type->get_member(i);
      if (!member.name.empty()) {
        set_member_name(id, (uint32_t)i, member.name);
      }
    }
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
  }
  else {
    add_global(Instruction(spv::OpTypeVoid, {id}));
  }

  // Memoize the requested type and put it in the reuse map: everything above
  // declares its type exactly the way the canonicality rules expect.  (The
  // void type is keyed under null, like a reseed of the map would key it.)
  modify_entry(id)._type = type;
  if (_type_map_valid) {
    _type_map[type] = id;
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

  args[0] = id;
  args[1] = define_type(sampled_scalar_type);

  add_global(Instruction(spv::OpTypeImage, args, nargs));

  modify_entry(id)._type = type;

  // This only becomes the canonical declaration for this ShaderType if it
  // is declared exactly the way define_type() would declare it.
  if (_type_map_valid &&
      depth == 2 && sampled == 2 && format == spv::ImageFormatUnknown) {
    _type_map[type] = id;
  }
  return id;
}

/**
 * Ensures that a pointer type to the given (already declared) type with the
 * given storage class is declared, and returns its id.
 */
uint32_t SpirVModule::
define_pointer_type(uint32_t type_id, spv::StorageClass storage_class) {
  nassertr_always(type_id != 0u, 0u);

  // The comparison is against the storage class as declared, not the
  // canonicalized one that get_storage_class reports: pointer types are
  // requested and declared in emission terms (a legacy-style buffer block
  // keeps its declared Uniform class).
  for (const Instruction &op : _globals) {
    if (op.opcode == spv::OpTypePointer && op.args.size() >= 3 &&
        op.args[2] == type_id &&
        op.args[1] == (uint32_t)storage_class) {
      return op.args[0];
    }
  }

  uint32_t pointer_type_id = allocate_id();
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

  for (const Instruction &op : _globals) {
    if (op.opcode == spv::OpTypeFunction && op.args.size() == 2) {
      const Instruction *return_decl = get_decl(op.args[1]);
      if (return_decl != nullptr && return_decl->opcode == spv::OpTypeVoid) {
        return op.args[0];
      }
    }
  }

  uint32_t return_type_id = define_type(return_type);
  uint32_t function_type_id = allocate_id();
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
  const Instruction *decl = get_decl(type_id);
  nassertv(decl != nullptr);

  if (decl->opcode == spv::OpTypeArray || decl->opcode == spv::OpTypeRuntimeArray) {
    // Make sure there's an ArrayStride decoration for this array, then
    // recurse into the element type.
    if (get_array_stride(type_id) == 0) {
      const ShaderType *type = resolve_type(type_id);
      const ShaderType::Array *array_type = (type != nullptr) ? type->as_array() : nullptr;
      nassertv(array_type != nullptr);
      decorate(type_id, spv::DecorationArrayStride, array_type->get_stride_bytes());
    }
    r_annotate_struct_layout(decl->args[1]);
    return;
  }

  if (decl->opcode != spv::OpTypeStruct) {
    return;
  }

  const ShaderType *type = resolve_type(type_id);
  const ShaderType::Struct *struct_type = (type != nullptr) ? type->as_struct() : nullptr;
  nassertv(struct_type != nullptr);

  uint32_t num_members = struct_type->get_num_members();

  for (uint32_t i = 0; i < num_members; ++i) {
    const ShaderType::Struct::Member &member = struct_type->get_member(i);

    if (get_member_offset(type_id, i) < 0) {
      decorate_member(type_id, i, spv::DecorationOffset, member.offset);
    }

    // Unwrap arrays to see if there's a matrix here.  Note that decl may be
    // refetched, since decorate() may not keep instruction pointers valid...
    // it does today, but this stays on the safe side of the contract.
    uint32_t member_type_id = get_member_type_id(type_id, i);
    const ShaderType *base_type = member.type;
    while (const ShaderType::Array *array_type = base_type->as_array()) {
      base_type = array_type->get_element_type();

      // Also make sure there's an ArrayStride decoration for this array.
      if (get_array_stride(member_type_id) == 0) {
        decorate(member_type_id, spv::DecorationArrayStride, array_type->get_stride_bytes());
      }
      const Instruction *member_decl = get_decl(member_type_id);
      nassertv(member_decl != nullptr && member_decl->args.size() >= 2);
      member_type_id = member_decl->args[1];
    }

    if (const ShaderType::Matrix *matrix_type = base_type->as_matrix()) {
      // Matrix types need to be explicitly laid out.
      if (get_member_decoration(type_id, i, spv::DecorationMatrixStride, 0) == 0) {
        decorate_member(type_id, i, spv::DecorationMatrixStride, matrix_type->get_row_stride_bytes());
        decorate_member(type_id, i, spv::DecorationColMajor);
      }
    } else {
      r_annotate_struct_layout(member_type_id);
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
  for (const Instruction &op : _globals) {
    if (op.opcode != spv::OpConstant || op.args.size() != 3 ||
        op.args[2] != (uint32_t)constant) {
      continue;
    }
    // Skip constants of a non-canonical type declaration (eg. a 16-bit int),
    // which cannot be reused interchangeably with a plain int constant.
    const Instruction *type_decl = get_decl(op.args[0]);
    if (type_decl != nullptr && type_decl->opcode == spv::OpTypeInt &&
        type_decl->args.size() == 3 && type_decl->args[1] == 32) {
      if (type_decl->args[2] != 0 || constant >= 0) {
        return op.args[1];
      }
    }
  }

  uint32_t type_id = define_type(ShaderType::INT);
  uint32_t constant_id = allocate_id();

  add_global(Instruction(spv::OpConstant, {type_id, constant_id, (uint32_t)constant}));
  return constant_id;
}

/**
 * Defines a new null constant of the given type, reusing an existing one if
 * one is already defined.
 */
uint32_t SpirVModule::
define_null_constant(const ShaderType *type) {
  uint32_t type_id = define_type(type);

  for (const Instruction &op : _globals) {
    if (op.opcode == spv::OpConstantNull && op.args.size() >= 2 &&
        op.args[0] == type_id) {
      return op.args[1];
    }
  }

  uint32_t constant_id = allocate_id();
  add_global(Instruction(spv::OpConstantNull, {type_id, constant_id}));
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
  } else {
    add_global(Instruction(spv::OpConstant, {type_id, constant_id, constant}));
  }
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
  return id;
}

/**
 * Writes a simple description of the given id to the output stream.
 */
void SpirVModule::
output_id(std::ostream &out, uint32_t id) const {
  const IdEntry &entry = get_entry(id);
  switch (entry._kind) {
  case IdKind::NONE:
    out << "undefined";
    break;

  case IdKind::TYPE:
    {
      const ShaderType *type = r_resolve_type(id);
      if (type != nullptr) {
        out << "type " << *type;
      } else {
        out << "type";
      }
    }
    break;

  case IdKind::POINTER_TYPE:
    {
      const Instruction *decl = get_decl(id);
      if (decl != nullptr && decl->args.size() >= 3) {
        out << "pointer to " << decl->args[2];
      } else {
        out << "pointer type";
      }
    }
    break;

  case IdKind::FUNCTION_TYPE:
    out << "function type";
    break;

  case IdKind::VARIABLE:
    out << "variable of type " << entry._type_id;
    break;

  case IdKind::CONSTANT:
    out << "constant";
    break;

  case IdKind::SPEC_CONSTANT:
    out << "spec constant";
    break;

  case IdKind::FUNCTION:
    out << "function";
    break;

  case IdKind::PARAMETER:
    out << "function parameter";
    break;

  case IdKind::TEMPORARY:
    out << "temporary";
    break;

  case IdKind::EXT_INST:
    out << "ext inst";
    break;

  case IdKind::STRING:
    out << "string";
    break;

  default:
    out << "invalid";
    break;
  }
}

/**
 * Issues an error about the given id.  Returns 0.
 */
uint32_t SpirVModule::
error_expected(uint32_t id, const char *msg) const {
  std::ostringstream str;
  str << id << " (";
  output_id(str, id);
  str << ") was expected to be " << msg << "\n";
  nassert_raise(str.str());
  return 0u;
}

/**
 * Returns true for type-declaring opcodes whose declarations SPIR-V forbids
 * a module from containing twice with identical operands.  The finalization
 * sweep run by the SpirVTransformer merges duplicate declarations of these
 * types.  Aggregate types (structs and arrays) are excluded, since SPIR-V
 * explicitly permits duplicating those and their duplication is meaningful
 * (eg. two structs with identical members may carry different decorations).
 *
 * Function types are also required to be unique by SPIR-V, but are excluded
 * here regardless: their operands may still change while a transformation
 * pass runs (parameters can be deleted), so the sweep handles them
 * separately, comparing their final post-deletion operands.
 */
bool SpirVModule::
is_unique_type_opcode(spv::Op opcode) {
  switch (opcode) {
  case spv::OpTypeVoid:
  case spv::OpTypeBool:
  case spv::OpTypeInt:
  case spv::OpTypeFloat:
  case spv::OpTypeVector:
  case spv::OpTypeMatrix:
  case spv::OpTypeImage:
  case spv::OpTypeSampler:
  case spv::OpTypeSampledImage:
  case spv::OpTypePointer:
  // Cooperative matrix types are parameterized non-aggregate types as well.
  case spv::OpTypeCooperativeMatrixKHR:
  case spv::OpTypeCooperativeMatrixNV:
    return true;
  default:
    // Note that some rarely-used non-aggregate types (such as the pipe and
    // event types) are also required to be unique, but those are not
    // supported by the deduplication mechanism.
    return false;
  }
}
