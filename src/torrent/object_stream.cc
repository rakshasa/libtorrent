// libTorrent - BitTorrent library
// Copyright (C) 2005-2006, Jari Sundell
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
#include <sstream>
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

    *object = Object(Object::TYPE_VALUE);
    *input >> object->as_value();

    if (input->get() != 'e')
      break;

    return;

  case 'l':
    input->get();
    *object = Object(Object::TYPE_LIST);

    if (++depth >= 1024)
      break;

    while (input->good()) {
      if (input->peek() == 'e') {
	input->get();
	return;
      }

      Object::list_type::iterator itr = object->as_list().insert(object->as_list().end(), Object());
      object_read_bencode(input, &*itr, depth);
    }

    break;

  case 'd':
    input->get();
    *object = Object(Object::TYPE_MAP);

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
      *object = Object(Object::TYPE_STRING);
      
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
  // A decent compiler should be able to optimize away the
  // Object::check_type calls.
  switch (object->type()) {
  case Object::TYPE_NONE:
    // Consider failing here?
    break;

  case Object::TYPE_VALUE:
    *output << 'i' << object->as_value() << 'e';
    break;

  case Object::TYPE_STRING:
    *output << object->as_string().size() << ':' << object->as_string();
    break;

  case Object::TYPE_LIST:
    // Add check for depth here?
    *output << 'l';

    for (Object::list_type::const_iterator itr = object->as_list().begin(), last = object->as_list().end(); itr != last && output->good(); ++itr)
      object_write_bencode(output, &*itr);

    *output << 'e';
    break;

  case Object::TYPE_MAP:
    // Add check for depth here?
    *output << 'd';

    for (Object::map_type::const_iterator itr = object->as_map().begin(), last = object->as_map().end(); itr != last && output->good(); ++itr) {
      *output << itr->first.size() << ':' << itr->first;
      object_write_bencode(output, &itr->second);
    }

    *output << 'e';
    break;
  }
}

// Would be nice to have a straight stream to hash conversion.
std::string
object_sha1(const Object* object) {
  std::stringstream str;
  str << *object;

  if (str.fail())
    throw bencode_error("Could not write bencode to stream");

  std::string s = str.str();
  Sha1 sha1;

  sha1.init();
  sha1.update(s.c_str(), s.size());

  char buffer[20];
  sha1.final_c(buffer);

  // Testing new write functions.
  Sha1 shaTest;
  char testBuffer[1024];

  shaTest.init();
  object_write_bencode_c(&object_write_to_sha1, &shaTest, std::make_pair(testBuffer, testBuffer + 1024), object);
  shaTest.final_c(testBuffer);

  if (std::memcmp(buffer, testBuffer, 20) != 0)
    throw internal_error("BORK BORK_");

  // End test.

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
  std::locale locale = output.imbue(std::locale::classic());

  object_write_bencode(&output, &object);

  output.imbue(locale);
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

    for (Object::list_type::const_iterator itr = object->as_list().begin(), last = object->as_list().end(); itr != last; ++itr)
      object_write_bencode_c_object(output, &*itr);

    object_write_bencode_c_char(output, 'e');
    break;

  case Object::TYPE_MAP:
    object_write_bencode_c_char(output, 'd');

    for (Object::map_type::const_iterator itr = object->as_map().begin(), last = object->as_map().end(); itr != last; ++itr) {
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

  return output.writeFunc(output.data, std::make_pair(output.buffer.first, output.pos));
}

object_buffer_t
object_write_to_buffer(void* data, object_buffer_t buffer) {
  if (buffer.first == buffer.second)
    throw internal_error("object_write_to_buffer(...) buffer overflow.");

  return std::make_pair(buffer.second, buffer.second);
}

object_buffer_t
object_write_to_sha1(void* data, object_buffer_t buffer) {
  ((Sha1*)data)->update(buffer.first, std::distance(buffer.first, buffer.second));

  return buffer;
}

}
