/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfWriter.cxx
 * @author rdb
 * @date 2017-08-18
 */

#include "gltfWriter.h"
#include "string_utils.h"

/**
 * Constructs a writer state wrapping around the given output stream.
 */
GltfWriter::
GltfWriter(ostream &out, int indent, bool use_comma, bool quote_keys) :
  _out(out),
  _indent(indent),
  _indent_level(0),
  _use_comma(use_comma),
  _need_comma(false),
  _key_quote(quote_keys ? "\"" : ""),
  _key_separator((_indent >= 0) ? ": " : ":") {
}

/**
 * Writes a GltfValue property.
 */
void GltfWriter::
write_value(const GltfValue &value) {
  switch (value._type) {
  case GltfValue::T_undefined:
    break;
  case GltfValue::T_null:
    _out << "null";
    break;
  case GltfValue::T_boolean:
    _out << (value._value.as_boolean ? "true" : "false");
    break;
  case GltfValue::T_number:
    write_value(value._value.as_number);
    break;
  case GltfValue::T_string:
    write_value(*value._value.as_string);
    break;
  case GltfValue::T_array:
    write_value(*value._value.as_array);
    break;
  case GltfValue::T_object:
    write_value(*value._value.as_object);
    break;
  }
}

/**
 * Writes an object, calling parse_property() on the given object each time a
 * key is encountered.  Returns true if the object was successfully parsed.
 */
void GltfWriter::
write_value(const GltfObject &value) {
  _out << '{';

  bool prev_need_comma = _need_comma;
  _need_comma = false;

  if (_indent >= 0) {
    _indent_level += _indent;
  }

  value.write_properties(*this);

  if (_indent >= 0) {
    _indent_level -= _indent;
    if (_need_comma) {
      _out.put('\n');
      indent(_out, _indent_level);
    }
  }

  _need_comma = prev_need_comma;
  _out << '}';
}

/**
 * Writes an object, calling parse_property() on the given object each time a
 * key is encountered.  Returns true if the object was successfully parsed.
 */
void GltfWriter::
write_value(const GltfDictionaryBase &value) {
  _out << '{';

  bool prev_need_comma = _need_comma;
  _need_comma = false;

  if (_indent >= 0) {
    _indent_level += _indent;
  }

  value.write_properties(*this);

  if (_indent >= 0) {
    _indent_level -= _indent;
    if (_need_comma) {
      _out.put('\n');
      indent(_out, _indent_level);
    }
  }

  _need_comma = prev_need_comma;
  _out << '}';
}

/**
 * Writes an object, calling parse_element() on the given object each time a
 * value is encountered.  Returns true if the object was successfully parsed.
 */
void GltfWriter::
write_value(const GltfArrayBase &value) {
  size_t size = value.size();
  if (size == 0) {
    _out << "[]";
    return;
  }

  if (_indent >= 0) {
    _indent_level += _indent;
  }

  _out.put('[');
  for (size_t i = 0; i < size; ++i) {
    if (i != 0) {
      _out << ',';
    }
    if (_indent >= 0) {
      _out.put('\n');
      indent(_out, _indent_level);
    }
    value.write_element(*this, i);
  }

  if (_indent >= 0) {
    _out.put('\n');
    _indent_level -= _indent;
    indent(_out, _indent_level);
  }
  _out.put(']');
}

/**
 * Writes a string.
 */
void GltfWriter::
write_value(const string &value) {
  _out << '"';

  for (size_t i = 0; i < value.size(); ++i) {
    char c = value[i];
    switch (c) {
    case '\b':
      _out << "\\b";
      break;
    case '\f':
      _out << "\\f";
      break;
    case '\n':
      _out << "\\n";
      break;
    case '\r':
      _out << "\\r";
      break;
    case '\t':
      _out << "\\t";
      break;
    case '\\':
      _out << "\\\\";
      break;
    case '"':
      _out << "\\\"";
      break;
    default:
      _out.put(c);
    }
  }

  _out << '"';
}

/**
 * Writes an unsigned long long integer.
 */
void GltfWriter::
write_value(unsigned long long value) {
  _out << format_string(value);
}

/**
 * Writes an unsigned long integer.
 */
void GltfWriter::
write_value(unsigned long value) {
  _out << format_string(value);
}

/**
 * Writes an unsigned integer.
 */
void GltfWriter::
write_value(unsigned int value) {
  _out << format_string(value);
}

/**
 * Writes a floating-point number.
 */
void GltfWriter::
write_value(double value) {
  _out << format_string(value);
}

/**
 * Writes a 3-component vector.
 */
void GltfWriter::
write_value(const LVecBase3d &value) {
  _out << '['
       << format_string(value[0]) << ", " << format_string(value[1]) << ", "
       << format_string(value[2]) << ']';
}

/**
 * Writes a 4-component vector.
 */
void GltfWriter::
write_value(const LVecBase4d &value) {
  _out << '['
       << format_string(value[0]) << ", " << format_string(value[1]) << ", "
       << format_string(value[2]) << ", " << format_string(value[3]) << ']';
}

/**
 * Writes a 16-component matrix.
 */
void GltfWriter::
write_value(const LMatrix4d &value) {
  _out.put('[');
  double *data = (double *)value.get_data();

  if (_indent >= 0) {
    _out.put('\n');
    _indent_level += _indent;
    indent(_out, _indent_level) << format_string(data[0]) << ", " << format_string(data[1]) << ", " << format_string(data[2]) << ", " << format_string(data[3]) << ",\n";
    indent(_out, _indent_level) << format_string(data[4]) << ", " << format_string(data[5]) << ", " << format_string(data[6]) << ", " << format_string(data[7]) << ",\n";
    indent(_out, _indent_level) << format_string(data[8]) << ", " << format_string(data[9]) << ", " << format_string(data[10]) << ", " << format_string(data[11]) << ",\n";
    indent(_out, _indent_level) << format_string(data[12]) << ", " << format_string(data[13]) << ", " << format_string(data[14]) << ", " << format_string(data[15]) << "\n";
    _indent_level -= _indent;
    indent(_out, _indent_level);
  } else {
    _out << format_string(data[0]);
    for (int i = 1; i < 16; ++i) {
      _out << ",";
      _out << format_string(data[i]);
    }
  }

  _out.put(']');
}

/**
 * Writes the key string of a JSON object possibly prefixed with a comma,
 * newline and indentation.
 * While writing an object, this is called in alternation with write_value in
 * order to write all the properties inside the object.
 */
void GltfWriter::
write_property_key(const string &key) {
  if (_need_comma) {
    _out.put(',');
  }
  if (_indent >= 0) {
    _out.put('\n');
  }
  _need_comma = _use_comma;
  indent(_out, _indent_level) << _key_quote << key << _key_quote << _key_separator;
}
