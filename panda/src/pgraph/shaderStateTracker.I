/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file shaderStateTracker.I
 * @author rdb
 * @date 2024-09-27
 */

/**
 *
 */
INLINE void ShaderStateTracker::
update_vertex_data(const GeomVertexDataPipelineReader *data_reader) {
  _data_reader = data_reader;
}
