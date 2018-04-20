/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfRoot.I
 * @author rdb
 * @date 2017-08-18
 */

/**
 * Creates a new scene and adds it to the list.
 */
INLINE GltfScene *GltfRoot::
make_scene(const string &name) {
  return _scenes.make(name);
}

/**
 * Creates a new node and adds it to the list.
 */
INLINE GltfNode *GltfRoot::
make_node(const string &name) {
  return _nodes.make(name);
}

/**
 * Creates a new buffer and adds it to the list.
 */
INLINE GltfBuffer *GltfRoot::
make_buffer(const string &name) {
  return _buffers.make(name);
}

/**
 * Creates a new buffer view pointing to the given buffer.  By default, it
 * points to the entire buffer.
 */
INLINE GltfBufferView *GltfRoot::
make_buffer_view(const string &name, GltfBuffer *buffer) {
  nassertr(buffer != nullptr, nullptr);
  GltfBufferView *view = _buffer_views.make(name);
  view->set_buffer(buffer);
  return view;
}

/**
 * Creates a new orthographic camera with the given parameters.
 */
INLINE GltfCamera *GltfRoot::
make_orthographic_camera(const string &name, double xmag, double ymag, double znear, double zfar) {
  nassertr(znear >= 0.0, nullptr);
  nassertr(zfar > 0.0, nullptr);
  GltfCamera *camera = _cameras.make(name);
  camera->set_orthographic(GltfCamera::Orthographic(xmag, ymag, znear, zfar));
  return camera;
}

/**
 * Creates a new perspective camera with the given parameters.
 */
INLINE GltfCamera *GltfRoot::
make_perspective_camera(const string &name, double yfov, double znear, double zfar) {
  nassertr(yfov > 0.0, nullptr);
  nassertr(znear > 0.0, nullptr);
  nassertr(zfar > 0.0, nullptr);
  GltfCamera *camera = _cameras.make(name);
  camera->set_perspective(GltfCamera::Perspective(yfov, znear, zfar));
  return camera;
}

/**
 * Creates a new image from a URI.
 */
INLINE GltfImage *GltfRoot::
make_image(const string &name, const GltfUriReference &uri) {
  GltfImage *image = _images.make(name);
  image->set_uri(uri);
  return image;
}

/**
 * Creates a new image from a buffer view and MIME type.
 * @param mime_type Should be either "image/jpeg" or "image/png".
 */
INLINE GltfImage *GltfRoot::
make_image(const string &name, GltfBufferView *buffer_view, const string &mime_type) {
  nassertr(buffer_view != nullptr, nullptr);
  nassertr(!mime_type.empty(), nullptr);
  GltfImage *image = _images.make(name);
  image->set_buffer_view(buffer_view);
  image->set_mime_type(mime_type);
  return image;
}

/**
 * Makes a new material, initialized to the default settings.
 */
INLINE GltfMaterial *GltfRoot::
make_material(const string &name) {
  return _materials.make(name);
}

/**
 * Makes a new mesh.  Keep in mind that a mesh is invalid unless it has at
 * least one primitive.
 */
INLINE GltfMesh *GltfRoot::
make_mesh(const string &name) {
  return _meshes.make(name);
}

/**
 * Makes a new sampler, set to all default values.
 */
INLINE GltfSampler *GltfRoot::
make_sampler(const string &name) {
  return _samplers.make(name);
}

/**
 * Makes a new skin.  It will be invalid until joints are assigned.
 */
INLINE GltfSkin *GltfRoot::
make_skin(const string &name) {
  return _skins.make(name);
}

/**
 * Makes a new texture.
 */
INLINE GltfTexture *GltfRoot::
make_texture(const string &name) {
  return _textures.make(name);
}

/**
 * Returns the filename this GltfRoot was read from.
 */
INLINE Filename GltfRoot::
get_gltf_filename() const {
  return _gltf_filename;
}

/**
 * Returns the timestamp of the glTF file on disk, at the time it was opened
 * for reading, or 0 if this information is not available.
 */
INLINE time_t GltfRoot::
get_gltf_timestamp() const {
  return _gltf_timestamp;
}

/**
 * Returns a reference to the glTF asset metadata object.  This object is
 * bound the lifetime of the GltfRoot object.
 */
INLINE GltfAsset &GltfRoot::
get_asset() {
  return _asset;
}

/**
 * Returns a reference to the glTF asset metadata object.  This object is
 * bound the lifetime of the GltfRoot object.
 */
INLINE const GltfAsset &GltfRoot::
get_asset() const {
  return _asset;
}

/**
 * Returns the default scene.
 */
INLINE GltfScene *GltfRoot::
get_scene() {
  return _scene;
}

/**
 * Returns the default scene.
 */
INLINE const GltfScene *GltfRoot::
get_scene() const {
  return _scene;
}

/**
 * Sets the default scene.  This must be a scene that belongs to this root.
 */
INLINE void GltfRoot::
set_scene(GltfScene *scene) {
  nassertv(std::find(_scenes.begin(), _scenes.end(), scene) != _scenes.end());
  _scene = scene;
}

/**
 *
 */
INLINE size_t GltfRoot::
get_num_accessors() const {
  return _accessors.size();
}

/**
 *
 */
INLINE GltfAccessor *GltfRoot::
get_accessor(size_t i) {
  return _accessors[i];
}

/**
 *
 */
INLINE const GltfAccessor *GltfRoot::
get_accessor(size_t i) const {
  return _accessors[i];
}

/**
 *
 */
INLINE size_t GltfRoot::
get_num_animations() const {
  return _animations.size();
}

/**
 *
 */
INLINE GltfAnimation *GltfRoot::
get_animation(size_t i) {
  return _animations[i];
}

/**
 *
 */
INLINE const GltfAnimation *GltfRoot::
get_animation(size_t i) const {
  return _animations[i];
}

/**
 *
 */
INLINE size_t GltfRoot::
get_num_buffers() const {
  return _buffers.size();
}

/**
 *
 */
INLINE GltfBuffer *GltfRoot::
get_buffer(size_t i) {
  return _buffers[i];
}

/**
 *
 */
INLINE const GltfBuffer *GltfRoot::
get_buffer(size_t i) const {
  return _buffers[i];
}

/**
 *
 */
INLINE size_t GltfRoot::
get_num_buffer_views() const {
  return _buffer_views.size();
}

/**
 *
 */
INLINE GltfBufferView *GltfRoot::
get_buffer_view(size_t i) {
  return _buffer_views[i];
}

/**
 *
 */
INLINE const GltfBufferView *GltfRoot::
get_buffer_view(size_t i) const {
  return _buffer_views[i];
}

/**
 *
 */
INLINE size_t GltfRoot::
get_num_cameras() const {
  return _cameras.size();
}

/**
 *
 */
INLINE GltfCamera *GltfRoot::
get_camera(size_t i) {
  return _cameras[i];
}

/**
 *
 */
INLINE const GltfCamera *GltfRoot::
get_camera(size_t i) const {
  return _cameras[i];
}

/**
 *
 */
INLINE size_t GltfRoot::
get_num_images() const {
  return _images.size();
}

/**
 *
 */
INLINE GltfImage *GltfRoot::
get_image(size_t i) {
  return _images[i];
}

/**
 *
 */
INLINE const GltfImage *GltfRoot::
get_image(size_t i) const {
  return _images[i];
}

/**
 *
 */
INLINE size_t GltfRoot::
get_num_materials() const {
  return _materials.size();
}

/**
 *
 */
INLINE GltfMaterial *GltfRoot::
get_material(size_t i) {
  return _materials[i];
}

/**
 *
 */
INLINE const GltfMaterial *GltfRoot::
get_material(size_t i) const {
  return _materials[i];
}

/**
 *
 */
INLINE size_t GltfRoot::
get_num_meshes() const {
  return _meshes.size();
}

/**
 *
 */
INLINE GltfMesh *GltfRoot::
get_mesh(size_t i) {
  return _meshes[i];
}

/**
 *
 */
INLINE const GltfMesh *GltfRoot::
get_mesh(size_t i) const {
  return _meshes[i];
}

/**
 *
 */
INLINE size_t GltfRoot::
get_num_nodes() const {
  return _nodes.size();
}

/**
 *
 */
INLINE GltfNode *GltfRoot::
get_node(size_t i) {
  return _nodes[i];
}

/**
 *
 */
INLINE const GltfNode *GltfRoot::
get_node(size_t i) const {
  return _nodes[i];
}

/**
 *
 */
INLINE size_t GltfRoot::
get_num_samplers() const {
  return _samplers.size();
}

/**
 *
 */
INLINE GltfSampler *GltfRoot::
get_sampler(size_t i) {
  return _samplers[i];
}

/**
 *
 */
INLINE const GltfSampler *GltfRoot::
get_sampler(size_t i) const {
  return _samplers[i];
}

/**
 *
 */
INLINE size_t GltfRoot::
get_num_scenes() const {
  return _scenes.size();
}

/**
 *
 */
INLINE GltfScene *GltfRoot::
get_scene(size_t i) {
  return _scenes[i];
}

/**
 *
 */
INLINE const GltfScene *GltfRoot::
get_scene(size_t i) const {
  return _scenes[i];
}

/**
 *
 */
INLINE size_t GltfRoot::
get_num_skins() const {
  return _skins.size();
}

/**
 *
 */
INLINE GltfSkin *GltfRoot::
get_skin(size_t i) {
  return _skins[i];
}

/**
 *
 */
INLINE const GltfSkin *GltfRoot::
get_skin(size_t i) const {
  return _skins[i];
}

/**
 *
 */
INLINE size_t GltfRoot::
get_num_textures() const {
  return _textures.size();
}

/**
 *
 */
INLINE GltfTexture *GltfRoot::
get_texture(size_t i) {
  return _textures[i];
}

/**
 *
 */
INLINE const GltfTexture *GltfRoot::
get_texture(size_t i) const {
  return _textures[i];
}

/**
 * Creates a new node.
 */

template<> INLINE GltfAccessor *GltfRoot::
resolve_id<GltfAccessor>(size_t id) {
  return _accessors.resolve_id(id);
}

template<> INLINE GltfAnimation *GltfRoot::
resolve_id<GltfAnimation>(size_t id) {
  return _animations.resolve_id(id);
}

template<> INLINE GltfBuffer *GltfRoot::
resolve_id<GltfBuffer>(size_t id) {
  return _buffers.resolve_id(id);
}

template<> INLINE GltfBufferView *GltfRoot::
resolve_id<GltfBufferView>(size_t id) {
  return _buffer_views.resolve_id(id);
}

template<> INLINE GltfCamera *GltfRoot::
resolve_id<GltfCamera>(size_t id) {
  return _cameras.resolve_id(id);
}

template<> INLINE GltfImage *GltfRoot::
resolve_id<GltfImage>(size_t id) {
  return _images.resolve_id(id);
}

template<> INLINE GltfMaterial *GltfRoot::
resolve_id<GltfMaterial>(size_t id) {
  return _materials.resolve_id(id);
}

template<> INLINE GltfMesh *GltfRoot::
resolve_id<GltfMesh>(size_t id) {
  return _meshes.resolve_id(id);
}

template<> INLINE GltfNode *GltfRoot::
resolve_id<GltfNode>(size_t id) {
  return _nodes.resolve_id(id);
}

template<> INLINE GltfSampler *GltfRoot::
resolve_id<GltfSampler>(size_t id) {
  return _samplers.resolve_id(id);
}

template<> INLINE GltfScene *GltfRoot::
resolve_id<GltfScene>(size_t id) {
  return _scenes.resolve_id(id);
}

template<> INLINE GltfSkin *GltfRoot::
resolve_id<GltfSkin>(size_t id) {
  return _skins.resolve_id(id);
}

template<> INLINE GltfTexture *GltfRoot::
resolve_id<GltfTexture>(size_t id) {
  return _textures.resolve_id(id);
}
