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
  uint32_t result_id = fn(builder);
  finish_replace(builder, first_inserted, result_id);
}
