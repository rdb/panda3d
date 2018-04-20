/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file config_gltf.cxx
 * @author rdb
 * @date 2017-07-27
 */

#include "config_gltf.h"
#include "loaderFileTypeGltf.h"
#include "loaderFileTypeRegistry.h"
#include "dconfig.h"

Configure(config_gltf);
NotifyCategoryDef(gltf, "");

ConfigureFn(config_gltf) {
  init_libgltf();
}

/**
 * Initializes the library.  This must be called at least once before any of
 * the functions or classes in this library can be used.  Normally it will be
 * called by the static initializers and need not be called explicitly, but
 * special cases exist.
 */
void
init_libgltf() {
  static bool initialized = false;
  if (initialized) {
    return;
  }
  initialized = true;

  LoaderFileTypeGltf::init_type();

  LoaderFileTypeRegistry *reg = LoaderFileTypeRegistry::get_global_ptr();
  reg->register_type(new LoaderFileTypeGltf);
}
