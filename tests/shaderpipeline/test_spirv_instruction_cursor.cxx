/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file test_spirv_instruction_cursor.cxx
 * @author rdb
 * @date 2026-07-07
 *
 * Tests for SpirVInstructionCursor: replacing an instruction with a sequence
 * whose final value takes over the original result id, inserting code before
 * an instruction, and removing instructions, all while keeping the module's
 * id index consistent (checked through SpirVTransformer::validate).
 */

#include "spirVInstructionCursor.h"
#include "spirVTransformPass.h"
#include "spirVTransformer.h"

#include "spirv_test_utils.h"
#include "catch_amalgamated.hpp"

namespace {

/**
 * Replaces every OpLoad with a load followed by a negation, the negation
 * taking over the load's original result id.
 */
class NegateLoadsPass final : public SpirVTransformPass {
public:
  virtual void run(SpirVModule &module) override {
    for (Function &function : module.modify_functions()) {
      SpirVInstructionCursor cursor(module, function);
      while (cursor.next()) {
        if (cursor->opcode == spv::OpLoad) {
          uint32_t var_id = cursor->args[2];
          cursor.replace([&](SpirVBuilder &builder) -> uint32_t {
            return builder.op_negate(builder.op_load(var_id));
          });
        }
      }
    }
  }
};

/**
 * Replaces every OpLoad with a load followed by a conversion that turns out
 * to be a no-op, so that the returned id is that of the load itself.  This
 * exercises the renumbering when the replacement collapses to a single
 * instruction.
 */
class NoOpConvertLoadsPass final : public SpirVTransformPass {
public:
  virtual void run(SpirVModule &module) override {
    for (Function &function : module.modify_functions()) {
      SpirVInstructionCursor cursor(module, function);
      while (cursor.next()) {
        if (cursor->opcode == spv::OpLoad) {
          uint32_t var_id = cursor->args[2];
          cursor.replace([&](SpirVBuilder &builder) -> uint32_t {
            uint32_t temp = builder.op_load(var_id);
            // Converting float to float returns temp unchanged.
            return builder.op_convert(ShaderType::ST_float, temp);
          });
        }
      }
    }
  }
};

/**
 * Replaces every OpReturn with an OpKill, exercising the replacement of an
 * instruction without a result id.
 */
class KillReturnsPass final : public SpirVTransformPass {
public:
  virtual void run(SpirVModule &module) override {
    for (Function &function : module.modify_functions()) {
      SpirVInstructionCursor cursor(module, function);
      while (cursor.next()) {
        if (cursor->opcode == spv::OpReturn) {
          cursor.replace([](SpirVBuilder &builder) -> uint32_t {
            builder.op_kill();
            return 0u;
          });
        }
      }
    }
  }
};

/**
 * Inserts a store of one variable into another before every OpReturn, and
 * counts the instructions visited (to check that inserted code is skipped).
 */
class StoreBeforeReturnPass final : public SpirVTransformPass {
public:
  StoreBeforeReturnPass(uint32_t from, uint32_t to) : _from(from), _to(to) {}

  virtual void run(SpirVModule &module) override {
    for (Function &function : module.modify_functions()) {
      SpirVInstructionCursor cursor(module, function);
      while (cursor.next()) {
        ++_num_visited;
        if (cursor->opcode == spv::OpReturn) {
          cursor.insert_before([&](SpirVBuilder &builder) {
            builder.op_store(_to, builder.op_load(_from));
          });
          // The cursor must still be on the OpReturn.
          REQUIRE(cursor->opcode == spv::OpReturn);
        }
      }
    }
  }

  uint32_t _from, _to;
  int _num_visited = 0;
};

/**
 * Removes every instruction with the given opcode.
 */
class RemoveOpcodePass final : public SpirVTransformPass {
public:
  RemoveOpcodePass(spv::Op opcode) : _opcode(opcode) {}

  virtual void run(SpirVModule &module) override {
    for (Function &function : module.modify_functions()) {
      SpirVInstructionCursor cursor(module, function);
      while (cursor.next()) {
        if (cursor->opcode == _opcode) {
          cursor.remove();
        }
      }
    }
  }

private:
  const spv::Op _opcode;
};

}

TEST_CASE("SpirVInstructionCursor replaces an instruction, keeping its result id", "[shaderpipeline]") {
  // main() { b = a; }
  SpirVModule module = make_module();
  uint32_t var_a = module.define_variable(ShaderType::FLOAT, spv::StorageClassPrivate);
  uint32_t var_b = module.define_variable(ShaderType::FLOAT, spv::StorageClassPrivate);
  uint32_t main_id = add_main(module);
  uint32_t loaded;
  {
    SpirVBuilder builder = builder_before_return(module, main_id);
    loaded = builder.op_load(var_a);
    builder.op_store(var_b, loaded);
  }

  InstructionStream stream = module.emit();
  REQUIRE(stream.validate());

  SpirVTransformer transformer(stream);
  transformer.run(NegateLoadsPass());
  CHECK(transformer.validate());

  InstructionStream result = transformer.get_result();
  CHECK(result.validate());

  // The negation took over the load's result id, so the store still reads
  // the same id, which now holds the negated value.
  CHECK(count_op(result, spv::OpLoad) == 1);
  CHECK(count_op(result, spv::OpFNegate) == 1);
  Instruction negate = find_op(result, spv::OpFNegate);
  CHECK(negate.args[1] == loaded);
  Instruction load = find_op(result, spv::OpLoad);
  CHECK(load.args[1] != loaded);
  CHECK(negate.args[2] == load.args[1]);
  Instruction store = find_op(result, spv::OpStore);
  CHECK(store.args[1] == loaded);
}

TEST_CASE("SpirVInstructionCursor handles a replacement that collapses to one op", "[shaderpipeline]") {
  SpirVModule module = make_module();
  uint32_t var_a = module.define_variable(ShaderType::FLOAT, spv::StorageClassPrivate);
  uint32_t var_b = module.define_variable(ShaderType::FLOAT, spv::StorageClassPrivate);
  uint32_t main_id = add_main(module);
  uint32_t loaded;
  {
    SpirVBuilder builder = builder_before_return(module, main_id);
    loaded = builder.op_load(var_a);
    builder.op_store(var_b, loaded);
  }

  SpirVTransformer transformer(module.emit());
  transformer.run(NoOpConvertLoadsPass());
  CHECK(transformer.validate());

  InstructionStream result = transformer.get_result();
  CHECK(result.validate());

  // The replacement load itself was renumbered to the original result id.
  CHECK(count_op(result, spv::OpLoad) == 1);
  Instruction load = find_op(result, spv::OpLoad);
  CHECK(load.args[1] == loaded);
  Instruction store = find_op(result, spv::OpStore);
  CHECK(store.args[1] == loaded);
}

TEST_CASE("SpirVInstructionCursor replaces an instruction without a result id", "[shaderpipeline]") {
  SpirVModule module = make_module();
  uint32_t main_id = add_main(module);
  (void)main_id;

  SpirVTransformer transformer(module.emit());
  transformer.run(KillReturnsPass());
  CHECK(transformer.validate());

  InstructionStream result = transformer.get_result();
  CHECK(result.validate());

  CHECK(count_op(result, spv::OpReturn) == 0);
  CHECK(count_op(result, spv::OpKill) == 1);
}

TEST_CASE("SpirVInstructionCursor does not visit code inserted before the cursor", "[shaderpipeline]") {
  SpirVModule module = make_module();
  uint32_t var_a = module.define_variable(ShaderType::FLOAT, spv::StorageClassPrivate);
  uint32_t var_b = module.define_variable(ShaderType::FLOAT, spv::StorageClassPrivate);
  uint32_t main_id = add_main(module);
  (void)main_id;

  // The function is OpFunction, OpLabel, OpReturn, OpFunctionEnd.
  SpirVTransformer transformer(module.emit());
  StoreBeforeReturnPass pass(var_a, var_b);
  transformer.run(pass);
  CHECK(transformer.validate());

  // The inserted OpLoad and OpStore were not themselves visited.
  CHECK(pass._num_visited == 4);

  InstructionStream result = transformer.get_result();
  CHECK(result.validate());
  CHECK(count_op(result, spv::OpStore) == 1);
  CHECK(count_op(result, spv::OpLoad) == 1);
}

TEST_CASE("SpirVInstructionCursor removes instructions and clears their ids", "[shaderpipeline]") {
  // main() { float unused = -a; b = a; }
  SpirVModule module = make_module();
  uint32_t var_a = module.define_variable(ShaderType::FLOAT, spv::StorageClassPrivate);
  uint32_t var_b = module.define_variable(ShaderType::FLOAT, spv::StorageClassPrivate);
  uint32_t main_id = add_main(module);
  {
    SpirVBuilder builder = builder_before_return(module, main_id);
    uint32_t loaded = builder.op_load(var_a);
    builder.op_negate(loaded);
    builder.op_store(var_b, loaded);
  }

  SpirVTransformer transformer(module.emit());
  transformer.run(RemoveOpcodePass(spv::OpFNegate));
  CHECK(transformer.validate());

  InstructionStream result = transformer.get_result();
  CHECK(result.validate());
  CHECK(count_op(result, spv::OpFNegate) == 0);
  CHECK(count_op(result, spv::OpLoad) == 1);
  CHECK(count_op(result, spv::OpStore) == 1);
}
