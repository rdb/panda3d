/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file gltfParser.h
 * @author rdb
 * @date 2017-07-26
 */

#ifndef GLTFPARSER_H
#define GLTFPARSER_H

#include "pandabase.h"
#include "luse.h"
#include "pointerToBase.h"
#include "gltfRoot.h"
#include "gltfObject.h"

class GltfArrayBase;
class GltfDictionaryBase;
class GltfUriReference;

/**
 * Contains the code to parse glTF files.  Not exported from this module.
 * Wraps around an input stream, storing additional parser state.
 */
class GltfParser {
public:
  GltfParser(istream &in, GltfRoot &data);

  GltfObject *get_parent() const {
    if (_stack.size() > 1) {
      return _stack[_stack.size() - 2];
    } else {
      return nullptr;
    }
  }

  bool skip_value();
  bool parse_value(GltfValue &into);
  bool parse_value(GltfObject &into);
  bool parse_value(GltfDictionaryBase &into);
  bool parse_value(GltfArrayBase &into);
  bool parse_value(string &into);
  bool parse_value(GltfUriReference &into);
  bool parse_value(unsigned int &into);
  bool parse_value(double &into);
  bool parse_value(LVecBase3d &into);
  bool parse_value(LVecBase4d &into);
  bool parse_value(LMatrix4d &into);
  bool parse_value(bool &into);

  template<class T>
  bool parse_value(PointerTo<T> &into) {
    unsigned int id;
    if (!parse_value(id)) {
      return false;
    }
    into = _data.resolve_id<T>(id);
    return (into != nullptr);
  }

  unsigned int get_bytes_read() const {
    return _bytes_read;
  }

  ostream &error() const;
  ostream &warning() const;

protected:
  bool expect(char c, char e);
  char next_nonspace();
  char next();
  void putback(char c) {
    _in.putback(c);
    --_bytes_read;
    if (c == '\n') {
      --_line;
    }
  }

private:
  istream &_in;
  int _line;
  GltfRoot &_data;
  unsigned int _bytes_read;
  typedef pdeque<GltfObject *> Stack;
  Stack _stack;
};

#endif
