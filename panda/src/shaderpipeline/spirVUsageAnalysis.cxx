/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file spirVUsageAnalysis.cxx
 * @author rdb
 * @date 2026-07-07
 */

#include "spirVUsageAnalysis.h"

#include "GLSL.std.450.h"

/**
 * Runs the analysis over the given module.
 */
SpirVUsageAnalysis::
SpirVUsageAnalysis(const SpirVModule &module) {
  uint32_t bound = module.get_id_bound();
  _flags.resize(bound, 0);
  _origins.resize(bound, 0);

  // Seed the module-scope state: variables and parameters originate from
  // themselves, constants are constant expressions.
  for (uint32_t id = 0; id < bound; ++id) {
    switch (module.get_kind(id)) {
    case SpirVModule::IdKind::VARIABLE:
    case SpirVModule::IdKind::PARAMETER:
      _origins[id] = id;
      break;

    case SpirVModule::IdKind::CONSTANT:
    case SpirVModule::IdKind::SPEC_CONSTANT:
      _flags[id] |= UF_constant_expression;
      break;

    default:
      break;
    }
  }

  // Identify the GLSL.std.450 extended instruction set, whose interpolation
  // and pointer-output functions consume variable pointers.
  pset<uint32_t> glsl_std_450;
  for (const SpirVModule::Instruction &op : module.get_ext_inst_imports()) {
    if (!op.args.empty() && op.get_string(1) == "GLSL.std.450") {
      glsl_std_450.insert(op.args[0]);
    }
  }

  for (const SpirVModule::Function &function : module.get_functions()) {
    for (const SpirVModule::Instruction &op : function.instructions) {
      switch (op.opcode) {
      case spv::OpLoad:
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
      case spv::OpAtomicFlagTestAndSet:
      case spv::OpAtomicFMinEXT:
      case spv::OpAtomicFMaxEXT:
      case spv::OpAtomicFAddEXT:
      case spv::OpCooperativeMatrixLoadKHR:
        // Loading through a pointer makes its variable used.
        if (op.args.size() >= 3) {
          set_origin(op.args[1], get_origin(op.args[2]));
          mark_used(op.args[1]);
        }
        break;

      case spv::OpStore:
      case spv::OpAtomicStore:
      case spv::OpAtomicFlagClear:
      case spv::OpCooperativeMatrixStoreKHR:
        // A write creates no result id, but the variable counts as used.
        if (!op.args.empty()) {
          mark_used(op.args[0]);
        }
        break;

      case spv::OpCopyMemory:
      case spv::OpCopyMemorySized:
        if (op.args.size() >= 2) {
          mark_used(op.args[0]);
          mark_used(op.args[1]);
        }
        break;

      case spv::OpAccessChain:
      case spv::OpInBoundsAccessChain:
      case spv::OpPtrAccessChain:
      case spv::OpInBoundsPtrAccessChain:
        // Chains propagate their base variable, so that a load through them
        // can transitively mark it used.
        if (op.args.size() >= 3) {
          set_origin(op.args[1], get_origin(op.args[2]));

          // If one of the indices (including the base element for
          // OpPtrAccessChain) isn't a constant expression, the variable is
          // dynamically indexed.
          for (size_t i = 3; i < op.args.size(); ++i) {
            if (!has_flag(op.args[i], UF_constant_expression)) {
              set_origin_flag(op.args[1], UF_dynamically_indexed);
              break;
            }
          }
        }
        break;

      case spv::OpImageTexelPointer:
      case spv::OpSampledImage:
        if (op.args.size() >= 3) {
          set_origin(op.args[1], get_origin(op.args[2]));
        }
        break;

      case spv::OpImage:
        if (op.args.size() >= 3) {
          set_origin(op.args[1], get_origin(op.args[2]));
          set_flag(op.args[1], UF_sampled_image);
        }
        break;

      case spv::OpCopyObject:
      case spv::OpExpectKHR:
      case spv::OpCompositeExtract:
        // These preserve where the value came from and whether it is a
        // constant expression.  (OpCompositeExtract does not propagate the
        // origin, only the flags; composites of pointers do not occur.)
        if (op.args.size() >= 3) {
          if (op.opcode != spv::OpCompositeExtract) {
            set_origin(op.args[1], get_origin(op.args[2]));
          }
          if (op.args[2] < _flags.size()) {
            _flags[op.args[1]] |= _flags[op.args[2]] &
              (UF_constant_expression | UF_sampled_image);
          }
        }
        break;

      case spv::OpBitcast:
        if (op.args.size() >= 3) {
          set_origin(op.args[1], get_origin(op.args[2]));
          if (has_flag(op.args[2], UF_constant_expression)) {
            set_flag(op.args[1], UF_constant_expression);
          }
          // Casting to a non-pointer type counts as a load.
          if (module.get_kind(op.args[0]) != SpirVModule::IdKind::POINTER_TYPE) {
            mark_used(op.args[1]);
          }
        }
        break;

      case spv::OpFunctionCall:
        // Mark all arguments as used.  In the future we could be smart
        // enough to only mark the arguments used if the relevant parameters
        // are used within the function itself.
        for (size_t i = 3; i < op.args.size(); ++i) {
          mark_used(op.args[i]);
        }
        // Mark the function as used (even if its return value is unused -
        // the function may have side effects).
        if (op.args.size() >= 3) {
          set_flag(op.args[2], UF_used);
        }
        break;

      case spv::OpExtInst:
        // Some GLSL standard functions take pointers as arguments.
        if (op.args.size() >= 5 && glsl_std_450.count(op.args[2])) {
          switch (op.args[3]) {
          case GLSLstd450Modf:
          case GLSLstd450Frexp:
            if (op.args.size() >= 6) {
              mark_used(op.args[5]);
            }
            break;

          case GLSLstd450InterpolateAtCentroid:
            mark_used(op.args[4]);
            break;

          case GLSLstd450InterpolateAtSample:
          case GLSLstd450InterpolateAtOffset:
            mark_used(op.args[4]);
            if (op.args.size() >= 6) {
              mark_used(op.args[5]);
            }
            break;
          }
        }
        break;

      case spv::OpImageSampleImplicitLod:
      case spv::OpImageSampleExplicitLod:
      case spv::OpImageSampleProjImplicitLod:
      case spv::OpImageSampleProjExplicitLod:
      case spv::OpImageFetch:
      case spv::OpImageGather:
      case spv::OpImageSparseSampleImplicitLod:
      case spv::OpImageSparseSampleExplicitLod:
      case spv::OpImageSparseSampleProjImplicitLod:
      case spv::OpImageSparseSampleProjExplicitLod:
      case spv::OpImageSparseFetch:
      case spv::OpImageSparseGather:
        if (op.args.size() >= 3) {
          set_origin_flag(op.args[2], UF_non_dref_sampled);
        }
        break;

      case spv::OpImageSampleDrefImplicitLod:
      case spv::OpImageSampleDrefExplicitLod:
      case spv::OpImageSampleProjDrefImplicitLod:
      case spv::OpImageSampleProjDrefExplicitLod:
      case spv::OpImageDrefGather:
      case spv::OpImageSparseSampleDrefImplicitLod:
      case spv::OpImageSparseSampleDrefExplicitLod:
      case spv::OpImageSparseSampleProjDrefImplicitLod:
      case spv::OpImageSparseSampleProjDrefExplicitLod:
      case spv::OpImageSparseDrefGather:
        if (op.args.size() >= 3) {
          set_origin_flag(op.args[2], UF_dref_sampled);
        }
        break;

      case spv::OpImageQuerySizeLod:
      case spv::OpImageQuerySize:
      case spv::OpImageQueryLevels:
        if (op.args.size() >= 3) {
          set_origin_flag(op.args[2], UF_queried_size_levels);
        }
        break;

      case spv::OpArrayLength:
      case spv::OpConvertPtrToU:
        if (op.args.size() >= 3) {
          mark_used(op.args[2]);
        }
        break;

      case spv::OpSelect:
        // This can in theory operate on pointers, which is why we handle it.
        if (op.args.size() >= 5) {
          mark_used(op.args[3]);
          mark_used(op.args[4]);
          if (has_flag(op.args[2], UF_constant_expression) &&
              has_flag(op.args[3], UF_constant_expression) &&
              has_flag(op.args[4], UF_constant_expression)) {
            set_flag(op.args[1], UF_constant_expression);
          }
        }
        break;

      case spv::OpReturnValue:
        // A pointer can be returned when certain caps are present.
        if (!op.args.empty()) {
          mark_used(op.args[0]);
        }
        break;

      case spv::OpPtrEqual:
      case spv::OpPtrNotEqual:
      case spv::OpPtrDiff:
        // Consider a variable "used" if its pointer value is being compared,
        // to be on the safe side.
        if (op.args.size() >= 4) {
          mark_used(op.args[2]);
          mark_used(op.args[3]);
        }
        break;

      // Unary operators that preserve constant-expression status.
      case spv::OpConvertFToU:
      case spv::OpConvertFToS:
      case spv::OpConvertSToF:
      case spv::OpConvertUToF:
      case spv::OpQuantizeToF16:
      case spv::OpSatConvertSToU:
      case spv::OpSatConvertUToS:
      case spv::OpConvertUToPtr:
      case spv::OpSNegate:
      case spv::OpFNegate:
      case spv::OpAny:
      case spv::OpAll:
      case spv::OpIsNan:
      case spv::OpIsInf:
      case spv::OpIsFinite:
      case spv::OpIsNormal:
      case spv::OpSignBitSet:
      case spv::OpNot:
      case spv::OpConvertFToBF16INTEL:
      case spv::OpConvertBF16ToFINTEL:
        if (op.args.size() >= 3 &&
            has_flag(op.args[2], UF_constant_expression)) {
          set_flag(op.args[1], UF_constant_expression);
        }
        break;

      // Binary operators that preserve constant-expression status.
      case spv::OpIAdd:
      case spv::OpFAdd:
      case spv::OpISub:
      case spv::OpFSub:
      case spv::OpIMul:
      case spv::OpFMul:
      case spv::OpUDiv:
      case spv::OpSDiv:
      case spv::OpFDiv:
      case spv::OpUMod:
      case spv::OpSRem:
      case spv::OpSMod:
      case spv::OpFRem:
      case spv::OpFMod:
      case spv::OpVectorTimesScalar:
      case spv::OpMatrixTimesScalar:
      case spv::OpVectorTimesMatrix:
      case spv::OpMatrixTimesVector:
      case spv::OpMatrixTimesMatrix:
      case spv::OpOuterProduct:
      case spv::OpDot:
      case spv::OpIAddCarry:
      case spv::OpISubBorrow:
      case spv::OpUMulExtended:
      case spv::OpSMulExtended:
      case spv::OpShiftRightLogical:
      case spv::OpShiftRightArithmetic:
      case spv::OpShiftLeftLogical:
      case spv::OpBitwiseOr:
      case spv::OpBitwiseXor:
      case spv::OpBitwiseAnd:
        if (op.args.size() >= 4 &&
            has_flag(op.args[2], UF_constant_expression) &&
            has_flag(op.args[3], UF_constant_expression)) {
          set_flag(op.args[1], UF_constant_expression);
        }
        break;

      default:
        break;
      }
    }
  }
}
