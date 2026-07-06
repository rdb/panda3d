/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file spirVFlattenStructPass.cxx
 * @author rdb
 * @date 2024-10-08
 */

#include "spirVFlattenStructPass.h"

/**
 *
 */
SpirVFlattenStructPass::
SpirVFlattenStructPass(uint32_t type_id) : _type_id(type_id) {
}

/**
 *
 */
void SpirVFlattenStructPass::
run(SpirVModule &module) {
  SpirVResultDatabase &db = module.modify_db();

  {
    const Definition &def = db.get_definition(_type_id);
    DCAST_INTO_V(_struct_type, def._type);
    _member_ids.resize(def._members.size(), 0);
  }

  // Find the variables of this struct type, along with the pointer types
  // that reference it.
  pvector<uint32_t> struct_var_ids;
  pvector<uint32_t> pointer_type_ids;
  for (const Instruction &op : module.get_globals()) {
    if (op.opcode == spv::OpTypePointer && op.args.size() >= 3 &&
        op.args[2] == _type_id) {
      pointer_type_ids.push_back(op.args[0]);
    }
    else if (op.opcode == spv::OpVariable && op.args.size() >= 3 &&
             db.get_definition(op.args[0])._type_id == _type_id) {
      struct_var_ids.push_back(op.args[1]);
    }
  }

  // Replace each variable with individual variables for all its members.
  for (uint32_t struct_var_id : struct_var_ids) {
    Definition &struct_def = db.modify_definition(struct_var_id);
    int struct_location = struct_def._location;
    std::string struct_var_name = struct_def._name;

    if (shader_cat.is_spam()) {
      shader_cat.spam()
        << "Removing variable " << struct_var_id << ": "
        << *struct_def._type << " " << struct_var_name << "\n";
    }
    module.delete_id(struct_var_id);
    _deleted.insert(struct_var_id);

    for (size_t mi = 0; mi < _struct_type->get_num_members(); ++mi) {
      const ShaderType::Struct::Member &member = _struct_type->get_member(mi);

      // Insert a new variable for this struct member.
      uint32_t variable_id = module.define_variable(member.type, spv::StorageClassUniformConstant);
      if (!member.name.empty()) {
        module.set_name(variable_id, member.name);
      }

      // The database keeps the qualified name, for consumers of the module's
      // parameter definitions.
      if (!struct_var_name.empty()) {
        db.modify_definition(variable_id)._name = struct_var_name + "." + member.name;
      }
      if (struct_location >= 0) {
        // Assign decorations to the individual members.
        module.decorate(variable_id, spv::DecorationLocation, (uint32_t)(struct_location + mi));
      }

      _member_ids[mi] = variable_id;
    }
  }

  // Delete the struct type and the pointer types that referenced it.
  for (uint32_t pointer_type_id : pointer_type_ids) {
    module.delete_id(pointer_type_id);
  }
  module.delete_id(_type_id);

  // Now rebase all accesses through the struct onto the new variables.
  for (Function &function : module.modify_functions()) {
    for (size_t i = 0; i < function.instructions.size(); ++i) {
      Instruction &op = function.instructions[i];
      switch (op.opcode) {
      case spv::OpAccessChain:
      case spv::OpInBoundsAccessChain:
      case spv::OpPtrAccessChain:
      case spv::OpInBoundsPtrAccessChain:
        if (_deleted_access_chains.count(op.args[2])) {
          // The base of this access chain is an access chain we deleted.
          uint32_t new_var_id = _deleted_access_chains[op.args[2]];

          const ShaderType *type = module.resolve_pointer_type(op.args[0]);
          uint32_t pointer_type_id = module.define_pointer_type(type, spv::StorageClassUniformConstant);

          op.args[0] = pointer_type_id;
          op.args[2] = new_var_id;

          Definition &def = db.modify_definition(op.args[1]);
          def._type_id = pointer_type_id;
          def._origin_id = new_var_id;
        }
        else if (_deleted.count(op.args[2])) {
          uint32_t index = module.resolve_constant(op.args[3]);
          nassertd(index < _member_ids.size()) continue;
          if (op.args.size() > 4) {
            // We also need to change the type if it has the wrong storage
            // class.  Just unwrap the first index.
            const ShaderType *type = module.resolve_pointer_type(op.args[0]);
            uint32_t pointer_type_id = module.define_pointer_type(type, spv::StorageClassUniformConstant);

            op.args[0] = pointer_type_id;
            op.args[2] = _member_ids[index];
            op.args.erase(op.args.begin() + 3);

            // Change the origin so that future loads through this access
            // chain will be able to mark the new variable as used.
            Definition &def = db.modify_definition(op.args[1]);
            def._type_id = pointer_type_id;
            def._origin_id = _member_ids[index];
          } else {
            // Delete the access chain entirely.
            uint32_t result_id = op.args[1];
            _deleted_access_chains[result_id] = _member_ids[index];
            _deleted.insert(result_id);
            module.delete_id(result_id);
          }
        }
        break;

      case spv::OpFunctionCall:
        for (size_t ai = 3; ai < op.args.size(); ++ai) {
          auto it = _deleted_access_chains.find(op.args[ai]);
          if (it != _deleted_access_chains.end()) {
            op.args[ai] = it->second;
          }
          db.mark_used(op.args[ai]);
        }
        break;

      case spv::OpImageTexelPointer:
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
        // If this triggers, the struct is being loaded into another variable,
        // which means we can't unwrap this (for now).
        {
          Definition &def = db.modify_definition(op.args[1]);
          auto it = _deleted_access_chains.find(op.args[2]);
          if (it != _deleted_access_chains.end()) {
            op.args[2] = it->second;
            def._origin_id = op.args[2];
          }
          else if (_deleted.count(def._origin_id)) {
            // Origin points to deleted variable, change to proper variable.
            const Definition &from = db.get_definition(op.args[2]);
            def._origin_id = from._origin_id;
          }
        }
        nassertd(!_deleted.count(op.args[2])) continue;
        db.mark_used(op.args[1]);
        break;

      case spv::OpStore:
      case spv::OpAtomicStore:
      case spv::OpAtomicFlagClear:
        // Can't store the struct pointer itself (yet)
        {
          auto it = _deleted_access_chains.find(op.args[0]);
          if (it != _deleted_access_chains.end()) {
            op.args[0] = it->second;
          }
        }
        nassertd(!_deleted.count(op.args[0])) continue;
        db.mark_used(op.args[0]);
        break;

      case spv::OpCopyMemory:
      case spv::OpCopyMemorySized:
        // Shouldn't be copying into or out of the struct directly.
        for (size_t ai = 0; ai < 2; ++ai) {
          auto it = _deleted_access_chains.find(op.args[ai]);
          if (it != _deleted_access_chains.end()) {
            op.args[ai] = it->second;
          }
          nassertd(!_deleted.count(op.args[ai])) continue;
          db.mark_used(op.args[ai]);
        }
        break;

      case spv::OpArrayLength:
      case spv::OpConvertPtrToU:
        {
          auto it = _deleted_access_chains.find(op.args[2]);
          if (it != _deleted_access_chains.end()) {
            op.args[2] = it->second;
          }
        }
        nassertd(!_deleted.count(op.args[2])) continue;
        db.mark_used(op.args[2]);
        break;

      case spv::OpCopyObject:
      case spv::OpExpectKHR:
        {
          auto it = _deleted_access_chains.find(op.args[2]);
          if (it != _deleted_access_chains.end()) {
            op.args[2] = it->second;

            Definition &def = db.modify_definition(op.args[1]);
            def._origin_id = op.args[2];
            def._type_id = module.get_type_id(op.args[2]);

            // Copy the type since the storage class may have changed.
            op.args[0] = def._type_id;
          }
          else if (_deleted.count(op.args[2])) {
            // If it's just a copy of the struct pointer, delete the copy.
            uint32_t result_id = op.args[1];
            _deleted.insert(result_id);
            module.delete_id(result_id);
          }
        }
        break;

      case spv::OpBitcast:
        {
          auto it = _deleted_access_chains.find(op.args[2]);
          if (it != _deleted_access_chains.end()) {
            op.args[2] = it->second;
            db.modify_definition(op.args[1])._origin_id = op.args[2];
          }
        }
        nassertd(!_deleted.count(op.args[2])) continue;

        if (!db.get_definition(op.args[0]).is_pointer_type()) {
          db.mark_used(op.args[1]);
        }
        break;

      case spv::OpSelect:
        db.mark_used(op.args[3]);
        db.mark_used(op.args[4]);
        break;

      case spv::OpReturnValue:
        db.mark_used(op.args[0]);
        break;

      case spv::OpCopyLogical:
        // Can't copy pointers using this instruction.
        nassertd(!_deleted.count(op.args[2])) continue;
        nassertd(!_deleted_access_chains.count(op.args[2])) continue;
        break;

      case spv::OpPtrEqual:
      case spv::OpPtrNotEqual:
      case spv::OpPtrDiff:
        db.mark_used(op.args[2]);
        db.mark_used(op.args[3]);
        break;

      default:
        break;
      }
    }
  }
}
