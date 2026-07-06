/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file test_spirv_remove_unused_variables_pass.cxx
 * @author rdb
 * @date 2026-07-02
 */

#include "spirVRemoveUnusedVariablesPass.h"

#include "spirv_test_utils.h"
#include "catch_amalgamated.hpp"

TEST_CASE("SpirVRemoveUnusedVariablesPass removes unused variables", "[shaderpipeline]") {
  // in vec4 in_color; out vec4 out_color; uniform float unused;
  // main() { out_color = in_color; }
  SpirVModule module = make_module();
  const ShaderType *vec4_type =
    ShaderType::register_type(ShaderType::Vector(ShaderType::ST_float, 4));

  uint32_t id_in_color = module.define_variable(vec4_type, spv::StorageClassInput);
  module.decorate(id_in_color, spv::DecorationLocation, 0u);
  uint32_t id_out_color = module.define_variable(vec4_type, spv::StorageClassOutput);
  module.decorate(id_out_color, spv::DecorationLocation, 0u);
  uint32_t id_unused = module.define_variable(ShaderType::FLOAT, spv::StorageClassUniformConstant);
  module.set_name(id_unused, "unused");

  uint32_t main_id = add_main(module, spv::ExecutionModelFragment,
                              {id_in_color, id_out_color});
  {
    SpirVBuilder builder = builder_before_return(module, main_id);
    uint32_t tmp = builder.op_load(id_in_color);
    builder.op_store(id_out_color, tmp);
  }

  InstructionStream stream = module.emit();
  REQUIRE(stream.validate());

  SpirVTransformer transformer(stream);
  transformer.run(SpirVRemoveUnusedVariablesPass());
  CHECK(transformer.validate_db());

  InstructionStream result = transformer.get_result();
  CHECK(result.validate());
  CHECK(!has_variable(result, id_unused));
  CHECK(has_variable(result, id_in_color));
  CHECK(has_variable(result, id_out_color));
  CHECK(!transformer.get_db().get_definition(id_unused).is_variable());
}
