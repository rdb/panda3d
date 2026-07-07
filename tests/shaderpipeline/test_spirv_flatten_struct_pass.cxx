/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file test_spirv_flatten_struct_pass.cxx
 * @author rdb
 * @date 2026-07-02
 */

#include "spirVFlattenStructPass.h"

#include "spirv_test_utils.h"
#include "catch_amalgamated.hpp"

TEST_CASE("SpirVFlattenStructPass turns struct members into variables", "[shaderpipeline]") {
  // struct S { float a; vec2 b; }; uniform S s;  main() loads s.a.
  SpirVModule module = make_module();
  const ShaderType *vec2_type =
    ShaderType::register_type(ShaderType::Vector(ShaderType::ST_float, 2));

  ShaderType::Struct st;
  st.add_member(ShaderType::FLOAT, "a");
  st.add_member(vec2_type, "b");
  const ShaderType *struct_type = ShaderType::register_type(std::move(st));

  uint32_t id_struct = module.define_type(struct_type);
  module.set_name(id_struct, "S");
  module.set_member_name(id_struct, 0, "a");
  module.set_member_name(id_struct, 1, "b");

  uint32_t id_var = module.define_variable(struct_type, spv::StorageClassUniformConstant);
  module.set_name(id_var, "s");

  uint32_t id_float = module.define_type(ShaderType::FLOAT);
  uint32_t id_const_0 = module.define_int_constant(0);

  uint32_t main_id = add_main(module, spv::ExecutionModelFragment);
  {
    SpirVBuilder builder = builder_before_return(module, main_id);
    uint32_t chain = builder.op_access_chain(id_var, {id_const_0});
    builder.op_load(chain);
  }

  InstructionStream stream = module.emit();
  REQUIRE(stream.validate());

  SpirVTransformer transformer(stream);
  transformer.run(SpirVFlattenStructPass(id_struct));
  CHECK(transformer.validate());

  InstructionStream result = transformer.get_result();
  CHECK(result.validate());
  CHECK(count_op(result, spv::OpTypeStruct) == 0);
  CHECK(!has_variable(result, id_var));

  // The members have become individual variables, whose reflection names are
  // qualified with the struct variable name.
  const SpirVModule &out_module = transformer.get_module();
  uint32_t var_a = 0, var_b = 0;
  for (uint32_t id = 0; id < transformer.get_id_bound(); ++id) {
    if (out_module.get_reflection_name(id) == "s.a") {
      var_a = id;
    }
    else if (out_module.get_reflection_name(id) == "s.b") {
      var_b = id;
    }
  }
  REQUIRE(var_a != 0);
  REQUIRE(var_b != 0);
  CHECK(out_module.resolve_value_type(var_a) == ShaderType::FLOAT);
  CHECK(out_module.resolve_value_type(var_b) == vec2_type);
  CHECK(out_module.get_name(var_a) == "a");
  CHECK(out_module.get_name(var_b) == "b");

  // The access chain is gone; the load reads the new variable directly.
  CHECK(count_op(result, spv::OpAccessChain) == 0);
  CHECK(find_load_pointer(result, id_float) == var_a);
}
