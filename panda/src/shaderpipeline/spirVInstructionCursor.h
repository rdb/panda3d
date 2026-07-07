/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file spirVInstructionCursor.h
 * @author rdb
 * @date 2026-07-07
 */

#ifndef SPIRVINSTRUCTIONCURSOR_H
#define SPIRVINSTRUCTIONCURSOR_H

#include "spirVModule.h"
#include "spirVBuilder.h"

/**
 * Iterates over the instructions of a function while rewriting them.  This
 * wraps the fiddly parts of in-place rewriting that every pass would
 * otherwise have to get right by hand: instruction references that are
 * invalidated by insertions, resynchronizing the loop index past inserted
 * code, and the choice between tombstoning and erasing a removed
 * instruction.
 *
 * The rewriting methods take a callable that receives a SpirVBuilder
 * positioned just before the current instruction:
 *
 * - insert_before() runs the callable and leaves the cursor on the current
 *   instruction, so that inserted code is not itself visited.
 *
 * - replace() additionally removes the current instruction.  The callable
 *   returns the id holding the replacement value, which must be the result
 *   of one of the instructions it inserted; that instruction is renumbered
 *   to produce the replaced instruction's original result id, so that
 *   downstream uses keep working without a rewrite.  Because the
 *   renumbering happens afterwards, the callable does not need to know in
 *   advance which of its instructions will produce the final value.  If the
 *   replaced instruction has no result id, the callable returns 0.
 *
 * - remove() deletes the current instruction: through
 *   SpirVModule::delete_id if it has a result id (clearing its names,
 *   annotations and index entry), by tombstoning otherwise.
 *
 * All of these keep the cursor valid; next() continues with the instruction
 * following the current one.  A callable may insert instructions and create
 * global definitions, but must not add or delete functions.
 */
class EXPCL_PANDA_SHADERPIPELINE SpirVInstructionCursor {
public:
  using Instruction = SpirVModule::Instruction;
  using Function = SpirVModule::Function;

  INLINE SpirVInstructionCursor(SpirVModule &module, Function &function);

  INLINE bool next();

  // Access to the current instruction.  The reference is invalidated by
  // insert_before() and replace(); take it again afterwards.
  INLINE Instruction &operator *();
  INLINE Instruction *operator ->();

  INLINE size_t get_index() const;
  INLINE SpirVModule &get_module();
  INLINE Function &get_function();

  template<class Callable>
  INLINE void insert_before(Callable &&fn);

  template<class Callable>
  INLINE void replace(Callable &&fn);

  void remove();

private:
  void after_insert(const SpirVBuilder &builder);
  void finish_replace(const SpirVBuilder &builder, size_t first_inserted,
                      uint32_t result_id);

  SpirVModule &_module;
  Function &_function;
  size_t _index = (size_t)-1;
};

#include "spirVInstructionCursor.I"

#endif
