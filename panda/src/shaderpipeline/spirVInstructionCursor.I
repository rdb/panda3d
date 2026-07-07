/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file spirVInstructionCursor.I
 * @author rdb
 * @date 2026-07-07
 */

/**
 * Creates a cursor over the given function, which must belong to the given
 * module.  The cursor starts before the first instruction; call next() to
 * advance onto it.
 */
INLINE SpirVInstructionCursor::
SpirVInstructionCursor(SpirVModule &module, Function &function) :
  _module(module),
  _function(function) {
}

/**
 * Advances to the next instruction, skipping tombstones.  Returns false when
 * the end of the function has been reached.
 */
INLINE bool SpirVInstructionCursor::
next() {
  while (++_index < _function.instructions.size()) {
    if (!_function.instructions[_index].is_nop()) {
      return true;
    }
  }
  return false;
}

/**
 * Returns the current instruction.
 */
INLINE SpirVInstructionCursor::Instruction &SpirVInstructionCursor::
operator * () {
  return _function.instructions[_index];
}

/**
 * Returns the current instruction.
 */
INLINE SpirVInstructionCursor::Instruction *SpirVInstructionCursor::
operator -> () {
  return &_function.instructions[_index];
}

/**
 * Returns the index of the current instruction within the function.
 */
INLINE size_t SpirVInstructionCursor::
get_index() const {
  return _index;
}

/**
 *
 */
INLINE SpirVModule &SpirVInstructionCursor::
get_module() {
  return _module;
}

/**
 *
 */
INLINE SpirVInstructionCursor::Function &SpirVInstructionCursor::
get_function() {
  return _function;
}

/**
 * Runs the given callable with a builder positioned just before the current
 * instruction.  Afterwards, the cursor is back on the current instruction,
 * which the inserted code now precedes.
 */
template<class Callable>
INLINE void SpirVInstructionCursor::
insert_before(Callable &&fn) {
  SpirVBuilder builder(_module, _function.id, _index);
  fn(builder);
  after_insert(builder);
}

/**
 * Runs the given callable with a builder positioned just past the current
 * instruction.  Afterwards, the cursor stands on the last inserted
 * instruction (or stays on the current one if nothing was inserted), so
 * that next() continues with the instruction that originally followed and
 * the inserted code is not itself visited.  Since the current instruction
 * is no longer addressable afterwards, finish rewriting it first.
 */
template<class Callable>
INLINE void SpirVInstructionCursor::
insert_after(Callable &&fn) {
  SpirVBuilder builder(_module, _function.id, _index + 1);
  fn(builder);
  nassertv(builder.get_current_function_id() == _function.id);
  _index = builder.get_insertion_index() - 1;
}

/**
 * Runs the given callable with a builder positioned just before the current
 * instruction, then removes the current instruction, renumbering the
 * inserted instruction whose result the callable returned so that it
 * produces the removed instruction's result id.  See the class description.
 */
template<class Callable>
INLINE void SpirVInstructionCursor::
replace(Callable &&fn) {
  size_t first_inserted = _index;
  SpirVBuilder builder(_module, _function.id, _index);
  SpirVId result_id = fn(builder);
  finish_replace(builder, first_inserted, result_id);
}

/**
 * Tombstones the current instruction without clearing its result id's
 * state, for a pass that defers the id deletion (through
 * SpirVModule::delete_id) to the end while keeping the id queryable.  The
 * cursor remains valid; next() skips the tombstone.
 */
INLINE void SpirVInstructionCursor::
detach() {
  nassertv(_index < _function.instructions.size());
  _function.instructions[_index] = Instruction(spv::OpNop);
}

/**
 * Creates a cursor over the globals section of the given module.  Only the
 * declarations present now will be visited.  The cursor starts before the
 * first declaration; call next() to advance onto it.
 */
INLINE SpirVGlobalsCursor::
SpirVGlobalsCursor(SpirVModule &module) :
  _module(module),
  _end(module.get_globals().size()) {
}

/**
 * Advances to the next declaration, skipping tombstones.  Returns false
 * when the end of the original section has been reached.
 */
INLINE bool SpirVGlobalsCursor::
next() {
  while (++_index < _end) {
    if (!_module.get_globals()[_index].is_nop()) {
      return true;
    }
  }
  return false;
}

/**
 * Returns the current declaration.  The reference is invalidated when a
 * declaration is appended; take it again afterwards.
 */
INLINE SpirVGlobalsCursor::Instruction &SpirVGlobalsCursor::
operator * () {
  return _module.modify_globals()[_index];
}

/**
 * Returns the current declaration.  The pointer is invalidated when a
 * declaration is appended; take it again afterwards.
 */
INLINE SpirVGlobalsCursor::Instruction *SpirVGlobalsCursor::
operator -> () {
  return &_module.modify_globals()[_index];
}

/**
 *
 */
INLINE SpirVModule &SpirVGlobalsCursor::
get_module() {
  return _module;
}
