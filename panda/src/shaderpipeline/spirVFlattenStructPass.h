/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file spirVFlattenStructPass.h
 * @author rdb
 * @date 2024-10-08
 */

#ifndef SPIRVFLATTENSTRUCTPASS_H
#define SPIRVFLATTENSTRUCTPASS_H

#include "spirVTransformPass.h"

/**
 * Converts the members of the struct type with the given ID to regular
 * variables.  Useful for unwrapping uniform blocks.
 */
class EXPCL_PANDA_SHADERPIPELINE SpirVFlattenStructPass final : public SpirVTransformPass {
public:
  SpirVFlattenStructPass(SpirVId type_id);

  virtual void run(SpirVModule &module) override;

private:
  const SpirVId _type_id;
  const ShaderType::Struct *_struct_type = nullptr;

  // The ids of the deleted struct variables and access chains thereinto.
  pset<SpirVId> _deleted;

  // Maps access chains accessing struct members to the created variable IDs
  // for that struct member.
  pmap<SpirVId, SpirVId> _deleted_access_chains;

  // Holds the new variable IDs for each of the struct members.
  pvector<SpirVId> _member_ids;
};

#endif
