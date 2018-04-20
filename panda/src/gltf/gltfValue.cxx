/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfValue.h
 * @author rdb
 * @date 2017-09-17
 */

#include "gltfValue.h"
#include "gltfArray.h"
#include "gltfDictionary.h"

GltfValue::
GltfValue(GltfArray<GltfValue> value) : _type(T_array) {
  _value.as_array = new GltfArray<GltfValue>(move(value));
}

GltfValue::
GltfValue(GltfDictionary<GltfValue> value) : _type(T_object) {
  _value.as_object = new GltfDictionary<GltfValue>(move(value));
}
