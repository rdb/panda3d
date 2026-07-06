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

/**
 *
 */
void SpirVRemoveUnusedVariablesPass::
run(SpirVModule &module) {
  const SpirVResultDatabase &db = module.get_db();
  pmap<uint32_t, BitArray> ftype_params_used;
  pset<uint32_t> dead_ids;

  for (uint32_t id = 0; id < module.get_id_bound(); ++id) {
    const Definition &def = db.get_definition(id);

    if (def.is_variable() && !def.is_used()) {
      if (shader_cat.is_debug()) {
        shader_cat.debug()
          << "Removing unused variable " << def._name << " (" << id << ")\n";
      }
      module.delete_id(id);
      dead_ids.insert(id);
    }
    else if (def.is_function()) {
      for (size_t i = 0; i < def._parameters.size(); ++i) {
        uint32_t param_id = def._parameters[i];
        const Definition &param_def = db.get_definition(param_id);
        if (param_def.is_used() || !db.get_definition(param_def._type_id).is_pointer_type()) {
          ftype_params_used[def._type_id].set_bit(i);
        }
      }
    }
  }

  // Remove any access chains and pointer copies based on the deleted
  // variables (a chain that is never loaded or stored does not mark its
  // variable used, so these may exist).
  module.delete_dead_code(dead_ids);

  for (const auto &item : ftype_params_used) {
    uint32_t func_type_id = item.first;
    const Definition &def = db.get_definition(func_type_id);

    pset<uint32_t> deleted_params;
    for (size_t i = 0; i < def._parameters.size(); ++i) {
      if (!item.second.get_bit(i)) {
        if (shader_cat.is_debug()) {
          shader_cat.debug()
            << "Removing unused function parameter " << i
            << " from function type " << func_type_id << "\n";
        }
        deleted_params.insert((uint32_t)i);
      }
    }
    module.remove_function_parameters(func_type_id, deleted_params);
  }
}
