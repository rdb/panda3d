/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfRoot.cxx
 * @author rdb
 * @date 2017-07-27
 */

#include "gltfRoot.h"
#include "gltfParser.h"
#include "streamReader.h"
#include "streamWriter.h"
#include "virtualFile.h"
#include "virtualFileSystem.h"

#define GLB_MAGIC 0x46546C67
#define GLB_CHUNK_JSON 0x4E4F534A
#define GLB_CHUNK_BIN 0x004E4942

/**
 *
 */
GltfRoot::
GltfRoot() : _gltf_timestamp(0) {
}

/**
 * Returns the GltfRoot to the default, empty state.
 */
void GltfRoot::
clear() {
  _asset = GltfAsset();
  _extensions_used.clear();
  _extensions_required.clear();
  _scene.clear();

  _accessors.clear();
  _animations.clear();
  _buffers.clear();
  _buffer_views.clear();
  _cameras.clear();
  _images.clear();
  _materials.clear();
  _meshes.clear();
  _nodes.clear();
  _samplers.clear();
  _scenes.clear();
  _skins.clear();
  _textures.clear();

  _gltf_filename.set_fullpath("");
  _gltf_timestamp = 0;

  GltfObject::clear();
}

/**
 * Parses the GltfRoot from the indicated file.
 */
bool GltfRoot::
read(Filename filename, BamCacheRecord *record) {
  _gltf_filename = filename;

  VirtualFileSystem *vfs = VirtualFileSystem::get_global_ptr();

  PT(VirtualFile) vfile = vfs->get_file(filename);
  if (vfile == nullptr) {
    gltf_cat.error() << "Could not find " << filename << "\n";
    return false;
  }
  _gltf_timestamp = vfile->get_timestamp();

  if (record != nullptr) {
    record->add_dependent_file(vfile);
  }

  istream *file = vfile->open_read_file(true);
  if (file == nullptr) {
    gltf_cat.error() << "Unable to open " << filename << "\n";
    return false;
  }

  gltf_cat.info()
    << "Reading " << filename << "\n";

  bool read_ok = read(*file);
  vfile->close_read_file(file);

  if (read_ok) {
    // Load the external references.
    DSearchPath path(vfile->get_filename().get_dirname());
    return load_externals(path, record);
  }
  return false;
}

/**
 * Parses the GltfRoot from the indicated stream.  Existing data will be
 * updated, but not cleared, so that incremental updates are possible.
 *
 * The stream may contain JSON data or GLB version 1 or 2.
 */
bool GltfRoot::
read(istream &in) {
  char c = in.peek();
  if (c == '{' || isspace(c)) {
    // This seems to be a regular JSON file.
    GltfParser parser(in, *this);
    return parser.parse_value(*this);

  } else if (c == 0xEF) {
    // Hum, is this a byte-order marker?  It's not technically allowed, but
    // we'll be lenient and just issue a warning.
    c = in.get();
    char c2 = in.get();
    char c3 = in.get();
    if (in.good() && c == 0xEF && c2 == 0xBB && c3 == 0xBF) {
      c = in.peek();
      if (c == '{' || isspace(c)) {
        gltf_cat.warning() << "Found UTF-8 BOM preceding JSON stream\n";
        GltfParser parser(in, *this);
        return parser.parse_value(*this);
      }
    }

  } else if (c == 'g') {
    // This appears to be a binary glTF file.
    StreamReader reader(in);
    if (reader.get_uint32() == GLB_MAGIC) {
      uint32_t version = reader.get_uint32();

      if (version != 1 && version != 2) {
        gltf_cat.error()
          << "GLB versions 1 and 2 are supported, file is version " << version << "\n";
        return false;
      }

      uint32_t length = reader.get_uint32();
      uint32_t json_length = reader.get_uint32();
      uint32_t type = reader.get_uint32();

      if ((version == 1 && type != 0) ||
          (version == 2 && type != GLB_CHUNK_JSON)) {
        gltf_cat.error()
          << "Expected 'JSON' chunk in GLB file\n";
        return false;
      }

      // Parse the JSON chunk.  It will stop reading at the final '}'.
      uint32_t bytes_read;
      {
        GltfParser parser(in, *this);
        if (!parser.parse_value(*this)) {
          return false;
        }
        bytes_read = parser.get_bytes_read();
      }

      // How many bytes have we read?
      if (bytes_read > json_length) {
        gltf_cat.error()
          << "Length of JSON chunk too small to fit entire object\n";
        return false;
      }

      // Read space characters until we have matched the chunk length.
      while (bytes_read < json_length) {
        ++bytes_read;
        char c = in.get();
        if (!isspace(c)) {
          gltf_cat.error()
            << "Unexpected '" << c << "' at end of JSON chunk\n";
          return false;
        }
      }
      if (bytes_read + 20 > length || in.eof()) {
        gltf_cat.error()
          << "JSON chunk length mismatch\n";
        return false;
      }

      if (json_length + 20 == length || _buffers.empty()) {
        // No binary data chunk to read.
        return true;
      }

      // Get the buffer we mean to populate.  This is always the first buffer.
      GltfBuffer *buffer = _buffers[0];
      if (buffer->has_uri()) {
        // The buffer has a URI, so it's not defined in the GLB file.
        return true;
      }

      // Now we are expecting the binary chunk.
      uint32_t bin_length;
      if (version == 1) {
        bin_length = length - bytes_read - 20;
      } else {
        bin_length = reader.get_uint32();
        uint32_t chunk_type = reader.get_uint32();

        if (chunk_type != GLB_CHUNK_BIN) {
          gltf_cat.error()
            << "Expected BIN chunk\n";
          return false;
        }
      }

      if (bin_length < buffer->get_byte_length()) {
        gltf_cat.error()
          << "Binary chunk length (" << bin_length << ") not large enough for"
             " buffer 0 (" << buffer->get_byte_length() << "\n";
        return false;
      }

      return buffer->read(in);
    }
  }

  gltf_cat.error()
    << "This does not appear to be a glTF file.\n";
  return false;
}

/**
 * Loads all external buffers.  This is normally called if you call read()
 * with a filename, but if you read it via a stream instead, you will need to
 * call this to load the external references.
 */
bool GltfRoot::
load_externals(const DSearchPath &searchpath, BamCacheRecord *record) {
  bool success = true;

  for (auto bi = _buffers.begin(); bi != _buffers.end(); ++bi) {
    GltfBuffer *buffer = *bi;
    if (buffer->has_uri()) {
      success = success && buffer->load(searchpath, record);
    }
  }

  // Also go through the images, just to resolve the filenames and add them
  // to the BamCacheRecord.
  for (auto ii = _images.begin(); ii != _images.end(); ++ii) {
    GltfImage *image = *ii;
    GltfUriReference &uri = image->get_uri();
    if (uri && !uri.resolve(searchpath, record)) {
      success = false;
    }
  }

  return success;
}

/**
 * Writes the glTF document as JSON to the given filename.  The indent
 * parameter indicates how many spaces to use for indentation.  It can be set
 * to 0 to only insert newlines, or -1 not to insert any whitespace.
 */
bool GltfRoot::
write_gltf(Filename filename, int indent) {
  VirtualFileSystem *vfs = VirtualFileSystem::get_global_ptr();
  filename.set_text();
  vfs->delete_file(filename);
  ostream *file = vfs->open_write_file(filename, true, true);
  if (file == nullptr) {
    gltf_cat.error() << "Unable to open " << filename << " for writing.\n";
    return false;
  }

  bool wrote_ok = write_gltf(*file, indent);
  vfs->close_write_file(file);
  return wrote_ok;
}

/**
 * Writes the glTF document as JSON to the given ostream.  The indent
 * parameter indicates how many spaces to use for indentation.  It can be set
 * to 0 to only insert newlines, or -1 not to insert any whitespace.
 */
bool GltfRoot::
write_gltf(ostream &out, int indent) {
  GltfWriter writer(out, indent);
  writer.write_value(*this);
  out.flush();
  return true;
}

/**
 * Writes the glTF document as GLB binary format to the given filename.
 */
bool GltfRoot::
write_glb(Filename filename) {
  VirtualFileSystem *vfs = VirtualFileSystem::get_global_ptr();
  filename.set_binary();
  vfs->delete_file(filename);
  ostream *file = vfs->open_write_file(filename, true, true);
  if (file == nullptr) {
    gltf_cat.error() << "Unable to open " << filename << " for writing.\n";
    return false;
  }

  bool wrote_ok = write_glb(*file);
  vfs->close_write_file(file);
  return wrote_ok;
}

/**
 * Writes the document in the glTF binary format to the given stream.
 */
bool GltfRoot::
write_glb(ostream &out) {
  // Write the JSON chunk to a temporary memory buffer.
  ostringstream json;
  {
    GltfWriter writer(json, -1);
    writer.write_value(*this);
  }

  // Pad the buffer with spaces to meet alignment requirements.
  size_t json_size = json.tellp();
  while ((json_size & 0x3) != 0) {
    json.put(' ');
    ++json_size;
  }

  // Do we have a first binary buffer?
  GltfBuffer *buffer = nullptr;
  size_t bin_size = 0;
  if (!_buffers.empty() && !_buffers[0]->has_uri()) {
    buffer = _buffers[0];
    bin_size = buffer->get_byte_length();

    while ((bin_size & 0x3) != 0) {
      ++bin_size;
    }
  }

  // Begin writing the header.
  StreamWriter writer(out);
  writer.add_uint32(GLB_MAGIC);
  writer.add_uint32(2);
  writer.add_uint32(12 + 8 + json_size + (buffer ? 8 + bin_size : 0));

  // Write the JSON chunk.
  writer.add_uint32(json_size);
  writer.add_uint32(GLB_CHUNK_JSON);
  writer.write(json.str());

  // Write the binary chunk, if any.
  if (buffer != nullptr) {
    writer.add_uint32(bin_size);
    writer.add_uint32(GLB_CHUNK_BIN);
    writer.append_data(buffer->get_data_pointer(), buffer->get_byte_length());
    writer.pad_bytes(bin_size - buffer->get_byte_length());
  }

  out.flush();
  return true;
}

/**
 * Removes any objects that are no longer being referenced by anything.  It is
 * usually a good idea to call this before reading or writing.
 * This may change the indices of all root objects.
 */
size_t GltfRoot::
garbage_collect() {
  size_t count = 0;
  count += _accessors.garbage_collect();
  count += _animations.garbage_collect();
  count += _buffers.garbage_collect();
  count += _buffer_views.garbage_collect();
  count += _cameras.garbage_collect();
  count += _images.garbage_collect();
  count += _materials.garbage_collect();
  count += _meshes.garbage_collect();
  count += _nodes.garbage_collect();
  count += _samplers.garbage_collect();
  count += _scenes.garbage_collect();
  count += _skins.garbage_collect();
  count += _textures.garbage_collect();
  return count;
}

/**
 * Writes the glTF data out to the indicated output stream.
 */
/*void GltfRoot::
write(ostream &out, int indent_level) const {

}*/

/**
 *
 */
bool GltfRoot::
parse_property(GltfParser &parser, const string &key) {
  if (key == "extensionsUsed") {
    return parser.parse_value(_extensions_used);
  }
  if (key == "extensionsRequired") {
    return parser.parse_value(_extensions_required);
  }
  if (key == "accessors") {
    return parser.parse_value(_accessors);
  }
  if (key == "animations") {
    return parser.parse_value(_animations);
  }
  if (key == "asset") {
    return parser.parse_value(_asset);
  }
  if (key == "buffers") {
    return parser.parse_value(_buffers);
  }
  if (key == "bufferViews") {
    return parser.parse_value(_buffer_views);
  }
  if (key == "cameras") {
    return parser.parse_value(_cameras);
  }
  if (key == "images") {
    return parser.parse_value(_images);
  }
  if (key == "materials") {
    return parser.parse_value(_materials);
  }
  if (key == "meshes") {
    return parser.parse_value(_meshes);
  }
  if (key == "nodes") {
    return parser.parse_value(_nodes);
  }
  if (key == "scene") {
    return parser.parse_value(_scene);
  }
  if (key == "samplers") {
    return parser.parse_value(_samplers);
  }
  if (key == "scenes") {
    return parser.parse_value(_scenes);
  }
  if (key == "skins") {
    return parser.parse_value(_skins);
  }
  if (key == "textures") {
    return parser.parse_value(_textures);
  }
  return GltfObject::parse_property(parser, key);
}

/**
 * Writes out the properties contained within this object to the writer.
 */
void GltfRoot::
write_properties(GltfWriter &writer) const {
  writer.write_property("asset", _asset);
  writer.write_property("extensionsUsed", _extensions_used);
  writer.write_property("extensionsRequired", _extensions_required);
  writer.write_property("accessors", _accessors);
  writer.write_property("animations", _animations);
  writer.write_property("buffers", _buffers);
  writer.write_property("bufferViews", _buffer_views);
  writer.write_property("cameras", _cameras);
  writer.write_property("images", _images);
  writer.write_property("materials", _materials);
  writer.write_property("meshes", _meshes);
  writer.write_property("nodes", _nodes);
  writer.write_property("scene", _scene);
  writer.write_property("samplers", _samplers);
  writer.write_property("scenes", _scenes);
  writer.write_property("skins", _skins);
  writer.write_property("textures", _textures);
  GltfObject::write_properties(writer);
}
