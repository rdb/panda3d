/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file config_gltf.h
 * @author rdb
 * @date 2017-07-27
 */

#ifndef CONFIG_GLTF_H
#define CONFIG_GLTF_H

#include "pandabase.h"
#include "notifyCategoryProxy.h"
#include "configVariableInt.h"

#ifdef BUILDING_GLTF
  #define EXPCL_GLTF EXPORT_CLASS
  #define EXPTP_GLTF EXPORT_TEMPL
#else
  #define EXPCL_GLTF IMPORT_CLASS
  #define EXPTP_GLTF IMPORT_TEMPL
#endif

NotifyCategoryDecl(gltf, EXPCL_GLTF, EXPTP_GLTF);

extern EXPCL_GLTF void init_libgltf();

#endif
