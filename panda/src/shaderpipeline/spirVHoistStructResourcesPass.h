/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file spirVHoistStructResourcesPass.h
 * @author rdb
 * @date 2024-10-08
 */

#ifndef SPIRVHOISTSTRUCTRESOURCESPASS_H
#define SPIRVHOISTSTRUCTRESOURCESPASS_H

#include "spirVTransformPass.h"

/**
 * Moves all opaque types (and arrays of opaque types, etc.) inside structs
 * outside the structs.
 */
class EXPCL_PANDA_SHADERPIPELINE SpirVHoistStructResourcesPass final : public SpirVTransformPass {
public:
  SpirVHoistStructResourcesPass(bool remove_empty_structs) :
    _remove_empty_structs(remove_empty_structs) {}

  virtual void run(SpirVModule &module) override;

private:
  void process_globals(SpirVModule &module);
  void process_function(SpirVModule &module, Function &function);

  INLINE bool is_deleted(uint32_t id) const {
    return _deleted.count(id) != 0;
  }

  // Remove structs that became empty due to only containing resources.
  const bool _remove_empty_structs;

  // Which type we need to hoist.
  pset<uint32_t> _hoist_types;

  // Ids whose declarations serve no further purpose.  The deletion is
  // applied at the end of the pass, so that the definitions remain
  // queryable while the function bodies are being rewritten.
  pset<uint32_t> _deleted;

  // Structs (and arrays thereof) that lost all their members because they
  // contained only resources.  Only used when _remove_empty_structs is false:
  // the type declaration itself is kept, since it may be a member of an
  // enclosing struct (where removing it would change the member numbering),
  // but pointers, variables and function parameters of these types serve no
  // further purpose and are removed.
  pset<uint32_t> _emptied_structs;

  // For each modified struct, a copy of its original member definitions (the
  // module's own member lists shrink as members are deleted), plus the map
  // from original member index to post-deletion index (-1 if deleted).
  pmap<uint32_t, SpirVResultDatabase::MemberDefinitions> _original_members;
  pmap<uint32_t, pvector<int> > _member_remap;

  // This stores the type IDs of all the types that (indirectly) contain the
  // type we want to unpack.  For each affected struct, access chains (struct
  // members only) leading to the hoisted type in question, as well as the
  // type that the wrapped additional variables should have.
  pmap<uint32_t, pvector<std::pair<const ShaderType *, AccessChain> > > _affected_types;
  pset<uint32_t> _affected_pointer_types;

public:
  // For each access chain consisting only of struct members
  // (prefixed by a variable id), map to the variable that has been hoisted
  pmap<AccessChain, uint32_t> _hoisted_vars;
};

#endif
