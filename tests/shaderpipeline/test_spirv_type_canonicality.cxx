/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file test_spirv_type_canonicality.cxx
 * @author rdb
 * @date 2026-07-06
 */

#include "spirVTransformer.h"

#include "spirv_test_utils.h"
#include "catch_amalgamated.hpp"

TEST_CASE("SpirVModule tracks type canonicality", "[shaderpipeline]") {
  // A type declaration is "canonical" if it is declared exactly the way
  // define_type() would declare it for its ShaderType; only canonical
  // declarations may be reused via find_type, since ShaderType does not
  // represent all SPIR-V type operands (bit widths, image depth/sampled/
  // format/multisample operands).  This module declares a zoo of canonical
  // and non-canonical variants; the canonical ones via define_type(), the
  // non-canonical ones as raw global instructions.  Note that it is
  // deliberately not validated, since the 16/64-bit types would require
  // extra capabilities that are irrelevant to what is being tested.
  SpirVModule module = make_module();

  // The 16-bit float is declared *before* the 32-bit one, to check that
  // canonicality does not depend on the declaration order for scalars.
  Id id_half = module.allocate_id();
  module.add_global(SpirVModule::Instruction(spv::OpTypeFloat, {id_half, 16}));
  Id id_float = module.define_type(ShaderType::FLOAT);
  Id id_int = module.define_type(ShaderType::INT);
  Id id_long = module.allocate_id();
  module.add_global(SpirVModule::Instruction(spv::OpTypeInt, {id_long, 64, 1}));

  const ShaderType *v4float_type =
    ShaderType::register_type(ShaderType::Vector(ShaderType::ST_float, 4));
  Id id_v4float = module.define_type(v4float_type);
  Id id_v4half = module.allocate_id();
  module.add_global(SpirVModule::Instruction(spv::OpTypeVector, {id_v4half, id_half, 4}));

  const ShaderType *array_float_type =
    ShaderType::register_type(ShaderType::Array(ShaderType::FLOAT, 2));
  Id id_array_float = module.define_type(array_float_type);
  Id id_const_2 = module.define_int_constant(2);
  Id id_array_half = module.allocate_id();
  module.add_global(SpirVModule::Instruction(spv::OpTypeArray, {id_array_half, id_half, id_const_2}));

  // A shadow sampler: the image declaration differs from the canonical
  // (storage image) declaration for its ShaderType, but the sampled image
  // type over it is exactly what define_type() would produce, since the
  // shadow flag is represented on ShaderType::SampledImage.
  const ShaderType *si_shadow_type = ShaderType::register_type(
    ShaderType::SampledImage(Texture::TT_cube_map, ShaderType::ST_float, true));
  Id id_si_shadow = module.define_type(si_shadow_type);
  Id id_image_shadow = module.get_type_id(id_si_shadow);

  // Multisampledness is not represented on ShaderType at all, so neither the
  // image nor the sampled image over it may be considered canonical.
  Id id_image_ms = module.allocate_id();
  module.add_global(SpirVModule::Instruction(spv::OpTypeImage,
    {id_image_ms, id_float, (uint32_t)spv::Dim2D, 0, 0, 1, 1,
     (uint32_t)spv::ImageFormatUnknown}));
  Id id_si_ms = module.allocate_id();
  module.add_global(SpirVModule::Instruction(spv::OpTypeSampledImage, {id_si_ms, id_image_ms}));

  // This is exactly how define_type() declares an image type...
  const ShaderType *image_2d_type = ShaderType::register_type(ShaderType::Image(
    Texture::TT_2d_texture, ShaderType::ST_float, ShaderType::Access::NONE));
  Id id_image_storage = module.define_type(image_2d_type);
  // ...and this differs only in the format operand, which ShaderType does not
  // represent.  Note that all three 2D images map to the same ShaderType.
  Id id_image_rgba8 =
    module.define_image_type(image_2d_type, 2, 2, spv::ImageFormatRgba8);

  // Emit the module and parse it back, so that we are checking the state as
  // freshly derived by the parser, not as maintained by define_type().
  InstructionStream stream = module.emit();
  SpirVTransformer transformer(stream);
  const SpirVModule &parsed = transformer.get_module();

  // Scalars: only the widths that define_type() emits are canonical.
  CHECK(!parsed.is_canonical_type(id_half));
  CHECK(parsed.is_canonical_type(id_float));
  CHECK(parsed.is_canonical_type(id_int));
  CHECK(!parsed.is_canonical_type(id_long));

  // Composites: canonicality must cascade from the component type, or a
  // vec4 of 16-bit floats would be handed out for a request for a regular
  // vec4 (both map to the same ShaderType).
  CHECK(parsed.is_canonical_type(id_v4float));
  CHECK(!parsed.is_canonical_type(id_v4half));
  CHECK(parsed.is_canonical_type(id_array_float));
  CHECK(!parsed.is_canonical_type(id_array_half));

  // Images and sampled images.
  CHECK(!parsed.is_canonical_type(id_image_shadow));
  CHECK(parsed.is_canonical_type(id_si_shadow));
  CHECK(!parsed.is_canonical_type(id_image_ms));
  CHECK(!parsed.is_canonical_type(id_si_ms));
  CHECK(parsed.is_canonical_type(id_image_storage));
  CHECK(!parsed.is_canonical_type(id_image_rgba8));
}
