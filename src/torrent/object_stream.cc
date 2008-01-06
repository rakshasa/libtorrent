// libTorrent - BitTorrent library
// Copyright (C) 2005-2007, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#include "config.h"

#include <iterator>
#include <iostream>
#include <rak/functional.h>

#include "utils/sha1.h"

#include "object.h"
#include "object_stream.h"

namespace torrent {

bool
object_read_string(std::istream* input, std::string& str) {
  uint32_t size;
  *input >> size;

  if (input->fail() || input->get() != ':')
    return false;
  
  str.resize(size);

  for (std::string::iterator itr = str.begin(); itr != str.end() && input->good(); ++itr)
    *itr = input->get();
  
  return !input->fail();
}

// Could consider making this non-recursive, but they seldomly are
// deep enough to make that worth-while.
void
object_read_bencode(std::istream* input, Object* object, uint32_t depth) {
  int c;

  switch ((c = input->peek())) {
  case 'i':
    input->get();

    *object = Object::create_value();
    *input >> object->as_value();

    if (input->get() != 'e')
      break;

    return;

  case 'l':
    input->get();
    *object = Object::create_list();

    if (++depth >= 1024)
      break;

    while (input->good()) {
      if (input->peek() == 'e') {
	input->get();
	return;
      }

      Object::list_iterator itr = object->as_list().insert(object->as_list().end(), Object());
      object_read_bencode(input, &*itr, depth);
    }

    break;

  case 'd':
    input->get();
    *object = Object::create_map();

    if (++depth >= 1024)
      break;

    while (input->good()) {
      if (input->peek() == 'e') {
	input->get();
	return;
      }

      Object::string_type str;

      if (!object_read_string(input, str))
	break;

      object_read_bencode(input, &object->as_map()[str], depth);
    }

    break;

  default:
    if (c >= '0' && c <= '9') {
      *object = Object::create_string();
      
      if (object_read_string(input, object->as_string()))
	return;
    }

    break;
  }

  input->setstate(std::istream::failbit);
  object->clear();
}

void
object_write_bencode(std::ostream* output, const Object* object) {
  char buffer[1024];
  object_write_bencode_c(&object_write_to_stream, output, object_buffer_t(buffer, buffer + 1024), object);
}

// Would be nice to have a straight stream to hash conversion.
std::string
object_sha1(const Object* object) {
  Sha1 sha;
  char buffer[1024];

  sha.init();
  object_write_bencode_c(&object_write_to_sha1, &sha, object_buffer_t(buffer, buffer + 1024), object);
  sha.final_c(buffer);

  return std::string(buffer, 20);
}

std::istream&
operator >> (std::istream& input, Object& object) {
  std::locale locale = input.imbue(std::locale::classic());

  object.clear();
  object_read_bencode(&input, &object);

  input.imbue(locale);
  return input;
}

std::ostream&
operator << (std::ostream& output, const Object& object) {
  object_write_bencode(&output, &object);
  return output;
}

struct object_write_data_t {
  object_write_t writeFunc;
  void* data;

  object_buffer_t buffer;
  char* pos;
};

void
object_write_bencode_c_string(object_write_data_t* output, const char* srcData, uint32_t srcLength) {
  do {
    uint32_t len = std::min<uint32_t>(srcLength, std::distance(output->pos, output->buffer.second));

    std::memcpy(output->pos, srcData, len);

    output->pos += len;

    if (output->pos == output->buffer.second) {
      output->buffer = output->writeFunc(output->data, output->buffer);
      output->pos = output->buffer.first;
    }

    srcData += len;
    srcLength -= len;

  } while (srcLength != 0);
}

void
object_write_bencode_c_char(object_write_data_t* output, char src) {
  if (output->pos == output->buffer.second) {
    output->buffer = output->writeFunc(output->data, output->buffer);
    output->pos = output->buffer.first;
  }

  *output->pos++ = src;
}

// A new wheel. Look, how shiny and new.
void
object_write_bencode_c_value(object_write_data_t* output, int64_t src) {
  if (src == 0)
    return object_write_bencode_c_char(output, '0');

  if (src < 0) {
    object_write_bencode_c_char(output, '-');
    src = -src;
  }

  char buffer[20];
  char* first = buffer + 20;

  // We don't need locale support, so just do this directly.
  while (src != 0) {
    *--first = '0' + src % 10;

    src /= 10;
  }

  object_write_bencode_c_string(output, first, 20 - std::distance(buffer, first));
}

void
object_write_bencode_c_object(object_write_data_t* output, const Object* object) {
  switch (object->type()) {
  case Object::TYPE_NONE:
    break;

  case Object::TYPE_VALUE:
    object_write_bencode_c_char(output, 'i');
    object_write_bencode_c_value(output, object->as_value());
    object_write_bencode_c_char(output, 'e');
    break;

  case Object::TYPE_STRING:
    object_write_bencode_c_value(output, object->as_string().size());
    object_write_bencode_c_char(output, ':');
    object_write_bencode_c_string(output, object->as_string().c_str(), object->as_string().size());
    break;

  case Object::TYPE_LIST:
    object_write_bencode_c_char(output, 'l');

    for (Object::list_const_iterator itr = object->as_list().begin(), last = object->as_list().end(); itr != last; ++itr)
      object_write_bencode_c_object(output, &*itr);

    object_write_bencode_c_char(output, 'e');
    break;

  case Object::TYPE_MAP:
    object_write_bencode_c_char(output, 'd');

    for (Object::map_const_iterator itr = object->as_map().begin(), last = object->as_map().end(); itr != last; ++itr) {
      object_write_bencode_c_value(output, itr->first.size());
      object_write_bencode_c_char(output, ':');
      object_write_bencode_c_string(output, itr->first.c_str(), itr->first.size());

      object_write_bencode_c_object(output, &itr->second);
    }

    object_write_bencode_c_char(output, 'e');
    break;
  }
}

object_buffer_t
object_write_bencode_c(object_write_t writeFunc, void* data, object_buffer_t buffer, const Object* object) {
  object_write_data_t output;
  output.writeFunc = writeFunc;
  output.data      = data;
  output.buffer    = buffer;
  output.pos       = buffer.first;

  object_write_bencode_c_object(&output, object);

  // Don't flush the buffer.
  if (output.pos == output.buffer.first)
    return output.buffer;

  return output.writeFunc(output.data, object_buffer_t(output.buffer.first, output.pos));
}

object_buffer_t
object_write_to_buffer(void* data, object_buffer_t buffer) {
  if (buffer.first == buffer.second)
    throw internal_error("object_write_to_buffer(...) buffer overflow.");

  return object_buffer_t(buffer.second, buffer.second);
}

object_buffer_t
object_write_to_sha1(void* data, object_buffer_t buffer) {
  reinterpret_cast<Sha1*>(data)->update(buffer.first, std::distance(buffer.first, buffer.second));

  return buffer;
}

object_buffer_t
object_write_to_stream(void* data, object_buffer_t buffer) {
  reinterpret_cast<std::ostream*>(data)->write(buffer.first, std::distance(buffer.first, buffer.second));

  if (reinterpret_cast<std::ostream*>(data)->bad())
    return object_buffer_t(buffer.first, buffer.first);

  return buffer;
}

}
