/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfParser.cxx
 * @author rdb
 * @date 2017-07-26
 */

#include "gltfParser.h"
#include "gltfUriReference.h"

/**
 * Constructs a parser state wrapping around the given input stream.
 * The data reference is necessary to resolve encountered identifiers.
 */
GltfParser::
GltfParser(istream &in, GltfRoot &data) : _in(in), _line(1), _data(data), _bytes_read(0) {
}

/**
 * Ignores the next value encountered in the glTF file.
 */
bool GltfParser::
skip_value() {
  char c = next_nonspace();

  if (c == '{') {
    char c;
    do {
      string key;
      if (!parse_value(key) ||
          !expect(next_nonspace(), ':') ||
          !skip_value()) {
        return false;
      }
      c = next_nonspace();
    } while (c == ',');

    return expect(c, '}');

  } else if (c == '"') {
    c = next();
    while (c != '"') {
      if (c == '\\') {
        c = next();
      }
      c = next();
    }
    return true;

  } else if (isalpha(c)) {
    // A keyword, like null, true or false.
    do {
      c = next();
    } while (isalpha(c));

    return true;

  } else if (c == '[') {
    // An array.
    do {
      if (!skip_value()) {
        return false;
      }
      c = next_nonspace();
    } while (c == ',');

    return expect(c, ']');

  } else if (c == '-' || isdigit(c)) {
    // Whatever, some number or something.  Just skip everything.
    while (c != ',' && c != '}' && c != ']') {
      c = next();
    }
    putback(c);
    return true;

  } else {
    error() << "Expected value, not '" << c << "'\n";
    return false;
  }
}

/**
 * Parses a custom value.
 */
bool GltfParser::
parse_value(GltfValue &into) {
  char c = next_nonspace();
  putback(c);

  switch (c) {
  case '{':
    into._type = GltfValue::T_object;
    into._value.as_object = new GltfDictionary<GltfValue>;
    return parse_value(*into._value.as_object);

  case '"':
    into._type = GltfValue::T_string;
    into._value.as_string = new string;
    return parse_value(*into._value.as_string);

  case 'n':
    into._type = GltfValue::T_null;
    return expect(next(), 'n')
        && expect(next(), 'u')
        && expect(next(), 'l')
        && expect(next(), 'l');

  case 't':
  case 'f':
    into._type = GltfValue::T_boolean;
    return parse_value(into._value.as_boolean);

  case '[':
    into._type = GltfValue::T_array;
    into._value.as_array = new GltfArray<GltfValue>;
    return parse_value(*into._value.as_array);

  default:
    if (c == '-' || isdigit(c)) {
      into._type = GltfValue::T_number;
      return parse_value(into._value.as_number);
    }
  }

  error() << "Expected value, not '" << c << "'\n";
  return false;
}

/**
 * Parses an object, calling parse_property() on the given object each time a
 * key is encountered.  Returns true if the object was successfully parsed.
 */
bool GltfParser::
parse_value(GltfObject &into) {
  if (!expect(next_nonspace(), '{')) {
    return false;
  }

  into.clear();

  // Check for empty object.
  char c = next_nonspace();
  if (c == '}') {
    return true;
  }
  putback(c);

  _stack.push_back(&into);
  do {
    string key;
    if (!parse_value(key) ||
        !expect(next_nonspace(), ':') ||
        !into.parse_property(*this, key)) {
      return false;
    }
    c = next_nonspace();
  } while (c == ',');

  _stack.pop_back();

  return expect(c, '}');
}

/**
 * Parses an object, calling parse_property() on the given object each time a
 * key is encountered.  Returns true if the object was successfully parsed.
 */
bool GltfParser::
parse_value(GltfDictionaryBase &into) {
  if (!expect(next_nonspace(), '{')) {
    return false;
  }

  into.clear();

  // Check for empty object.
  char c = next_nonspace();
  if (c == '}') {
    return true;
  }
  putback(c);

  do {
    string key;
    if (!parse_value(key) ||
        !expect(next_nonspace(), ':') ||
        !into.parse_property(*this, key)) {
      return false;
    }
    c = next_nonspace();
  } while (c == ',');

  return expect(c, '}');
}

/**
 * Parses an object, calling parse_element() on the given object each time a
 * value is encountered.  Returns true if the object was successfully parsed.
 */
bool GltfParser::
parse_value(GltfArrayBase &into) {
  if (!expect(next_nonspace(), '[')) {
    return false;
  }

  into.clear();

  size_t i = 0;

  char c;
  do {
    if (!into.parse_element(*this, i++)) {
      return false;
    }
    c = next_nonspace();
  } while (c == ',');

  return expect(c, ']');
}

/**
 * Parses a string.  Returns true if the string was successfully parsed.
 */
bool GltfParser::
parse_value(string &into) {
  into.clear();

  char c = next_nonspace();
  if (c != '"') {
    error() << "Expected string, not '" << c << "'\n";
    return false;
  }

  c = next();
  while (c != '"') {
    if (c == '\\') {
      c = next();
      switch (c) {
      case 'b':
        c = '\b';
        break;
      case 'f':
        c = '\f';
        break;
      case 'n':
        c = '\n';
        break;
      case 'r':
        c = '\r';
        break;
      case 't':
        c = '\t';
        break;
      case 'u':
        //TODO:
        break;
      default:
        break;
      }
    }
    into += c;
    c = next();
  }

  return true;
}

/**
 * Parses a URI reference.  Returns true if successful.
 */
bool GltfParser::
parse_value(GltfUriReference &into) {
  string str;
  if (parse_value(str)) {
    into = str;
    return true;
  } else {
    return false;
  }
}

/**
 * Parses an identifier integer.  Returns true if successfully parsed.
 */
bool GltfParser::
parse_value(unsigned int &into) {
  char c = next_nonspace();
  if (!isdigit(c)) {
    error() << "Expected positive integer, not '" << c << "'\n";
    return false;
  }

  into = (c - '0');

  c = next();
  while (isdigit(c)) {
    into = into * 10 + (c - '0');
    c = next();
  }

  putback(c);
  return true;
}

/**
 * Parses a floating-point number.  Returns true on success.
 */
bool GltfParser::
parse_value(double &into) {
  char c = next_nonspace();
  if (c != '-' && !isdigit(c)) {
    error() << "Expected number, not '" << c << "'\n";
    return false;
  }

  bool negative = false;
  if (c == '-') {
    negative = true;
    c = next();
  }

  unsigned long long num = 0;

  if (c != '0') {
    num = (c - '0');

    c = next();
    while (isdigit(c)) {
      num = num * 10 + (c - '0');
      c = next();
    }
  } else {
    c = next();
  }

  double flnum = num;

  if (c == '.') {
    double mult = 0.1;
    c = next();
    while (isdigit(c)) {
      flnum += mult * (c - '0');
      mult *= 0.1;
      c = next();
    }
  }

  if (c == 'e' || c == 'E') {
    bool neg_exp = false;
    c = next();
    if (c == '+') {
      c = next();
    } else if (c == '-') {
      neg_exp = true;
      c = next();
    }

    double exp = 0;
    while (isdigit(c)) {
      exp = (exp * 10.0) + (c - '0');
      c = next();
    }

    if (neg_exp) {
      flnum /= pow(10.0, exp);
    } else {
      flnum *= pow(10.0, exp);
    }
  }

  if (negative) {
    flnum = -flnum;
  }
  into = flnum;

  putback(c);
  return true;
}

/**
 * Parses a 3-component vector.  Returns true if the object was successfully
 * parsed.
 */
bool GltfParser::
parse_value(LVecBase3d &into) {
  if (!expect(next_nonspace(), '[')) {
    return false;
  }

  parse_value(into[0]);
  expect(next_nonspace(), ',');
  parse_value(into[1]);
  expect(next_nonspace(), ',');
  parse_value(into[2]);
  expect(next_nonspace(), ']');
  return true;
}

/**
 * Parses a 4-component vector.  Returns true if the object was successfully
 * parsed.
 */
bool GltfParser::
parse_value(LVecBase4d &into) {
  if (!expect(next_nonspace(), '[')) {
    return false;
  }

  parse_value(into[0]);
  expect(next_nonspace(), ',');
  parse_value(into[1]);
  expect(next_nonspace(), ',');
  parse_value(into[2]);
  expect(next_nonspace(), ',');
  parse_value(into[3]);
  expect(next_nonspace(), ']');
  return true;
}

/**
 * Parses a 16-component matrix.  Returns true if the object was successfully
 * parsed.
 */
bool GltfParser::
parse_value(LMatrix4d &into) {
  if (!expect(next_nonspace(), '[')) {
    return false;
  }

  double *data = (double *)into.get_data();

  parse_value(data[0]);
  for (int i = 1; i < 16; ++i) {
    expect(next_nonspace(), ',');
    parse_value(data[i]);
  }
  expect(next_nonspace(), ']');
  return true;
}

/**
 * Parses a true or false value.
 */
bool GltfParser::
parse_value(bool &into) {
  char c = next_nonspace();
  if (c != 't' && c != 'f') {
    error() << "Expected 'true' or 'false', not '" << c << "'\n";
    return false;
  }
  if (c == 't') {
    expect(next(), 'r');
    expect(next(), 'u');
    expect(next(), 'e');
    into = true;
    return true;
  }
  if (c == 'f') {
    expect(next(), 'a');
    expect(next(), 'l');
    expect(next(), 's');
    expect(next(), 'e');
    into = false;
    return true;
  }
  return false;
}

/**
 * Issues an error at the current line.
 */
ostream &GltfParser::
error() const {
  gltf_cat.error()
    << "At line " << _line << " of " << _data.get_gltf_filename() << ":\n";
  return gltf_cat.error() << "  ";
}

/**
 * Issues a warning at the current line.
 */
ostream &GltfParser::
warning() const {
  gltf_cat.warning()
    << "At line " << _line << " of " << _data.get_gltf_filename() << ":\n";
  return gltf_cat.warning() << "  ";
}

/**
 * Outputs an error message and returns false if the given character 'c'
 * doesn't match the expected character 'e'.
 */
bool GltfParser::
expect(char c, char e) {
  if (c == e) {
    return true;
  } else {
    error() << "Expected '" << e << "', not '" << c << "'\n";
    return false;
  }
}

/**
 * Returns the next non-whitespace character in the input stream.  The line
 * counter is incremented if a newline is encountered.
 */
char GltfParser::
next_nonspace() {
  char c = _in.get();
  ++_bytes_read;
  while (isspace(c)) {
    if (c == '\n') {
      ++_line;
    }
    c = _in.get();
    ++_bytes_read;
  }
  return c;
}

/**
 * Returns the next character in the input stream.  The line counter is
 * incremented if a newline is encountered.
 */
char GltfParser::
next() {
  char c = _in.get();
  if (c == '\n') {
    ++_line;
  }
  ++_bytes_read;
  return c;
}
