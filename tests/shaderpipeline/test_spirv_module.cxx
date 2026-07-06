/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file test_spirv_module.cxx
 * @author rdb
 * @date 2026-07-07
 *
 * Tests for the contract of the SpirVModule IR layer itself: id deletion
 * (including entry point interface, name and annotation cleanup), function
 * parameter removal with call argument stripping, type replacement with
 * duplicate merging, struct member deletion with renumbering, the definition
 * helpers, and the emit-time topological sort.  Uses small custom passes
 * rather than the real transform passes, so that each mechanism is exercised
 * in isolation.
 */

#include "spirVTransformPass.h"
#include "spirVTransformer.h"

#include "spirv_test_utils.h"
#include "catch_amalgamated.hpp"

namespace {

/**
 * Deletes the given (unused) variables.
 */
class DeleteVariablesPass final : public SpirVTransformPass {
public:
  DeleteVariablesPass(pset<uint32_t> ids) : _ids(std::move(ids)) {}

  virtual void run(SpirVModule &module) override {
    pset<uint32_t> dead = _ids;
    for (uint32_t id : _ids) {
      module.delete_id(id);
    }
    module.delete_dead_code(dead);
  }

private:
  const pset<uint32_t> _ids;
};

/**
 * Removes the given parameters (given as pairs of function type id and
 * parameter index).
 */
class RemoveParametersPass final : public SpirVTransformPass {
public:
  RemoveParametersPass(pvector<std::pair<uint32_t, uint32_t> > params) :
    _params(std::move(params)) {}

  virtual void run(SpirVModule &module) override {
    pmap<uint32_t, pset<uint32_t> > grouped;
    for (const auto &pair : _params) {
      grouped[pair.first].insert(pair.second);
    }
    for (const auto &pair : grouped) {
      module.remove_function_parameters(pair.first, pair.second);
    }
  }

private:
  const pvector<std::pair<uint32_t, uint32_t> > _params;
};

/**
 * Replaces one image type with a fresh declaration that has a different
 * depth flag (like SpirVEmulateTextureQueriesPass does with cube shadow
 * samplers), relying on the deduplication to merge whatever downstream
 * declarations become identical as a result.
 */
class RetypeImagePass final : public SpirVTransformPass {
public:
  RetypeImagePass(uint32_t before) : _before(before) {}

  uint32_t _new_id = 0;

  virtual void run(SpirVModule &module) override {
    const SpirVModule::Instruction *decl = module.get_global(_before);
    REQUIRE(decl != nullptr);
    REQUIRE(decl->opcode == spv::OpTypeImage);
    _new_id = module.define_image_type(
      module.get_db().get_definition(_before)._type, 0, (*decl)[6],
      (spv::ImageFormat)(*decl)[7]);
    module.replace_type_id(_before, _new_id);
  }

private:
  const uint32_t _before;
};

/**
 * Deletes a struct member by (current) index.
 */
class DeleteMemberPass final : public SpirVTransformPass {
public:
  DeleteMemberPass(uint32_t type_id, uint32_t member_index) :
    _type_id(type_id), _member_index(member_index) {}

  virtual void run(SpirVModule &module) override {
    module.delete_struct_member(_type_id, _member_index);
  }

private:
  const uint32_t _type_id;
  const uint32_t _member_index;
};

/**
 * Calls define_int_constant, optionally after deleting an existing constant.
 */
class DefineConstantPass final : public SpirVTransformPass {
public:
  DefineConstantPass(int32_t value, uint32_t delete_id = 0) :
    _value(value), _delete_id(delete_id) {}

  uint32_t _const_id = 0;

  virtual void run(SpirVModule &module) override {
    if (_delete_id != 0) {
      module.delete_id(_delete_id);
    }
    _const_id = module.define_int_constant(_value);
  }

private:
  const int32_t _value;
  const uint32_t _delete_id;
};

/**
 * Builds a function with the given parameter types that returns its last
 * parameter (which must be of the return type), for testing parameter
 * removal.  The database is not updated; the caller round-trips the module
 * through emit() and a fresh parse.
 */
struct TestFunction {
  uint32_t type_id;
  uint32_t func_id;
  pvector<uint32_t> param_ids;
};

TestFunction
make_float_function(SpirVModule &module, const pvector<uint32_t> &param_type_ids) {
  uint32_t float_tid = module.define_type(ShaderType::FLOAT);

  TestFunction fn;
  fn.type_id = module.allocate_id();
  {
    SpirVModule::Instruction op(spv::OpTypeFunction, {fn.type_id, float_tid});
    op.args.insert(op.args.end(), param_type_ids.begin(), param_type_ids.end());
    module.add_global(std::move(op));
  }

  fn.func_id = module.allocate_id();
  module.modify_functions().push_back(SpirVModule::Function {fn.func_id, {}});

  SpirVBuilder builder(module, fn.func_id, 0);
  builder.insert(spv::OpFunction, {float_tid, fn.func_id, spv::FunctionControlMaskNone, fn.type_id});
  for (uint32_t param_type_id : param_type_ids) {
    uint32_t param_id = module.allocate_id();
    builder.insert(spv::OpFunctionParameter, {param_type_id, param_id});
    fn.param_ids.push_back(param_id);
  }
  builder.insert(spv::OpLabel, {module.allocate_id()});
  builder.insert(spv::OpReturnValue, {fn.param_ids.back()});
  builder.insert(spv::OpFunctionEnd, {});
  return fn;
}

}

TEST_CASE("SpirVModule prunes deleted variables from entry point interfaces", "[shaderpipeline]") {
  // Three unused inputs; the first two (adjacent in the interface) get
  // deleted.  Their declarations, names, decorations and interface entries
  // must all disappear.
  SpirVModule module = make_module();

  uint32_t id_in1 = module.define_variable(ShaderType::FLOAT, spv::StorageClassInput);
  uint32_t id_in2 = module.define_variable(ShaderType::FLOAT, spv::StorageClassInput);
  uint32_t id_in3 = module.define_variable(ShaderType::FLOAT, spv::StorageClassInput);
  module.set_name(id_in1, "in1");
  module.set_name(id_in2, "in2");
  module.set_name(id_in3, "in3");
  module.decorate(id_in1, spv::DecorationLocation, 0u);
  module.decorate(id_in2, spv::DecorationLocation, 1u);
  module.decorate(id_in3, spv::DecorationLocation, 2u);
  add_main(module, spv::ExecutionModelFragment, {id_in1, id_in2, id_in3});

  InstructionStream stream = module.emit();
  REQUIRE(stream.validate());

  SpirVTransformer transformer(stream);
  DeleteVariablesPass pass({id_in1, id_in2});
  transformer.run(pass);
  CHECK(transformer.validate_db());

  InstructionStream result = transformer.get_result();
  CHECK(result.validate());

  // The interface lists only in3, and the word count is consistent with that.
  Instruction ep = find_op(result, spv::OpEntryPoint);
  REQUIRE(ep.opcode == spv::OpEntryPoint);
  REQUIRE(ep.nargs == 5);  // model, main, "main" (two words), in3
  CHECK(ep.args[4] == id_in3);

  // The variables, their names and their decorations are gone as well.
  CHECK(count_op(result, spv::OpVariable) == 1);
  CHECK(has_variable(result, id_in3));
  CHECK(count_op(result, spv::OpName) == 1);
  CHECK(count_op(result, spv::OpDecorate) == 1);
}

TEST_CASE("SpirVModule strips call arguments when removing function parameters", "[shaderpipeline]") {
  // float f(sampler2D t, float x) and float g(float y).  Removing f's
  // sampler parameter makes f's function type identical to g's; the
  // deduplication merges the two.  The call to f must lose its sampler
  // argument, and the call to g (whose type the merge joins with f's, even
  // though g never had any parameters removed) must keep its argument.
  SpirVModule module = make_module();

  const ShaderType *sampler_type = ShaderType::register_type(
    ShaderType::SampledImage(Texture::TT_2d_texture, ShaderType::ST_float, false));
  uint32_t float_tid = module.define_type(ShaderType::FLOAT);
  uint32_t ptr_tid = module.define_pointer_type(sampler_type, spv::StorageClassUniformConstant);
  uint32_t id_tex = module.define_variable(sampler_type, spv::StorageClassUniformConstant);
  uint32_t id_c1 = module.define_float_constant(1.0f);

  TestFunction fn_f = make_float_function(module, {ptr_tid, float_tid});
  TestFunction fn_g = make_float_function(module, {float_tid});

  uint32_t id_main = add_main(module, spv::ExecutionModelFragment);
  uint32_t id_call1, id_call2;
  {
    SpirVBuilder builder = builder_before_return(module, id_main);
    id_call1 = module.allocate_id();
    builder.insert(spv::OpFunctionCall, {float_tid, id_call1, fn_f.func_id, id_tex, id_c1});
    id_call2 = module.allocate_id();
    builder.insert(spv::OpFunctionCall, {float_tid, id_call2, fn_g.func_id, id_c1});
  }

  InstructionStream stream = module.emit();
  REQUIRE(stream.validate());

  SpirVTransformer transformer(stream);
  RemoveParametersPass pass({{fn_f.type_id, 0}});
  transformer.run(pass);
  CHECK(transformer.validate_db());

  InstructionStream result = transformer.get_result();
  CHECK(result.validate());

  // The rebuilt type merged onto the identical other declaration.
  CHECK(count_op(result, spv::OpTypeFunction) == 2);  // void(), float(float)
  CHECK(count_op(result, spv::OpFunctionParameter) == 2);

  bool found_call1 = false, found_call2 = false;
  for (Instruction op : result) {
    if (op.opcode == spv::OpFunctionCall && op.args[1] == id_call1) {
      REQUIRE(op.nargs == 4);
      CHECK(op.args[2] == fn_f.func_id);
      CHECK(op.args[3] == id_c1);
      found_call1 = true;
    }
    else if (op.opcode == spv::OpFunctionCall && op.args[1] == id_call2) {
      REQUIRE(op.nargs == 4);
      CHECK(op.args[2] == fn_g.func_id);
      CHECK(op.args[3] == id_c1);
      found_call2 = true;
    }
  }
  CHECK(found_call1);
  CHECK(found_call2);

  // The parameter list recorded for f reflects the removal; g is untouched.
  const SpirVResultDatabase::Definition &f_def =
    transformer.get_db().get_definition(fn_f.func_id);
  REQUIRE(f_def._parameters.size() == 1);
  CHECK(f_def._parameters[0] == fn_f.param_ids[1]);

  const SpirVResultDatabase::Definition &g_def =
    transformer.get_db().get_definition(fn_g.func_id);
  REQUIRE(g_def._parameters.size() == 1);
  CHECK(g_def._parameters[0] == fn_g.param_ids[0]);
}

TEST_CASE("SpirVModule merges duplicates cascading from a type replacement", "[shaderpipeline]") {
  // A shadow sampler and a plain sampler of the same dimensionality.  The
  // pass replaces the shadow image type with a fresh non-depth image type,
  // which the deduplication merges with the plain image type; this cascades
  // through the sampled image and pointer types.  The replacement type is
  // declared at the end of the globals section while its first use precedes
  // it, so this also exercises the emit-time topological sort.
  SpirVModule module = make_module();

  const ShaderType *shadow_type = ShaderType::register_type(
    ShaderType::SampledImage(Texture::TT_2d_texture, ShaderType::ST_float, true));
  const ShaderType *plain_type = ShaderType::register_type(
    ShaderType::SampledImage(Texture::TT_2d_texture, ShaderType::ST_float, false));

  uint32_t id_tex_shadow = module.define_variable(shadow_type, spv::StorageClassUniformConstant);
  uint32_t id_tex_plain = module.define_variable(plain_type, spv::StorageClassUniformConstant);

  uint32_t id_main = add_main(module, spv::ExecutionModelFragment);
  {
    SpirVBuilder builder = builder_before_return(module, id_main);
    builder.op_load(id_tex_shadow);
    builder.op_load(id_tex_plain);
  }

  // Find the shadow image type declaration (the image operand of the shadow
  // sampled image type).
  uint32_t shadow_image_tid = module.get_db().get_definition(
    module.modify_db().find_type(shadow_type))._type_id;

  InstructionStream stream = module.emit();
  REQUIRE(stream.validate());
  REQUIRE(count_op(stream, spv::OpTypeImage) == 2);
  REQUIRE(count_op(stream, spv::OpTypeSampledImage) == 2);
  REQUIRE(count_op(stream, spv::OpTypePointer) == 2);

  SpirVTransformer transformer(stream);
  RetypeImagePass pass(shadow_image_tid);
  transformer.run(pass);
  CHECK(transformer.validate_db());

  InstructionStream result = transformer.get_result();
  CHECK(result.validate());

  // Everything merged down to a single image/sampled image/pointer type.
  CHECK(count_op(result, spv::OpTypeImage) == 1);
  CHECK(count_op(result, spv::OpTypeSampledImage) == 1);
  CHECK(count_op(result, spv::OpTypePointer) == 1);
  CHECK(count_op(result, spv::OpVariable) == 2);
  CHECK(count_op(result, spv::OpLoad) == 2);
}

TEST_CASE("SpirVModule renumbers member decorations when deleting a member", "[shaderpipeline]") {
  // A uniform block with three members; deleting the middle one must shift
  // the third member's name and offset decoration down.
  SpirVModule module = make_module();

  ShaderType::Struct block;
  block.add_member(ShaderType::FLOAT, "a");
  block.add_member(ShaderType::INT, "b");
  block.add_member(ShaderType::FLOAT, "c");
  const ShaderType *block_type = ShaderType::register_type(std::move(block));

  uint32_t id_block = module.define_variable(block_type, spv::StorageClassUniform);
  uint32_t block_tid = module.modify_db().find_type(block_type);
  REQUIRE(block_tid != 0);
  module.decorate(block_tid, spv::DecorationBlock);
  module.decorate(id_block, spv::DecorationBinding, 0u);
  module.decorate(id_block, spv::DecorationDescriptorSet, 0u);
  add_main(module, spv::ExecutionModelFragment);

  uint32_t offset_c = module.get_db().get_definition(block_tid)._members[2]._offset;

  InstructionStream stream = module.emit();
  REQUIRE(stream.validate());

  SpirVTransformer transformer(stream);
  DeleteMemberPass pass(block_tid, 1);
  transformer.run(pass);
  CHECK(transformer.validate_db());

  InstructionStream result = transformer.get_result();
  CHECK(result.validate());

  Instruction op = find_op(result, spv::OpTypeStruct);
  REQUIRE(op.opcode == spv::OpTypeStruct);
  CHECK(op.nargs == 3);  // result id + two members

  // The member names shifted down along with the members.
  const SpirVResultDatabase::Definition &def =
    transformer.get_db().get_definition(block_tid);
  REQUIRE(def._members.size() == 2);
  CHECK(def._members[0]._name == "a");
  CHECK(def._members[1]._name == "c");

  // Member decorations were renumbered, keeping c's original offset.
  int num_member_decorations = 0;
  for (Instruction ann : result) {
    if (ann.opcode == spv::OpMemberDecorate && ann.args[0] == block_tid) {
      ++num_member_decorations;
      REQUIRE(ann.args[1] <= 1);
      if (ann.args[1] == 1 && ann.args[2] == spv::DecorationOffset) {
        CHECK(ann.args[3] == (uint32_t)offset_c);
      }
    }
  }
  CHECK(num_member_decorations == 2);
}

TEST_CASE("SpirVModule reuses live constants but not deleted ones", "[shaderpipeline]") {
  SpirVModule module = make_module();

  uint32_t id_c7 = module.define_int_constant(7);
  uint32_t id_main = add_main(module, spv::ExecutionModelFragment);

  // Reference the constant so that it is not dead weight (not required, but
  // more realistic).
  uint32_t id_var = module.define_variable(ShaderType::INT, spv::StorageClassPrivate);
  {
    SpirVBuilder builder = builder_before_return(module, id_main);
    builder.op_store(id_var, id_c7);
  }

  InstructionStream stream = module.emit();
  REQUIRE(stream.validate());

  SECTION("an existing constant is reused") {
    SpirVTransformer transformer(stream);
    DefineConstantPass pass(7);
    transformer.run(pass);
    CHECK(transformer.validate_db());
    CHECK(pass._const_id == id_c7);

    InstructionStream result = transformer.get_result();
    CHECK(result.validate());
    CHECK(count_op(result, spv::OpConstant) == 1);
  }

  SECTION("a deleted constant is never handed back out") {
    // Build a variant module in which the constant is unused, so that the
    // pass may delete it before requesting a constant with the same value.
    SpirVModule module2 = make_module();
    uint32_t id_unused = module2.define_int_constant(7);
    add_main(module2, spv::ExecutionModelFragment);

    InstructionStream stream2 = module2.emit();
    REQUIRE(stream2.validate());

    SpirVTransformer transformer(stream2);
    DefineConstantPass pass(7, id_unused);
    transformer.run(pass);
    CHECK(transformer.validate_db());
    CHECK(pass._const_id != id_unused);
    CHECK(pass._const_id != 0);

    InstructionStream result = transformer.get_result();
    CHECK(result.validate());
    CHECK(count_op(result, spv::OpConstant) == 1);
  }
}

TEST_CASE("SpirVModule round-trips a module through parse and emit", "[shaderpipeline]") {
  // Parse-emit round trip: the metadata sections are reconstructed from the
  // materialized state, so this checks that nothing is lost along the way.
  SpirVModule module = make_module();

  const ShaderType *vec4_type = ShaderType::register_type(ShaderType::Vector(ShaderType::ST_float, 4));
  uint32_t id_in = module.define_variable(vec4_type, spv::StorageClassInput);
  uint32_t id_out = module.define_variable(vec4_type, spv::StorageClassOutput);
  module.set_name(id_in, "color_in");
  module.set_name(id_out, "color_out");
  module.decorate(id_in, spv::DecorationLocation, 3u);
  module.decorate(id_out, spv::DecorationLocation, 0u);
  uint32_t id_main = add_main(module, spv::ExecutionModelFragment, {id_in, id_out});
  {
    SpirVBuilder builder = builder_before_return(module, id_main);
    builder.op_store(id_out, builder.op_load(id_in));
  }

  InstructionStream stream = module.emit();
  REQUIRE(stream.validate());

  SpirVModule reparsed(stream);
  InstructionStream stream2 = reparsed.emit();
  REQUIRE(stream2.validate());

  CHECK(stream.get_data_size() == stream2.get_data_size());
  CHECK(count_op(stream2, spv::OpName) == 2);
  CHECK(count_op(stream2, spv::OpDecorate) == 2);
  CHECK(count_op(stream2, spv::OpVariable) == 2);
  CHECK(count_op(stream2, spv::OpLoad) == 1);
  CHECK(count_op(stream2, spv::OpStore) == 1);

  const SpirVResultDatabase::Definition &def = reparsed.get_db().get_definition(id_in);
  CHECK(def._name == "color_in");
  CHECK(def._location == 3);
  CHECK(def._type == vec4_type);
  CHECK(def._storage_class == spv::StorageClassInput);
}
