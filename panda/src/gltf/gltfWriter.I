/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfWriter.I
 * @author rdb
 * @date 2017-08-28
 */

/**
 * Writes a GltfValue property.
 */
void GltfWriter::
write_property(const char *key, const GltfValue &value) {
  write_property_key(key);
  write_value(value);
}

/**
 * Writes a property key/value pair.  This is called repeatedly in order to
 * build up a glTF object definition.
 */
void GltfWriter::
write_property(const char *key, const GltfObject &value) {
  write_property_key(key);
  write_value(value);
}

/**
 * Writes a property key/value pair.  This is called repeatedly in order to
 * build up a glTF object definition.
 */
void GltfWriter::
write_property(const char *key, const GltfDictionaryBase &value) {
  write_property_key(key);
  write_value(value);
}

/**
 * Writes a property key/value pair.  This is called repeatedly in order to
 * build up a glTF object definition.
 */
void GltfWriter::
write_property(const char *key, const GltfArrayBase &value) {
  size_t size = value.size();
  if (size > 0) {
    write_property_key(key);
    write_value(value);
  }
}

/**
 * Writes a string.
 */
void GltfWriter::
write_property(const char *key, const string &value) {
  write_property_key(key);
  write_value(value);
}

/**
 * Writes a URI reference.
 */
void GltfWriter::
write_property(const char *key, const GltfUriReference &value) {
  write_property_key(key);
  write_value((const string &)value);
}

/**
 * Writes an unsigned integer.
 */
void GltfWriter::
write_property(const char *key, unsigned int value) {
  write_property_key(key);
  write_value(value);
}

/**
 * Writes a floating-point number.
 */
void GltfWriter::
write_property(const char *key, double value) {
  write_property_key(key);
  write_value(value);
}

/**
 * Writes a 3-component vector.
 */
void GltfWriter::
write_property(const char *key, const LVecBase3d &value) {
  write_property_key(key);
  write_value(value);
}

/**
 * Writes a 4-component vector.
 */
void GltfWriter::
write_property(const char *key, const LVecBase4d &value) {
  write_property_key(key);
  write_value(value);
}

/**
 * Writes a 16-component matrix.
 */
void GltfWriter::
write_property(const char *key, const LMatrix4d &value) {
  write_property_key(key);
  write_value(value);
}

/**
 * Writes a true or false value.
 */
void GltfWriter::
write_property(const char *key, bool value) {
  write_property_key(key);
  _out << (value ? "true" : "false");
}

/**
 * In glTF, null pointers are invalid syntax.
 */
INLINE void GltfWriter::
write_property(const char *key, const GltfRootObject *value) {
  if (value != nullptr) {
    write_property_key(key);
    write_value((unsigned int)value->get_index());
  }
}

/**
 * Writes a true or false value.
 */
INLINE void GltfWriter::
write_value(bool value) {
  if (value) {
    _out << "true";
  } else {
    _out << "false";
  }
}

/**
 * Writes a null value.
 */
INLINE void GltfWriter::
write_value(nullptr_t) {
  _out << "null";
}

/**
 * This helps the compiler disambiguate between the bool and the string
 * overload when passing in an URI.
 */
INLINE void GltfWriter::
write_value(const GltfUriReference &value) {
  write_value((const string &)value);
}

/**
 * Writes a pointer to an identifiable object.  May not be nullptr.
 */
INLINE void GltfWriter::
write_value(const GltfRootObject &value) {
  write_value((unsigned int)value.get_index());
}
