/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file spirVRemoveUnusedVariablesPass.cxx
 * @author rdb
 * @date 2024-10-08
 */

#include "spirVRemoveUnusedVariablesPass.h"
#include "spirVUsageAnalysis.h"

/**
 *
 */
void SpirVRemoveUnusedVariablesPass::
run(SpirVModule &module) {
  SpirVUsageAnalysis usage(module);

  pmap<Id, BitArray> ftype_params_used;
  pset<Id> dead_ids;

  for (uint32_t word = 0; word < module.get_id_bound(); ++word) {
    Id id(word);
    if (module.get_kind(id) == IdKind::VARIABLE && !usage.is_used(id)) {
      if (shader_cat.is_debug()) {
        shader_cat.debug()
          << "Removing unused variable " << module.get_name(id)
          << " (" << id << ")\n";
      }
      module.delete_id(id);
      dead_ids.insert(id);
    }
  }

  for (const Function &function : module.get_functions()) {
    Id func_type_id = module.get_type_id(function.id);
    pvector<Id> params = module.get_function_parameters(function.id);
    for (size_t i = 0; i < params.size(); ++i) {
      // Only unused pointer parameters can be removed; a value parameter may
      // not simply have its argument dropped without changing semantics.
      if (usage.is_used(params[i]) ||
          module.get_kind(module.get_type_id(params[i])) != IdKind::POINTER_TYPE) {
        ftype_params_used[func_type_id].set_bit(i);
      }
    }
  }

  // Remove any access chains and pointer copies based on the deleted
  // variables (a chain that is never loaded or stored does not mark its
  // variable used, so these may exist).
  module.delete_dead_code(dead_ids);

  for (const auto &item : ftype_params_used) {
    Id func_type_id = item.first;
    uint32_t num_params = 0;
    {
      const Instruction *decl = module.get_decl(func_type_id);
      nassertd(decl != nullptr && decl->opcode == spv::OpTypeFunction) continue;
      num_params = (uint32_t)decl->args.size() - 2;
    }

    pset<uint32_t> deleted_params;
    for (uint32_t i = 0; i < num_params; ++i) {
      if (!item.second.get_bit(i)) {
        if (shader_cat.is_debug()) {
          shader_cat.debug()
            << "Removing unused function parameter " << i
            << " from function type " << func_type_id << "\n";
        }
        deleted_params.insert(i);
      }
    }
    module.remove_function_parameters(func_type_id, deleted_params);
  }
}
