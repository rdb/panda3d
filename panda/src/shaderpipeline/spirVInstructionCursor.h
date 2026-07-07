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
 * positioned adjacent to the current instruction; scoping the builder to the
 * callable means only one builder exists at a time, and the cursor resyncs
 * from it at a single point when it returns:
 *
 * - insert_before() positions the builder just before the current
 *   instruction and leaves the cursor on the current instruction, so that
 *   inserted code is not itself visited.
 *
 * - insert_after() positions the builder just past the current instruction;
 *   afterwards the cursor stands on the last inserted instruction (finish
 *   rewriting the current instruction first, since it is no longer
 *   addressable), and next() continues with the instruction that originally
 *   followed, so that inserted code is not itself visited.
 *
 * - replace() inserts before the current instruction and additionally
 *   removes it.  The callable returns the id holding the replacement value,
 *   which must be the result of one of the instructions it inserted; that
 *   instruction is renumbered to produce the replaced instruction's original
 *   result id, so that downstream uses keep working without a rewrite.
 *   Because the renumbering happens afterwards, the callable does not need
 *   to know in advance which of its instructions will produce the final
 *   value.  If the replaced instruction has no result id, the callable
 *   returns 0.
 *
 * - remove() deletes the current instruction: through
 *   SpirVModule::delete_id if it has a result id (clearing its names,
 *   annotations and index entry), by tombstoning otherwise.
 *
 * - detach() tombstones the current instruction without touching the id
 *   state, for a pass that needs the id to remain queryable while the rest
 *   of the module is rewritten; the pass calls SpirVModule::delete_id at the
 *   end (which tolerates the declaring instruction already being gone).
 *
 * All of these keep the cursor valid; next() continues with the instruction
 * following the current one.  A callable may insert instructions and create
 * global definitions, but must not add or delete functions.
 */
class EXPCL_PANDA_SHADERPIPELINE SpirVInstructionCursor {
public:
  using Instruction = SpirVModule::Instruction;
  using Function = SpirVModule::Function;
  using Id = SpirVId;

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
  INLINE void insert_after(Callable &&fn);

  template<class Callable>
  INLINE void replace(Callable &&fn);

  void remove();
  INLINE void detach();

private:
  void after_insert(const SpirVBuilder &builder);
  void finish_replace(const SpirVBuilder &builder, size_t first_inserted,
                      SpirVId result_id);

  SpirVModule &_module;
  Function &_function;
  size_t _index = (size_t)-1;
};

/**
 * Iterates over the globals section of a module while rewriting it.  Only
 * the declarations present when the cursor was created are visited: the
 * definitions a pass appends during the walk (define_type and friends) are
 * not, and do not disturb the iteration.
 *
 * Appending may reallocate the section, so do not hold a reference to the
 * current instruction across a definition call; access it through the
 * cursor, which refetches by index.  (Reading operands as arguments to the
 * call that appends is fine: arguments are evaluated first.)
 *
 * There are no insertion methods; new declarations are appended through the
 * module, which needs no particular position thanks to the emit-time
 * topological sort.  To delete the current declaration, call
 * SpirVModule::delete_id, which tombstones it in place.
 */
class EXPCL_PANDA_SHADERPIPELINE SpirVGlobalsCursor {
public:
  using Instruction = SpirVModule::Instruction;
  using Id = SpirVId;

  INLINE SpirVGlobalsCursor(SpirVModule &module);

  INLINE bool next();

  INLINE Instruction &operator *();
  INLINE Instruction *operator ->();

  INLINE SpirVModule &get_module();

private:
  SpirVModule &_module;
  size_t _index = (size_t)-1;
  size_t _end;
};

#include "spirVInstructionCursor.I"

#endif
