/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfUriReference.cxx
 * @author rdb
 * @date 2017-08-18
 */

#include "gltfUriReference.h"
#include "gltfRoot.h"
#include "bamCacheRecord.h"
#include "virtualFileSystem.h"

// Base64 decoding table.
static const int8_t b64_table[128] = {
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
  52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
  -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
  15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
  -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
  41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
};

/**
 * Parses the URI from the given string.
 */
GltfUriReference::
GltfUriReference(const string &uri) : _uri(uri) {
  size_t i = 0;
  while (isalpha(_uri[i]) && i < uri.size()) {
    ++i;
  }
  if (i < uri.size() && uri[i] == ':') {
    // This is a URI.
    _hier_start = i + 1;
  } else {
    // This is a URI reference.
    i = 0;
    _hier_start = 0;
  }

  // Read until a question mark or fragment mark.
  while (uri[i] != '?' && uri[i] != '#' && i < uri.size()) {
    ++i;
  }

  _query_start = i;
  if (uri[i] == '?') {
    _query_start = i;

    while (uri[i] != '#' && i < uri.size()) {
      ++i;
    }

    _fragment_start = i;
  } else {
    _fragment_start = uri.size();
  }
}

/**
 * Constructs a file URI from the given filename.
 */
GltfUriReference::
GltfUriReference(const Filename &fn) {
  ostringstream result;
  result << "file://";
  result << hex << setfill('0');

  string source = fn.to_os_specific();
  for (string::const_iterator si = source.begin(); si != source.end(); ++si) {
    char ch = (*si);
    switch (ch) {
    case '_':
    case ',':
    case '.':
    case '-':
    case '/':
      // Safe character.
      result << ch;
      break;

    case '\\':
      // Write as forward slash.
      result << '/';
      break;

    default:
      if (isalnum(ch)) {
        // Letters and digits are safe.
        result << ch;

      } else {
        // Otherwise, escape it.
        result << '%' << setw(2) << (int)ch;
      }
    }
  }

  _uri = result.str();
  _hier_start = 5;
  _query_start = _uri.size();
  _fragment_start = _uri.size();
}

/**
 * Opens the file referenced by this URI for reading.
 */
istream *GltfUriReference::
open_read() const {
  if (has_filename()) {
    // It's a regular file.
    VirtualFileSystem *vfs = VirtualFileSystem::get_global_ptr();
    return vfs->open_read_file(get_fullpath(), false);

  } else if (_uri.compare(0, _hier_start, "data:") == 0) {
    // It's data URI.
    return new istringstream(read());
  }

  gltf_cat.error()
    << "Don't know how to read URI " << *this << "\n";
  return false;
}

/**
 * Reads the URI as a string.
 */
string GltfUriReference::
read() const {
  if (has_filename()) {
    VirtualFileSystem *vfs = VirtualFileSystem::get_global_ptr();
    return vfs->read_file(get_fullpath(), false);

  } else if (_uri.compare(0, _hier_start, "data:") == 0) {
    // It's a data URI.  Skip the media type for now.
    size_t i = _hier_start;
    bool is_base64 = false;
    while (i < _query_start && _uri[i] != ',') {
      if (_uri[i++] == ';') {
        // Is this followed by "base64"?
        if (_uri.compare(i, 7, "base64,") == 0) {
          is_base64 = true;
          i += 6;
          break;
        }
      }
    }
    if (_uri[i] == ',') {
      ++i;
    } else {
      gltf_cat.error()
        << "Expected ',' in data URI " << *this << "\n";
      return string();
    }

    string decoded = decode_part(i, _query_start);
    if (!is_base64) {
      return decoded;
    }

    // Decode the base-64 string.
    string data;
    int val = 0, valb = -8;
    for (i = 0; i < decoded.size(); ++i) {
      char c = decoded[i];
      if ((unsigned char)c > 127) {
        // Not an ASCII character.
        break;
      }
      int8_t lookup = b64_table[(unsigned char)c];
      if (lookup < 0) {
        // Not a valid character.
        break;
      }
      val = (val << 6) + lookup;
      valb += 6;
      if (valb >= 0) {
          data.push_back((char)((val >> valb) & 0xFF));
          valb -= 8;
      }
    }
    return data;
  }

  gltf_cat.error()
    << "Don't know how to read URI " << *this << "\n";
  return false;
}

/**
 * Resolves the filename and stores the result in fullpath.  Returns true on
 * success, false if the file could not be located along the search path.
 *
 * If this is not a file URI, returns true if we have enough information to
 * read whatever data it refers to.
 */
bool GltfUriReference::
resolve(const DSearchPath &search_path, BamCacheRecord *record) {
  if (!has_filename()) {
    return _uri.compare(0, _hier_start, "data:") == 0;
  }

  VirtualFileSystem *vfs = VirtualFileSystem::get_global_ptr();

  Filename fn = get_filename();
  if (vfs->resolve_filename(fn, search_path)) {
    _fullpath = fn;
    if (record != nullptr) {
      record->add_dependent_file(fn);
    }
    return true;
  } else {
    gltf_cat.error()
      << "Could not locate " << fn << " in " << search_path << "\n";
    return false;
  }
}

/**
 * Returns the filename represented by this URI reference.  Only valid if
 * has_filename returns true.
 */
Filename GltfUriReference::
get_filename() const {
  nassertr(has_filename(), Filename());

  size_t start = _hier_start;
  if (_uri[start] == '/' && _uri[start + 1] == '/') {
    start += 2;
  }

  return Filename::from_os_specific(decode_part(start, _query_start));
}

/**
 * Sets the query string part of the URI reference.  Do not include the '?'.
 */
void GltfUriReference::
set_query(const string &query) {
  string encoded_query = encode(query);
  _uri = _uri.substr(0, _query_start) + "?" + encoded_query + _uri.substr(_fragment_start);
  _fragment_start = _query_start + encoded_query.size() + 1;
}

/**
 * Sets the fragment part of the URI reference.  Do not include the '#'.
 */
void GltfUriReference::
set_fragment(const string &fragment) {
  _uri = _uri.substr(0, _fragment_start) + "#" + encode(fragment);
}

/**
 * Returns the URL-decoded version of the given section of the URL.
 */
string GltfUriReference::
decode_part(size_t begin, size_t end) const {
  string result;

  // Do any URI decoding that might be necessary.
  size_t p = begin;
  while (p < end) {
    if (_uri[p] == '%' && p + 2 < end) {
      int hex = 0;
      p++;
      for (int i = 0; i < 2; i++) {
        int value;
        char ch = _uri[p + i];
        if (isdigit(ch)) {
          value = ch - '0';
        } else {
          value = tolower(ch) - 'a' + 10;
        }
        hex = (hex << 4) | value;
      }
      result += (char)hex;
      p += 2;

    } else {
      result += _uri[p];
      p++;
    }
  }

  return result;
}

/**
 * URL-encodes the given string.
 */
string GltfUriReference::
encode(const string &input) {
  ostringstream result;
  result << hex << setfill('0');

  for (string::const_iterator si = input.begin(); si != input.end(); ++si) {
    char ch = (*si);
    switch (ch) {
    case '_':
    case ',':
    case '.':
    case '-':
    case '/':
      // Safe character.
      result << ch;
      break;

    default:
      if (isalnum(ch)) {
        // Letters and digits are safe.
        result << ch;

      } else {
        // Otherwise, escape it.
        result << '%' << setw(2) << (int)ch;
      }
    }
  }

  return result.str();
}
