/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file loaderFileTypeGltf.cxx
 * @author rdb
 * @date 2017-07-26
 */

#include "loaderFileTypeGltf.h"
#include "modelRoot.h"
#include "geomNode.h"
#include "gltfRoot.h"
#include "gltfLoader.h"
#include "gltfSaver.h"

TypeHandle LoaderFileTypeGltf::_type_handle;

/**
 *
 */
LoaderFileTypeGltf::
LoaderFileTypeGltf() {
}

/**
 *
 */
string LoaderFileTypeGltf::
get_name() const {
  return "glTF";
}

/**
 *
 */
string LoaderFileTypeGltf::
get_extension() const {
  return "gltf";
}

/**
 * Returns a space-separated list of extension, in addition to the one
 * returned by get_extension(), that are recognized by this loader.
 */
string LoaderFileTypeGltf::
get_additional_extensions() const {
  return "glb";
}

/**
 * Returns true if this file type can transparently load compressed files
 * (with a .pz or .gz extension), false otherwise.
 */
bool LoaderFileTypeGltf::
supports_compressed() const {
  return true;
}

/**
 * Returns true if the file type can be used to load files, and load_file() is
 * supported.  Returns false if load_file() is unimplemented and will always
 * fail.
 */
bool LoaderFileTypeGltf::
supports_load() const {
  return true;
}

/**
 * Returns true if the file type can be used to save files, and save_file() is
 * supported.  Returns false if save_file() is unimplemented and will always
 * fail.
 */
bool LoaderFileTypeGltf::
supports_save() const {
  return true;
}

/**
 *
 */
PT(PandaNode) LoaderFileTypeGltf::
load_file(const Filename &path, const LoaderOptions &,
          BamCacheRecord *record) const {

  GltfRoot data;
  if (!data.read(path, record)) {
    return nullptr;
  }

  GltfLoader loader(data);
  loader.update_all();
  PT(PandaNode) node = loader.get_root_node();
  return node;
}

/**
 *
 */
bool LoaderFileTypeGltf::
save_file(const Filename &path, const LoaderOptions &options,
          PandaNode *node) const {
  GltfRoot data;
  /*GltfSaver saver(data);
  if (node->is_of_type(ModelRoot::get_class_type())) {
    data.set_scene(saver.add_scene(node));
  } else {
    GltfScene *scene = data.make_scene();
    scene->add_node(saver.add_node(node));
    scene->set_name(node->get_name());
    data.set_scene(scene);
  }*/

  return data.write_gltf(path);
}
