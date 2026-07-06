/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file spirVMakeBlockPass.cxx
 * @author rdb
 * @date 2024-10-11
 */

#include "spirVMakeBlockPass.h"

/**
 *
 */
SpirVMakeBlockPass::
SpirVMakeBlockPass(const ShaderType::Struct *block_type, const pvector<uint32_t> &member_ids,
                   spv::StorageClass storage_class, uint32_t binding, uint32_t set) :
  _block_type(block_type),
  _storage_class(storage_class),
  _binding(binding),
  _set(set) {
  nassertv(block_type->get_num_members() == member_ids.size());

  for (uint32_t i = 0; i < (uint32_t)member_ids.size(); ++i) {
    uint32_t member_id = member_ids[i];
    if (member_id > 0) {
      _member_indices[member_id] = i;
    }
  }
}

/**
 *
 */
void SpirVMakeBlockPass::
run(SpirVModule &module) {
  SpirVResultDatabase &db = module.modify_db();

  // Define the block type and variable.
  uint32_t block_type_id = module.define_type(_block_type);
  _block_var_id = module.define_variable(_block_type, _storage_class);

  module.decorate(block_type_id, spv::DecorationBlock);

  if (_storage_class != spv::StorageClassPushConstant) {
    module.decorate(_block_var_id, spv::DecorationBinding, _binding);
    module.decorate(_block_var_id, spv::DecorationDescriptorSet, _set);
  }

  for (const auto &item : _member_indices) {
    const std::string &name = db.get_definition(item.first)._name;
    if (!name.empty()) {
      module.set_member_name(block_type_id, item.second, name);
    }
  }

  // Rewrite all accesses to the old variables into accesses to the block.
  for (Function &function : module.modify_functions()) {
    for (size_t i = 0; i < function.instructions.size(); ++i) {
      SpirVBuilder builder(module, function.id, i);

      // Replaces the given operand of the instruction being visited with a
      // freshly inserted access chain, if it refers to one of the old
      // variables.  Note that the insertion shifts the instruction itself
      // past the builder's cursor.
      auto replace_arg = [&](size_t arg_index) {
        Instruction &op = function.instructions[builder.get_insertion_index()];
        if (arg_index >= op.args.size()) {
          return;
        }
        auto it = _member_indices.find(op.args[arg_index]);
        if (it != _member_indices.end()) {
          uint32_t constant_id = module.define_int_constant(it->second);
          uint32_t chain_id = builder.op_access_chain(_block_var_id, {constant_id});
          module.mark_used(chain_id);
          function.instructions[builder.get_insertion_index()].args[arg_index] = chain_id;
        }
      };

      switch (function.instructions[i].opcode) {
      case spv::OpAccessChain:
      case spv::OpInBoundsAccessChain:
        {
          Instruction &op = function.instructions[i];
          if (op.args.size() >= 3 && _member_indices.count(op.args[2])) {
            uint32_t member_index = _member_indices[op.args[2]];
            uint32_t constant_id = module.define_int_constant(member_index);

            // Get a type pointer with the correct storage class, and prepend
            // our new block variable to the existing access chain.
            op.args[0] = module.define_pointer_type(
              module.resolve_pointer_type(op.args[0]), _storage_class);
            op.args[2] = _block_var_id;
            op.args.insert(op.args.begin() + 3, constant_id);

            db.set_origin(op.args[1], _block_var_id);
          }
        }
        break;

      case spv::OpFunctionCall:
        // Add access chains when passing a load of a member id to a function.
        for (size_t ai = 3; ai < function.instructions[builder.get_insertion_index()].args.size(); ++ai) {
          replace_arg(ai);
        }
        break;

      case spv::OpPtrEqual:
      case spv::OpPtrNotEqual:
      case spv::OpPtrDiff:
        replace_arg(3);
        replace_arg(2);
        break;

      case spv::OpLoad:
      case spv::OpAtomicLoad:
      case spv::OpAtomicExchange:
      case spv::OpAtomicCompareExchange:
      case spv::OpAtomicCompareExchangeWeak:
      case spv::OpAtomicIIncrement:
      case spv::OpAtomicIDecrement:
      case spv::OpAtomicIAdd:
      case spv::OpAtomicISub:
      case spv::OpAtomicSMin:
      case spv::OpAtomicUMin:
      case spv::OpAtomicSMax:
      case spv::OpAtomicUMax:
      case spv::OpAtomicAnd:
      case spv::OpAtomicOr:
      case spv::OpAtomicXor:
      case spv::OpAtomicFlagTestAndSet:
      case spv::OpAtomicFMinEXT:
      case spv::OpAtomicFMaxEXT:
      case spv::OpAtomicFAddEXT:
      case spv::OpImageTexelPointer:
      case spv::OpCopyObject:
      case spv::OpExpectKHR:
      case spv::OpBitcast:
      case spv::OpCopyLogical:
        // Add access chains before all loads to access the right block member.
        replace_arg(2);
        break;

      case spv::OpCopyMemory:
      case spv::OpCopyMemorySized:
        replace_arg(1);
        replace_arg(0);
        break;

      case spv::OpStore:
      case spv::OpAtomicStore:
      case spv::OpAtomicFlagClear:
      case spv::OpReturnValue:
        replace_arg(0);
        break;

      default:
        break;
      }

      // Skip past whatever the builder inserted before this instruction.
      i = builder.get_insertion_index();
    }
  }

  // Delete the old individual variables.
  for (const auto &item : _member_indices) {
    module.delete_id(item.first);
  }
}
