/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file shaderGenerator.cxx
 * @author jyelon
 * @date 2007-12-15
 * @author weifengh, PandaSE
 * @date 2010-04-15
 * @author agartner, PandaSE
 * @date 2010-04-16
 * TextureStage::M_modulate (before this, separate textures formatted as
 * alpha wiped color off resulting rgb)
 */

#include "shaderGenerator.h"

#include "renderState.h"
#include "shaderAttrib.h"
#include "auxBitplaneAttrib.h"
#include "alphaTestAttrib.h"
#include "colorBlendAttrib.h"
#include "transparencyAttrib.h"
#include "textureAttrib.h"
#include "colorAttrib.h"
#include "lightAttrib.h"
#include "materialAttrib.h"
#include "lightRampAttrib.h"
#include "texMatrixAttrib.h"
#include "texGenAttrib.h"
#include "colorScaleAttrib.h"
#include "clipPlaneAttrib.h"
#include "fogAttrib.h"
#include "texture.h"
#include "ambientLight.h"
#include "directionalLight.h"
#include "renderModeAttrib.h"
#include "rescaleNormalAttrib.h"
#include "colorWriteAttrib.h"
#include "pointLight.h"
#include "sphereLight.h"
#include "spotlight.h"
#include "lightLensNode.h"
#include "lvector4.h"
#include "config_pgraphnodes.h"
#include "pStatTimer.h"
#include "bamCache.h"
#include "bamReader.h"
#include "bamWriter.h"
#include "datagramInputFile.h"
#include "datagramOutputFile.h"
#include "lightMutexHolder.h"
#include "virtualFileSystem.h"
#include "string_utils.h"

#ifndef CPPPARSER
#include "shaderModuleSpirV.h"
#include "spirVBuilder.h"
#include "spirVModule.h"
#endif

static ConfigVariableFilename shader_generator_cache_filename
("shader-generator-cache-filename", "",
 PRC_DESC("Sets the location of the cache file in which generated shaders are "
          "stored.  Leave empty to write it to the default location, in the "
          "model-cache.  To disable it, set shader-generator-cache-limit to 0."));

static ConfigVariableInt shader_generator_cache_limit
("shader-generator-cache-limit", 2048,
 PRC_DESC("Sets an upper limit on the number of shaders that are stored in the "
          "shader generator cache.  When the limit is reached, shaders that "
          "have not been used for a while are pruned.  This limit prevents the "
          "cache file from growing so large that it takes too long to load and "
          "write each run.  Set to 0 to disable the shader cache (it will be "
          "deleted upon the next run) or -1 not to have any limit at all."));

static ConfigVariableInt shader_generator_cache_max_age
("shader-generator-cache-max-age", 26 * 7 * 24 * 60 * 60,
 PRC_DESC("Sets a maximum, in seconds, on how long unused shaders remain in "
          "the generated shader cache.  The default is about 6 months.  Set to "
          "-1 to disable pruning the shader generator cache based on age."));

using std::string;

TypeHandle ShaderGenerator::_type_handle;

#define UNPACK_COMBINE_SRC(from, n) (TextureStage::CombineSource)((from >> ((uint16_t)n * 5u)) & 7u)
#define UNPACK_COMBINE_OP(from, n) (TextureStage::CombineOperand)(((from >> (((uint16_t)n * 5u) + 3u)) & 3u) + 1u)

static inline uint16_t
pack_combine(TextureStage::CombineSource src0, TextureStage::CombineOperand op0,
             TextureStage::CombineSource src1, TextureStage::CombineOperand op1,
             TextureStage::CombineSource src2, TextureStage::CombineOperand op2) {
  if (op0 == TextureStage::CO_undefined) op0 = TextureStage::CO_src_alpha;
  if (op1 == TextureStage::CO_undefined) op1 = TextureStage::CO_src_alpha;
  if (op2 == TextureStage::CO_undefined) op2 = TextureStage::CO_src_alpha;

  return ((uint16_t)src0) | ((((uint16_t)op0 - 1u) & 3u) << 3u) |
         ((uint16_t)src1 << 5u) | ((((uint16_t)op1 - 1u) & 3u) << 8u) |
         ((uint16_t)src2 << 10u) | ((((uint16_t)op2 - 1u) & 3u) << 13u);
}

static std::string sgc_header("sgc\0\n\r", 6);

static PStatCollector lookup_collector("*:Munge:ShaderGen:Lookup");
static PStatCollector synthesize_collector("*:Munge:ShaderGen:Synthesize");

/**
 * Create a ShaderGenerator.  This has no state, except possibly to cache
 * certain results.  The parameter that must be passed is the GSG to which the
 * shader generator belongs.
 */
ShaderGenerator::
ShaderGenerator(const GraphicsStateGuardianBase *gsg) {
  uint64_t caps = gsg->get_supported_shader_capabilities();

  // Can we use point coordinates provided by the driver?
  _use_pointcoord = (caps & Shader::C_point_coord) != 0;

  // Do we want to use the ARB_shadow extension?  This also allows us to use
  // hardware shadows PCF.
  _use_shadow_filter = (caps & Shader::C_shadow_samplers) != 0 && gsg->get_supports_shadow_filter();

  // How many joints can we pass to the shader?  On older hardware (and DX9)
  // we are limited to 256 vectors, giving us only 85 3x4 matrices.
  if (gsg->_max_vertex_shader_parameter_vectors >= 500) {
    _num_indexed_transforms = 120;
  }
  else if (gsg->_max_vertex_shader_parameter_vectors >= 250) {
    _num_indexed_transforms = 80;
  }
  else if (gsg->_max_vertex_shader_parameter_vectors >= 128) {
    _num_indexed_transforms = 36;
  }
  else {
    _num_indexed_transforms = 4;
  }

  Filename fn = shader_generator_cache_filename;
  fn.set_binary();
  if (fn.empty()) {
    BamCache *cache = BamCache::get_global_ptr();
    if (cache != nullptr && cache->get_active()) {
      fn = Filename(cache->get_root(), "genshader.boo");
      fn.set_binary();
      _cache_filename = fn;
      reload_cache();
    }
  }
  else {
    _cache_filename = fn;
    reload_cache();
  }
}

/**
 * Destroy a ShaderGenerator.
 */
ShaderGenerator::
~ShaderGenerator() {
  flush_cache();
}

/**
 * Reloads the generated shader cache from disk.  The results will be combined
 * with the current generated shader map, you can call clear_generated_shaders
 * first to just get the disk cache.
 *
 * @since 1.11.0
 */
bool ShaderGenerator::
reload_cache() {
  if (shader_generator_cache_limit == 0) {
    return true;
  }

  LightMutexHolder holder(_lock);

  TrueClock *clock = TrueClock::get_global_ptr();
  double begin_time = clock->get_short_raw_time();

  DatagramInputFile din;
  if (!din.open(_cache_filename)) {
    if (pgraphnodes_cat.is_debug()) {
      pgraphnodes_cat.debug()
        << "Could not read shader generator cache file: "
        << _cache_filename << "\n";
    }
    return false;
  }

  string head;
  if (!din.read_header(head, sgc_header.size())) {
    if (pgraphnodes_cat.is_debug()) {
      pgraphnodes_cat.debug()
        << _cache_filename << " is not a shader generator cache file.\n";
    }
    return false;
  }

  if (head != sgc_header) {
    if (pgraphnodes_cat.is_debug()) {
      pgraphnodes_cat.debug()
        << _cache_filename << " is not a shader generator cache file.\n";
    }
    return false;
  }

  BamReader reader(&din);
  if (!reader.init()) {
    return false;
  }

  unsigned int count = 0;

  // The cache furthermore just consists of pairs of a raw ShaderKey followed
  // by the generated ShaderAttrib.
  Datagram dg;
  while (din.get_datagram(dg)) {
    DatagramIterator scan(dg);

    BamEnums::BamObjectCode boc = (BamEnums::BamObjectCode)scan.get_uint8();
    if (boc != BamEnums::BOC_ignore) {
      pgraphnodes_cat.warning()
        << _cache_filename << " is corrupted: unexpected BOC 0x"
        << std::hex << (int)boc << std::dec << "\n";
      return false;
    }

    time_t age = (time_t)scan.get_uint32();

    ShaderKey key;
    key.read_datagram(scan);

    GeneratedShader &gen_shader = _generated_shaders[std::move(key)];
    gen_shader._last_use = std::max(age, gen_shader._last_use);

    if (gen_shader._shader != nullptr) {
      // Optimization: don't read shaders that are already in memory.
      reader.skip_object();
      continue;
    }

    TypedWritable *shader = reader.read_object();
    if (!shader->is_of_type(Shader::get_class_type())) {
      pgraphnodes_cat.warning()
        << _cache_filename << " is corrupted: expected Shader instead of "
        << shader->get_type() << "\n";
      return false;
    }

    gen_shader._shader = DCAST(Shader, shader);
    ++count;
  }

  if (!reader.resolve()) {
    pgraphnodes_cat.error()
      << "Unable to fully resolve shader generator cache file.\n";
    return false;
  }

  double end_time = clock->get_short_raw_time();
  double time_delta = end_time - begin_time;

  pgraphnodes_cat.info()
    << "Read " << count << " generated shaders from "
    << _cache_filename << " in " << (time_delta * 1000) << " ms\n";

  return true;
}

/**
 * Writes all generated shaders to the cache.
 *
 * @since 1.11.0
 */
bool ShaderGenerator::
flush_cache() {
  Filename cache_filename;
  TrueClock *clock;
  double begin_time;
  VirtualFileSystem *vfs;
  int limit;
  std::vector<std::pair<ShaderKey, GeneratedShader> > gen_shaders;
  {
    LightMutexHolder holder(_lock);
    if (_generated_shaders.empty()) {
      return true;
    }
    if (_cache_filename.empty()) {
      return true;
    }
    cache_filename = _cache_filename;

    vfs = VirtualFileSystem::get_global_ptr();

    limit = shader_generator_cache_limit;
    if (limit == 0) {
      vfs->delete_file(cache_filename);
      return true;
    }

    clock = TrueClock::get_global_ptr();
    begin_time = clock->get_short_raw_time();

    time_t min_last_use = 0;
    int max_age = shader_generator_cache_max_age;
    if (max_age >= 0) {
      min_last_use = time(nullptr) - max_age;
    }

    gen_shaders.reserve(_generated_shaders.size());
    GeneratedShaders::const_iterator it;
    for (const GeneratedShaders::value_type &entry : _generated_shaders) {
      // Entries without a shader are memoized generation failures; those are
      // not written out to the cache.
      if (entry.second._shader != nullptr &&
          entry.second._last_use >= min_last_use) {
        gen_shaders.push_back(std::move(entry));
      }
    }
  }

  unsigned int count = 0;
  {
    DatagramOutputFile dout;
    if (!dout.open(cache_filename)) {
      pgraphnodes_cat.error()
        << "Unable to open " << cache_filename << "\n";
      vfs->delete_file(cache_filename);
      return false;
    }

    if (!dout.write_header(sgc_header)) {
      pgraphnodes_cat.error()
        << "Unable to write to " << cache_filename << "\n";
      vfs->delete_file(cache_filename);
      return false;
    }

    BamWriter writer(&dout);
    if (!writer.init()) {
      vfs->delete_file(cache_filename);
      return false;
    }

    if (limit > 0 && gen_shaders.size() > (size_t)limit) {
      // We need to prune down the generated shader list.  Sort by time of last
      // use, in reverse order, so we can easily eliminate the oldest shaders.
      std::sort(gen_shaders.begin(), gen_shaders.end(),
        [=](std::pair<ShaderKey, GeneratedShader> &a, std::pair<ShaderKey, GeneratedShader> &b) {
          return a.second._last_use > b.second._last_use;
        });

      gen_shaders.resize((size_t)limit);
    }

    for (std::pair<ShaderKey, GeneratedShader> &entry : gen_shaders) {
      Datagram dg;
      dg.add_uint8(BamEnums::BOC_ignore);
      dg.add_uint32(entry.second._last_use);
      entry.first.write_datagram(dg);
      dout.put_datagram(dg);

      if (!writer.write_object(entry.second._shader)) {
        break;
      }
      ++count;
    }
  }

  double end_time = clock->get_short_raw_time();
  double time_delta = end_time - begin_time;

  pgraphnodes_cat.info()
    << "Wrote " << count << " generated shaders to "
    << cache_filename << " in " << (time_delta * 1000) << " ms\n";

  return true;
}

/**
 * Analyzes the RenderState prior to shader generation.  The results of the
 * analysis are stored in instance variables of the Shader Generator.
 */
void ShaderGenerator::
analyze_renderstate(ShaderKey &key, const RenderState *rs) {
  const ShaderAttrib *shader_attrib;
  rs->get_attrib_def(shader_attrib);
  nassertv(shader_attrib->auto_shader());

  // Decide whether or not we need alpha testing or alpha blending.
  bool have_alpha_test = false;
  bool have_alpha_blend = false;
  const AlphaTestAttrib *alpha_test;
  rs->get_attrib_def(alpha_test);
  if (alpha_test->get_mode() != RenderAttrib::M_none &&
      alpha_test->get_mode() != RenderAttrib::M_always) {
    have_alpha_test = true;
  }
  const ColorBlendAttrib *color_blend;
  rs->get_attrib_def(color_blend);
  if (color_blend->get_mode() != ColorBlendAttrib::M_none) {
    have_alpha_blend = true;
  }
  const TransparencyAttrib *transparency;
  rs->get_attrib_def(transparency);
  if (transparency->get_mode() == TransparencyAttrib::M_alpha ||
      transparency->get_mode() == TransparencyAttrib::M_premultiplied_alpha ||
      transparency->get_mode() == TransparencyAttrib::M_dual) {
    have_alpha_blend = true;
  }

  // Are we writing to the color channel at all?
  bool have_color_write = false;
  const ColorWriteAttrib *color_write;
  rs->get_attrib_def(color_write);
  if (color_write->get_channels() != ColorWriteAttrib::C_off) {
    have_color_write = true;
  } else {
    key._flags |= ShaderKey::F_disable_color_write;

    // Subsume alpha test so we can eliminate the color output
    if (have_alpha_test) {
      key._flags |= (alpha_test->get_mode() & 0x7) << ShaderKey::F_ALPHA_TEST_SHIFT;
      key._alpha_test_ref = alpha_test->get_reference_alpha();
    }
  }

  if (have_alpha_blend || have_alpha_test) {
    key._flags |= ShaderKey::F_calc_primary_alpha;
  }

  // Decide what to send to the framebuffer alpha, if anything.
  const AuxBitplaneAttrib *aux_bitplane;
  rs->get_attrib_def(aux_bitplane);
  int outputs = aux_bitplane->get_outputs();

  if (outputs & AuxBitplaneAttrib::ABO_glow) {
    if (have_alpha_blend) {
      key._flags |= ShaderKey::F_disable_alpha_write;
    }
    else if (have_color_write) {
      key._flags |= ShaderKey::F_out_alpha_glow;
      if (have_alpha_test) {
        // Subsume the alpha test in our shader.
        key._flags |= (alpha_test->get_mode() & 0x7) << ShaderKey::F_ALPHA_TEST_SHIFT;
        key._alpha_test_ref = alpha_test->get_reference_alpha();
      }
    }
  }

  if (outputs & AuxBitplaneAttrib::ABO_aux_normal) {
    key._flags |= ShaderKey::F_out_aux_normal;
  }
  if (outputs & AuxBitplaneAttrib::ABO_aux_glow) {
    key._flags |= ShaderKey::F_out_aux_glow;
  }

  // Determine whether or not vertex colors or flat colors are present.
  const ColorAttrib *color;
  rs->get_attrib_def(color);
  if (color->get_color_type() == ColorAttrib::T_flat) {
    key._flags |= ShaderKey::F_flat_color;
  }
  else if (color->get_color_type() == ColorAttrib::T_vertex) {
    key._flags |= ShaderKey::F_vertex_color;
  }

  // Store the material flags (not the material values itself).
  const MaterialAttrib *material;
  rs->get_attrib_def(material);
  Material *mat = material->get_material();
  if (mat != nullptr) {
    // The next time the Material flags change, the Material should cause the
    // states to be rehashed.
    mat->mark_used_by_auto_shader();
    key._flags |= (mat->get_flags() & ShaderKey::F_MATERIAL_FLAGS_MASK);

    if ((key._flags & Material::F_base_color) != 0) {
      key._flags |= (Material::F_diffuse | Material::F_specular | Material::F_ambient);
      key._flags &= ~Material::F_base_color;
    }
  }

  // Break out the lights by type.
  const LightAttrib *la;
  rs->get_attrib_def(la);
  bool have_ambient = false;

  for (size_t i = 0; i < la->get_num_on_lights(); ++i) {
    NodePath np = la->get_on_light(i);
    nassertv(!np.is_empty());
    PandaNode *node = np.node();
    nassertv(node != nullptr);

    key._flags |= ShaderKey::F_lighting;

    if (node->is_ambient_light()) {
      have_ambient = true;
      continue;
    }

    int light_flags = 0;

    if (node->is_of_type(LightLensNode::get_class_type())) {
      const LightLensNode *llnode = (const LightLensNode *)node;

      if (llnode->is_of_type(DirectionalLight::get_class_type())) {
        light_flags |= ShaderKey::LF_type_directional;
      }
      else if (llnode->is_of_type(Spotlight::get_class_type())) {
        light_flags |= ShaderKey::LF_type_spot;
      }
      else if (llnode->is_of_type(PointLight::get_class_type())) {
        if (llnode->is_of_type(SphereLight::get_class_type())) {
          light_flags |= ShaderKey::LF_type_sphere;
        }
        else {
          light_flags |= ShaderKey::LF_type_point;
        }
      }

      if (have_color_write && shader_attrib->auto_shadow_on()) {
        if (llnode->is_shadow_caster()) {
          light_flags |= ShaderKey::LF_has_shadows;

          if (_use_shadow_filter) {
            key._flags |= ShaderKey::F_use_shadow_filter;
          }
        }

        // Make sure that the next time the shadows are toggled on this
        // light, it triggers a state rehash.
        llnode->mark_used_by_auto_shader();
      }
      if (llnode->has_specular_color()) {
        light_flags |= ShaderKey::LF_has_specular_color;
      }
    }

    key._lights.push_back(light_flags);
  }

  // See if there is a normal map, height map, gloss map, or glow map.  Also
  // check if anything has TexGen.

  const TextureAttrib *texture;
  rs->get_attrib_def(texture);
  const TexGenAttrib *tex_gen;
  rs->get_attrib_def(tex_gen);
  const TexMatrixAttrib *tex_matrix;
  rs->get_attrib_def(tex_matrix);

  size_t num_textures = texture->get_num_on_stages();
  for (size_t i = 0; i < num_textures; ++i) {
    TextureStage *stage = texture->get_on_stage(i);
    Texture *tex = texture->get_on_texture(stage);
    nassertd(tex != nullptr) continue;

    // Mark this TextureStage as having been used by the shader generator, so
    // that the next time its properties change, it will cause the state to be
    // rehashed to ensure that the shader is regenerated if needed.
    stage->mark_used_by_auto_shader();

    ShaderKey::TextureInfo info;
    info._type = tex->get_texture_type();
    info._mode = stage->get_mode();
    info._flags = 0;
    info._combine_rgb = 0u;
    info._combine_alpha = 0u;

    // While we look at the mode, determine whether we need to change the mode
    // in order to reflect disabled features.
    switch (info._mode) {
    case TextureStage::M_modulate:
      {
        Texture::Format format = tex->get_format();
        if (format != Texture::F_alpha) {
          info._flags |= ShaderKey::TF_has_rgb;
        }
        if (Texture::has_alpha(format)) {
          info._flags |= ShaderKey::TF_has_alpha;
        }
      }
      break;

    case TextureStage::M_modulate_glow:
      if (shader_attrib->auto_glow_on()) {
        info._flags = ShaderKey::TF_map_glow;
      } else {
        info._mode = TextureStage::M_modulate;
        info._flags = ShaderKey::TF_has_rgb;
      }
      break;

    case TextureStage::M_modulate_gloss:
      if (shader_attrib->auto_gloss_on()) {
        info._flags = ShaderKey::TF_map_gloss;
      } else {
        info._mode = TextureStage::M_modulate;
        info._flags = ShaderKey::TF_has_rgb;
      }
      break;

    case TextureStage::M_normal_height:
      if (parallax_mapping_samples == 0) {
        info._mode = TextureStage::M_normal;
      } else if (!shader_attrib->auto_normal_on() ||
                 (key._lights.empty() && (outputs & AuxBitplaneAttrib::ABO_aux_normal) == 0)) {
        info._mode = TextureStage::M_height;
        info._flags = ShaderKey::TF_has_alpha;
      } else {
        info._flags = ShaderKey::TF_map_normal | ShaderKey::TF_map_height;
      }
      break;

    case TextureStage::M_normal_gloss:
      if (!shader_attrib->auto_gloss_on() || key._lights.empty()) {
        info._mode = TextureStage::M_normal;
      } else if (!shader_attrib->auto_normal_on()) {
        info._mode = TextureStage::M_gloss;
      } else {
        info._flags = ShaderKey::TF_map_normal | ShaderKey::TF_map_gloss;
      }
      break;

    case TextureStage::M_combine:
      // If we have this rare, special mode, we encode all these extra
      // parameters as flags to prevent bloating the shader key.
      info._flags |= (uint32_t)stage->get_combine_rgb_mode() << ShaderKey::TF_COMBINE_RGB_MODE_SHIFT;
      info._flags |= (uint32_t)stage->get_combine_alpha_mode() << ShaderKey::TF_COMBINE_ALPHA_MODE_SHIFT;
      if (stage->get_rgb_scale() == 2) {
        info._flags |= ShaderKey::TF_rgb_scale_2;
      }
      if (stage->get_rgb_scale() == 4) {
        info._flags |= ShaderKey::TF_rgb_scale_4;
      }
      if (stage->get_alpha_scale() == 2) {
        info._flags |= ShaderKey::TF_alpha_scale_2;
      }
      if (stage->get_alpha_scale() == 4) {
        info._flags |= ShaderKey::TF_alpha_scale_4;
      }

      info._combine_rgb = pack_combine(
        stage->get_combine_rgb_source0(), stage->get_combine_rgb_operand0(),
        stage->get_combine_rgb_source1(), stage->get_combine_rgb_operand1(),
        stage->get_combine_rgb_source2(), stage->get_combine_rgb_operand2());
      info._combine_alpha = pack_combine(
        stage->get_combine_alpha_source0(), stage->get_combine_alpha_operand0(),
        stage->get_combine_alpha_source1(), stage->get_combine_alpha_operand1(),
        stage->get_combine_alpha_source2(), stage->get_combine_alpha_operand2());

      if (stage->uses_primary_color()) {
        info._flags |= ShaderKey::TF_uses_primary_color;
      }
      if (stage->uses_last_saved_result()) {
        info._flags |= ShaderKey::TF_uses_last_saved_result;
      }
      break;

    default:
      break;
    }

    // In fact, perhaps this stage should be disabled altogether?
    bool skip = false;
    switch (info._mode) {
    case TextureStage::M_normal:
      if (!shader_attrib->auto_normal_on() ||
          (key._lights.empty() && (outputs & AuxBitplaneAttrib::ABO_aux_normal) == 0)) {
        skip = true;
      } else {
        info._flags = ShaderKey::TF_map_normal;
      }
      break;
    case TextureStage::M_glow:
      if (shader_attrib->auto_glow_on()) {
        info._flags = ShaderKey::TF_map_glow;
      } else {
        skip = true;
      }
      break;
    case TextureStage::M_gloss:
      if (!key._lights.empty() && shader_attrib->auto_gloss_on()) {
        info._flags = ShaderKey::TF_map_gloss;
      } else {
        skip = true;
      }
      break;
    case TextureStage::M_height:
      if (parallax_mapping_samples > 0) {
        info._flags = ShaderKey::TF_map_height;
      } else {
        skip = true;
      }
      break;
    case TextureStage::M_emission:
      info._flags = ShaderKey::TF_map_emission;
      break;
    case TextureStage::M_occlusion:
    case TextureStage::M_occlusion_metallic_roughness:
      info._flags = ShaderKey::TF_map_occlusion;
      break;
    default:
      break;
    }
    // We can't just drop a disabled slot from the list, since then the
    // indices for the texture stages will no longer match up.  So we keep it,
    // but set it to a noop state to indicate that it should be skipped.
    if (skip) {
      info._type = Texture::TT_1d_texture;
      info._mode = TextureStage::M_modulate;
      info._flags = 0;
      key._textures.push_back(info);
      continue;
    }

    // Check if this state has a texture matrix to transform the texture
    // coordinates.
    if (tex_matrix->has_stage(stage)) {
      CPT(TransformState) transform = tex_matrix->get_transform(stage);
      if (!transform->is_identity()) {
        // Optimize for common case: if we only have a scale component, we
        // can get away with fewer shader inputs and operations.
        if (transform->has_components() && !transform->has_nonzero_shear() &&
            transform->get_pos() == LPoint3::zero() &&
            transform->get_hpr() == LVecBase3::zero()) {
          info._flags |= ShaderKey::TF_has_texscale;
        } else {
          info._flags |= ShaderKey::TF_has_texmat;
        }
      }
    }

    if (tex_gen->has_stage(stage)) {
      info._texcoord_name = nullptr;
      info._gen_mode = tex_gen->get_mode(stage);
    } else {
      info._texcoord_name = stage->get_texcoord_name();
      info._gen_mode = TexGenAttrib::M_off;
    }

    // Does this stage require saving its result?
    if (stage->get_saved_result()) {
      info._flags |= ShaderKey::TF_saved_result;
    }

    // Does this stage need a texcolor_# input?
    if (stage->uses_color() || stage->involves_color_scale()) {
      info._flags |= ShaderKey::TF_uses_color;
    }

    key._textures.push_back(info);
    key._texture_flags |= info._flags;
  }

  // Does nothing use the saved result?  If so, don't bother saving it.
  if ((key._texture_flags & ShaderKey::TF_uses_last_saved_result) == 0 &&
      (key._texture_flags & ShaderKey::TF_saved_result) != 0) {

    pvector<ShaderKey::TextureInfo>::iterator it;
    for (it = key._textures.begin(); it != key._textures.end(); ++it) {
      (*it)._flags &= ~ShaderKey::TF_saved_result;
    }
    key._texture_flags &= ~ShaderKey::TF_saved_result;
  }

  // Decide whether to separate ambient and diffuse calculations.
  if (have_ambient) {
    if (key._texture_flags & ShaderKey::TF_map_occlusion) {
      key._flags |= ShaderKey::F_have_separate_ambient;
    }
    else if (key._flags & Material::F_ambient) {
      key._flags |= ShaderKey::F_have_separate_ambient;
    }
    else {
      if (key._flags & Material::F_diffuse) {
        key._flags |= ShaderKey::F_have_separate_ambient;
      }
    }
  }

  if (have_color_write && shader_attrib->auto_ramp_on()) {
    const LightRampAttrib *light_ramp;
    if (rs->get_attrib(light_ramp)) {
      key._light_ramp_mode = light_ramp->get_mode();
      key._light_ramp_level[0] = light_ramp->get_level(0);
      key._light_ramp_level[1] = light_ramp->get_level(1);
      key._light_ramp_threshold[0] = light_ramp->get_threshold(0);
      key._light_ramp_threshold[1] = light_ramp->get_threshold(1);
      if (key._flags & ShaderKey::F_lighting) {
        key._flags |= ShaderKey::F_have_separate_ambient;
      }
    }
  }

  // Check for clip planes.
  const ClipPlaneAttrib *clip_plane;
  rs->get_attrib_def(clip_plane);
  key._num_clip_planes = clip_plane->get_num_on_planes();

  // Check for fog.
  const FogAttrib *fog;
  if (have_color_write && rs->get_attrib(fog) && !fog->is_off()) {
    key._flags |= ((int)fog->get_fog()->get_mode() + 1) << ShaderKey::F_FOG_MODE_SHIFT;
  }

  // Must we calculate the point size in the shader?
  const RenderModeAttrib *render_mode;
  if (rs->get_attrib(render_mode) && render_mode->get_perspective()) {
    key._flags |= ShaderKey::F_perspective_points;
  }
}

/**
 * Analyzes the RenderState prior to shader generation.  The results of the
 * analysis are stored in instance variables of the Shader Generator.
 */
void ShaderGenerator::
analyze_renderstate(ShaderKey &key, const RenderState *rs,
                    const GeomVertexAnimationSpec &anim) {

  analyze_renderstate(key, rs);

  if (anim.get_animation_type() == GeomVertexAnimationSpec::AT_hardware) {
    if (anim.get_indexed_transforms()) {
      key._flags |= ShaderKey::F_indexed_transforms;
      key._num_anim_transforms = _num_indexed_transforms;
    } else {
      key._num_anim_transforms = std::min(4, anim.get_num_transforms());
    }
  }
}

/**
 * Rehashes all the states with generated shaders, removing the ones that are
 * no longer fresh.
 *
 * Call this if certain state has changed in such a way as to require a rerun
 * of the shader generator.  This should be rare because in most cases, the
 * shader generator will automatically regenerate shaders as necessary.
 *
 * @since 1.10.0
 */
void ShaderGenerator::
rehash_generated_shaders() {
  LightMutexHolder holder2(_lock);
  LightReMutexHolder holder(*RenderState::_states_lock);

  // With uniquify-states turned on, we can actually go through all the states
  // and check whether their generated shader is still OK.
  size_t size = RenderState::_states.get_num_entries();
  for (size_t si = 0; si < size; ++si) {
    const RenderState *state = RenderState::_states.get_key(si);

    if (state->_generated_shader != nullptr) {
      ShaderKey key;
      analyze_renderstate(key, state);

      GeneratedShaders::const_iterator si;
      si = _generated_shaders.find(key);
      if (si != _generated_shaders.end()) {
        if (si->second._attrib != state->_generated_shader) {
          state->_generated_shader = si->second._attrib;
          state->_munged_states.clear();
        }
      } else {
        // We have not yet generated a shader for this modified state.
        state->_generated_shader.clear();
        state->_munged_states.clear();
      }
    }
  }

  // If we don't have uniquify-states, however, the above list won't contain
  // all the state.  We can change a global seq value to require Panda to
  // rehash the states the next time it tries to render an object with it.
  if (!uniquify_states) {
    GraphicsStateGuardianBase::mark_rehash_generated_shaders();
  }
}

/**
 * Removes all previously generated shaders, requiring all shaders to be
 * regenerated.  Does not clear cache of compiled shaders.
 *
 * @since 1.10.0
 */
void ShaderGenerator::
clear_generated_shaders() {
  LightMutexHolder holder2(_lock);
  LightReMutexHolder holder(*RenderState::_states_lock);

  size_t size = RenderState::_states.get_num_entries();
  for (size_t si = 0; si < size; ++si) {
    const RenderState *state = RenderState::_states.get_key(si);
    state->_generated_shader.clear();
  }

  _generated_shaders.clear();

  // If we don't have uniquify-states, we can't clear all the ShaderAttribs
  // that are cached on the states, but we can simulate the effect of that.
  if (!uniquify_states) {
    GraphicsStateGuardianBase::mark_rehash_generated_shaders();
  }
}

/**
 * Returns the varying location assigned to the given texture coordinate set,
 * or -1 if no varying was assigned for it.
 */
int ShaderGenerator::ShaderPlan::
find_texcoord_location(const InternalName *name) const {
  for (const auto &pair : _texcoord_locations) {
    if (pair.first == name) {
      return pair.second;
    }
  }
  return -1;
}

/**
 * Derives from the given key which intermediate results the vertex shader
 * needs to calculate and pass along to the fragment shader, and assigns the
 * varying locations.
 */
ShaderGenerator::ShaderPlan ShaderGenerator::
make_plan(const ShaderKey &key) const {
  ShaderPlan plan;

  // Figure out whether we need to calculate any of these variables.
  plan._need_world_position = (key._num_clip_planes > 0);
  plan._need_eye_position = (key._flags & ShaderKey::F_lighting) != 0;
  plan._need_eye_normal = !key._lights.empty() ||
                          (key._flags & ShaderKey::F_out_aux_normal) != 0;
  plan._need_tangents = (key._texture_flags & ShaderKey::TF_map_normal) != 0;
  plan._need_point_size = (key._flags & ShaderKey::F_perspective_points) != 0;

  if (key._flags & ShaderKey::F_lighting) {
    if (key._flags & Material::F_specular) {
      plan._have_specular = true;
    } else if ((key._texture_flags & ShaderKey::TF_map_gloss) != 0) {
      plan._have_specular = true;
    }
  }

  if ((key._flags & (ShaderKey::F_flat_color | ShaderKey::F_vertex_color)) != 0) {
    if (key._flags & ShaderKey::F_lighting) {
      if (((key._flags & Material::F_ambient) == 0 &&
           (key._flags & ShaderKey::F_have_separate_ambient) != 0) ||
          (key._flags & Material::F_diffuse) == 0 ||
          (key._flags & ShaderKey::F_calc_primary_alpha) != 0) {
        plan._need_color = true;
      }
    } else {
      plan._need_color = true;
    }
  }

  int next_location = 0;

  for (size_t i = 0; i < key._textures.size(); ++i) {
    const ShaderKey::TextureInfo &tex = key._textures[i];

    switch (tex._gen_mode) {
    case TexGenAttrib::M_world_cube_map:
      plan._need_fragment_view_to_world = true;
      // fall through
    case TexGenAttrib::M_eye_sphere_map:
    case TexGenAttrib::M_eye_cube_map:
      plan._need_eye_position = true;
      plan._need_eye_normal = true;
      plan._need_eye_reflection = true;
      break;
    case TexGenAttrib::M_world_normal:
      plan._need_world_normal = true;
      break;
    case TexGenAttrib::M_eye_normal:
      plan._need_eye_normal = true;
      break;
    case TexGenAttrib::M_world_position:
      plan._need_world_position = true;
      break;
    case TexGenAttrib::M_eye_position:
      plan._need_eye_position = true;
      break;
    case TexGenAttrib::M_point_sprite:
      plan._need_point_coord = true;
      break;
    default:
      break;
    }

    if (tex._texcoord_name != nullptr &&
        plan.find_texcoord_location(tex._texcoord_name) < 0) {
      plan._texcoord_locations.push_back(
        std::make_pair(tex._texcoord_name.p(), next_location++));
    }

    if (plan._tangent_input.empty() &&
        (tex._flags & (ShaderKey::TF_map_normal | ShaderKey::TF_map_height)) != 0) {
      PT(InternalName) tangent_name = InternalName::get_tangent();
      PT(InternalName) binormal_name = InternalName::get_binormal();

      if (tex._texcoord_name != nullptr &&
          tex._texcoord_name != InternalName::get_texcoord()) {
        tangent_name = tangent_name->append(tex._texcoord_name->get_basename());
        binormal_name = binormal_name->append(tex._texcoord_name->get_basename());
      }

      plan._tangent_input = tangent_name->join("_");
      plan._binormal_input = binormal_name->join("_");
    }

    if (tex._flags & ShaderKey::TF_map_glow) {
      plan._map_index_glow = (int)i;
    }
    if (tex._flags & ShaderKey::TF_map_gloss) {
      plan._map_index_gloss = (int)i;
    }
    if (tex._flags & ShaderKey::TF_map_emission) {
      plan._map_index_emission = (int)i;
    }
  }

  plan._calc_point_coord = plan._need_point_coord && !_use_pointcoord;

  plan._need_normal = (key._texture_flags & ShaderKey::TF_map_height) != 0 ||
                      plan._need_world_normal || plan._need_eye_normal;

  // Assign the remaining varying locations.
  if (plan._need_tangents) {
    plan._tangent_location = next_location++;
    plan._binormal_location = next_location++;
  }
  if (plan._need_color && (key._flags & ShaderKey::F_vertex_color) != 0) {
    plan._color_location = next_location++;
  }
  if (plan._need_world_position) {
    plan._world_position_location = next_location++;
  }
  if (plan._need_world_normal) {
    plan._world_normal_location = next_location++;
  }
  if (plan._need_eye_position) {
    plan._eye_position_location = next_location++;
  }
  if (plan._need_eye_normal) {
    plan._eye_normal_location = next_location++;
  }
  if (key._texture_flags & ShaderKey::TF_map_height) {
    plan._eyevec_location = next_location++;
  }
  if ((key._flags & ShaderKey::F_FOG_MODE_MASK) != 0) {
    plan._hpos_location = next_location++;
  }
  for (size_t i = 0; i < key._lights.size(); ++i) {
    if (key._lights[i] & ShaderKey::LF_has_shadows) {
      plan._lightcoord_locations.push_back(next_location++);
    } else {
      plan._lightcoord_locations.push_back(-1);
    }
  }
  if (plan._calc_point_coord) {
    plan._pointcoord_location = next_location++;
  }

  return plan;
}

#ifndef CPPPARSER

namespace {

using Id = SpirVId;

/**
 * Wraps the SpirVModule and SpirVBuilder building the single entry point of
 * one generated shader stage, providing the amenities the generator needs:
 * creating named interface variables, common types and constants, and short
 * spellings for the most frequent operations.
 */
class StageBuilder {
public:
  explicit StageBuilder(spv::ExecutionModel model) :
    _builder(_module),
    _model(model)
  {
    _module.add_capability(spv::CapabilityShader);
    _module.set_memory_model(spv::AddressingModelLogical, spv::MemoryModelGLSL450);

    _float = ShaderType::FLOAT;
    _vec2 = ShaderType::register_type(ShaderType::Vector(ShaderType::ST_float, 2));
    _vec3 = ShaderType::register_type(ShaderType::Vector(ShaderType::ST_float, 3));
    _vec4 = ShaderType::register_type(ShaderType::Vector(ShaderType::ST_float, 4));
    _mat3 = ShaderType::register_type(ShaderType::Matrix(ShaderType::ST_float, 3, 3));
    _mat4 = ShaderType::register_type(ShaderType::Matrix(ShaderType::ST_float, 4, 4));
    _mat3x4 = ShaderType::register_type(ShaderType::Matrix(ShaderType::ST_float, 3, 4));

    // The entry point is created up front, so that the interface variable
    // helpers below can add variables to its interface as they go.
    _function_id = _builder.op_function(nullptr);
    _module.add_entry_point(model, _function_id, "main");
    _builder.op_label();
  }

  /**
   * Finishes the entry point function and wraps the module up into a
   * ShaderModule.
   */
  PT(ShaderModule) finish(ShaderModule::Stage stage) {
    _builder.op_return();
    _builder.op_function_end();

    if (_model == spv::ExecutionModelFragment) {
      _module.add_execution_mode(_function_id, spv::ExecutionModeOriginUpperLeft);
    }

    ShaderModuleSpirV::InstructionStream stream = _module.emit();
    if (pgraphnodes_cat.is_spam()) {
      std::ostream &out = pgraphnodes_cat.spam()
        << "Generated " << stage << " shader:\n";
      stream.disassemble(out);
    }
    return new ShaderModuleSpirV(stage, std::move((std::vector<uint32_t> &)stream));
  }

  Id make_input(const ShaderType *type, const std::string &name, int location) {
    Id var = _module.define_variable(type, spv::StorageClassInput);
    _module.set_name(var, name);
    _module.set_location(var, location);
    _module.add_to_interfaces(_model, var);
    return var;
  }

  Id make_output(const ShaderType *type, const std::string &name, int location) {
    Id var = _module.define_variable(type, spv::StorageClassOutput);
    _module.set_name(var, name);
    _module.set_location(var, location);
    _module.add_to_interfaces(_model, var);
    return var;
  }

  Id make_builtin_input(spv::BuiltIn builtin) {
    return _module.ensure_builtin_input(_model, builtin);
  }

  Id make_builtin_output(spv::BuiltIn builtin) {
    return _module.ensure_builtin_output(_model, builtin);
  }

  Id make_uniform(const ShaderType *type, const std::string &name) {
    Id var = _module.define_variable(type, spv::StorageClassUniformConstant);
    _module.set_name(var, name);
    return var;
  }

  Id make_sampler(Texture::TextureType texture_type, bool shadow,
                  const std::string &name) {
    switch (texture_type) {
    case Texture::TT_1d_texture:
    case Texture::TT_1d_texture_array:
      _module.add_capability(spv::CapabilitySampled1D);
      break;
    case Texture::TT_cube_map_array:
      _module.add_capability(spv::CapabilitySampledCubeArray);
      break;
    case Texture::TT_buffer_texture:
      _module.add_capability(spv::CapabilitySampledBuffer);
      break;
    default:
      break;
    }
    const ShaderType *type = ShaderType::register_type(ShaderType::SampledImage(
      texture_type, ShaderType::ST_float, shadow));
    return make_uniform(type, name);
  }

  // Constants.
  Id fconst(float value) {
    return _module.define_float_constant(value);
  }
  Id vconst(float x, float y) {
    return _module.define_composite_constant(_vec2, {fconst(x), fconst(y)});
  }
  Id vconst(float x, float y, float z) {
    return _module.define_composite_constant(_vec3, {fconst(x), fconst(y), fconst(z)});
  }
  Id vconst(float x, float y, float z, float w) {
    return _module.define_composite_constant(_vec4,
      {fconst(x), fconst(y), fconst(z), fconst(w)});
  }
  Id splat_const(float value, uint32_t num_components) {
    const ShaderType *type = ShaderType::register_type(
      ShaderType::Vector(ShaderType::ST_float, num_components));
    return _module.define_composite_constant(type,
      pvector<Id>(num_components, fconst(value)));
  }

  // Short spellings for the frequent operations.
  Id load(Id var) { return _builder.op_load(var); }
  void store(Id var, Id value) { _builder.op_store(var, value); }
  Id add(Id a, Id b) { return _builder.op_add(a, b); }
  Id sub(Id a, Id b) { return _builder.op_sub(a, b); }
  Id mul(Id a, Id b) { return _builder.op_multiply(a, b); }
  Id div(Id a, Id b) { return _builder.op_div(a, b); }
  Id dot(Id a, Id b) { return _builder.op_dot(a, b); }
  Id neg(Id a) { return _builder.op_negate(a); }
  Id sat(Id a) { return _builder.op_saturate(a); }
  Id normalize(Id a) { return _builder.op_normalize(a); }
  Id swiz(Id vec, std::initializer_list<uint32_t> components) {
    return _builder.op_swizzle(vec, components);
  }
  Id xyz(Id vec) { return swiz(vec, {0, 1, 2}); }
  Id comp(Id vec, uint32_t i) { return _builder.op_composite_extract(vec, {i}); }

  // Broadcasts a scalar into a vector.
  Id splat(Id scalar, uint32_t num_components) {
    const ShaderType *type = ShaderType::register_type(
      ShaderType::Vector(ShaderType::ST_float, num_components));
    return _builder.op_composite_construct(type,
      pvector<Id>(num_components, scalar));
  }

  Id vec3_of(Id a, Id b) {
    return _builder.op_composite_construct(_vec3, {a, b});
  }
  Id vec3_of(Id a, Id b, Id c) {
    return _builder.op_composite_construct(_vec3, {a, b, c});
  }
  Id vec4_of(Id a, Id b) {
    return _builder.op_composite_construct(_vec4, {a, b});
  }
  Id vec4_of(Id a, Id b, Id c) {
    return _builder.op_composite_construct(_vec4, {a, b, c});
  }

  // Replaces the xyz (rgb) components of the given vec4.
  Id set_xyz(Id vec, Id new_xyz) {
    return _builder.op_vector_shuffle(vec, new_xyz, {4, 5, 6, 3});
  }
  // Replaces the w (alpha) component of the given vec4.
  Id set_w(Id vec, Id new_w) {
    return _builder.op_composite_insert(new_w, vec, {3});
  }

  // Extracts the upper-left 3x3 part of a 3x4 or 4x4 matrix value, like
  // Cg's (float3x3)mat cast.
  Id mat3_of(Id mat) {
    pvector<Id> rows;
    for (uint32_t i = 0; i < 3; ++i) {
      rows.push_back(swiz(_builder.op_composite_extract(mat, {i}), {0, 1, 2}));
    }
    return _builder.op_composite_construct(_mat3, rows);
  }

  // Converts a bool to 1.0 or 0.0.
  Id bool_to_float(Id value) {
    return _builder.op_convert(ShaderType::ST_float, value);
  }

  // Emits a "discard" that executes when cond (a bool) is true.
  void kill_if(Id cond) {
    Id merge_label = _builder.branch_if(cond);
    _builder.op_kill();
    _builder.branch_endif(merge_label);
  }

public:
  SpirVModule _module;
  SpirVBuilder _builder;
  spv::ExecutionModel _model;
  Id _function_id;

  const ShaderType *_float;
  const ShaderType *_vec2;
  const ShaderType *_vec3;
  const ShaderType *_vec4;
  const ShaderType *_mat3;
  const ShaderType *_mat4;
  const ShaderType *_mat3x4;
};

}  // anonymous namespace

/**
 * Generates the vertex shader module for the given key and plan.  This may
 * not use any mutable state of the ShaderGenerator, so that it can one day
 * run in parallel with generate_fragment_shader.
 */
PT(ShaderModule) ShaderGenerator::
generate_vertex_shader(const ShaderKey &key, const ShaderPlan &plan) const {
  StageBuilder s(spv::ExecutionModelVertex);
  SpirVBuilder &b = s._builder;

  bool need_heightmap = (key._texture_flags & ShaderKey::TF_map_height) != 0;
  bool have_fog = (key._flags & ShaderKey::F_FOG_MODE_MASK) != 0;
  bool need_model_to_view = plan._need_eye_position || plan._need_tangents ||
                            plan._need_point_size;

  // Vertex inputs.
  int next_input = 0;
  Id vtx_position = s.make_input(s._vec4, "vtx_position", next_input++);

  Id vtx_normal;
  if (plan._need_normal) {
    vtx_normal = s.make_input(s._vec3, "vtx_normal", next_input++);
  }
  Id vtx_color;
  if (plan._color_location >= 0) {
    vtx_color = s.make_input(s._vec4, "vtx_color", next_input++);
  }
  Id vtx_tangent, vtx_binormal;
  if (!plan._tangent_input.empty()) {
    vtx_tangent = s.make_input(s._vec4, "vtx_" + plan._tangent_input, next_input++);
    vtx_binormal = s.make_input(s._vec4, "vtx_" + plan._binormal_input, next_input++);
  }
  Id vtx_transform_weight, vtx_transform_index;
  if (key._num_anim_transforms > 0) {
    vtx_transform_weight = s.make_input(s._vec4, "vtx_transform_weight", next_input++);
    if (key._flags & ShaderKey::F_indexed_transforms) {
      const ShaderType *ivec4_type = ShaderType::register_type(
        ShaderType::Vector(ShaderType::ST_int, 4));
      vtx_transform_index = s.make_input(ivec4_type, "vtx_transform_index", next_input++);
    }
  }
  pvector<std::pair<Id, Id> > texcoord_vars;
  for (const auto &pair : plan._texcoord_locations) {
    std::string name = pair.first->join("_");
    Id in_var = s.make_input(s._vec4, "vtx_" + name, next_input++);
    Id out_var = s.make_output(s._vec4, "l_" + name, pair.second);
    texcoord_vars.push_back(std::make_pair(in_var, out_var));
  }

  // Uniforms.
  Id trans_model_to_world, trans_model_to_view, tpose_view_to_model;
  Id mspos_view, attr_pointparams, tbl_transforms;
  if (plan._need_world_position || plan._need_world_normal) {
    trans_model_to_world = s.make_uniform(s._mat3x4, "trans_model_to_world");
  }
  if (need_model_to_view) {
    trans_model_to_view = s.make_uniform(s._mat3x4, "trans_model_to_view");
  }
  if (plan._need_eye_normal) {
    tpose_view_to_model = s.make_uniform(s._mat3, "tpose_view_to_model");
  }
  if (need_heightmap) {
    mspos_view = s.make_uniform(s._vec4, "mspos_view");
  }
  pvector<Id> mat_shadow(key._lights.size(), Id());
  for (size_t i = 0; i < key._lights.size(); ++i) {
    if (key._lights[i] & ShaderKey::LF_has_shadows) {
      mat_shadow[i] = s.make_uniform(s._mat4, "mat_shadow_" + format_string(i));
    }
  }
  if (key._num_anim_transforms > 0) {
    const ShaderType *table_type = ShaderType::register_type(
      ShaderType::Array(s._mat3x4, key._num_anim_transforms));
    tbl_transforms = s.make_uniform(table_type, "tbl_transforms");
  }
  if (plan._need_point_size || plan._calc_point_coord) {
    attr_pointparams = s.make_uniform(s._vec3, "attr_pointparams");
  }
  Id mat_modelproj = s.make_uniform(s._mat4, "mat_modelproj");

  // Outputs.
  Id o_position = s.make_builtin_output(spv::BuiltInPosition);
  Id o_point_size;
  if (plan._need_point_size) {
    o_point_size = s.make_builtin_output(spv::BuiltInPointSize);
  }
  Id o_hpos, o_world_position, o_world_normal, o_eye_position, o_eye_normal;
  Id o_tangent, o_binormal, o_color, o_eyevec, o_pointcoord;
  if (have_fog) {
    o_hpos = s.make_output(s._vec4, "l_hpos", plan._hpos_location);
  }
  if (plan._need_world_position) {
    o_world_position = s.make_output(s._vec4, "l_world_position", plan._world_position_location);
  }
  if (plan._need_world_normal) {
    o_world_normal = s.make_output(s._vec4, "l_world_normal", plan._world_normal_location);
  }
  if (plan._need_eye_position) {
    o_eye_position = s.make_output(s._vec4, "l_eye_position", plan._eye_position_location);
  }
  if (plan._need_eye_normal) {
    o_eye_normal = s.make_output(s._vec3, "l_eye_normal", plan._eye_normal_location);
  }
  if (plan._need_tangents) {
    o_tangent = s.make_output(s._vec4, "l_tangent", plan._tangent_location);
    o_binormal = s.make_output(s._vec4, "l_binormal", plan._binormal_location);
  }
  if (plan._color_location >= 0) {
    o_color = s.make_output(s._vec4, "l_color", plan._color_location);
  }
  if (need_heightmap) {
    o_eyevec = s.make_output(s._vec3, "l_eyevec", plan._eyevec_location);
  }
  if (plan._calc_point_coord) {
    o_pointcoord = s.make_output(s._vec3, "l_pointcoord", plan._pointcoord_location);
  }
  pvector<Id> o_lightcoord(key._lights.size(), Id());
  for (size_t i = 0; i < key._lights.size(); ++i) {
    if (key._lights[i] & ShaderKey::LF_has_shadows) {
      o_lightcoord[i] = s.make_output(s._vec4, "l_lightcoord" + format_string(i),
                                      plan._lightcoord_locations[i]);
    }
  }

  // Now generate the function body.
  Id position = s.load(vtx_position);
  Id normal;
  if (vtx_normal) {
    normal = s.load(vtx_normal);
  }
  Id tangent, binormal;
  if (vtx_tangent) {
    tangent = s.xyz(s.load(vtx_tangent));
    binormal = s.xyz(s.load(vtx_binormal));
  }

  if (key._num_anim_transforms > 0) {
    // Hardware skinning: blend the transform table entries together using
    // the given weights, then transform the position and normal by it.
    Id weights = s.load(vtx_transform_weight);
    Id indices;
    int num_taps = 4;
    if (vtx_transform_index) {
      indices = s.load(vtx_transform_index);
    } else {
      num_taps = std::min(4, key._num_anim_transforms);
    }

    // We can't add matrices together, so we do it per row.
    Id row0, row1, row2;
    for (int i = 0; i < num_taps; ++i) {
      Id index;
      if (indices) {
        index = b.op_composite_extract(indices, {(uint32_t)i});
      } else {
        index = s._module.define_int_constant(i);
      }
      Id matrix = b.op_load(b.op_access_chain(tbl_transforms, {index}));
      matrix = s.mul(matrix, b.op_composite_extract(weights, {(uint32_t)i}));

      Id r0 = b.op_composite_extract(matrix, {0});
      Id r1 = b.op_composite_extract(matrix, {1});
      Id r2 = b.op_composite_extract(matrix, {2});
      if (i == 0) {
        row0 = r0;
        row1 = r1;
        row2 = r2;
      } else {
        row0 = s.add(row0, r0);
        row1 = s.add(row1, r1);
        row2 = s.add(row2, r2);
      }
    }
    Id vtxmat = b.op_composite_construct(s._mat3x4, {row0, row1, row2});

    position = s.vec4_of(s.mul(position, vtxmat), s.fconst(1.0f));
    if (normal) {
      normal = s.mul(normal, s.mat3_of(vtxmat));
    }
  }

  Id clip_pos = s.mul(position, s.load(mat_modelproj));
  s.store(o_position, clip_pos);
  if (o_hpos) {
    s.store(o_hpos, clip_pos);
  }

  if (trans_model_to_world) {
    Id model_to_world = s.load(trans_model_to_world);
    if (o_world_position) {
      s.store(o_world_position,
              s.vec4_of(s.mul(position, model_to_world), s.fconst(1.0f)));
    }
    if (o_world_normal) {
      Id world_normal = s.mul(s.vec4_of(normal, s.fconst(0.0f)), model_to_world);
      s.store(o_world_normal, s.vec4_of(world_normal, s.fconst(0.0f)));
    }
  }

  Id model_to_view;
  if (trans_model_to_view) {
    model_to_view = s.load(trans_model_to_view);
  }

  Id eye_position3, eye_position4;
  if (plan._need_eye_position || plan._need_point_size) {
    eye_position3 = s.mul(position, model_to_view);
    if (o_eye_position) {
      eye_position4 = s.vec4_of(eye_position3, s.fconst(1.0f));
      s.store(o_eye_position, eye_position4);
    }
  }

  Id pointparams;
  if (attr_pointparams) {
    pointparams = s.load(attr_pointparams);
  }
  Id point_size;
  if (plan._need_point_size) {
    point_size = s.add(s.comp(pointparams, 1),
                       s.div(s.comp(pointparams, 2), b.op_length(eye_position3)));
    s.store(o_point_size, point_size);
  }
  if (plan._calc_point_coord) {
    // The driver does not provide point coordinates, so we pass along what
    // the fragment shader needs to calculate them from the position.
    Id double_w = s.mul(s.comp(clip_pos, 3), s.fconst(2.0f));
    Id xy = s.div(s.swiz(clip_pos, {0, 1}), s.splat(double_w, 2));
    xy = s.add(xy, s.vconst(0.5f, 0.5f));
    Id inv_size;
    if (plan._need_point_size) {
      inv_size = s.div(s.fconst(1.0f), point_size);
    } else {
      // Static point size (non-perspective points)
      inv_size = s.div(s.fconst(1.0f), s.comp(pointparams, 0));
    }
    s.store(o_pointcoord, s.vec3_of(xy, inv_size));
  }

  // Pass through all texcoord inputs as-is.
  for (const auto &pair : texcoord_vars) {
    s.store(pair.second, s.load(pair.first));
  }
  if (vtx_color) {
    s.store(o_color, s.load(vtx_color));
  }
  if (plan._need_tangents) {
    Id model_to_view3 = s.mat3_of(model_to_view);
    Id vs_tangent = s.normalize(s.mul(tangent, model_to_view3));
    s.store(o_tangent, s.vec4_of(vs_tangent, s.fconst(0.0f)));
    Id vs_binormal = s.normalize(s.mul(s.neg(binormal), model_to_view3));
    s.store(o_binormal, s.vec4_of(vs_binormal, s.fconst(0.0f)));
  }
  for (size_t i = 0; i < key._lights.size(); ++i) {
    if (o_lightcoord[i]) {
      s.store(o_lightcoord[i], s.mul(eye_position4, s.load(mat_shadow[i])));
    }
  }
  if (need_heightmap) {
    Id eyedir = s.sub(s.xyz(s.load(mspos_view)), s.xyz(position));
    Id eyevec = s.vec3_of(s.dot(tangent, eyedir),
                          s.dot(binormal, eyedir),
                          s.dot(normal, eyedir));
    s.store(o_eyevec, s.normalize(eyevec));
  }
  if (plan._need_eye_normal) {
    Id eye_normal = s.normalize(s.mul(normal, s.load(tpose_view_to_model)));
    s.store(o_eye_normal, eye_normal);
  }

  return s.finish(ShaderModule::Stage::VERTEX);
}

/**
 * Generates the fragment shader module for the given key and plan.  This may
 * not use any mutable state of the ShaderGenerator, so that it can one day
 * run in parallel with generate_vertex_shader.
 */
PT(ShaderModule) ShaderGenerator::
generate_fragment_shader(const ShaderKey &key, const ShaderPlan &plan) const {
  StageBuilder s(spv::ExecutionModelFragment);
  SpirVBuilder &b = s._builder;

  bool have_fog = (key._flags & ShaderKey::F_FOG_MODE_MASK) != 0;
  bool need_heightmap = (key._texture_flags & ShaderKey::TF_map_height) != 0;
  bool have_color_write = (key._flags & ShaderKey::F_disable_color_write) == 0;
  bool have_aux = (key._flags & (ShaderKey::F_out_aux_normal | ShaderKey::F_out_aux_glow)) != 0;
  bool use_shadow_filter = (key._flags & ShaderKey::F_use_shadow_filter) != 0;
  bool have_lighting = (key._flags & ShaderKey::F_lighting) != 0;
  bool have_separate_ambient = (key._flags & ShaderKey::F_have_separate_ambient) != 0;
  size_t num_textures = key._textures.size();

  // Inputs.
  Id l_hpos, l_world_position, l_world_normal, l_eye_position, l_eye_normal;
  Id l_tangent, l_binormal, l_color, l_eyevec, l_pointcoord;
  Id pointcoord_builtin, fragcoord_builtin;
  if (have_fog) {
    l_hpos = s.make_input(s._vec4, "l_hpos", plan._hpos_location);
  }
  if (plan._need_point_coord) {
    if (!plan._calc_point_coord) {
      // The driver provides point coordinates.
      pointcoord_builtin = s.make_builtin_input(spv::BuiltInPointCoord);
    } else {
      // We need to calculate them from the fragment coordinates.
      l_pointcoord = s.make_input(s._vec3, "l_pointcoord", plan._pointcoord_location);
      fragcoord_builtin = s.make_builtin_input(spv::BuiltInFragCoord);
    }
  }
  if (plan._need_world_position) {
    l_world_position = s.make_input(s._vec4, "l_world_position", plan._world_position_location);
  }
  if (plan._need_world_normal) {
    l_world_normal = s.make_input(s._vec4, "l_world_normal", plan._world_normal_location);
  }
  if (plan._need_eye_position) {
    l_eye_position = s.make_input(s._vec4, "l_eye_position", plan._eye_position_location);
  }
  if (plan._need_eye_normal) {
    l_eye_normal = s.make_input(s._vec3, "l_eye_normal", plan._eye_normal_location);
  }
  pvector<Id> texcoord_inputs;
  for (const auto &pair : plan._texcoord_locations) {
    texcoord_inputs.push_back(
      s.make_input(s._vec4, "l_" + pair.first->join("_"), pair.second));
  }
  if (plan._need_tangents) {
    l_tangent = s.make_input(s._vec4, "l_tangent", plan._tangent_location);
    l_binormal = s.make_input(s._vec4, "l_binormal", plan._binormal_location);
  }
  if (need_heightmap) {
    l_eyevec = s.make_input(s._vec3, "l_eyevec", plan._eyevec_location);
  }
  if (plan._color_location >= 0) {
    l_color = s.make_input(s._vec4, "l_color", plan._color_location);
  }
  pvector<Id> l_lightcoord(key._lights.size(), Id());
  for (size_t i = 0; i < key._lights.size(); ++i) {
    if (key._lights[i] & ShaderKey::LF_has_shadows) {
      l_lightcoord[i] = s.make_input(s._vec4, "l_lightcoord" + format_string(i),
                                     plan._lightcoord_locations[i]);
    }
  }

  // Uniforms.
  Id attr_fog, attr_fogcolor, sys_windowsize, trans_view_to_world;
  if (have_fog) {
    attr_fog = s.make_uniform(s._vec4, "attr_fog");
    attr_fogcolor = s.make_uniform(s._vec4, "attr_fogcolor");
  }
  if (plan._calc_point_coord) {
    sys_windowsize = s.make_uniform(s._vec2, "sys_windowsize");
  }
  if (plan._need_fragment_view_to_world) {
    trans_view_to_world = s.make_uniform(s._mat3, "trans_view_to_world");
  }

  pvector<Id> tex_vars(num_textures, Id());
  pvector<Id> texscale_vars(num_textures, Id());
  pvector<Id> texmat_vars(num_textures, Id());
  pvector<Id> texcolor_vars(num_textures, Id());
  pvector<Id> texconst_vars(num_textures, Id());
  for (size_t i = 0; i < num_textures; ++i) {
    const ShaderKey::TextureInfo &tex = key._textures[i];
    if (tex._mode == TextureStage::M_modulate && tex._flags == 0) {
      // Skip this stage.
      continue;
    }
    tex_vars[i] = s.make_sampler(tex._type, false, "tex_" + format_string(i));

    if (tex._flags & ShaderKey::TF_has_texscale) {
      texscale_vars[i] = s.make_uniform(s._vec3, "texscale_" + format_string(i));
    } else if (tex._flags & ShaderKey::TF_has_texmat) {
      texmat_vars[i] = s.make_uniform(s._mat4, "texmat_" + format_string(i));
    }
    if (tex._flags & ShaderKey::TF_uses_color) {
      texcolor_vars[i] = s.make_uniform(s._vec4, "texcolor_" + format_string(i));
    }
    if (tex._gen_mode == TexGenAttrib::M_constant) {
      texconst_vars[i] = s.make_uniform(s._vec4, "texconst_" + format_string(i));
    }
  }

  pvector<Id> attr_light(key._lights.size(), Id());
  pvector<Id> attr_lspec(key._lights.size(), Id());
  pvector<Id> shadow_vars(key._lights.size(), Id());
  for (size_t i = 0; i < key._lights.size(); ++i) {
    int light_flags = key._lights[i];
    attr_light[i] = s.make_uniform(s._mat4, "attr_light" + format_string(i));

    if (light_flags & ShaderKey::LF_has_shadows) {
      ShaderKey::LightFlags light_type =
        (ShaderKey::LightFlags)(light_flags & ShaderKey::LF_TYPE_MASK);
      bool is_cube = (light_type == ShaderKey::LF_type_point ||
                      light_type == ShaderKey::LF_type_sphere);
      shadow_vars[i] = s.make_sampler(
        is_cube ? Texture::TT_cube_map : Texture::TT_2d_texture,
        use_shadow_filter, "shadow_" + format_string(i));
    }
    if (light_flags & ShaderKey::LF_has_specular_color) {
      attr_lspec[i] = s.make_uniform(s._vec4, "attr_lspec" + format_string(i));
    }
  }

  Id attr_material;
  if (key._flags & (Material::F_ambient | Material::F_diffuse |
                    Material::F_emission | Material::F_specular)) {
    attr_material = s.make_uniform(s._mat4, "attr_material");
  }
  Id attr_color_var;
  if (plan._need_color && (key._flags & ShaderKey::F_flat_color) != 0) {
    attr_color_var = s.make_uniform(s._vec4, "attr_color");
  }
  pvector<Id> clipplane_vars;
  for (int i = 0; i < key._num_clip_planes; ++i) {
    clipplane_vars.push_back(s.make_uniform(s._vec4, "clipplane_" + format_string(i)));
  }
  Id attr_ambient;
  if (have_lighting) {
    attr_ambient = s.make_uniform(s._vec4, "attr_ambient");
  }
  Id attr_colorscale_var = s.make_uniform(s._vec4, "attr_colorscale");

  // Outputs.
  Id o_color, o_aux;
  if (have_color_write) {
    o_color = s.make_output(s._vec4, "o_color", 0);
  }
  if (have_aux) {
    o_aux = s.make_output(s._vec4, "o_aux", 1);
  }

  // Now generate the function body.
  Id world_position;
  if (l_world_position) {
    world_position = s.load(l_world_position);
  }
  Id eye_position;
  if (l_eye_position) {
    eye_position = s.load(l_eye_position);
  }

  // Clipping first!
  for (int i = 0; i < key._num_clip_planes; ++i) {
    Id plane = s.load(clipplane_vars[i]);
    Id dist = s.add(s.dot(s.xyz(world_position), s.xyz(plane)), s.comp(plane, 3));
    s.kill_if(b.op_compare(spv::OpFOrdLessThanEqual, dist, s.fconst(0.0f)));
  }

  Id eye_normal;
  if (l_eye_normal) {
    // Correct the surface normal for interpolation effects.
    eye_normal = s.normalize(s.load(l_eye_normal));
  }
  Id norm_eye_position;
  if (plan._need_eye_reflection ||
      (plan._need_eye_position && plan._have_specular &&
       (key._flags & Material::F_local) != 0 && !key._lights.empty())) {
    norm_eye_position = s.normalize(s.xyz(eye_position));
  }
  Id eye_reflection;
  if (plan._need_eye_reflection) {
    Id ndot = s.dot(eye_normal, norm_eye_position);
    eye_reflection = s.sub(norm_eye_position,
                           s.mul(eye_normal, s.mul(ndot, s.fconst(2.0f))));
  }

  Id aux_rgb, aux_a;
  if (have_aux) {
    aux_rgb = s.vconst(0.0f, 0.0f, 0.0f);
    aux_a = s.fconst(0.0f);
  }

  auto find_texcoord_input = [&](const InternalName *name) -> Id {
    for (size_t ti = 0; ti < plan._texcoord_locations.size(); ++ti) {
      if (plan._texcoord_locations[ti].first == name) {
        return texcoord_inputs[ti];
      }
    }
    return Id();
  };

  // Now generate any texture coordinates according to TexGenAttrib.  If it
  // has a TexMatrixAttrib, also transform them.
  pvector<Id> texcoords(num_textures, Id());
  for (size_t i = 0; i < num_textures; ++i) {
    const ShaderKey::TextureInfo &tex = key._textures[i];
    if (tex._mode == TextureStage::M_modulate && tex._flags == 0) {
      // Skip this stage.
      continue;
    }
    Id texcoord;
    switch (tex._gen_mode) {
    case TexGenAttrib::M_off:
      texcoord = s.load(find_texcoord_input(tex._texcoord_name));
      break;
    case TexGenAttrib::M_eye_sphere_map:
      {
        Id offset_refl = s.add(eye_reflection, s.vconst(0.0f, -1.0f, 0.0f));
        Id scale = s.div(s.fconst(1.0f),
                         s.mul(b.op_length(offset_refl), s.fconst(2.0f)));
        Id xz = s.mul(s.swiz(eye_reflection, {0, 2}), scale);
        xz = s.add(xz, s.vconst(0.5f, 0.5f));
        texcoord = s.vec4_of(xz, s.fconst(0.0f), s.fconst(1.0f));
      }
      break;
    case TexGenAttrib::M_world_cube_map:
      texcoord = s.vec4_of(s.mul(eye_reflection, s.load(trans_view_to_world)),
                           s.fconst(1.0f));
      break;
    case TexGenAttrib::M_eye_cube_map:
      texcoord = s.vec4_of(eye_reflection, s.fconst(1.0f));
      break;
    case TexGenAttrib::M_world_normal:
      texcoord = s.load(l_world_normal);
      break;
    case TexGenAttrib::M_eye_normal:
      texcoord = s.vec4_of(eye_normal, s.fconst(1.0f));
      break;
    case TexGenAttrib::M_world_position:
      texcoord = world_position;
      break;
    case TexGenAttrib::M_eye_position:
      texcoord = s.vec4_of(s.xyz(eye_position), s.fconst(1.0f));
      break;
    case TexGenAttrib::M_point_sprite:
      if (!plan._calc_point_coord) {
        texcoord = s.vec4_of(s.load(pointcoord_builtin), s.fconst(0.0f), s.fconst(1.0f));
      } else {
        Id pointcoord = s.load(l_pointcoord);
        Id fragcoord = s.swiz(s.load(fragcoord_builtin), {0, 1});
        Id xy = s.sub(fragcoord, s.mul(s.swiz(pointcoord, {0, 1}),
                                       s.load(sys_windowsize)));
        xy = s.mul(xy, s.comp(pointcoord, 2));
        xy = s.mul(xy, s.vconst(1.0f, -1.0f));
        xy = s.sub(xy, s.vconst(0.5f, 0.5f));
        texcoord = s.vec4_of(xy, s.fconst(0.0f), s.fconst(1.0f));
      }
      break;
    case TexGenAttrib::M_constant:
      texcoord = s.load(texconst_vars[i]);
      break;
    default:
      texcoord = s.vconst(0.0f, 0.0f, 0.0f, 0.0f);
      pgraphnodes_cat.error()
        << "Unsupported TexGenAttrib mode: " << tex._gen_mode << "\n";
    }

    if (tex._flags & ShaderKey::TF_has_texscale) {
      texcoord = s.set_xyz(texcoord,
        s.mul(s.xyz(texcoord), s.load(texscale_vars[i])));
    } else if (tex._flags & ShaderKey::TF_has_texmat) {
      texcoord = s.mul(texcoord, s.load(texmat_vars[i]));
      texcoord = s.set_xyz(texcoord,
        s.div(s.xyz(texcoord), s.splat(s.comp(texcoord, 3), 3)));
    }
    texcoords[i] = texcoord;
  }

  // Samples the given texture stage with the given texture coordinates.
  auto sample_texture = [&](const ShaderKey::TextureInfo &tex, Id var,
                            Id texcoord, bool allow_proj) -> Id {
    Id image = s.load(var);
    switch (tex._type) {
    case Texture::TT_cube_map_array:
      return b.op_image_sample(image, texcoord);
    case Texture::TT_cube_map:
    case Texture::TT_3d_texture:
    case Texture::TT_2d_texture_array:
      return b.op_image_sample(image, s.xyz(texcoord));
    case Texture::TT_2d_texture:
      if (allow_proj) {
        // Regular sampling is done projectively, dividing by w.
        return b.op_image_sample_proj(image, s.swiz(texcoord, {0, 1, 3}));
      }
      return b.op_image_sample(image, s.swiz(texcoord, {0, 1}));
    case Texture::TT_1d_texture_array:
      return b.op_image_sample(image, s.swiz(texcoord, {0, 1}));
    case Texture::TT_1d_texture:
      return b.op_image_sample(image, s.comp(texcoord, 0));
    case Texture::TT_buffer_texture:
      return b.op_image_fetch(image,
        b.op_convert(ShaderType::ST_int, s.comp(texcoord, 0)));
    default:
      pgraphnodes_cat.error() << "Unsupported texture type!\n";
      return b.op_image_sample(image, s.swiz(texcoord, {0, 1}));
    }
  };

  // Fetch the height maps first and apply parallax mapping.
  pvector<Id> tex_vals(num_textures, Id());
  Id parallax_offset;
  if (need_heightmap) {
    Id eyevec = s.load(l_eyevec);

    for (size_t i = 0; i < num_textures; ++i) {
      const ShaderKey::TextureInfo &tex = key._textures[i];
      if ((tex._flags & ShaderKey::TF_map_height) == 0) {
        continue;
      }
      Id sample = sample_texture(tex, tex_vars[i], texcoords[i], false);
      tex_vals[i] = sample;

      Id height;
      if (tex._mode == TextureStage::M_normal_height ||
          (tex._flags & ShaderKey::TF_has_alpha) != 0) {
        height = s.splat(s.comp(sample, 3), 3);
      } else {
        height = s.xyz(sample);
      }
      // parallax_offset = l_eyevec.xyz * (height * 2 - 1) * scale
      Id expanded = s.sub(s.mul(height, s.fconst(2.0f)), s.splat_const(1.0f, 3));
      parallax_offset = s.mul(s.mul(eyevec, expanded),
                              s.fconst((float)(double)parallax_mapping_scale));
      // Additional samples
      for (int j = 0; j < (int)parallax_mapping_samples - 1; ++j) {
        parallax_offset = s.mul(s.mul(eyevec, s.add(parallax_offset, expanded)),
                                s.fconst((float)(0.5 * parallax_mapping_scale)));
      }
    }
  }

  // Fetch all the other textures.
  for (size_t i = 0; i < num_textures; ++i) {
    const ShaderKey::TextureInfo &tex = key._textures[i];
    if (tex._mode == TextureStage::M_modulate && tex._flags == 0) {
      // Skip this stage.
      continue;
    }
    if (tex._flags & ShaderKey::TF_map_height) {
      // Already sampled above.
      continue;
    }
    Id texcoord = texcoords[i];
    if (need_heightmap) {
      // Parallax mapping pushes the texture coordinates of the other
      // textures away from the camera.
      texcoord = s.set_xyz(texcoord, s.sub(s.xyz(texcoord), parallax_offset));
    }
    tex_vals[i] = sample_texture(tex, tex_vars[i], texcoord, true);
  }

  if (plan._need_tangents) {
    // Translate tangent-space normal in map to view-space.  Use Reoriented
    // Normal Mapping to blend additional normal maps.
    Id tsnormal;
    bool is_first = true;
    for (size_t i = 0; i < num_textures; ++i) {
      const ShaderKey::TextureInfo &tex = key._textures[i];
      if ((tex._flags & ShaderKey::TF_map_normal) == 0) {
        continue;
      }
      Id expanded = s.sub(s.mul(s.xyz(tex_vals[i]), s.fconst(2.0f)),
                          s.splat_const(1.0f, 3));
      if (is_first) {
        if (tex._flags & ShaderKey::TF_has_texscale) {
          tsnormal = s.normalize(s.mul(expanded, s.load(texscale_vars[i])));
        } else if (tex._flags & ShaderKey::TF_has_texmat) {
          tsnormal = s.normalize(s.xyz(s.mul(s.vec4_of(expanded, s.fconst(0.0f)),
                                             s.load(texmat_vars[i]))));
        } else {
          tsnormal = s.normalize(expanded);
        }
        is_first = false;
        continue;
      }
      tsnormal = b.op_composite_insert(
        s.add(s.comp(tsnormal, 2), s.fconst(1.0f)), tsnormal, {2});
      Id tmp = s.add(s.mul(s.xyz(tex_vals[i]), s.vconst(-2.0f, -2.0f, 2.0f)),
                     s.vconst(1.0f, 1.0f, -1.0f));
      if (tex._flags & ShaderKey::TF_has_texscale) {
        tmp = s.mul(tmp, s.load(texscale_vars[i]));
      } else if (tex._flags & ShaderKey::TF_has_texmat) {
        tmp = s.xyz(s.mul(s.vec4_of(tmp, s.fconst(0.0f)), s.load(texmat_vars[i])));
      }
      tsnormal = s.normalize(s.sub(s.mul(tsnormal, s.dot(tsnormal, tmp)),
                                   s.mul(tmp, s.comp(tsnormal, 2))));
    }
    eye_normal = s.mul(eye_normal, s.comp(tsnormal, 2));
    eye_normal = s.add(eye_normal, s.mul(s.normalize(s.xyz(s.load(l_tangent))),
                                         s.comp(tsnormal, 0)));
    eye_normal = s.add(eye_normal, s.mul(s.normalize(s.xyz(s.load(l_binormal))),
                                         s.comp(tsnormal, 1)));
    eye_normal = s.normalize(eye_normal);
  }
  if (key._flags & ShaderKey::F_out_aux_normal) {
    // Output the camera-space surface normal.
    aux_rgb = s.add(s.mul(eye_normal, s.fconst(0.5f)), s.vconst(0.5f, 0.5f, 0.5f));
  }

  Id material;
  if (attr_material) {
    material = s.load(attr_material);
  }
  Id color_val;
  if (l_color) {
    color_val = s.load(l_color);
  } else if (attr_color_var) {
    color_val = s.load(attr_color_var);
  }

  // Begin view-space light calculations.
  Id tot_ambient, tot_diffuse, tot_specular, shininess;
  if (have_lighting) {
    Id ambient = s.load(attr_ambient);
    if (have_separate_ambient) {
      tot_ambient = ambient;
      tot_diffuse = s.vconst(0.0f, 0.0f, 0.0f, 0.0f);
    } else {
      tot_diffuse = ambient;
    }
    if (plan._have_specular) {
      tot_specular = s.vconst(0.0f, 0.0f, 0.0f, 0.0f);
      if (key._flags & Material::F_specular) {
        shininess = b.op_composite_extract(material, {3, 3});
      } else {
        // No shininess specified, use default.
        shininess = s.fconst(50.0f);
      }
    }
  }

  for (size_t i = 0; i < key._lights.size(); ++i) {
    int light_flags = key._lights[i];
    ShaderKey::LightFlags light_type =
      (ShaderKey::LightFlags)(light_flags & ShaderKey::LF_TYPE_MASK);
    bool has_shadows = (light_flags & ShaderKey::LF_has_shadows) != 0;

    Id light = s.load(attr_light[i]);
    Id lightcoord;
    if (has_shadows) {
      lightcoord = s.load(l_lightcoord[i]);
    }

    Id lcolor = b.op_composite_extract(light, {0});
    Id lspec;
    if (plan._have_specular) {
      if (light_flags & ShaderKey::LF_has_specular_color) {
        lspec = s.load(attr_lspec[i]);
      } else {
        lspec = lcolor;
      }
    }

    // Samples the shadow map of a directional or spot light.
    auto sample_shadow_2d = [&]() -> Id {
      if (use_shadow_filter) {
        // Do the projective division ourselves rather than using the
        // ProjDref instruction, which the NVIDIA driver fails to translate
        // ("Can't convert to expr: textureProj").  This also matches what
        // the Cg preamble used to do for shadow2DProj.
        Id q = s.comp(lightcoord, 3);
        Id coord = s.div(s.swiz(lightcoord, {0, 1}), s.splat(q, 2));
        Id dref = s.div(s.comp(lightcoord, 2), q);
        return b.op_image_sample_dref(s.load(shadow_vars[i]), coord, dref);
      } else {
        Id sample = b.op_image_sample_proj(s.load(shadow_vars[i]),
                                           s.swiz(lightcoord, {0, 1, 3}));
        return s.bool_to_float(b.op_compare(spv::OpFOrdGreaterThan,
          s.comp(sample, 0),
          s.div(s.comp(lightcoord, 2), s.comp(lightcoord, 3))));
      }
    };

    if (light_type == ShaderKey::LF_type_directional) {
      Id lvec = s.xyz(b.op_composite_extract(light, {3}));
      lcolor = s.mul(lcolor, s.sat(s.dot(eye_normal, lvec)));
      if (has_shadows) {
        Id lshad = sample_shadow_2d();
        lcolor = s.mul(lcolor, lshad);
        if (lspec) {
          lspec = s.mul(lspec, lshad);
        }
      }
      tot_diffuse = s.add(tot_diffuse, lcolor);
      if (plan._have_specular) {
        Id lhalf;
        if (key._flags & Material::F_local) {
          lhalf = s.normalize(s.sub(lvec, norm_eye_position));
        } else {
          lhalf = s.normalize(s.sub(lvec, s.vconst(0.0f, 1.0f, 0.0f)));
        }
        lspec = s.mul(lspec, b.op_pow(s.sat(s.dot(eye_normal, lhalf)), shininess));
        tot_specular = s.add(tot_specular, lspec);
      }
    }
    else if (light_type == ShaderKey::LF_type_point ||
             light_type == ShaderKey::LF_type_sphere) {
      Id latten = b.op_composite_extract(light, {1});
      Id lpoint = b.op_composite_extract(light, {3});
      Id lvec = s.sub(s.xyz(lpoint), s.xyz(eye_position));
      Id ldist = b.op_length(lvec);
      lvec = s.div(lvec, s.splat(ldist, 3));
      if (light_type == ShaderKey::LF_type_sphere) {
        ldist = b.op_max(ldist, b.op_composite_extract(light, {2, 3}));
      }
      Id lattenv = s.div(s.fconst(1.0f),
        s.add(s.add(s.comp(latten, 0), s.mul(s.comp(latten, 1), ldist)),
              s.mul(s.comp(latten, 2), s.mul(ldist, ldist))));
      lcolor = s.mul(lcolor, s.mul(lattenv, s.sat(s.dot(eye_normal, lvec))));
      if (has_shadows) {
        // Reconstruct the depth value from the cube map direction vector.
        Id dist = b.op_max(b.op_abs(s.comp(lightcoord, 0)),
                  b.op_max(b.op_abs(s.comp(lightcoord, 1)),
                           b.op_abs(s.comp(lightcoord, 2))));
        Id far_plane = s.comp(latten, 3);
        Id near_plane = s.comp(lpoint, 3);
        Id range = s.sub(far_plane, near_plane);
        dist = s.add(s.div(s.add(far_plane, near_plane), range),
                     s.div(s.mul(s.mul(far_plane, near_plane), s.fconst(-2.0f)),
                           s.mul(dist, range)));
        Id dref = s.add(s.mul(dist, s.fconst(0.5f)), s.fconst(0.5f));
        Id lshad;
        if (use_shadow_filter) {
          lshad = b.op_image_sample_dref(s.load(shadow_vars[i]),
                                         s.xyz(lightcoord), dref);
        } else {
          Id sample = b.op_image_sample(s.load(shadow_vars[i]), s.xyz(lightcoord));
          lshad = s.bool_to_float(b.op_compare(spv::OpFOrdGreaterThanEqual,
                                               s.comp(sample, 0), dref));
        }
        lcolor = s.mul(lcolor, lshad);
        if (lspec) {
          lspec = s.mul(lspec, lshad);
        }
      }
      tot_diffuse = s.add(tot_diffuse, lcolor);
      if (plan._have_specular) {
        Id lhalf;
        if (key._flags & Material::F_local) {
          lhalf = s.normalize(s.sub(lvec, norm_eye_position));
        } else {
          lhalf = s.normalize(s.sub(lvec, s.vconst(0.0f, 1.0f, 0.0f)));
        }
        lspec = s.mul(lspec, lattenv);
        lspec = s.mul(lspec, b.op_pow(s.sat(s.dot(eye_normal, lhalf)), shininess));
        tot_specular = s.add(tot_specular, lspec);
      }
    }
    else if (light_type == ShaderKey::LF_type_spot) {
      Id latten = b.op_composite_extract(light, {1});
      Id ldir = b.op_composite_extract(light, {2});
      Id lpoint = b.op_composite_extract(light, {3});
      Id lvec = s.sub(s.xyz(lpoint), s.xyz(eye_position));
      Id ldist = b.op_length(lvec);
      lvec = s.div(lvec, s.splat(ldist, 3));
      Id langle = s.sat(s.dot(s.xyz(ldir), lvec));
      Id lattenv = s.div(s.fconst(1.0f),
        s.add(s.add(s.comp(latten, 0), s.mul(s.comp(latten, 1), ldist)),
              s.mul(s.comp(latten, 2), s.mul(ldist, ldist))));
      lattenv = s.mul(lattenv, b.op_pow(langle, s.comp(latten, 3)));
      lattenv = b.op_select(
        b.op_compare(spv::OpFOrdLessThan, langle, s.comp(ldir, 3)),
        s.fconst(0.0f), lattenv);
      lcolor = s.mul(lcolor, s.mul(lattenv, s.sat(s.dot(eye_normal, lvec))));
      if (has_shadows) {
        Id lshad = sample_shadow_2d();
        lcolor = s.mul(lcolor, lshad);
        if (lspec) {
          lspec = s.mul(lspec, lshad);
        }
      }
      tot_diffuse = s.add(tot_diffuse, lcolor);
      if (plan._have_specular) {
        Id lhalf;
        if (key._flags & Material::F_local) {
          lhalf = s.normalize(s.sub(lvec, norm_eye_position));
        } else {
          lhalf = s.normalize(s.sub(lvec, s.vconst(0.0f, 1.0f, 0.0f)));
        }
        lspec = s.mul(lspec, lattenv);
        lspec = s.mul(lspec, b.op_pow(s.sat(s.dot(eye_normal, lhalf)), shininess));
        tot_specular = s.add(tot_specular, lspec);
      }
    }
  }

  Id result;
  if (have_lighting) {
    switch (key._light_ramp_mode) {
    case LightRampAttrib::LRT_single_threshold:
      {
        // Single-threshold light ramp.
        Id lr_in = s.dot(s.xyz(tot_diffuse), s.vconst(0.33f, 0.34f, 0.33f));
        Id lr_scale = b.op_select(
          b.op_compare(spv::OpFOrdLessThan, lr_in,
                       s.fconst((float)key._light_ramp_threshold[0])),
          s.fconst(0.0f),
          s.div(s.fconst((float)key._light_ramp_level[0]), lr_in));
        tot_diffuse = s.mul(tot_diffuse, lr_scale);
        break;
      }
    case LightRampAttrib::LRT_double_threshold:
      {
        // Double-threshold light ramp.
        Id lr_in = s.dot(s.xyz(tot_diffuse), s.vconst(0.33f, 0.34f, 0.33f));
        Id lr_out = b.op_select(
          b.op_compare(spv::OpFOrdGreaterThan, lr_in,
                       s.fconst((float)key._light_ramp_threshold[0])),
          s.fconst((float)key._light_ramp_level[0]), s.fconst(0.0f));
        lr_out = b.op_select(
          b.op_compare(spv::OpFOrdGreaterThan, lr_in,
                       s.fconst((float)key._light_ramp_threshold[1])),
          s.fconst((float)key._light_ramp_level[1]), lr_out);
        tot_diffuse = s.mul(tot_diffuse, s.div(lr_out, lr_in));
        break;
      }
    default:
      break;
    }

    // Begin view-space light summation.
    if (key._flags & Material::F_emission) {
      result = b.op_composite_extract(material, {2});
    } else if (key._texture_flags & (ShaderKey::TF_map_glow | ShaderKey::TF_map_emission)) {
      result = s.vconst(1.0f, 1.0f, 1.0f, 0.0f);
    } else {
      result = s.vconst(0.0f, 0.0f, 0.0f, 0.0f);
    }
    if (key._texture_flags & ShaderKey::TF_map_emission) {
      result = s.set_xyz(result,
        s.mul(s.xyz(result), s.xyz(tex_vals[plan._map_index_emission])));
    }
    if (key._texture_flags & ShaderKey::TF_map_glow) {
      result = s.mul(result, s.sat(s.mul(
        s.sub(s.comp(tex_vals[plan._map_index_glow], 3), s.fconst(0.5f)),
        s.fconst(2.0f))));
    }
    if (have_separate_ambient) {
      for (size_t i = 0; i < num_textures; ++i) {
        if (key._textures[i]._flags & ShaderKey::TF_map_occlusion) {
          tot_ambient = s.mul(tot_ambient, s.comp(tex_vals[i], 0));
        }
      }
      if (key._flags & Material::F_ambient) {
        result = s.add(result, s.mul(tot_ambient, b.op_composite_extract(material, {0})));
      } else if (color_val) {
        result = s.add(result, s.mul(tot_ambient, color_val));
      } else {
        result = s.add(result, tot_ambient);
      }
    }
    if (key._flags & Material::F_diffuse) {
      result = s.add(result, s.mul(tot_diffuse, b.op_composite_extract(material, {1})));
    } else if (color_val) {
      result = s.add(result, s.mul(tot_diffuse, color_val));
    } else {
      result = s.add(result, tot_diffuse);
    }
    if (key._light_ramp_mode == LightRampAttrib::LRT_default) {
      result = s.sat(result);
    }
    // End view-space light calculations.

    // Combine in alpha, which bypasses lighting calculations.
    if (key._flags & ShaderKey::F_calc_primary_alpha) {
      Id alpha;
      if (key._flags & Material::F_diffuse) {
        alpha = b.op_composite_extract(material, {1, 3});
      } else if (color_val) {
        alpha = s.comp(color_val, 3);
      } else {
        alpha = s.fconst(1.0f);
      }
      result = s.set_w(result, alpha);
    }
  } else {
    if (color_val) {
      result = color_val;
    } else {
      result = s.vconst(1.0f, 1.0f, 1.0f, 1.0f);
    }
  }

  // Apply the color scale.
  Id colorscale = s.load(attr_colorscale_var);
  result = s.mul(result, colorscale);

  // Store these if any stages will use it.
  Id primary_color, last_saved_result;
  if (key._texture_flags & ShaderKey::TF_uses_primary_color) {
    primary_color = result;
  }
  if (key._texture_flags & ShaderKey::TF_uses_last_saved_result) {
    last_saved_result = result;
  }

  pvector<Id> texcolor_vals(num_textures, Id());
  auto get_texcolor = [&](size_t i) -> Id {
    if (!texcolor_vals[i]) {
      texcolor_vals[i] = s.load(texcolor_vars[i]);
    }
    return texcolor_vals[i];
  };

  // Emits one source of a combine mode, as a vec3 (for the rgb combiner) or
  // a scalar (for the alpha combiner).
  auto combine_source = [&](const ShaderKey::TextureInfo &tex, int n,
                            bool alpha, size_t i) -> Id {
    TextureStage::CombineSource c_src;
    TextureStage::CombineOperand c_op;
    if (!alpha) {
      c_src = UNPACK_COMBINE_SRC(tex._combine_rgb, n);
      c_op = UNPACK_COMBINE_OP(tex._combine_rgb, n);
    } else {
      c_src = UNPACK_COMBINE_SRC(tex._combine_alpha, n);
      c_op = UNPACK_COMBINE_OP(tex._combine_alpha, n);
    }
    Id src;
    switch (c_src) {
    case TextureStage::CS_undefined:
    case TextureStage::CS_texture:
      src = tex_vals[i];
      break;
    case TextureStage::CS_constant:
      src = get_texcolor(i);
      break;
    case TextureStage::CS_primary_color:
      src = primary_color;
      break;
    case TextureStage::CS_previous:
      src = result;
      break;
    case TextureStage::CS_constant_color_scale:
      src = s.mul(colorscale, get_texcolor(i));
      break;
    case TextureStage::CS_last_saved_result:
      src = last_saved_result;
      break;
    }
    bool one_minus = (c_op == TextureStage::CO_one_minus_src_color ||
                      c_op == TextureStage::CO_one_minus_src_alpha);
    bool use_color = (c_op == TextureStage::CO_src_color ||
                      c_op == TextureStage::CO_one_minus_src_color);
    if (alpha || !use_color) {
      Id value = s.comp(src, 3);
      if (one_minus) {
        value = s.sat(s.sub(s.fconst(1.0f), value));
      }
      if (alpha) {
        return value;
      }
      // Dunno if it's legal in the FFP at all, but let's just allow it.
      return s.splat(value, 3);
    } else {
      Id value = s.xyz(src);
      if (one_minus) {
        value = s.sat(s.sub(s.splat_const(1.0f, 3), value));
      }
      return value;
    }
  };

  // Emits the scalar dot3 combine calculation, using the sources of the rgb
  // or alpha combiner as indicated.  Scalar (alpha) sources are broadcast,
  // like Cg promoted them in the original generator.
  auto combine_dot3 = [&](const ShaderKey::TextureInfo &tex, bool alpha, size_t i) -> Id {
    Id half3 = s.splat_const(0.5f, 3);
    Id src0 = combine_source(tex, 0, alpha, i);
    Id src1 = combine_source(tex, 1, alpha, i);
    if (alpha) {
      src0 = s.splat(src0, 3);
      src1 = s.splat(src1, 3);
    }
    return s.mul(s.dot(s.sub(src0, half3), s.sub(src1, half3)), s.fconst(4.0f));
  };

  // Emits a combine mode, as a vec3 (rgb combiner) or scalar (alpha).
  auto combine_mode = [&](const ShaderKey::TextureInfo &tex,
                          TextureStage::CombineMode c_mode,
                          bool alpha, size_t i) -> Id {
    switch (c_mode) {
    case TextureStage::CM_modulate:
      return s.mul(combine_source(tex, 0, alpha, i),
                   combine_source(tex, 1, alpha, i));
    case TextureStage::CM_add:
      return s.add(combine_source(tex, 0, alpha, i),
                   combine_source(tex, 1, alpha, i));
    case TextureStage::CM_add_signed:
      {
        Id value = s.add(combine_source(tex, 0, alpha, i),
                         combine_source(tex, 1, alpha, i));
        if (alpha) {
          return s.sub(value, s.fconst(0.5f));
        } else {
          return s.sub(value, s.splat_const(0.5f, 3));
        }
      }
    case TextureStage::CM_interpolate:
      return b.op_mix(combine_source(tex, 1, alpha, i),
                      combine_source(tex, 0, alpha, i),
                      combine_source(tex, 2, alpha, i));
    case TextureStage::CM_subtract:
      return s.sub(combine_source(tex, 0, alpha, i),
                   combine_source(tex, 1, alpha, i));
    case TextureStage::CM_dot3_rgb:
    case TextureStage::CM_dot3_rgba:
      {
        Id value = combine_dot3(tex, alpha, i);
        return alpha ? value : s.splat(value, 3);
      }
    case TextureStage::CM_replace:
    default: // Not sure if this is correct as default value.
      return combine_source(tex, 0, alpha, i);
    }
  };

  // Now loop through the textures to compose our magic blending formulas.
  for (size_t i = 0; i < num_textures; ++i) {
    const ShaderKey::TextureInfo &tex = key._textures[i];
    if (tex._mode == TextureStage::M_modulate && tex._flags == 0) {
      // Skip this stage.
      continue;
    }
    switch (tex._mode) {
    case TextureStage::M_modulate:
      if ((tex._flags & ShaderKey::TF_has_rgb) != 0 &&
          (tex._flags & ShaderKey::TF_has_alpha) != 0) {
        result = s.mul(result, tex_vals[i]);
      } else if (tex._flags & ShaderKey::TF_has_alpha) {
        result = s.set_w(result, s.mul(s.comp(result, 3), s.comp(tex_vals[i], 3)));
      } else if (tex._flags & ShaderKey::TF_has_rgb) {
        result = s.set_xyz(result, s.mul(s.xyz(result), s.xyz(tex_vals[i])));
      }
      break;
    case TextureStage::M_modulate_glow:
    case TextureStage::M_modulate_gloss:
      // in the case of glow or spec we currently see the specularity evenly
      // across the surface even if transparency or masking is present not
      // sure if this is desired behavior or not.  *MOST* would construct a
      // spec map based off of what isisn't seen based on the masktransparency
      // this may have to be left alone for now agartner
      result = s.set_xyz(result, s.mul(s.xyz(result), s.xyz(tex_vals[i])));
      break;
    case TextureStage::M_decal:
      result = s.set_xyz(result, b.op_mix(s.xyz(result), s.xyz(tex_vals[i]),
                                          s.splat(s.comp(tex_vals[i], 3), 3)));
      break;
    case TextureStage::M_blend:
      result = s.set_xyz(result, b.op_mix(s.xyz(result), s.xyz(get_texcolor(i)),
                                          s.xyz(tex_vals[i])));
      if (key._flags & ShaderKey::F_calc_primary_alpha) {
        result = s.set_w(result, s.mul(s.comp(result, 3), s.comp(tex_vals[i], 3)));
      }
      break;
    case TextureStage::M_replace:
      result = tex_vals[i];
      break;
    case TextureStage::M_add:
      result = s.set_xyz(result, s.add(s.xyz(result), s.xyz(tex_vals[i])));
      if (key._flags & ShaderKey::F_calc_primary_alpha) {
        result = s.set_w(result, s.mul(s.comp(result, 3), s.comp(tex_vals[i], 3)));
      }
      break;
    case TextureStage::M_combine:
      {
        TextureStage::CombineMode combine_rgb = (TextureStage::CombineMode)
          ((tex._flags & ShaderKey::TF_COMBINE_RGB_MODE_MASK) >> ShaderKey::TF_COMBINE_RGB_MODE_SHIFT);
        TextureStage::CombineMode combine_alpha = (TextureStage::CombineMode)
          ((tex._flags & ShaderKey::TF_COMBINE_ALPHA_MODE_MASK) >> ShaderKey::TF_COMBINE_ALPHA_MODE_SHIFT);
        int rgb_scale = 1, alpha_scale = 1;
        if (tex._flags & ShaderKey::TF_rgb_scale_2) {
          rgb_scale *= 2;
        }
        if (tex._flags & ShaderKey::TF_rgb_scale_4) {
          rgb_scale *= 4;
        }
        if (tex._flags & ShaderKey::TF_alpha_scale_2) {
          alpha_scale *= 2;
        }
        if (tex._flags & ShaderKey::TF_alpha_scale_4) {
          alpha_scale *= 4;
        }
        if (combine_rgb == TextureStage::CM_dot3_rgba) {
          Id value = s.splat(combine_dot3(tex, false, i), 4);
          if (rgb_scale != 1 || alpha_scale != 1) {
            value = s.mul(value, s.vconst((float)rgb_scale, (float)rgb_scale,
                                          (float)rgb_scale, (float)alpha_scale));
          }
          result = s.sat(value);
        } else {
          Id rgb = combine_mode(tex, combine_rgb, false, i);
          if (rgb_scale != 1) {
            rgb = s.mul(rgb, s.fconst((float)rgb_scale));
          }
          rgb = s.sat(rgb);
          Id alpha = combine_mode(tex, combine_alpha, true, i);
          if (alpha_scale != 1) {
            alpha = s.mul(alpha, s.fconst((float)alpha_scale));
          }
          alpha = s.sat(alpha);
          result = s.set_w(s.set_xyz(result, rgb), alpha);
        }
      }
      break;
    case TextureStage::M_blend_color_scale:
      result = s.set_xyz(result, b.op_mix(s.xyz(result),
        s.mul(s.xyz(get_texcolor(i)), s.xyz(colorscale)), s.xyz(tex_vals[i])));
      if (key._flags & ShaderKey::F_calc_primary_alpha) {
        result = s.set_w(result, s.mul(s.comp(result, 3), s.comp(tex_vals[i], 3)));
      }
      break;
    default:
      break;
    }
    if (tex._flags & ShaderKey::TF_saved_result) {
      last_saved_result = result;
    }
  }

  // Shader subsumes the alpha test.
  if ((key._flags & ShaderKey::F_ALPHA_TEST_MASK) != 0) {
    Id ref = s.fconst((float)key._alpha_test_ref);
    Id alpha = s.comp(result, 3);
    switch (key.get_alpha_test_mode()) {
    case RenderAttrib::M_never:
      s.kill_if(s._module.define_constant(ShaderType::BOOL, 1));
      break;
    case RenderAttrib::M_less:
      s.kill_if(b.op_compare(spv::OpFOrdGreaterThanEqual, alpha, ref));
      break;
    case RenderAttrib::M_equal:
      s.kill_if(b.op_compare(spv::OpFUnordNotEqual, alpha, ref));
      break;
    case RenderAttrib::M_less_equal:
      s.kill_if(b.op_compare(spv::OpFOrdGreaterThan, alpha, ref));
      break;
    case RenderAttrib::M_greater:
      s.kill_if(b.op_compare(spv::OpFOrdLessThanEqual, alpha, ref));
      break;
    case RenderAttrib::M_not_equal:
      s.kill_if(b.op_compare(spv::OpFOrdEqual, alpha, ref));
      break;
    case RenderAttrib::M_greater_equal:
      s.kill_if(b.op_compare(spv::OpFOrdLessThan, alpha, ref));
      break;
    case RenderAttrib::M_none:
    case RenderAttrib::M_always:
      break;
    }
  }

  if (key._flags & ShaderKey::F_out_alpha_glow) {
    if (key._texture_flags & ShaderKey::TF_map_glow) {
      result = s.set_w(result, s.comp(tex_vals[plan._map_index_glow], 3));
    } else {
      result = s.set_w(result, s.fconst(0.5f));
    }
  }
  if (key._flags & ShaderKey::F_out_aux_glow) {
    if (key._texture_flags & ShaderKey::TF_map_glow) {
      aux_a = s.comp(tex_vals[plan._map_index_glow], 3);
    } else {
      aux_a = s.fconst(0.5f);
    }
  }

  if (plan._have_specular) {
    if (key._flags & Material::F_specular) {
      tot_specular = s.mul(tot_specular, b.op_composite_extract(material, {3}));
    }
    if (key._texture_flags & ShaderKey::TF_map_gloss) {
      tot_specular = s.mul(tot_specular, s.comp(tex_vals[plan._map_index_gloss], 3));
    }
    result = s.set_xyz(result, s.add(s.xyz(result), s.xyz(tot_specular)));
  }

  // Apply the HDR light ramps.
  switch (key._light_ramp_mode) {
  case LightRampAttrib::LRT_hdr0:
    {
      Id rgb = s.xyz(result);
      Id rgb2 = s.mul(rgb, rgb);
      Id rgb3 = s.mul(rgb2, rgb);
      Id numerator = s.add(s.add(rgb3, rgb2), rgb);
      result = s.set_xyz(result,
        s.div(numerator, s.add(numerator, s.splat_const(1.0f, 3))));
      break;
    }
  case LightRampAttrib::LRT_hdr1:
    {
      Id rgb = s.xyz(result);
      Id numerator = s.add(s.mul(rgb, rgb), rgb);
      result = s.set_xyz(result,
        s.div(numerator, s.add(numerator, s.splat_const(1.0f, 3))));
      break;
    }
  case LightRampAttrib::LRT_hdr2:
    {
      Id rgb = s.xyz(result);
      result = s.set_xyz(result,
        s.div(rgb, s.add(rgb, s.splat_const(1.0f, 3))));
      break;
    }
  default:
    break;
  }

  // Apply fog.
  if (have_fog) {
    Fog::Mode fog_mode = (Fog::Mode)
      (((key._flags & ShaderKey::F_FOG_MODE_MASK) >> ShaderKey::F_FOG_MODE_SHIFT) - 1);
    Id fog_params = s.load(attr_fog);
    Id fog_color = s.xyz(s.load(attr_fogcolor));
    Id hpos_z = s.comp(s.load(l_hpos), 2);

    Id factor;
    switch (fog_mode) {
    case Fog::M_linear:
      factor = s.sat(s.mul(s.sub(s.comp(fog_params, 2), hpos_z),
                           s.comp(fog_params, 3)));
      break;
    case Fog::M_exponential: // 1.442695f = 1 / log(2)
      factor = s.sat(b.op_exp2(s.mul(s.mul(s.comp(fog_params, 0), hpos_z),
                                     s.fconst(-1.442695f))));
      break;
    case Fog::M_exponential_squared:
      {
        Id density = s.comp(fog_params, 0);
        Id value = s.mul(s.mul(density, density), s.mul(hpos_z, hpos_z));
        factor = s.sat(b.op_exp2(s.mul(value, s.fconst(-1.442695f))));
        break;
      }
    default:
      factor = s.fconst(1.0f);
      break;
    }
    result = s.set_xyz(result, b.op_mix(fog_color, s.xyz(result), s.splat(factor, 3)));
  }

  if (o_color) {
    s.store(o_color, result);
  }
  if (o_aux) {
    s.store(o_aux, s.vec4_of(aux_rgb, aux_a));
  }

  return s.finish(ShaderModule::Stage::FRAGMENT);
}

#endif  // CPPPARSER

/**
 * This is the routine that implements the next-gen fixed function pipeline by
 * synthesizing a shader.  It also takes care of setting up any buffers needed
 * to produce the requested effects.
 *
 * The shader is generated directly as a pair of SPIR-V modules, using the
 * SpirVBuilder, with the same (Cg-style) parameter names as the old
 * Cg-based generator used.
 *
 * Currently supports:
 * - flat colors
 * - vertex colors
 * - lighting
 * - normal maps, even multiple
 * - gloss maps, but not multiple
 * - glow maps, but not multiple
 * - materials, but not updates to materials
 * - 2D textures
 * - all texture stage modes, including combine modes
 * - color scale attrib
 * - light ramps (for cartoon shading)
 * - shadow mapping
 * - most texgen modes
 * - texmatrix
 * - 1D/2D/3D textures, cube textures, 2D tex arrays
 * - linear/exp/exp2 fog
 * - animation
 *
 * Potential optimizations
 * - omit attenuation calculations if attenuation off
 *
 */
CPT(ShaderAttrib) ShaderGenerator::
synthesize_shader(const RenderState *rs, const GeomVertexAnimationSpec &anim) {
  ShaderKey key;

  // First look up the state key in the table of already generated shaders.
  {
    PStatTimer timer(lookup_collector);
    analyze_renderstate(key, rs, anim);

    LightMutexHolder holder(_lock);
    GeneratedShaders::iterator si;
    si = _generated_shaders.find(key);
    if (si != _generated_shaders.end()) {
      // We've already generated a shader for this state.
      GeneratedShader &gen_shader = si->second;
      if (gen_shader._attrib == nullptr) {
        // We have a shader, but haven't created a ShaderAttrib for it yet.
        CPT(ShaderAttrib) attrib = make_attrib(key, gen_shader._shader);
        gen_shader._attrib = std::move(attrib);
      }
      gen_shader._last_use = time(nullptr);
      return gen_shader._attrib;
    }
  }

  PStatTimer timer(synthesize_collector);

  if (pgraphnodes_cat.is_debug()) {
    pgraphnodes_cat.debug()
      << "Generating shader for render state " << rs << ":\n";
    rs->write(pgraphnodes_cat.debug(false), 2);
  }

  // Generate the vertex and fragment modules.  Each is generated
  // independently, from the key and the plan alone, so that they may one day
  // be generated in parallel.
  ShaderPlan plan = make_plan(key);
  PT(ShaderModule) vertex_module = generate_vertex_shader(key, plan);
  PT(ShaderModule) fragment_module = generate_fragment_shader(key, plan);
  nassertr(vertex_module != nullptr, nullptr);
  nassertr(fragment_module != nullptr, nullptr);

  // Insert the modules into a shader.
  PT(Shader) shader = new Shader(Shader::SL_Cg);
  shader->get_prepare_shader_pcollector() =
    PStatCollector("Draw:Prepare:Shader:generated-shader");
  if (!shader->add_module(std::move(vertex_module)) ||
      !shader->add_module(std::move(fragment_module)) ||
      !shader->link()) {
    pgraphnodes_cat.error()
      << "Failed to link generated shader for render state " << *rs << "\n";

    // Memoize the failure with an empty ShaderAttrib, so that we don't
    // re-attempt (and re-log) the generation every time an object with this
    // state is rendered.  Entries without a shader are not written to the
    // on-disk cache, so the generation is re-attempted in the next session.
    CPT(ShaderAttrib) attr = DCAST(ShaderAttrib, ShaderAttrib::make());
    LightMutexHolder holder(_lock);
    GeneratedShader &gen_shader = _generated_shaders[key];
    gen_shader._attrib = attr;
    gen_shader._last_use = time(nullptr);
    return attr;
  }

  shader->_subsumes_alpha_test = (key._flags & ShaderKey::F_ALPHA_TEST_MASK) != 0;

  CPT(ShaderAttrib) attr = make_attrib(key, shader);

  LightMutexHolder holder(_lock);
  GeneratedShader &gen_shader = _generated_shaders[key];
  gen_shader._shader = std::move(shader);
  gen_shader._attrib = attr;
  gen_shader._last_use = time(nullptr);
  return attr;
}

/**
 * Constructs a ShaderAttrib around the given generated Shader object.
 */
CPT(ShaderAttrib) ShaderGenerator::
make_attrib(const ShaderKey &key, Shader *shader) {
  CPT(RenderAttrib) shattr = ShaderAttrib::make(shader);
  int flags = 0;
  if ((key._flags & ShaderKey::F_ALPHA_TEST_MASK) != 0) {
    flags |= ShaderAttrib::F_subsume_alpha_test;
  }
  if (key._flags & ShaderKey::F_disable_alpha_write) {
    flags |= ShaderAttrib::F_disable_alpha_write;
  }
  if (key._flags & ShaderKey::F_perspective_points) {
    flags |= ShaderAttrib::F_shader_point_size;
  }
  if (key._num_anim_transforms != 0) {
    flags |= ShaderAttrib::F_hardware_skinning;
  }
  if (flags != 0) {
    shattr = DCAST(ShaderAttrib, shattr)->set_flag(flags, true);
  }
  return DCAST(ShaderAttrib, shattr);
}

/**
 * Initializes the ShaderKey to the empty state.
 */
ShaderGenerator::ShaderKey::
ShaderKey() :
  _flags(0),
  _texture_flags(0),
  _alpha_test_ref(0.0),
  _num_clip_planes(0),
  _light_ramp_mode(LightRampAttrib::LRT_default),
  _light_ramp_level{0, 0},
  _light_ramp_threshold{0, 0},
  _num_anim_transforms(0) {
}

/**
 * Returns true if this ShaderKey sorts less than the other one.  This is an
 * arbitrary, but consistent ordering.
 */
bool ShaderGenerator::ShaderKey::
operator < (const ShaderKey &other) const {
  if (_flags != other._flags) {
    return _flags < other._flags;
  }
  if (_texture_flags != other._texture_flags) {
    return _texture_flags < other._texture_flags;
  }
  if (_textures.size() != other._textures.size()) {
    return _textures.size() < other._textures.size();
  }
  if (_lights.size() != other._lights.size()) {
    return _lights.size() < other._lights.size();
  }
  for (size_t i = 0; i < _textures.size(); ++i) {
    const ShaderKey::TextureInfo &tex = _textures[i];
    const ShaderKey::TextureInfo &other_tex = other._textures[i];
    if (tex._texcoord_name != other_tex._texcoord_name) {
      return tex._texcoord_name < other_tex._texcoord_name;
    }
    if (tex._type != other_tex._type) {
      return tex._type < other_tex._type;
    }
    if (tex._mode != other_tex._mode) {
      return tex._mode < other_tex._mode;
    }
    if (tex._gen_mode != other_tex._gen_mode) {
      return tex._gen_mode < other_tex._gen_mode;
    }
    if (tex._flags != other_tex._flags) {
      return tex._flags < other_tex._flags;
    }
    if (tex._combine_rgb != other_tex._combine_rgb) {
      return tex._combine_rgb < other_tex._combine_rgb;
    }
    if (tex._combine_alpha != other_tex._combine_alpha) {
      return tex._combine_alpha < other_tex._combine_alpha;
    }
  }
  for (size_t i = 0; i < _lights.size(); ++i) {
    int light_flags = _lights[i];
    int other_light_flags = other._lights[i];
    if (light_flags != other_light_flags) {
      return light_flags < other_light_flags;
    }
  }
  if (_alpha_test_ref != other._alpha_test_ref) {
    return _alpha_test_ref < other._alpha_test_ref;
  }
  if (_num_clip_planes != other._num_clip_planes) {
    return _num_clip_planes < other._num_clip_planes;
  }
  if (_light_ramp_mode != other._light_ramp_mode) {
    return _light_ramp_mode < other._light_ramp_mode;
  }
  if (_light_ramp_mode != LightRampAttrib::LRT_default &&
      _light_ramp_mode != LightRampAttrib::LRT_identity) {
    if (_light_ramp_level[0] != other._light_ramp_level[0]) {
      return _light_ramp_level[0] < other._light_ramp_level[0];
    }
    if (_light_ramp_level[1] != other._light_ramp_level[1]) {
      return _light_ramp_level[1] < other._light_ramp_level[1];
    }
    if (_light_ramp_threshold[0] != other._light_ramp_threshold[0]) {
      return _light_ramp_threshold[0] < other._light_ramp_threshold[0];
    }
    if (_light_ramp_threshold[1] != other._light_ramp_threshold[1]) {
      return _light_ramp_threshold[1] < other._light_ramp_threshold[1];
    }
  }
  return _num_anim_transforms < other._num_anim_transforms;
}

/**
 * Returns true if this ShaderKey is equal to the other one.
 */
bool ShaderGenerator::ShaderKey::
operator == (const ShaderKey &other) const {
  if (_flags != other._flags) {
    return false;
  }
  if (_texture_flags != other._texture_flags) {
    return false;
  }
  if (_textures.size() != other._textures.size()) {
    return false;
  }
  if (_lights.size() != other._lights.size()) {
    return false;
  }
  for (size_t i = 0; i < _textures.size(); ++i) {
    const ShaderKey::TextureInfo &tex = _textures[i];
    const ShaderKey::TextureInfo &other_tex = other._textures[i];
    if (tex._texcoord_name != other_tex._texcoord_name ||
        tex._type != other_tex._type ||
        tex._mode != other_tex._mode ||
        tex._gen_mode != other_tex._gen_mode ||
        tex._flags != other_tex._flags ||
        tex._combine_rgb != other_tex._combine_rgb ||
        tex._combine_alpha != other_tex._combine_alpha) {
      return false;
    }
  }
  for (size_t i = 0; i < _lights.size(); ++i) {
    int light_flags = _lights[i];
    int other_light_flags = other._lights[i];
    if (light_flags != other_light_flags) {
      return false;
    }
  }
  return _alpha_test_ref == other._alpha_test_ref
      && _num_clip_planes == other._num_clip_planes
      && _light_ramp_mode == other._light_ramp_mode
      && _light_ramp_level[0] == other._light_ramp_level[0]
      && _light_ramp_level[1] == other._light_ramp_level[1]
      && _light_ramp_threshold[0] == other._light_ramp_threshold[0]
      && _light_ramp_threshold[1] == other._light_ramp_threshold[1]
      && _num_anim_transforms == other._num_anim_transforms;
}

/**
 * Deserializes this shader key.
 */
void ShaderGenerator::ShaderKey::
read_datagram(DatagramIterator &scan) {
  _flags = scan.get_uint32();

  _texture_flags = 0;

  _textures.resize(scan.get_uint16());
  for (ShaderKey::TextureInfo &tex : _textures) {
    tex._texcoord_name = scan.get_string();
    tex._type = (Texture::TextureType)scan.get_uint8();
    tex._mode = (TextureStage::Mode)scan.get_uint8();
    tex._gen_mode = (TexGenAttrib::Mode)scan.get_uint8();
    tex._flags = scan.get_uint32();
    if (tex._mode == TextureStage::M_combine) {
      tex._combine_rgb = scan.get_uint16();
      tex._combine_alpha = scan.get_uint16();
    } else {
      tex._combine_rgb = 0u;
      tex._combine_alpha = 0u;
    }
    _texture_flags |= tex._flags;
  }

  _lights.resize(scan.get_uint16());
  for (int &light_flags : _lights) {
    light_flags = (int)scan.get_uint8();
  }

  if ((_flags & ShaderKey::F_ALPHA_TEST_MASK) != 0) {
    _alpha_test_ref = scan.get_float32();
  } else {
    _alpha_test_ref = 0;
  }

  _num_clip_planes = scan.get_uint16();
  _light_ramp_mode = (LightRampAttrib::LightRampMode)scan.get_uint8();
  if (_light_ramp_mode != LightRampAttrib::LRT_default &&
      _light_ramp_mode != LightRampAttrib::LRT_identity) {
    _light_ramp_level[0] = scan.get_float32();
    _light_ramp_level[1] = scan.get_float32();
    _light_ramp_threshold[0] = scan.get_float32();
    _light_ramp_threshold[1] = scan.get_float32();
  }
  _num_anim_transforms = scan.get_int32();
}

/**
 * Serializes this shader key.
 */
void ShaderGenerator::ShaderKey::
write_datagram(Datagram &dg) const {
  dg.add_uint32(_flags);

  dg.add_uint16(_textures.size());
  for (const ShaderKey::TextureInfo &tex : _textures) {
    dg.add_string(tex._texcoord_name->get_name());
    dg.add_uint8(tex._type);
    dg.add_uint8(tex._mode);
    dg.add_uint8(tex._gen_mode);
    dg.add_uint32(tex._flags);
    if (tex._mode == TextureStage::M_combine) {
      dg.add_uint16(tex._combine_rgb);
      dg.add_uint16(tex._combine_alpha);
    }
  }

  dg.add_uint16(_lights.size());
  for (int light_flags : _lights) {
    dg.add_uint8(light_flags);
  }

  if ((_flags & ShaderKey::F_ALPHA_TEST_MASK) != 0) {
    dg.add_float32(_alpha_test_ref);
  }

  dg.add_uint16(_num_clip_planes);
  dg.add_uint8(_light_ramp_mode);
  if (_light_ramp_mode != LightRampAttrib::LRT_default &&
      _light_ramp_mode != LightRampAttrib::LRT_identity) {
    dg.add_float32(_light_ramp_level[0]);
    dg.add_float32(_light_ramp_level[1]);
    dg.add_float32(_light_ramp_threshold[0]);
    dg.add_float32(_light_ramp_threshold[1]);
  }
  dg.add_int32(_num_anim_transforms);
}
