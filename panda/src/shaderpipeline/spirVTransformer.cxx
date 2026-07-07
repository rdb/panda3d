/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file spirVTransformer.cxx
 * @author rdb
 * @date 2024-10-08
 */

#include "spirVTransformer.h"
#include "spirVTransformPass.h"

/**
 * Materializes the given instruction stream.
 */
SpirVTransformer::
SpirVTransformer(const InstructionStream &stream) : _module(stream) {
}

/**
 * Runs the given transformation pass object (which can be used only once) on
 * the module.  Afterwards, merges any duplicate unique-type declarations the
 * pass may have created.
 */
void SpirVTransformer::
run(SpirVTransformPass &pass) {
  // A pass object carries per-run state and may only be used once.
  nassertv(!pass._ran);
  pass._ran = true;

  pass.run(_module);

  _module.deduplicate_types();
}

/**
 * Serializes the current state of the module.
 */
ShaderModuleSpirV::InstructionStream SpirVTransformer::
get_result() const {
  return _module.emit();
}

/**
 * Checks that the module's id index and memoized type resolutions - the only
 * derived state the module keeps - are consistent with a fresh parse of the
 * emitted module.  This catches a pass editing an instruction's result type
 * in place without calling record_result, and a mutation path failing to
 * invalidate the type resolutions.  Logs an error for every mismatch and
 * returns false if any was found.  Intended for validation in the test
 * suite.
 */
bool SpirVTransformer::
validate() const {
  SpirVModule fresh(get_result());
  nassertr(get_id_bound() == fresh.get_id_bound(), false);

  bool consistent = true;
  auto report = [&](uint32_t id, const char *field, int64_t value, int64_t fresh_value) {
    shader_cat.error()
      << "Inconsistent id index after transformation: id " << id << " has "
      << field << " " << value << ", but " << fresh_value
      << " when parsed from the module\n";
    consistent = false;
  };

  for (uint32_t word = 0; word < get_id_bound(); ++word) {
    SpirVId id(word);
    SpirVModule::IdKind kind = _module.get_kind(id);
    SpirVModule::IdKind fresh_kind = fresh.get_kind(id);

    if (kind != fresh_kind) {
      report(id, "kind", (int64_t)kind, (int64_t)fresh_kind);
      continue;
    }
    if (kind == SpirVModule::IdKind::NONE) {
      continue;
    }

    if (kind != SpirVModule::IdKind::TYPE &&
        kind != SpirVModule::IdKind::POINTER_TYPE &&
        kind != SpirVModule::IdKind::FUNCTION_TYPE &&
        kind != SpirVModule::IdKind::EXT_INST &&
        kind != SpirVModule::IdKind::STRING) {
      // The index mirrors the result type operand for values; the mirror
      // must match what the instructions say.
      SpirVId type_id = _module.get_type_id(id);
      SpirVId fresh_type_id = fresh.get_type_id(id);
      if (type_id != fresh_type_id) {
        report(id, "type id", type_id, fresh_type_id);
      }
    }

    if (_module.get_function_id(id) != fresh.get_function_id(id)) {
      report(id, "function id", _module.get_function_id(id),
             fresh.get_function_id(id));
    }

    // The resolutions are derived from the same declarations by the same
    // code, so a mismatch means a memoized resolution went stale (a mutation
    // path failed to invalidate).
    const ShaderType *type = nullptr;
    const ShaderType *fresh_type = nullptr;
    switch (kind) {
    case SpirVModule::IdKind::TYPE:
      type = _module.resolve_type(id);
      fresh_type = fresh.resolve_type(id);
      break;

    case SpirVModule::IdKind::POINTER_TYPE:
      type = _module.resolve_pointer_type(id);
      fresh_type = fresh.resolve_pointer_type(id);
      break;

    case SpirVModule::IdKind::VARIABLE:
    case SpirVModule::IdKind::CONSTANT:
    case SpirVModule::IdKind::SPEC_CONSTANT:
    case SpirVModule::IdKind::PARAMETER:
      type = _module.resolve_value_type(id);
      fresh_type = fresh.resolve_value_type(id);
      break;

    default:
      break;
    }
    if (type != fresh_type) {
      shader_cat.error()
        << "Inconsistent type resolution after transformation: id " << id
        << " resolves to ";
      if (type != nullptr) {
        shader_cat.error(false) << *type;
      } else {
        shader_cat.error(false) << "(null)";
      }
      shader_cat.error(false) << ", but ";
      if (fresh_type != nullptr) {
        shader_cat.error(false) << *fresh_type;
      } else {
        shader_cat.error(false) << "(null)";
      }
      shader_cat.error(false) << " when parsed from the module\n";
      consistent = false;
    }
  }

  // Deleting an id cleans up the annotations *on* it, but a handful of
  // annotation forms embed ids in their operands, and execution modes can
  // reference constants; a pass that deletes an id these refer to must
  // remove them itself, which is easy to forget.  Sweep for danglers.
  auto check_reference = [&](SpirVId id, const char *referrer, uint32_t referrer_id) {
    if (id == 0 || id >= get_id_bound() ||
        _module.get_kind(id) == SpirVModule::IdKind::NONE) {
      shader_cat.error()
        << "Dangling reference after transformation: " << referrer << " on id "
        << referrer_id << " refers to nonexistent id " << id << "\n";
      consistent = false;
    }
  };

  for (uint32_t word = 0; word < get_id_bound(); ++word) {
    const SpirVModule::Annotations *annotations = _module.get_annotations(SpirVId(word));
    if (annotations == nullptr) {
      continue;
    }
    for (const SpirVModule::Annotation &annotation : *annotations) {
      switch (annotation.opcode) {
      case spv::OpDecorateId:
        // Every operand after the decoration is an id.
        for (size_t ai = 1; ai < annotation.args.size(); ++ai) {
          check_reference(SpirVId(annotation.args[ai]), "OpDecorateId", word);
        }
        break;

      case spv::OpGroupDecorate:
        // Every operand is a decoration target.
        for (size_t ai = 0; ai < annotation.args.size(); ++ai) {
          check_reference(SpirVId(annotation.args[ai]), "OpGroupDecorate", word);
        }
        break;

      case spv::OpGroupMemberDecorate:
        // Pairs of target id and member index.
        for (size_t ai = 0; ai + 1 < annotation.args.size(); ai += 2) {
          check_reference(SpirVId(annotation.args[ai]), "OpGroupMemberDecorate", word);
        }
        break;

      default:
        break;
      }
    }
  }

  for (const SpirVModule::Instruction &op : _module.get_execution_modes()) {
    if (op.args.empty()) {
      continue;
    }
    check_reference(SpirVId(op.args[0]), "an execution mode", op.args[0]);
    if (op.opcode == spv::OpExecutionModeId) {
      // Every operand after the mode is an id (eg. LocalSizeId constants).
      for (size_t ai = 2; ai < op.args.size(); ++ai) {
        check_reference(SpirVId(op.args[ai]), "OpExecutionModeId", op.args[0]);
      }
    }
  }

  return consistent;
}

/**
 * Assigns location decorations to all input and output variables that do not
 * have a location decoration yet.  Does not touch uniform constants.
 */
void SpirVTransformer::
assign_interface_locations(ShaderModule::Stage stage) {
  // Determine which locations have already been assigned.
  bool has_unassigned_locations = false;
  BitArray input_locations;
  BitArray output_locations;

  for (uint32_t word = 0; word < get_id_bound(); ++word) {
    SpirVId id(word);
    if (_module.get_kind(id) != SpirVModule::IdKind::VARIABLE ||
        _module.get_function_id(id) != 0) {
      continue;
    }
    spv::StorageClass storage_class = _module.get_storage_class(id);
    int location = _module.get_location(id);
    if (location < 0) {
      if (_module.get_builtin(id) == spv::BuiltInMax &&
          (storage_class == spv::StorageClassInput ||
           storage_class == spv::StorageClassOutput)) {
        // A non-built-in variable definition without a location.
        has_unassigned_locations = true;
      }
    }
    else if (storage_class == spv::StorageClassInput) {
      const ShaderType *type = _module.resolve_value_type(id);
      input_locations.set_range(location, type != nullptr ? type->get_num_interface_locations() : 1);
    }
    else if (storage_class == spv::StorageClassOutput) {
      const ShaderType *type = _module.resolve_value_type(id);
      output_locations.set_range(location, type != nullptr ? type->get_num_interface_locations() : 1);
    }
  }

  if (!has_unassigned_locations) {
    return;
  }

  for (uint32_t word = 0; word < get_id_bound(); ++word) {
    SpirVId id(word);
    if (_module.get_kind(id) != SpirVModule::IdKind::VARIABLE ||
        _module.get_function_id(id) != 0 ||
        _module.get_location(id) >= 0 ||
        _module.get_builtin(id) != spv::BuiltInMax) {
      continue;
    }
    const ShaderType *type = _module.resolve_value_type(id);
    if (type == nullptr) {
      continue;
    }
    spv::StorageClass storage_class = _module.get_storage_class(id);

    int location;
    int num_locations;
    const char *sc_str;

    if (storage_class == spv::StorageClassInput) {
      num_locations = type->get_num_interface_locations();
      if (num_locations == 0) {
        continue;
      }

      const std::string &name = _module.get_name(id);
      if (stage == ShaderModule::Stage::VERTEX && !input_locations.get_bit(0) &&
          name != "vertex" && name != "p3d_Vertex" &&
          name != "vtx_position") {
        // Leave location 0 open for the vertex position attribute.
        location = input_locations.find_off_range(num_locations, 1);
      } else {
        location = input_locations.find_off_range(num_locations);
      }
      input_locations.set_range(location, num_locations);

      sc_str = "input";
    }
    else if (storage_class == spv::StorageClassOutput) {
      num_locations = type->get_num_interface_locations();
      if (num_locations == 0) {
        continue;
      }

      location = output_locations.find_off_range(num_locations);
      output_locations.set_range(location, num_locations);

      sc_str = "output";
    }
    else {
      continue;
    }
    nassertd(location >= 0) continue;

    if (shader_cat.is_debug()) {
      if (num_locations == 1) {
        shader_cat.debug()
          << "Assigning " << _module.get_name(id) << " to " << sc_str
          << " location " << location << "\n";
      } else {
        shader_cat.debug()
          << "Assigning " << _module.get_name(id) << " to " << sc_str
          << " locations " << location << ".."
          << (location + num_locations - 1) << "\n";
      }
    }

    _module.decorate(id, spv::DecorationLocation, (uint32_t)location);
  }
}

/**
 * Assigns location decorations based on the given remapping.
 */
void SpirVTransformer::
assign_locations(pmap<uint32_t, int> remap) {
  for (const auto &item : remap) {
    _module.set_location(SpirVId(item.first), item.second);
  }
}

/**
 * Assigns missing Flat decoration on uint/double outputs.
 */
void SpirVTransformer::
assign_flat_decorations() {
  for (uint32_t word = 0; word < get_id_bound(); ++word) {
    SpirVId id(word);
    if (_module.get_kind(id) != SpirVModule::IdKind::VARIABLE ||
        _module.get_function_id(id) != 0 ||
        _module.get_builtin(id) != spv::BuiltInMax ||
        _module.get_storage_class(id) != spv::StorageClassOutput ||
        _module.has_decoration(id, spv::DecorationFlat)) {
      continue;
    }
    const ShaderType *type = _module.resolve_value_type(id);
    if (type == nullptr) {
      continue;
    }
    if (type->contains_scalar_type(ShaderType::ST_int) ||
        type->contains_scalar_type(ShaderType::ST_uint) ||
        type->contains_scalar_type(ShaderType::ST_double)) {
      // These must be flat.
      const ShaderType *unwrapped = type;
      while (const ShaderType::Array *array = unwrapped->as_array()) {
        unwrapped = array->get_element_type();
      }
      //TODO: handle assigning "flat" to struct members.
      if (unwrapped->as_struct() == nullptr) {
        _module.decorate(id, spv::DecorationFlat);
      }
    }
  }
}

/**
 * Assigns procedural names consisting of a prefix followed by an index.
 */
void SpirVTransformer::
assign_procedural_names(const char *prefix, const pmap<uint32_t, int> &suffixes) {
  char buffer[32];
  for (const auto &item : suffixes) {
    sprintf(buffer, "%d", item.second);
    _module.set_name(SpirVId(item.first), std::string(prefix) + buffer);
  }
}

/**
 * Removes location decorations from uniforms.
 */
void SpirVTransformer::
strip_uniform_locations() {
  for (uint32_t word = 0; word < get_id_bound(); ++word) {
    SpirVId id(word);
    if (_module.get_kind(id) == SpirVModule::IdKind::VARIABLE &&
        _module.get_function_id(id) == 0 &&
        _module.get_storage_class(id) == spv::StorageClassUniformConstant) {
      _module.remove_decoration(id, spv::DecorationLocation);
    }
  }
}

/**
 * Removes all binding and descriptor set decorations.
 */
void SpirVTransformer::
strip_bindings() {
  _module.strip_decoration(spv::DecorationBinding);
  _module.strip_decoration(spv::DecorationDescriptorSet);
}

/**
 * Assign descriptor bindings for a descriptor set based on the given ids.
 * To create gaps in the descriptor set, entries in ids may be 0.
 */
void SpirVTransformer::
bind_descriptor_set(uint32_t set, const pvector<uint32_t> &ids) {
  for (uint32_t binding = 0; binding < ids.size(); ++binding) {
    SpirVId id(ids[binding]);
    if (id != 0) {
      _module.set_decoration(id, spv::DecorationDescriptorSet, set);
      _module.set_decoration(id, spv::DecorationBinding, binding);
    }
  }
}
