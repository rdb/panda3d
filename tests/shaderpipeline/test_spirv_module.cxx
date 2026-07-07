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
  DeleteVariablesPass(pset<Id> ids) : _ids(std::move(ids)) {}

  virtual void run(SpirVModule &module) override {
    pset<Id> dead = _ids;
    for (Id id : _ids) {
      module.delete_id(id);
    }
    module.delete_dead_code(dead);
  }

private:
  const pset<Id> _ids;
};

/**
 * Removes the given parameters (given as pairs of function type id and
 * parameter index).
 */
class RemoveParametersPass final : public SpirVTransformPass {
public:
  RemoveParametersPass(pvector<std::pair<Id, uint32_t> > params) :
    _params(std::move(params)) {}

  virtual void run(SpirVModule &module) override {
    pmap<Id, pset<uint32_t> > grouped;
    for (const auto &pair : _params) {
      grouped[pair.first].insert(pair.second);
    }
    for (const auto &pair : grouped) {
      module.remove_function_parameters(pair.first, pair.second);
    }
  }

private:
  const pvector<std::pair<Id, uint32_t> > _params;
};

/**
 * Replaces one image type with a fresh declaration that has a different
 * depth flag (like SpirVEmulateTextureQueriesPass does with cube shadow
 * samplers), relying on the deduplication to merge whatever downstream
 * declarations become identical as a result.
 */
class RetypeImagePass final : public SpirVTransformPass {
public:
  RetypeImagePass(Id before) : _before(before) {}

  Id _new_id;

  virtual void run(SpirVModule &module) override {
    const SpirVModule::Instruction *decl = module.get_global(_before);
    REQUIRE(decl != nullptr);
    REQUIRE(decl->opcode == spv::OpTypeImage);
    uint32_t sampled = (*decl)[6];
    spv::ImageFormat format = (spv::ImageFormat)(*decl)[7];
    _new_id = module.define_image_type(
      module.resolve_type(_before), 0, sampled, format);
    module.replace_type_id(_before, _new_id);
  }

private:
  const Id _before;
};

/**
 * Deletes a struct member by (current) index.
 */
class DeleteMemberPass final : public SpirVTransformPass {
public:
  DeleteMemberPass(Id type_id, uint32_t member_index) :
    _type_id(type_id), _member_index(member_index) {}

  virtual void run(SpirVModule &module) override {
    module.delete_struct_member(_type_id, _member_index);
  }

private:
  const Id _type_id;
  const uint32_t _member_index;
};

/**
 * Calls define_int_constant, optionally after deleting an existing constant.
 */
class DefineConstantPass final : public SpirVTransformPass {
public:
  DefineConstantPass(int32_t value, Id delete_id = Id()) :
    _value(value), _delete_id(delete_id) {}

  Id _const_id;

  virtual void run(SpirVModule &module) override {
    if (_delete_id != 0) {
      module.delete_id(_delete_id);
    }
    _const_id = module.define_int_constant(_value);
  }

private:
  const int32_t _value;
  const Id _delete_id;
};

/**
 * Builds a function with the given parameter types that returns its last
 * parameter (which must be of the return type), for testing parameter
 * removal.
 */
struct TestFunction {
  Id type_id;
  Id func_id;
  pvector<Id> param_ids;
};

TestFunction
make_float_function(SpirVModule &module, const pvector<Id> &param_type_ids) {
  Id float_tid = module.define_type(ShaderType::FLOAT);

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
  for (Id param_type_id : param_type_ids) {
    Id param_id = module.allocate_id();
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

  Id id_in1 = module.define_variable(ShaderType::FLOAT, spv::StorageClassInput);
  Id id_in2 = module.define_variable(ShaderType::FLOAT, spv::StorageClassInput);
  Id id_in3 = module.define_variable(ShaderType::FLOAT, spv::StorageClassInput);
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
  CHECK(transformer.validate());

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
  Id float_tid = module.define_type(ShaderType::FLOAT);
  Id ptr_tid = module.define_pointer_type(sampler_type, spv::StorageClassUniformConstant);
  Id id_tex = module.define_variable(sampler_type, spv::StorageClassUniformConstant);
  Id id_c1 = module.define_float_constant(1.0f);

  TestFunction fn_f = make_float_function(module, {ptr_tid, float_tid});
  TestFunction fn_g = make_float_function(module, {float_tid});

  Id id_main = add_main(module, spv::ExecutionModelFragment);
  Id id_call1, id_call2;
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
  CHECK(transformer.validate());

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

  // The parameter list of f reflects the removal; g is untouched.
  pvector<Id> f_params =
    transformer.get_module().get_function_parameters(fn_f.func_id);
  REQUIRE(f_params.size() == 1);
  CHECK(f_params[0] == fn_f.param_ids[1]);

  pvector<Id> g_params =
    transformer.get_module().get_function_parameters(fn_g.func_id);
  REQUIRE(g_params.size() == 1);
  CHECK(g_params[0] == fn_g.param_ids[0]);
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

  Id id_tex_shadow = module.define_variable(shadow_type, spv::StorageClassUniformConstant);
  Id id_tex_plain = module.define_variable(plain_type, spv::StorageClassUniformConstant);

  Id id_main = add_main(module, spv::ExecutionModelFragment);
  {
    SpirVBuilder builder = builder_before_return(module, id_main);
    builder.op_load(id_tex_shadow);
    builder.op_load(id_tex_plain);
  }

  // Find the shadow image type declaration (the image operand of the shadow
  // sampled image type).
  Id shadow_image_tid = module.get_type_id(module.find_type(shadow_type));

  InstructionStream stream = module.emit();
  REQUIRE(stream.validate());
  REQUIRE(count_op(stream, spv::OpTypeImage) == 2);
  REQUIRE(count_op(stream, spv::OpTypeSampledImage) == 2);
  REQUIRE(count_op(stream, spv::OpTypePointer) == 2);

  SpirVTransformer transformer(stream);
  RetypeImagePass pass(shadow_image_tid);
  transformer.run(pass);
  CHECK(transformer.validate());

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

  Id id_block = module.define_variable(block_type, spv::StorageClassUniform);
  Id block_tid = module.find_type(block_type);
  REQUIRE(block_tid != 0);
  module.decorate(block_tid, spv::DecorationBlock);
  module.decorate(id_block, spv::DecorationBinding, 0u);
  module.decorate(id_block, spv::DecorationDescriptorSet, 0u);
  add_main(module, spv::ExecutionModelFragment);

  int offset_c = module.get_member_offset(block_tid, 2);

  InstructionStream stream = module.emit();
  REQUIRE(stream.validate());

  SpirVTransformer transformer(stream);
  DeleteMemberPass pass(block_tid, 1);
  transformer.run(pass);
  CHECK(transformer.validate());

  InstructionStream result = transformer.get_result();
  CHECK(result.validate());

  Instruction op = find_op(result, spv::OpTypeStruct);
  REQUIRE(op.opcode == spv::OpTypeStruct);
  CHECK(op.nargs == 3);  // result id + two members

  // The member names shifted down along with the members.
  const SpirVModule &out_module = transformer.get_module();
  REQUIRE(out_module.get_num_members(block_tid) == 2);
  CHECK(out_module.get_member_name(block_tid, 0) == "a");
  CHECK(out_module.get_member_name(block_tid, 1) == "c");

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

  Id id_c7 = module.define_int_constant(7);
  Id id_main = add_main(module, spv::ExecutionModelFragment);

  // Reference the constant so that it is not dead weight (not required, but
  // more realistic).
  Id id_var = module.define_variable(ShaderType::INT, spv::StorageClassPrivate);
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
    CHECK(transformer.validate());
    CHECK(pass._const_id == id_c7);

    InstructionStream result = transformer.get_result();
    CHECK(result.validate());
    CHECK(count_op(result, spv::OpConstant) == 1);
  }

  SECTION("a deleted constant is never handed back out") {
    // Build a variant module in which the constant is unused, so that the
    // pass may delete it before requesting a constant with the same value.
    SpirVModule module2 = make_module();
    Id id_unused = module2.define_int_constant(7);
    add_main(module2, spv::ExecutionModelFragment);

    InstructionStream stream2 = module2.emit();
    REQUIRE(stream2.validate());

    SpirVTransformer transformer(stream2);
    DefineConstantPass pass(7, id_unused);
    transformer.run(pass);
    CHECK(transformer.validate());
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
  Id id_in = module.define_variable(vec4_type, spv::StorageClassInput);
  Id id_out = module.define_variable(vec4_type, spv::StorageClassOutput);
  module.set_name(id_in, "color_in");
  module.set_name(id_out, "color_out");
  module.decorate(id_in, spv::DecorationLocation, 3u);
  module.decorate(id_out, spv::DecorationLocation, 0u);
  Id id_main = add_main(module, spv::ExecutionModelFragment, {id_in, id_out});
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

  CHECK(reparsed.get_name(id_in) == "color_in");
  CHECK(reparsed.get_location(id_in) == 3);
  CHECK(reparsed.resolve_value_type(id_in) == vec4_type);
  CHECK(reparsed.get_storage_class(id_in) == spv::StorageClassInput);
}

TEST_CASE("SpirVModule classifies OpSpecConstantOp as a spec constant", "[shaderpipeline]") {
  // An array sized by a spec constant expression (2 * spec constant), which
  // is the shape glslang emits for spec-constant-sized arrays.  Parsing and
  // resolving such a module must not trip any assertion: the expression is a
  // SPEC_CONSTANT whose value resolve_constant reports as 0.
  SpirVModule module = make_module();

  Id id_spec = module.define_spec_constant(ShaderType::INT, 4);
  module.decorate(id_spec, spv::DecorationSpecId, 12u);

  Id int_tid = module.define_type(ShaderType::INT);
  Id id_c2 = module.define_int_constant(2);
  Id id_size = module.allocate_id();
  module.add_global(SpirVModule::Instruction(spv::OpSpecConstantOp,
    {int_tid, id_size, (uint32_t)spv::OpIMul, id_spec, id_c2}));

  const ShaderType *float_type = ShaderType::FLOAT;
  Id float_tid = module.define_type(float_type);
  Id array_tid = module.allocate_id();
  module.add_global(SpirVModule::Instruction(spv::OpTypeArray,
    {array_tid, float_tid, id_size}));

  Id ptr_tid = module.define_pointer_type(array_tid, spv::StorageClassPrivate);
  Id var_id = module.allocate_id();
  module.add_global(SpirVModule::Instruction(spv::OpVariable,
    {ptr_tid, var_id, (uint32_t)spv::StorageClassPrivate}));

  add_main(module, spv::ExecutionModelFragment);

  InstructionStream stream = module.emit();
  REQUIRE(stream.validate());

  SpirVModule reparsed(stream);
  CHECK(reparsed.get_kind(id_size) == SpirVModule::IdKind::SPEC_CONSTANT);
  CHECK(reparsed.resolve_constant(id_size) == 0);

  // The array resolves (with unknowable size 0) rather than raising.
  const ShaderType *type = reparsed.resolve_type(array_tid);
  REQUIRE(type != nullptr);
  CHECK(type->as_array() != nullptr);

  // A fresh emit is still valid and the index is consistent.
  SpirVTransformer transformer(stream);
  CHECK(transformer.validate());
  CHECK(transformer.get_result().validate());
}

TEST_CASE("SpirVBuilder branch_endif terminates a fall-through block", "[shaderpipeline]") {
  // main() { if (true) { b = a; } }
  SpirVModule module = make_module();
  Id var_a = module.define_variable(ShaderType::FLOAT, spv::StorageClassPrivate);
  Id var_b = module.define_variable(ShaderType::FLOAT, spv::StorageClassPrivate);
  Id main_id = add_main(module);
  Id merge;
  {
    SpirVBuilder builder = builder_before_return(module, main_id);
    Id cond = module.define_constant(ShaderType::BOOL, 1);
    merge = builder.branch_if(cond);
    builder.op_store(var_b, builder.op_load(var_a));
    builder.branch_endif(merge);
  }

  InstructionStream stream = module.emit();
  CHECK(stream.validate());

  // The store does not terminate the true block, so branch_endif must have
  // inserted the branch to the merge block itself.
  REQUIRE(count_op(stream, spv::OpBranch) == 1);
  Instruction branch = find_op(stream, spv::OpBranch);
  CHECK(branch.args[0] == merge);
}

TEST_CASE("SpirVBuilder positions at the start of a function body", "[shaderpipeline]") {
  // A function whose entry block declares a local variable; code inserted at
  // the body start must land after the OpVariable, not before it.
  SpirVModule module = make_module();
  Id var_in = module.define_variable(ShaderType::FLOAT, spv::StorageClassPrivate);
  Id main_id = add_main(module);
  {
    // Add a function-local variable right after the entry label.
    SpirVModule::Function *function = module.get_function(main_id);
    REQUIRE(function != nullptr);
    size_t label_index = 0;
    while (function->instructions[label_index].opcode != spv::OpLabel) {
      ++label_index;
    }
    SpirVBuilder builder(module, main_id, label_index + 1);
    Id local_ptr_tid = module.define_pointer_type(ShaderType::FLOAT, spv::StorageClassFunction);
    Id local_id = module.allocate_id();
    builder.insert(spv::OpVariable, {local_ptr_tid, local_id, (uint32_t)spv::StorageClassFunction});

    // Now insert a store "at the body start"; it must go after the variable.
    SpirVBuilder body_builder(module);
    body_builder.set_insertion_point_to_body_start(main_id);
    body_builder.op_store(local_id, body_builder.op_load(var_in));
  }

  InstructionStream stream = module.emit();
  CHECK(stream.validate());
}
