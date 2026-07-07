/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file test_small_vector.cxx
 * @author rdb
 * @date 2026-07-07
 *
 * Tests for the range constructor, assign and range insert that were added
 * for the SPIR-V instruction operand lists.
 */

#include "small_vector.h"

#include "catch_amalgamated.hpp"

TEST_CASE("small_vector constructs from an iterator range", "[dtoolutil]") {
  const uint32_t values[6] = {1, 2, 3, 4, 5, 6};

  // Within the inline capacity.
  small_vector<uint32_t, 4> small(values, values + 3);
  REQUIRE(small.size() == 3);
  CHECK(small[0] == 1);
  CHECK(small[2] == 3);

  // Spilling to the heap.
  small_vector<uint32_t, 4> large(values, values + 6);
  REQUIRE(large.size() == 6);
  for (uint32_t i = 0; i < 6; ++i) {
    CHECK(large[i] == i + 1);
  }

  // Empty range.
  small_vector<uint32_t, 4> empty(values, values);
  CHECK(empty.empty());
}

TEST_CASE("small_vector assign replaces the contents", "[dtoolutil]") {
  small_vector<uint32_t, 4> vec {1, 2, 3};

  const uint32_t values[6] = {9, 8, 7, 6, 5, 4};
  vec.assign(values, values + 6);
  REQUIRE(vec.size() == 6);
  CHECK(vec[0] == 9);
  CHECK(vec[5] == 4);

  // Assigning a shorter range shrinks it again.
  vec.assign(values, values + 2);
  REQUIRE(vec.size() == 2);
  CHECK(vec[0] == 9);
  CHECK(vec[1] == 8);

  // Assigning from another small_vector's range.
  small_vector<uint32_t, 4> other {11, 22, 33};
  vec.assign(other.begin() + 1, other.end());
  REQUIRE(vec.size() == 2);
  CHECK(vec[0] == 22);
  CHECK(vec[1] == 33);
}

TEST_CASE("small_vector inserts an iterator range", "[dtoolutil]") {
  small_vector<uint32_t, 4> vec {1, 2, 7, 8};

  // Insert in the middle, forcing a spill to the heap.
  const uint32_t mid[4] = {3, 4, 5, 6};
  auto it = vec.insert(vec.begin() + 2, mid, mid + 4);
  CHECK(*it == 3);
  REQUIRE(vec.size() == 8);
  for (uint32_t i = 0; i < 8; ++i) {
    CHECK(vec[i] == i + 1);
  }

  // Insert at the end.
  const uint32_t tail[2] = {9, 10};
  vec.insert(vec.end(), tail, tail + 2);
  REQUIRE(vec.size() == 10);
  CHECK(vec[8] == 9);
  CHECK(vec[9] == 10);

  // Insert at the beginning.
  const uint32_t head[1] = {0};
  vec.insert(vec.begin(), head, head + 1);
  REQUIRE(vec.size() == 11);
  for (uint32_t i = 0; i < 11; ++i) {
    CHECK(vec[i] == i);
  }

  // An empty range is a no-op.
  it = vec.insert(vec.begin() + 5, head, head);
  CHECK(it == vec.begin() + 5);
  REQUIRE(vec.size() == 11);

  // A single-element range insert without a spill.
  small_vector<uint32_t, 4> small {1, 3};
  const uint32_t two[1] = {2};
  small.insert(small.begin() + 1, two, two + 1);
  REQUIRE(small.size() == 3);
  CHECK(small[0] == 1);
  CHECK(small[1] == 2);
  CHECK(small[2] == 3);
}
