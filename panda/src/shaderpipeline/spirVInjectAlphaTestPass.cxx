/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file spirVInjectAlphaTestPass.cxx
 * @author rdb
 * @date 2024-11-10
 */

#include "spirVInjectAlphaTestPass.h"

/**
 *
 */
void SpirVInjectAlphaTestPass::
run(SpirVModule &module) {
  if (_mode == M_always || _mode == M_none) {
    // You probably needn't have bothered running this pass...
    return;
  }

  spv::Op opcode = spv::OpNop;
  switch (_mode) {
  case M_never:
    break;
  case M_less:
    opcode = spv::OpFOrdGreaterThanEqual;
    break;
  case M_equal:
    opcode = spv::OpFUnordNotEqual;
    break;
  case M_less_equal:
    opcode = spv::OpFOrdGreaterThan;
    break;
  case M_greater:
    opcode = spv::OpFOrdLessThanEqual;
    break;
  case M_not_equal:
    opcode = spv::OpFOrdEqual;
    break;
  case M_greater_equal:
    opcode = spv::OpFOrdLessThan;
    break;
  default:
    nassertv(false);
    return;
  }

  // For each fragment entry point, find the output variable with location 0.
  pmap<uint32_t, uint32_t> entry_points;
  for (const EntryPoint &ep : module.get_entry_points()) {
    if (ep.model == spv::ExecutionModelFragment) {
      for (uint32_t var_id : ep.interface_vars) {
        if (module.get_location(var_id) == 0) {
          entry_points[ep.function_id] = var_id;
          break;
        }
      }
    }
  }

  for (const auto &item : entry_points) {
    Function *function = module.get_function(item.first);
    nassertd(function != nullptr) continue;
    uint32_t var_id = item.second;

    // There may be multiple returns.  Insert an alpha test before every
    // return statement.
    for (size_t i = 0; i < function->instructions.size(); ++i) {
      spv::Op ret_opcode = function->instructions[i].opcode;
      if (ret_opcode != spv::OpReturn && ret_opcode != spv::OpReturnValue) {
        continue;
      }

      SpirVBuilder builder(module, function->id, i);

      if (_mode == M_never) {
        // Replace the OpReturn with an OpKill.
        builder.op_kill();
        function->instructions.erase(
          function->instructions.begin() + builder.get_insertion_index());
        i = builder.get_insertion_index() - 1;
        continue;
      }

      if (_alpha_ref_var_id == 0) {
        if (_spec_constant) {
          _alpha_ref_var_id = module.define_spec_constant(ShaderType::FLOAT, 0);
          if (_ref_location >= 0) {
            module.decorate(_alpha_ref_var_id, spv::DecorationSpecId, (uint32_t)_ref_location);
          }
        } else {
          _alpha_ref_var_id = module.define_variable(ShaderType::FLOAT, spv::StorageClassUniformConstant);
          if (_ref_location >= 0) {
            module.decorate(_alpha_ref_var_id, spv::DecorationLocation, (uint32_t)_ref_location);
          }
        }
      }
      uint32_t alpha = builder.op_load(builder.op_access_chain(var_id, {module.define_int_constant(3)}));
      uint32_t ref = _spec_constant ? _alpha_ref_var_id : builder.op_load(_alpha_ref_var_id);

      uint32_t cond = builder.op_compare(opcode, alpha, ref);
      _compare_op_ids.push_back(cond);

      uint32_t branch = builder.branch_if(cond);
        builder.op_kill();
      builder.branch_endif(branch);

      // Continue past the original return, which follows the injected code.
      i = builder.get_insertion_index();
    }
  }
}
