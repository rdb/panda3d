/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file spirVReplaceVariableTypePass.cxx
 * @author rdb
 * @date 2024-10-08
 */

#include "spirVReplaceVariableTypePass.h"

/**
 *
 */
SpirVReplaceVariableTypePass::
SpirVReplaceVariableTypePass(uint32_t variable_id, const ShaderType *type,
                             spv::StorageClass storage_class) :
  _variable_id(variable_id),
  _new_type(type),
  _new_storage_class(storage_class) {

  _pointer_ids.insert(variable_id);
}

/**
 *
 */
void SpirVReplaceVariableTypePass::
run(SpirVModule &module) {
  SpirVResultDatabase &db = module.modify_db();

  // Retype the variable declaration itself.
  {
    Instruction *decl = module.get_global(_variable_id);
    nassertv(decl != nullptr && decl->opcode == spv::OpVariable);

    Definition &def = db.modify_definition(_variable_id);
    if (shader_cat.is_debug()) {
      shader_cat.debug()
        << "Changing type of variable " << _variable_id << " (" << def._name
        << ") from " << *def._type << " to " << *_new_type << "\n";
    }
    _pointer_type_id = module.define_pointer_type(_new_type, _new_storage_class);
    _type_id = module.unwrap_pointer_type(_pointer_type_id);

    decl->args[0] = _pointer_type_id;
    decl->args[2] = (uint32_t)_new_storage_class;

    def._type = _new_type;
    def._type_id = _pointer_type_id;
    def._storage_class = _new_storage_class;
    if (def.is_used()) {
      db.mark_used(_variable_id);
    }
  }

  // Now fix up all the loads and copies of the variable.
  for (Function &function : module.modify_functions()) {
    for (size_t i = 0; i < function.instructions.size(); ++i) {
      switch (function.instructions[i].opcode) {
      case spv::OpLoad:
        if (_pointer_ids.count(function.instructions[i].args[2])) {
          uint32_t result_id = function.instructions[i].args[1];
          uint32_t var_id = function.instructions[i].args[2];
          Definition &def = db.modify_definition(result_id);

          // If both are vectors or scalars, we can try a conversion.
          const ShaderType::Vector *old_vector = _new_type->as_vector();
          const ShaderType::Scalar *old_scalar = _new_type->as_scalar();
          const ShaderType::Vector *new_vector = def._type->as_vector();
          const ShaderType::Scalar *new_scalar = def._type->as_scalar();
          if ((old_vector != nullptr && new_vector != nullptr && old_vector != new_vector) ||
              (old_scalar != nullptr && new_scalar != nullptr && old_scalar != new_scalar) ||
              (old_vector != nullptr && new_scalar != nullptr) ||
              (old_scalar != nullptr && new_vector != nullptr)) {
            // Replace the original load with a load of the new type followed
            // by a conversion, of which the final op takes over the original
            // result id.
            SpirVBuilder builder(module, function.id, i);

            uint32_t temp = builder.op_load(var_id);
            ShaderType::ScalarType new_scalar_type;
            bool done = false;
            if (new_vector != nullptr && old_vector != nullptr) {
              // Swizzle the vector.
              new_scalar_type = new_vector->get_scalar_type();
              if (new_vector->get_num_components() != old_vector->get_num_components()) {
                pvector<uint32_t> components;
                uint32_t ci = 0;
                while (ci < new_vector->get_num_components() && ci < old_vector->get_num_components()) {
                  components.push_back(ci);
                  ++ci;
                }
                // The remaining components are undefined.
                while (ci < new_vector->get_num_components()) {
                  components.push_back(0xffffffff);
                  ++ci;
                }

                if (new_vector->get_scalar_type() == old_vector->get_scalar_type()) {
                  builder.push_id(result_id);
                  builder.op_vector_shuffle(temp, temp, components);
                  done = true;
                } else {
                  temp = builder.op_vector_shuffle(temp, temp, components);
                }
              }
            }
            else if (new_vector != nullptr) {
              // Convert the scalar, then replicate it into a vector.
              new_scalar_type = new_vector->get_scalar_type();
              temp = builder.op_convert(new_scalar_type, temp);
              pvector<uint32_t> components(new_vector->get_num_components(), temp);
              builder.push_id(result_id);
              builder.op_composite_construct(new_vector, components);
              done = true;
            }
            else if (new_scalar != nullptr) {
              new_scalar_type = new_scalar->get_scalar_type();
              if (old_vector != nullptr) {
                if (old_vector->get_scalar_type() == new_scalar_type) {
                  builder.push_id(result_id);
                  builder.op_composite_extract(temp, {0});
                  done = true;
                } else {
                  // Convert vector to scalar.
                  temp = builder.op_composite_extract(temp, {0});
                }
              }
            }
            else {
              new_scalar_type = new_scalar->get_scalar_type();
            }

            if (!done) {
              builder.push_id(result_id);
              builder.op_convert(new_scalar_type, temp);
            }

            // Remove the original load, which now follows the insertions.
            size_t orig = builder.get_insertion_index();
            function.instructions.erase(function.instructions.begin() + orig);
            i = orig - 1;
          }
          else {
            def._type = _new_type;
            def._type_id = _type_id;

            function.instructions[i].args[0] = _type_id;
            _object_ids.insert(result_id);
          }
        }
        break;

      case spv::OpAtomicLoad:
      case spv::OpAtomicExchange:
      case spv::OpAtomicCompareExchange:
      case spv::OpAtomicCompareExchangeWeak:
      case spv::OpAtomicIIncrement:
      case spv::OpAtomicIDecrement:
      case spv::OpAtomicIAdd:
      case spv::OpAtomicISub:
      case spv::OpAtomicSMin:
      case spv::OpAtomicUMin:
      case spv::OpAtomicSMax:
      case spv::OpAtomicUMax:
      case spv::OpAtomicAnd:
      case spv::OpAtomicOr:
      case spv::OpAtomicXor:
      case spv::OpAtomicFMinEXT:
      case spv::OpAtomicFMaxEXT:
      case spv::OpAtomicFAddEXT:
        // These loads turn a pointer into a dereferenced object.
        if (_pointer_ids.count(function.instructions[i].args[2])) {
          Instruction &op = function.instructions[i];
          Definition &def = db.modify_definition(op.args[1]);
          op.args[0] = _type_id;
          def._type = _new_type;
          def._type_id = _type_id;
          _object_ids.insert(op.args[1]);
        }
        break;

      case spv::OpCopyObject:
      case spv::OpExpectKHR:
        // This clones a pointer or object verbatim, so keep following the chain.
        {
          Instruction &op = function.instructions[i];
          if (_pointer_ids.count(op.args[2])) {
            Definition &def = db.modify_definition(op.args[1]);
            op.args[0] = _pointer_type_id;
            def._type = _new_type;
            def._type_id = _pointer_type_id;
            _pointer_ids.insert(op.args[1]);
          }
          if (_object_ids.count(op.args[2])) {
            Definition &def = db.modify_definition(op.args[1]);
            op.args[0] = _type_id;
            def._type = _new_type;
            def._type_id = _type_id;
            _object_ids.insert(op.args[1]);
          }
        }
        break;

      case spv::OpSelect:
        {
          Instruction &op = function.instructions[i];

          // The result type for this op must be the same for both operands.
          nassertd(_pointer_ids.count(op.args[3]) == _pointer_ids.count(op.args[4])) {}
          nassertd(_object_ids.count(op.args[3]) == _object_ids.count(op.args[4])) {}

          if (_pointer_ids.count(op.args[3])) {
            Definition &def = db.modify_definition(op.args[1]);
            op.args[0] = _pointer_type_id;
            def._type = _new_type;
            def._type_id = _pointer_type_id;
            _pointer_ids.insert(op.args[1]);
          }
          if (_object_ids.count(op.args[3])) {
            Definition &def = db.modify_definition(op.args[1]);
            op.args[0] = _type_id;
            def._type = _new_type;
            def._type_id = _type_id;
            _object_ids.insert(op.args[1]);
          }
        }
        break;

      default:
        break;
      }
    }
  }
}
