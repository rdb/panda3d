/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfLoader.cxx
 * @author rdb
 * @date 2017-07-28
 */

#include "gltfLoader.h"
#include "gltfLoaderCallbacks.h"
#include "geomLines.h"
#include "geomLinestrips.h"
#include "geomPoints.h"
#include "geomTriangles.h"
#include "geomTrifans.h"
#include "geomTristrips.h"
#include "geomVertexReader.h"
#include "geomVertexWriter.h"
#include "material.h"
#include "renderState.h"
#include "alphaTestAttrib.h"
#include "cullFaceAttrib.h"
#include "materialAttrib.h"
#include "textureAttrib.h"
#include "transparencyAttrib.h"
#include "texturePool.h"
#include "orthographicLens.h"
#include "perspectiveLens.h"
#include "pnmImage.h"

/**
 * Creates a GltfLoader that loads the given GltfRoot.
 * @param coordsys Indicates which system to convert the data to (the glTF
 *                 data itself is always in right-handed Y-up).
 */
GltfLoader::
GltfLoader(const GltfRoot &root, CoordinateSystem coordsys) :
  _root(root),
  _coordsys(coordsys)
{
  if (_coordsys == CS_default) {
    _coordsys = get_default_coordinate_system();
  }
  _cs_matrix = LMatrix4::convert_mat(CS_yup_right, _coordsys);
  _inv_cs_matrix = LMatrix4::convert_mat(_coordsys, CS_yup_right);

  _default_scene = new ModelRoot(_root.get_gltf_filename(), _root.get_gltf_timestamp());
}

/**
 * This is the primary interface to the glTF loader.  It recursively checks
 * whether any of the structures in the glTF file need to be (re-)converted,
 * and does so.
 * @return true if anything was updated, false otherwise.
 */
bool GltfLoader::
update_all() {
  _any_updated = false;

  // Update all the meshes.
  _meshes.reserve(_root.get_num_meshes());
  for (size_t i = 0; i < _root.get_num_meshes(); ++i) {
    const GltfMesh *mesh = _root.get_mesh(i);
    update_mesh(mesh);
  }

  // Update all the nodes.
  _nodes.reserve(_root.get_num_nodes());
  for (size_t i = 0; i < _root.get_num_nodes(); ++i) {
    const GltfNode *node = _root.get_node(i);
    update_node(node);
  }

  // Update the default scene.
  const GltfScene *scene = _root.get_scene();
  load_scene(_default_scene, scene);

  return _any_updated;
}

/**
 * Updates the Panda representation of the given node, if needed.
 */
PandaNode *GltfLoader::
update_node(const GltfNode *gltf_node) {
  size_t index = gltf_node->get_index();
  if (index >= _nodes.size()) {
    _nodes.resize(index + 1);
  }

  ConvertedNode &node = _nodes[gltf_node->get_index()];
  UpdateSeq modified = gltf_node->get_modified();
  if (node._node != nullptr && node._updated == modified) {
    // It has not been modified.
    return node._node.p();
  }

  // Protect against reentry.
  if (node._updating) {
    gltf_cat.error()
      << "Detected attempt to recursively update node " << *gltf_node << ". "
         "Check that there are no cycles in the scene graph.\n";
    return node._node;
  }
  node._updating = true;

  // We remove all the children, and readd them as we do the conversion.  This
  // makes things significantly easier when dealing with corner cases such as
  // nodes changing type.
  if (node._node) {
    node._node->remove_all_children();
  }

  GltfLoaderCallbacks *callbacks = GltfLoaderCallbacks::_first;
  if (callbacks == nullptr) {
    // We have no registered callbacks.
    node._node = do_update_node(node._node.p(), gltf_node);
  } else {
    // Call the first callback, which will cycle through the chain.
    callbacks->_loader = this;
    node._node = callbacks->update_node(node._node.p(), gltf_node);
    callbacks->_loader = nullptr;
  }
  node._updated = modified;
  node._updating = false;
  _any_updated = true;
  return node._node.p();
}

/**
 * Updates the Panda representation of the given mesh, if needed.
 * What's returned is a GeomNode that is merely used as a container for the
 * meshes within, and may not necessarily be present in the scene graph.
 */
const GeomNode *GltfLoader::
update_mesh(const GltfMesh *gltf_mesh) {
  size_t index = gltf_mesh->get_index();
  if (index >= _meshes.size()) {
    _meshes.resize(index + 1);
  }

  ConvertedMesh &mesh = _meshes[gltf_mesh->get_index()];
  UpdateSeq modified = gltf_mesh->get_modified();
  if (mesh._node != nullptr && mesh._updated == modified) {
    // It has not been modified.
    return mesh._node.p();
  }

  // We create a GeomNode for this mesh, but we only use it as a temporary
  // container to store the geoms.
  if (mesh._node == nullptr) {
    mesh._node = new GeomNode("");
  }

  mesh._node->remove_all_geoms();

  //TODO: should we sort the primitives by material index, unifying them?

  GltfLoaderCallbacks *callbacks = GltfLoaderCallbacks::_first;
  if (callbacks == nullptr) {
    // We have no registered callbacks.
    for (size_t i = 0; i < gltf_mesh->get_num_primitives(); ++i) {
      const GltfMesh::Primitive &prim = gltf_mesh->get_primitive(i);
      mesh._node->add_geom(make_geom(gltf_mesh, prim),
                           make_geom_state(gltf_mesh, prim));
    }
  } else {
    // Call the first callback, which will cycle through the chain.
    callbacks->_loader = this;
    for (size_t i = 0; i < gltf_mesh->get_num_primitives(); ++i) {
      const GltfMesh::Primitive &prim = gltf_mesh->get_primitive(i);
      mesh._node->add_geom(callbacks->make_geom(gltf_mesh, prim),
                           callbacks->make_geom_state(gltf_mesh, prim));
    }
    callbacks->_loader = nullptr;
  }
  mesh._updated = modified;
  _any_updated = true;
  return mesh._node.p();
}

/**
 * Updates the Panda representation of the given material, if needed.  Note
 * that the returned material is not a complete representation of the state.
 * Instead, use update_material_state() to get the updated state.
 */
Material *GltfLoader::
update_material(const GltfMaterial *gltf_material) {
  size_t index = gltf_material->get_index();
  if (index >= _materials.size()) {
    _materials.resize(index + 1);
  }

  ConvertedMaterial &mat = _materials[gltf_material->get_index()];
  UpdateSeq modified = gltf_material->get_modified();
  if (mat._material != nullptr && mat._updated == modified) {
    // It has not been modified.
    return mat._material;
  }

  GltfLoaderCallbacks *callbacks = GltfLoaderCallbacks::_first;
  if (callbacks == nullptr) {
    // We have no registered callbacks.
    mat._material = do_update_material(mat._material.p(), gltf_material);
  } else {
    // Call the first callback, which will cycle through the chain.
    callbacks->_loader = this;
    mat._material = callbacks->update_material(mat._material.p(), gltf_material);
    callbacks->_loader = nullptr;
  }
  mat._updated = modified;
  _any_updated = true;
  return mat._material.p();
}

/**
 * Updates the Panda representation of the given texture, if needed.
 */
Texture *GltfLoader::
update_texture(const GltfTexture *gltf_texture) {
  size_t index = gltf_texture->get_index();
  if (index >= _textures.size()) {
    _textures.resize(index + 1);
  }

  ConvertedTexture &mat = _textures[gltf_texture->get_index()];
  UpdateSeq modified = gltf_texture->get_modified();
  UpdateSeq source_modified = gltf_texture->get_source_modified();
  UpdateSeq sampler_modified = gltf_texture->get_sampler_modified();
  if (mat._texture != nullptr &&
      mat._updated == modified &&
      mat._source_updated == source_modified &&
      mat._sampler_updated == sampler_modified) {
    // It has not been modified.
    return mat._texture;
  }

  GltfLoaderCallbacks *callbacks = GltfLoaderCallbacks::_first;
  if (callbacks == nullptr) {
    // We have no registered callbacks.
    mat._texture = do_update_texture(mat._texture.p(), gltf_texture);
  } else {
    // Call the first callback, which will cycle through the chain.
    callbacks->_loader = this;
    mat._texture = callbacks->update_texture(mat._texture.p(), gltf_texture);
    callbacks->_loader = nullptr;
  }
  mat._updated = modified;
  mat._source_updated = source_modified;
  mat._sampler_updated = sampler_modified;
  _any_updated = true;
  return mat._texture.p();
}

/**
 * Loads the indicated scene under the given scene root.  Note that nodes are
 * not duplicated if this is called multiple times for the same scene,
 * although the nodes themselves will be updated by update_all() or
 * update_node(), though the scene itself will not.
 *
 * @param root The root under which the scene nodes will be instantiated.
 * @param scene The glTF scene to convert the nodes of.
 */
void GltfLoader::
load_scene(PandaNode *root, const GltfScene *scene) {
  root->remove_all_children();

  for (size_t i = 0; i < scene->get_num_nodes(); ++i) {
    const GltfNode *node = scene->get_node(i);
    root->add_child(update_node(node));
  }
}

/**
 * Loads the indicated mesh primitive.
 */
PT(Geom) GltfLoader::
make_geom(const GltfMesh *mesh, const GltfMesh::Primitive &prim) {
  PT(GeomVertexArrayFormat) afmt = new GeomVertexArrayFormat;
  make_column(afmt, prim, "POSITION", InternalName::get_vertex(), GeomEnums::C_point);
  make_column(afmt, prim, "NORMAL", InternalName::get_normal(), GeomEnums::C_normal);
  make_column(afmt, prim, "COLOR_0", InternalName::get_color(), GeomEnums::C_color);
  make_column(afmt, prim, "TEXCOORD_0", InternalName::get_texcoord(), GeomEnums::C_texcoord);
  make_column(afmt, prim, "TEXCOORD_1", InternalName::get_texcoord_name("1"), GeomEnums::C_texcoord);
  if (afmt->has_column(InternalName::get_normal()) &&
      make_column(afmt, prim, "TANGENT", InternalName::get_tangent(), GeomEnums::C_vector)) {
    afmt->add_column(InternalName::get_binormal(), 3, GeomEnums::NT_float32, GeomEnums::C_vector);
  }
  make_column(afmt, prim, "JOINTS_0", InternalName::get_transform_index(), GeomEnums::C_index);
  make_column(afmt, prim, "WEIGHTS_0", InternalName::get_transform_weight(), GeomEnums::C_other);

  // Get the max row size for all the attributes.
  size_t num_rows = 0;
  const GltfMesh::Primitive::Attributes &attribs = prim.get_attributes();
  GltfMesh::Primitive::Attributes::const_iterator it;
  for (it = attribs.begin(); it != attribs.end(); ++it) {
    const GltfAccessor *accessor = it->second;
    num_rows = max(num_rows, accessor->get_count());
  }

  PT(GeomVertexData) data = new GeomVertexData(mesh->get_name(), GeomVertexFormat::register_format(afmt), GeomEnums::UH_static);
  data->unclean_set_num_rows(num_rows);
  load_column(data, prim, "POSITION", InternalName::get_vertex());
  load_column(data, prim, "NORMAL", InternalName::get_normal());
  load_column(data, prim, "COLOR_0", InternalName::get_color());
  load_column(data, prim, "TEXCOORD_0", InternalName::get_texcoord());
  load_column(data, prim, "TEXCOORD_1", InternalName::get_texcoord_name("1"));
  load_column(data, prim, "TANGENT", InternalName::get_tangent());
  load_column(data, prim, "JOINTS_0", InternalName::get_transform_index());
  load_column(data, prim, "WEIGHTS_0", InternalName::get_transform_weight());

  // Generate binormals from 4-component tangents.
  if (afmt->has_column(InternalName::get_binormal())) {
    GeomVertexReader tangents(data, InternalName::get_tangent());
    GeomVertexReader normals(data, InternalName::get_normal());
    GeomVertexWriter binormals(data, InternalName::get_binormal());

#ifndef NDEBUG
    bool has_unexpected_w = false;
#endif

    while (!tangents.is_at_end() && !normals.is_at_end()) {
      LVector4 tangent = tangents.get_data4();
      binormals.set_data3(normals.get_data3().cross(tangent.get_xyz()) * tangent.get_w());
#ifndef NDEBUG
      has_unexpected_w |= (tangent.get_w() != 1.0 && tangent.get_w() != -1.0);
#endif
    }

#ifndef NDEBUG
    if (has_unexpected_w) {
      gltf_cat.warning()
        << "glTF mesh tangent w values should be 1.0 or -1.0\n";
    }
#endif
  }

  PT(GeomPrimitive) primitive;
  switch (prim.get_mode()) {
  case GltfMesh::Primitive::M_points:
    primitive = new GeomPoints(GeomEnums::UH_static);
    break;
  case GltfMesh::Primitive::M_lines:
    primitive = new GeomLines(GeomEnums::UH_static);
    break;
  case GltfMesh::Primitive::M_line_loop:
    // We don't support lineloops in Panda, but we can emulate them easily.
    primitive = new GeomLinestrips(GeomEnums::UH_static);
    break;
  case GltfMesh::Primitive::M_line_strip:
    primitive = new GeomLinestrips(GeomEnums::UH_static);
    break;
  case GltfMesh::Primitive::M_triangles:
    primitive = new GeomTriangles(GeomEnums::UH_static);
    break;
  case GltfMesh::Primitive::M_triangle_strip:
    primitive = new GeomTristrips(GeomEnums::UH_static);
    break;
  case GltfMesh::Primitive::M_triangle_fan:
    primitive = new GeomTrifans(GeomEnums::UH_static);
    break;
  default:
    gltf_cat.error() << "Encountered unsupported primitive type.\n";
    return nullptr;
  }

  const GltfAccessor *indices = prim.get_indices();
  nassertr(indices != nullptr, nullptr);

  GeomEnums::NumericType index_type;
  switch (indices->get_component_type()) {
  case GltfAccessor::CT_unsigned_byte:
    index_type = GeomEnums::NT_uint8;
    break;
  case GltfAccessor::CT_unsigned_short:
    index_type = GeomEnums::NT_uint16;
    break;
  case GltfAccessor::CT_unsigned_int:
    index_type = GeomEnums::NT_uint32;
    break;
  default:
    gltf_cat.error() << "Invalid index type.\n";
    return nullptr;
  }
  primitive->set_index_type(index_type);

  {
    PT(GeomVertexArrayDataHandle) handle = primitive->modify_vertices_handle(Thread::get_current_thread());
    handle->unclean_set_num_rows(indices->get_count());
    load_accessor(handle, 0, indices);
  }

  if (prim.get_mode() == GltfMesh::Primitive::M_line_loop) {
    // For each primitive, we have to repeat the first vertex.
    //TODO: implement line loops properly
  }

  PT(Geom) geom = new Geom(data);
  geom->add_primitive(primitive);

  // Transform the vertices into Panda's coordinate system.
  geom->transform_vertices(_cs_matrix);

  return geom;
}

/**
 * Loads the indicated attribute.
 */
bool GltfLoader::
make_column(GeomVertexArrayFormat *format, const GltfMesh::Primitive &prim,
            const string &attrib, const InternalName *name, GeomEnums::Contents contents) {

  if (!prim.has_attribute(attrib)) {
    return false;
  }
  const GltfAccessor *accessor = prim.get_attribute(attrib);

  GeomEnums::NumericType type;
  switch (accessor->get_component_type()) {
  case GltfAccessor::CT_byte:
    type = GeomEnums::NT_int8;
    break;
  case GltfAccessor::CT_unsigned_byte:
    type = GeomEnums::NT_uint8;
    break;
  case GltfAccessor::CT_short:
    type = GeomEnums::NT_int16;
    break;
  case GltfAccessor::CT_unsigned_short:
    type = GeomEnums::NT_uint16;
    break;
  case GltfAccessor::CT_unsigned_int:
    type = GeomEnums::NT_uint32;
    break;
  case GltfAccessor::CT_float:
    type = GeomEnums::NT_float32;
    break;
  default:
    return false;
  }

  format->add_column(name, accessor->get_num_components(), type, contents);
  return true;
}

/**
 * Loads the indicated attribute.
 */
void GltfLoader::
load_column(GeomVertexData *vdata, const GltfMesh::Primitive &prim,
            const string &attrib, const InternalName *name) {
  if (!prim.has_attribute(attrib)) {
    return;
  }

  const GeomVertexFormat *format = vdata->get_format();
  PT(GeomVertexArrayDataHandle) handle = vdata->modify_array_handle(format->get_array_with(name));
  load_accessor(handle, format->get_column(name)->get_start(), prim.get_attribute(attrib));
}

/**
 * Loads the indicated attribute.
 */
void GltfLoader::
load_accessor(GeomVertexArrayDataHandle *handle, size_t offset, const GltfAccessor *accessor) {
  const GltfBufferView *view = accessor->get_buffer_view();
  const GltfBuffer *buffer = view->get_buffer();

  const size_t width = accessor->get_component_size() * accessor->get_num_components();
  const size_t count = accessor->get_count();
  size_t src_stride = view->get_byte_stride();
  if (src_stride == 0) {
    src_stride = width;
  }

  nassertv(width <= src_stride);

  // Do some sanity checks.
  nassertv(view->get_byte_offset() + view->get_byte_length() <= buffer->get_byte_length());
  nassertv(accessor->get_byte_offset() + src_stride * count <= view->get_byte_length());
  nassertv(accessor->get_byte_offset() + view->get_byte_offset() + src_stride * count <= buffer->get_byte_length());

  unsigned char *dst = handle->get_write_pointer();
  nassertv(dst != nullptr);
  const size_t dst_stride = handle->get_array_format()->get_stride();
  dst += offset;

  nassertv((size_t)handle->get_num_rows() >= count);

  const unsigned char *src = buffer->get_data_pointer();
  nassertv(src != nullptr);
  src += accessor->get_byte_offset() + view->get_byte_offset();

  for (size_t i = 0; i < count; ++i) {
    memcpy(dst, src, width);
    src += src_stride;
    dst += dst_stride;
  }
}

/**
 * Does the actual updating of a node.  This will create a GeomNode if this
 * node contains a mesh, a Camera if it contains a camera, or a tree with the
 * GeomNode at the root if it contains multiple node types.
 */
PT(PandaNode) GltfLoader::
do_update_node(PandaNode *panda_node, const GltfNode *node) {
  const GltfMesh *mesh = node->get_mesh();
  if (mesh != nullptr) {
    // Make sure the mesh is up-to-date.
    /*update_mesh(mesh);
    ConvertedMesh &conv_mesh = _meshes[mesh->get_index()];

    // Add the mesh GeomNode as child, for now.
    panda_node->add_child(conv_mesh._node);*/

    /*GeomNode *geom_node;
    if (panda_node == nullptr) {
      geom_node = new GeomNode(node->get_name());
      panda_node = geom_node;

    } else if (!panda_node->is_geom_node()) {
      // We already have a node, and it's not a GeomNode.  Add the GeomNode
      // that we created when we made the mesh.
      geom_node = new GeomNode("");
      panda_node->add_child(geom_node);

    } else {
      geom_node = (GeomNode *)panda_node;
      geom_node->remove_all_geoms();
    }*/
  }

  if (panda_node == nullptr) {
    panda_node = new PandaNode(node->get_name());
  }

  if (mesh != nullptr) {
    // Make sure the mesh is up-to-date.
    update_mesh(mesh);
    ConvertedMesh &conv_mesh = _meshes[mesh->get_index()];

    // Add the mesh GeomNode as child, for now.
    panda_node->add_child(conv_mesh._node);
  }

  LMatrix4 mat = LCAST(PN_stdfloat, node->get_matrix());
  panda_node->set_transform(TransformState::make_mat(_cs_matrix * mat * _inv_cs_matrix));

  // Now import the child relationships.
  for (size_t ci = 0; ci < node->get_num_children(); ++ci) {
    const GltfNode *child = node->get_child(ci);
    panda_node->add_child(update_node(child));
  }

  return panda_node;
}

/**
 * Does the actual update of a material.
 */
PT(Material) GltfLoader::
do_update_material(Material *panda_mat, const GltfMaterial *material) {
  if (panda_mat == nullptr) {
    panda_mat = new Material();
  }
  panda_mat->set_name(material->get_name());

  const GltfMaterial::PbrMetallicRoughness &pbr = material->get_pbr_metallic_roughness();
  panda_mat->set_base_color(LCAST(PN_stdfloat, pbr.get_base_color_factor()));
  panda_mat->set_metallic(pbr.get_metallic_factor());
  panda_mat->set_roughness(pbr.get_roughness_factor());

  if (material->has_emissive_factor()) {
    LRGBColord emit = material->get_emissive_factor();
    panda_mat->set_emission(LColor(emit[0], emit[1], emit[2], 1));
  }

  return panda_mat;
}

/**
 * Does the actual update of a texture.
 */
PT(Texture) GltfLoader::
do_update_texture(PT(Texture) panda_tex, const GltfTexture *texture) {
  const GltfImage *image = texture->get_source();
  const GltfSampler *sampler = texture->get_sampler();

  if (image != nullptr) {
    // Is this image loaded from a file on disk?
    const GltfUriReference &uri = image->get_uri();
    if (uri.has_filename()) {
      if (panda_tex == nullptr || panda_tex->get_fullpath() != uri.get_fullpath()) {
        // The filename has changed, so reload this texture.
        panda_tex = TexturePool::load_texture(uri.get_fullpath());
        if (panda_tex == nullptr) {
          gltf_cat.error()
            << "Failed to load texture: " << uri << "\n";

          // Just make an empty texture object, and pretend we loaded it.
          panda_tex = new Texture;
          panda_tex->set_fullpath(uri.get_fullpath());
        }
      }

      // Set the original filename.
      panda_tex->set_filename(uri.get_filename());
    } else {
      // It's a data URI or something.
      if (panda_tex == nullptr || panda_tex->has_fullpath()) {
        // If the existing texture was gotten from the texture pool, we'll
        // need a fresh texture object.
        panda_tex = new Texture;
      }

      istream *in = image->open_read();
      PNMImage img;
      if (in != nullptr && img.read(*in)) {
        panda_tex->load(img);
      } else {
        gltf_cat.error()
          << "Failed to load image " << image->get_name() << "\n";
      }
      delete in;
    }
  } else if (panda_tex == nullptr || panda_tex->has_fullpath()) {
    // If the existing texture was gotten from the texture pool, we'll need a
    // fresh texture object.
    panda_tex = new Texture;
  }

  panda_tex->set_name(texture->get_name());

  if (sampler != nullptr) {
    switch (sampler->get_mag_filter()) {
    case GltfSampler::FT_unspecified:
      // Leave it at the default.
      break;
    case GltfSampler::FT_nearest:
      panda_tex->set_minfilter(SamplerState::FT_nearest);
      break;
    case GltfSampler::FT_linear:
      panda_tex->set_minfilter(SamplerState::FT_linear);
      break;
    default:
      gltf_cat.error()
        << "Invalid magFilter specified in " << *sampler << "\n";
    }

    switch (sampler->get_min_filter()) {
    case GltfSampler::FT_unspecified:
      // Leave it at the default.
      break;
    case GltfSampler::FT_nearest:
      panda_tex->set_minfilter(SamplerState::FT_nearest);
      break;
    case GltfSampler::FT_linear:
      panda_tex->set_minfilter(SamplerState::FT_linear);
      break;
    case GltfSampler::FT_nearest_mipmap_nearest:
      panda_tex->set_minfilter(SamplerState::FT_nearest_mipmap_nearest);
      break;
    case GltfSampler::FT_linear_mipmap_nearest:
      panda_tex->set_minfilter(SamplerState::FT_linear_mipmap_nearest);
      break;
    case GltfSampler::FT_nearest_mipmap_linear:
      panda_tex->set_minfilter(SamplerState::FT_nearest_mipmap_linear);
      break;
    case GltfSampler::FT_linear_mipmap_linear:
      panda_tex->set_minfilter(SamplerState::FT_linear_mipmap_linear);
      break;
    default:
      gltf_cat.error()
        << "Invalid minFilter specified in " << *sampler << "\n";
    }

    switch (sampler->get_wrap_s()) {
    case GltfSampler::WM_clamp_to_edge:
      panda_tex->set_wrap_u(SamplerState::WM_clamp);
      break;
    case GltfSampler::WM_mirrored_repeat:
      panda_tex->set_wrap_u(SamplerState::WM_mirror);
      break;
    case GltfSampler::WM_repeat:
      panda_tex->set_wrap_u(SamplerState::WM_repeat);
      break;
    default:
      gltf_cat.error()
        << "Invalid wrapS specified in " << *sampler << "\n";
    }

    switch (sampler->get_wrap_t()) {
    case GltfSampler::WM_clamp_to_edge:
      panda_tex->set_wrap_v(SamplerState::WM_clamp);
      break;
    case GltfSampler::WM_mirrored_repeat:
      panda_tex->set_wrap_v(SamplerState::WM_mirror);
      break;
    case GltfSampler::WM_repeat:
      panda_tex->set_wrap_v(SamplerState::WM_repeat);
      break;
    default:
      gltf_cat.error()
        << "Invalid wrapT specified in " << *sampler << "\n";
    }
  }

  return panda_tex;
}

/**
 * Converts the material
 */
CPT(RenderState) GltfLoader::
make_geom_state(const GltfMesh *mesh, const GltfMesh::Primitive &primitive) {
  const GltfMaterial *material = primitive.get_material();
  if (material == nullptr) {
    return RenderState::make_empty();
  }
  Material *panda_mat = update_material(material);
  CPT(RenderState) state = RenderState::make(MaterialAttrib::make(panda_mat));

  string alpha_mode = material->get_alpha_mode();
  if (!alpha_mode.empty()) {
    // An alpha mode is translated to TransparencyAttrib.
    TransparencyAttrib::Mode mode = TransparencyAttrib::M_none;
    if (alpha_mode == "OPAQUE") {
    } else if (alpha_mode == "MASK") {
      mode = TransparencyAttrib::M_binary;
    } else if (alpha_mode == "BLEND") {
      mode = TransparencyAttrib::M_alpha;
    } else {
      gltf_cat.error()
        << "Invalid alpha mode '" << alpha_mode << "'\n";
    }
    state = state->set_attrib(TransparencyAttrib::make(mode));
  }

  if (alpha_mode == "MASK" && material->has_alpha_cutoff()) {
    // An explicit cutoff value translates to an AlphaTestAttrib.
    state = state->set_attrib(AlphaTestAttrib::make(AlphaTestAttrib::M_greater_equal,
                                                    material->get_alpha_cutoff()));
  }

  if (material->get_double_sided()) {
    state = state->set_attrib(CullFaceAttrib::make(CullFaceAttrib::M_cull_none));
  }

  // Apply the PBR textures.
  CPT(RenderAttrib) texattr = TextureAttrib::make_default();

  const GltfMaterial::PbrMetallicRoughness &pbr = material->get_pbr_metallic_roughness();
  const GltfMaterial::TextureInfo &color_texture = pbr.get_base_color_texture();
  if (color_texture) {
    const GltfTexture *texture = color_texture.get_texture();
    int tex_coord = color_texture.get_tex_coord();

    // Use the same TextureStage for all glTF models, one for each index.
    static pvector<PT(TextureStage)> color_stages(1, TextureStage::get_default());
    if (tex_coord >= (int)color_stages.size()) {
      color_stages.resize(tex_coord + 1);
    }
    PT(TextureStage) stage = color_stages[tex_coord];
    if (stage == nullptr) {
      string num = format_string(tex_coord);
      stage = new TextureStage("color." + num);
      stage->set_texcoord_name(num);
      color_stages[tex_coord] = stage;
    }

    texattr = ((const TextureAttrib *)texattr.p())->add_on_stage(stage, update_texture(texture));
  }

  // Apply the normal texture, if one exists.
  const GltfMaterial::TextureInfo &normal_texture = material->get_normal_texture();
  if (normal_texture) {
    const GltfTexture *texture = normal_texture.get_texture();
    int tex_coord = normal_texture.get_tex_coord();

    // Use the same TextureStage for all glTF models, one for each index.
    static pvector<PT(TextureStage)> normal_stages(1, new TextureStage("normal"));
    if (tex_coord >= (int)normal_stages.size()) {
      normal_stages.resize(tex_coord + 1);
    }
    PT(TextureStage) stage = normal_stages[tex_coord];
    if (stage == nullptr) {
      string num = format_string(tex_coord);
      stage = new TextureStage("normal." + num);
      stage->set_texcoord_name(num);
      normal_stages[tex_coord] = stage;
    }

    stage->set_mode(TextureStage::M_normal);
    texattr = ((const TextureAttrib *)texattr.p())->add_on_stage(stage, update_texture(texture));
  }

  if (!((const TextureAttrib *)texattr.p())->is_identity()) {
    state = state->set_attrib(texattr);
  }

  return state;
}
