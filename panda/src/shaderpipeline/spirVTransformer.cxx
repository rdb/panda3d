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
 * pass may have created and reconciles the database with the module, so that
 * the database always ends up identical to a fresh parse of the result (see
 * validate_db).
 */
void SpirVTransformer::
run(SpirVTransformPass &pass) {
  // A pass object carries per-run state and may only be used once.
  nassertv(!pass._ran);
  pass._ran = true;

  pass.run(_module);

  _module.deduplicate_types();
  _module.reparse_db();
}

/**
 * Serializes the current state of the module.
 */
ShaderModuleSpirV::InstructionStream SpirVTransformer::
get_result() const {
  return _module.emit();
}

/**
 * Checks that the definition database, which is updated incrementally by the
 * transform passes and reconciled after each pass, is consistent with a
 * database that is freshly parsed from the resulting module.  Logs an error
 * for every mismatch and returns false if any was found.  Intended for
 * validation in the test suite.
 *
 * Names are not compared, since the database keeps qualified names (such as
 * "block.member" after running SpirVFlattenStructPass) that intentionally
 * differ from the names stored in the module.  Usage tracking is compared in
 * one direction only: the passes may conservatively consider a definition
 * used, but a variable or function parameter that the module uses must never
 * be marked unused, or SpirVRemoveUnusedVariablesPass would delete it.
 */
bool SpirVTransformer::
validate_db() const {
  SpirVModule fresh(get_result());
  nassertr(get_id_bound() == fresh.get_id_bound(), false);

  const SpirVResultDatabase &db = _module.get_db();
  const SpirVResultDatabase &fresh_db = fresh.get_db();

  bool consistent = true;
  auto report = [&](uint32_t id, const char *field, int64_t db_value, int64_t fresh_value) {
    shader_cat.error()
      << "Inconsistent database after transformation: id " << id << " has "
      << field << " " << db_value << ", but " << fresh_value
      << " when parsed from the module\n";
    consistent = false;
  };

  // Decoration-derived flags that both the incremental updates and a fresh
  // parse must agree on.  Usage-tracking flags are excluded.
  static const int flag_mask =
    SpirVResultDatabase::DF_buffer_block |
    SpirVResultDatabase::DF_block |
    SpirVResultDatabase::DF_non_writable |
    SpirVResultDatabase::DF_non_readable |
    SpirVResultDatabase::DF_relaxed_precision |
    SpirVResultDatabase::DF_flat;

  // Member decorations that affect the database and should round-trip.
  static const int member_flag_mask =
    SpirVResultDatabase::DF_non_writable |
    SpirVResultDatabase::DF_non_readable |
    SpirVResultDatabase::DF_flat;

  // The instructions inserted by a pass may record the results of arithmetic
  // operations as fully-typed temporaries, whereas parse_instruction only
  // records the result type id for these, or the other way around.  Consider
  // these definitions equivalent as long as the type id matches.
  auto is_expression = [](const Definition &def) {
    return def.is_temporary() || def._dtype == 0 /* DT_none */;
  };

  for (uint32_t id = 0; id < get_id_bound(); ++id) {
    const Definition &db_def = db.get_definition(id);
    const Definition &fresh_def = fresh_db.get_definition(id);

    if (db_def._dtype != fresh_def._dtype) {
      if (!is_expression(db_def) || !is_expression(fresh_def)) {
        report(id, "definition type", db_def._dtype, fresh_def._dtype);
      }
      else if (db_def._type_id != fresh_def._type_id) {
        report(id, "type id", db_def._type_id, fresh_def._type_id);
      }
      // The other fields are meaningless if the definition type differs.
      continue;
    }

    if (db_def._type != fresh_def._type) {
      bool equivalent = false;
      if (db_def._type == nullptr || fresh_def._type == nullptr) {
        // A fresh parse doesn't resolve the type of arithmetic results, so
        // only require the type id to match in that case.
        equivalent = (db_def._type_id == fresh_def._type_id);
      }
      else if (const ShaderType::StorageBuffer *sbuffer = fresh_def._type->as_storage_buffer()) {
        // A buffer declared the legacy way (Uniform storage class with the
        // BufferBlock decoration) is canonicalized to a StorageBuffer type
        // when parsed, but the passes work with the unwrapped struct type.
        equivalent = (sbuffer->get_contained_type() == db_def._type);
      }
      if (!equivalent) {
        shader_cat.error()
          << "Inconsistent database after transformation: id " << id << " has type ";
        if (db_def._type != nullptr) {
          shader_cat.error(false) << *db_def._type;
        } else {
          shader_cat.error(false) << "(null)";
        }
        shader_cat.error(false) << ", but ";
        if (fresh_def._type != nullptr) {
          shader_cat.error(false) << *fresh_def._type;
        } else {
          shader_cat.error(false) << "(null)";
        }
        shader_cat.error(false) << " when parsed from the module\n";
        consistent = false;
      }
    }
    if (db_def._type_id != fresh_def._type_id) {
      report(id, "type id", db_def._type_id, fresh_def._type_id);
    }
    if (db_def._location != fresh_def._location) {
      report(id, "location", db_def._location, fresh_def._location);
    }
    if (db_def._builtin != fresh_def._builtin) {
      shader_cat.error()
        << "Inconsistent database after transformation: id " << id
        << " has builtin " << spv::BuiltInToString(db_def._builtin) << ", but "
        << spv::BuiltInToString(fresh_def._builtin)
        << " when parsed from the module\n";
      consistent = false;
    }
    if ((db_def._flags & flag_mask) != (fresh_def._flags & flag_mask)) {
      report(id, "flags", db_def._flags & flag_mask, fresh_def._flags & flag_mask);
    }
    if (db_def._array_stride != fresh_def._array_stride) {
      report(id, "array stride", db_def._array_stride, fresh_def._array_stride);
    }

    if ((db_def.is_constant() || db_def.is_spec_constant()) &&
        db_def._constant != fresh_def._constant) {
      report(id, "constant value", db_def._constant, fresh_def._constant);
    }
    if (db_def.is_spec_constant() && db_def._spec_id != fresh_def._spec_id) {
      report(id, "spec id", db_def._spec_id, fresh_def._spec_id);
    }
    if ((db_def.is_variable() || db_def.is_pointer_type()) &&
        db_def._storage_class != fresh_def._storage_class &&
        // See above: parsing canonicalizes legacy-style buffer blocks to the
        // StorageBuffer storage class.
        !(db_def._storage_class == spv::StorageClassUniform &&
          fresh_def._storage_class == spv::StorageClassStorageBuffer)) {
      shader_cat.error()
        << "Inconsistent database after transformation: id " << id
        << " has storage class " << spv::StorageClassToString(db_def._storage_class)
        << ", but " << spv::StorageClassToString(fresh_def._storage_class)
        << " when parsed from the module\n";
      consistent = false;
    }

    // Usage is tracked conservatively by the passes, so a definition may be
    // marked as used even if the module no longer uses it.  The reverse is a
    // genuine problem, however: if a variable or function parameter that the
    // module relies on is considered unused, SpirVRemoveUnusedVariablesPass
    // would delete it.
    if ((db_def.is_variable() || db_def.is_function_parameter()) &&
        fresh_def.is_used() && !db_def.is_used()) {
      shader_cat.error()
        << "Inconsistent database after transformation: id " << id
        << " is not marked as used, but the module uses it\n";
      consistent = false;
    }
    if ((db_def.is_variable() || db_def.is_function_parameter() ||
         db_def.is_temporary()) &&
        db_def._function_id != fresh_def._function_id) {
      report(id, "function id", db_def._function_id, fresh_def._function_id);
    }

    if (db_def.is_function() || db_def.is_function_type()) {
      if (db_def._parameters.size() != fresh_def._parameters.size()) {
        report(id, "parameter count", db_def._parameters.size(), fresh_def._parameters.size());
      } else {
        for (size_t pi = 0; pi < db_def._parameters.size(); ++pi) {
          if (db_def._parameters[pi] != fresh_def._parameters[pi]) {
            report(id, "parameter", db_def._parameters[pi], fresh_def._parameters[pi]);
          }
        }
      }
    }

    if (db_def._members.size() != fresh_def._members.size()) {
      report(id, "member count", db_def._members.size(), fresh_def._members.size());
    } else {
      for (size_t mi = 0; mi < db_def._members.size(); ++mi) {
        const MemberDefinition &db_member = db_def._members[mi];
        const MemberDefinition &fresh_member = fresh_def._members[mi];
        if (db_member._type_id != fresh_member._type_id) {
          report(id, "member type id", db_member._type_id, fresh_member._type_id);
        }
        if (db_member._offset != fresh_member._offset) {
          report(id, "member offset", db_member._offset, fresh_member._offset);
        }
        if (db_member._location != fresh_member._location) {
          report(id, "member location", db_member._location, fresh_member._location);
        }
        if (db_member._builtin != fresh_member._builtin) {
          report(id, "member builtin", db_member._builtin, fresh_member._builtin);
        }
        if ((db_member._flags & member_flag_mask) !=
            (fresh_member._flags & member_flag_mask)) {
          report(id, "member flags", db_member._flags & member_flag_mask,
                 fresh_member._flags & member_flag_mask);
        }
        if (db_member._matrix_stride != fresh_member._matrix_stride) {
          report(id, "member matrix stride", db_member._matrix_stride,
                 fresh_member._matrix_stride);
        }
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
  const SpirVResultDatabase &db = _module.get_db();

  // Determine which locations have already been assigned.
  bool has_unassigned_locations = false;
  BitArray input_locations;
  BitArray output_locations;

  for (uint32_t id = 0; id < get_id_bound(); ++id) {
    const Definition &def = db.get_definition(id);
    if (def.is_variable()) {
      if (!def.has_location()) {
        if (!def.is_builtin() &&
            (def._storage_class == spv::StorageClassInput ||
             def._storage_class == spv::StorageClassOutput)) {
          // A non-built-in variable definition without a location.
          has_unassigned_locations = true;
        }
      }
      else if (def._storage_class == spv::StorageClassInput) {
        input_locations.set_range(def._location, def._type ? def._type->get_num_interface_locations() : 1);
      }
      else if (def._storage_class == spv::StorageClassOutput) {
        output_locations.set_range(def._location, def._type ? def._type->get_num_interface_locations() : 1);
      }
    }
  }

  if (!has_unassigned_locations) {
    return;
  }

  for (uint32_t id = 0; id < get_id_bound(); ++id) {
    const Definition &def = db.get_definition(id);
    if (def.is_variable() && !def.has_location() && !def.is_builtin() &&
        def._type != nullptr) {
      int location;
      int num_locations;
      const char *sc_str;

      if (def._storage_class == spv::StorageClassInput) {
        num_locations = def._type->get_num_interface_locations();
        if (num_locations == 0) {
          continue;
        }

        if (stage == ShaderModule::Stage::VERTEX && !input_locations.get_bit(0) &&
            def._name != "vertex" && def._name != "p3d_Vertex" &&
            def._name != "vtx_position") {
          // Leave location 0 open for the vertex position attribute.
          location = input_locations.find_off_range(num_locations, 1);
        } else {
          location = input_locations.find_off_range(num_locations);
        }
        input_locations.set_range(location, num_locations);

        sc_str = "input";
      }
      else if (def._storage_class == spv::StorageClassOutput) {
        num_locations = def._type->get_num_interface_locations();
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
            << "Assigning " << def._name << " to " << sc_str << " location "
            << location << "\n";
        } else {
          shader_cat.debug()
            << "Assigning " << def._name << " to " << sc_str << " locations "
            << location << ".." << (location + num_locations - 1) << "\n";
        }
      }

      _module.decorate(id, spv::DecorationLocation, (uint32_t)location);
    }
  }
}

/**
 * Assigns location decorations based on the given remapping.
 */
void SpirVTransformer::
assign_locations(pmap<uint32_t, int> remap) {
  for (const auto &item : remap) {
    _module.set_location(item.first, item.second);
  }
}

/**
 * Assigns missing Flat decoration on uint/double outputs.
 */
void SpirVTransformer::
assign_flat_decorations() {
  const SpirVResultDatabase &db = _module.get_db();

  for (uint32_t id = 0; id < get_id_bound(); ++id) {
    const Definition &def = db.get_definition(id);
    if (def.is_variable() && !def.is_builtin() && def._storage_class == spv::StorageClassOutput && (def._flags & SpirVResultDatabase::DF_flat) == 0) {
      if (def._type->contains_scalar_type(ShaderType::ST_int) ||
          def._type->contains_scalar_type(ShaderType::ST_uint) ||
          def._type->contains_scalar_type(ShaderType::ST_double)) {
        // These must be flat.
        const ShaderType *unwrapped = def._type;
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
}

/**
 * Assigns procedural names consisting of a prefix followed by an index.
 */
void SpirVTransformer::
assign_procedural_names(const char *prefix, const pmap<uint32_t, int> &suffixes) {
  char buffer[32];
  for (const auto &item : suffixes) {
    sprintf(buffer, "%d", item.second);
    _module.set_name(item.first, std::string(prefix) + buffer);
  }
}

/**
 * Removes location decorations from uniforms.
 */
void SpirVTransformer::
strip_uniform_locations() {
  SpirVResultDatabase &db = _module.modify_db();

  for (uint32_t id = 0; id < get_id_bound(); ++id) {
    Definition &def = db.modify_definition(id);
    if (def.is_variable() && def.has_location() &&
        def._storage_class == spv::StorageClassUniformConstant) {
      _module.remove_decoration(id, spv::DecorationLocation);
      def._location = -1;
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
    uint32_t id = ids[binding];
    if (id > 0) {
      _module.set_decoration(id, spv::DecorationDescriptorSet, set);
      _module.set_decoration(id, spv::DecorationBinding, binding);
    }
  }
}
