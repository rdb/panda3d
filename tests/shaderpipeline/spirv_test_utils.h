/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file spirv_test_utils.h
 * @author rdb
 * @date 2026-07-02
 */

#ifndef SPIRV_TEST_UTILS_H
#define SPIRV_TEST_UTILS_H

#include "spirVTransformer.h"
#include "spirVBuilder.h"
#include "shaderType.h"

using Instruction = ShaderModuleSpirV::Instruction;
using InstructionStream = ShaderModuleSpirV::InstructionStream;

/**
 * Creates an empty module with the standard capability and memory model.
 */
inline SpirVModule
make_module() {
  SpirVModule module;
  module.add_capability(spv::CapabilityShader);
  module.set_memory_model(spv::AddressingModelLogical, spv::MemoryModelGLSL450);
  return module;
}

/**
 * Adds an empty main function (just a label and a return) with an entry
 * point for the given execution model, and returns the function id.  The
 * given variables are added to the entry point interface.
 */
inline uint32_t
add_main(SpirVModule &module, spv::ExecutionModel model = spv::ExecutionModelFragment,
         pvector<uint32_t> interface_vars = {}, const char *name = "main") {
  SpirVBuilder builder(module);
  uint32_t function_id = builder.op_function(nullptr);
  builder.op_label();
  builder.op_return();
  builder.op_function_end();
  module.add_entry_point(model, function_id, name, std::move(interface_vars));
  if (model == spv::ExecutionModelFragment) {
    module.add_execution_mode(function_id, spv::ExecutionModeOriginUpperLeft);
  }
  return function_id;
}

/**
 * Returns a builder whose cursor is positioned just before the final
 * OpReturn of the given function, for adding code to a main function made
 * with add_main.
 */
inline SpirVBuilder
builder_before_return(SpirVModule &module, uint32_t function_id) {
  SpirVModule::Function *function = module.get_function(function_id);
  size_t index = function->instructions.size();
  while (index > 0 && function->instructions[index - 1].opcode != spv::OpReturn) {
    --index;
  }
  return SpirVBuilder(module, function_id, index > 0 ? index - 1 : 0);
}

/**
 * Counts the instructions with the given opcode.
 */
inline int
count_op(InstructionStream &stream, spv::Op opcode) {
  int count = 0;
  for (Instruction op : stream) {
    if (op.opcode == opcode) {
      ++count;
    }
  }
  return count;
}

/**
 * Returns the index'th instruction with the given opcode.  The returned
 * Instruction points into the stream, which must outlive it.  Returns an
 * OpNop instruction if there was no match.
 */
inline Instruction
find_op(InstructionStream &stream, spv::Op opcode, int index = 0) {
  for (Instruction op : stream) {
    if (op.opcode == opcode && index-- == 0) {
      return op;
    }
  }
  return Instruction {spv::OpNop, 0, nullptr};
}

/**
 * Returns true if the module still defines a variable with this id.
 */
inline bool
has_variable(InstructionStream &stream, uint32_t id) {
  for (Instruction op : stream) {
    if (op.opcode == spv::OpVariable && op.args[1] == id) {
      return true;
    }
  }
  return false;
}

/**
 * Returns the pointer operand of the only OpLoad with the given result type.
 */
inline uint32_t
find_load_pointer(InstructionStream &stream, uint32_t type_id) {
  for (Instruction op : stream) {
    if (op.opcode == spv::OpLoad && op.args[0] == type_id) {
      return op.args[2];
    }
  }
  return 0;
}

#endif
