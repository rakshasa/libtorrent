// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
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
#include <cmath>
#include <limits>
#include <stdexcept>
#include <rak/algorithm.h>
#include <rak/string_manip.h>

#include "utils/sha1.h"

#include "object.h"
#include "object_stream.h"
#include "object_static_map.h"

namespace torrent {

bool
object_read_string(std::istream* input, std::string& str) {
  uint32_t size;
  *input >> size;

  if (input->fail() || input->get() != ':')
    return false;
  
  try {
  	str.resize(size);
  }
  catch (std::length_error& e){ 
	  return false;
  }

  for (std::string::iterator itr = str.begin(); itr != str.end() && input->good(); ++itr)
    *itr = input->get();
  
  return !input->fail();
}

const char*
object_read_bencode_c_value(const char* first, const char* last, int64_t& value) {
  if (first == last)
    return first;

  bool neg = false;

  if (*first == '-') {
    // Don't allow '-0', or '-' followed by non-numeral.
    if ((first + 1) == last || *(first + 1) <= '0' || *(first + 1) > '9')
      return first;

    neg = true;
    first++;
  }

  value = 0;

  while (first != last && *first >= '0' && *first <= '9')
    value = value * 10 + (*first++ - '0');

  if (neg)
    value = -value;

  return first;
}

raw_string
object_read_bencode_c_string(const char* first, const char* last) {
  // Set the most-significant bit so that if there are no numbers in
  // the input it will fail the length check, while "0" will shift the
  // bit out.
  unsigned int length = 0x1 << (std::numeric_limits<unsigned int>::digits - 1);

  while (first != last && *first >= '0' && *first <= '9')
    length = length * 10 + (*first++ - '0');

  if (length + 1 > (unsigned int)std::distance(first, last) || length + 1 == 0
		  || *first++ != ':')
    throw torrent::bencode_error("Invalid bencode data.");
  
  return raw_string(first, length);
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

      // The unordered flag is inherited also from list elements who
      // have been marked as unordered, though e.g. unordered strings
      // in the list itself does not cause this flag to be set.
      if (itr->flags() & Object::flag_unordered)
        object->set_internal_flags(Object::flag_unordered);
    }

    break;

  case 'd': {
    input->get();
    *object = Object::create_map();

    if (++depth >= 1024)
      break;

    Object::string_type last;

    while (input->good()) {
      if (input->peek() == 'e') {
	input->get();
	return;
      }

      Object::string_type str;

      if (!object_read_string(input, str))
	break;

      // We do not set flag_unordered if the first key was zero
      // length, while multiple zero length keys will trigger the
      // unordered_flag.
      if (str <= last && !object->as_map().empty())
        object->set_internal_flags(Object::flag_unordered);

      Object* value = &object->as_map()[str];
      object_read_bencode(input, value, depth);

      if (value->flags() & Object::flag_unordered)
        object->set_internal_flags(Object::flag_unordered);

      str.swap(last);
    }

    break;
  }

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

const char*
object_read_bencode_c(const char* first, const char* last, Object* object, uint32_t depth) {
  if (first == last)
    throw torrent::bencode_error("Invalid bencode data.");

  switch (*first) {
  case 'i':
    *object = Object::create_value();
    first = object_read_bencode_c_value(first + 1, last, object->as_value());

    if (first == last || *first++ != 'e')
      break;

    return first;

  case 'l':
    if (++depth >= 1024)
      break;

    first++;
    *object = Object::create_list();

    while (first != last) {
      if (*first == 'e')
	return first + 1;

      Object::list_iterator itr = object->as_list().insert(object->as_list().end(), Object());
      first = object_read_bencode_c(first, last, &*itr, depth);

      // The unordered flag is inherited also from list elements who
      // have been marked as unordered, though e.g. unordered strings
      // in the list itself does not cause this flag to be set.
      if (itr->flags() & Object::flag_unordered)
        object->set_internal_flags(Object::flag_unordered);
    }

    break;

  case 'd': {
    if (++depth >= 1024)
      break;

    first++;
    *object = Object::create_map();

    Object::string_type prev;

    while (first != last) {
      if (*first == 'e')
	return first + 1;

      raw_string raw_str = object_read_bencode_c_string(first, last);
      first = raw_str.end();

      Object::string_type str = raw_str.as_string();

      // We do not set flag_unordered if the first key was zero
      // length, while multiple zero length keys will trigger the
      // unordered_flag.
      if (str <= prev && !object->as_map().empty())
        object->set_internal_flags(Object::flag_unordered);

      Object* value = &object->as_map()[str];
      first = object_read_bencode_c(first, last, value, depth);

      if (value->flags() & Object::flag_unordered)
        object->set_internal_flags(Object::flag_unordered);

      str.swap(prev);
    }

    break;
  }

  default:
    if (*first < '0' || *first > '9')
      throw torrent::bencode_error("Invalid bencode data.");

    raw_string raw_str = object_read_bencode_c_string(first, last);
    *object = raw_str.as_string();

    return raw_str.end();
  }

  object->clear();
  throw torrent::bencode_error("Invalid bencode data.");
}

inline bool object_is_not_digit(char c) { return c < '0' || c > '9'; }

const char*
object_read_bencode_skip_c(const char* first, const char* last) {
  char stack[128] = { 0 };
  char* stack_itr = stack;

  while (first != last) {
    if (*first == 'e') {
      if (stack_itr-- == stack)
        throw torrent::bencode_error("Invalid bencode data.");

      first++;

      if (stack_itr == stack)
        return first;

      continue;
    }

    // Currently reading a dictionary, so ensure the first entry is a
    // string.
    if (*stack_itr && (first = object_read_bencode_c_string(first, last).end()) == last)
      break;

    switch (*first) {
    case 'i':
      if (first != last && *++first == '-') {
        if (first != last && *++first == '0')
          throw torrent::bencode_error("Invalid bencode data.");
      }

      first = std::find_if(first, last, &object_is_not_digit);

      if (first == last || *first++ != 'e')
        throw torrent::bencode_error("Invalid bencode data.");

      break;

    case 'l':
    case 'd':
      if (++stack_itr == stack + 128)
        throw torrent::bencode_error("Invalid bencode data.");

      *stack_itr = (*first++ == 'd');
      continue;

    default:
      first = object_read_bencode_c_string(first, last).end();
    };

    if (stack_itr == stack)
      return first;
  }

  throw torrent::bencode_error("Invalid bencode data.");
}

const char*
object_read_bencode_raw_c(const char* first, const char* last, torrent::Object* object, char type) {
  const char* tmp = first;
  first = object_read_bencode_skip_c(first, last);

  raw_bencode obj = raw_bencode(tmp, std::distance(tmp, first));

//   if (obj.is_empty())
//     throw torrent::bencode_error("Invalid bencode data.");

  switch (type) {
  case 'S':
    if (obj.is_raw_string())
      *object = obj.as_raw_string();
    break;
  case 'L':
    if (obj.is_raw_list())
      *object = obj.as_raw_list();
    break;
  case 'M':
    if (obj.is_raw_map())
      *object = obj.as_raw_map();
    break;
  default:
    *object = obj;
  };

  return first;
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
  while (srcLength != 0) {
    uint32_t len = std::min<uint32_t>(srcLength, std::distance(output->pos, output->buffer.second));
    std::memcpy(output->pos, srcData, len);

    output->pos += len;

    if (output->pos == output->buffer.second) {
      output->buffer = output->writeFunc(output->data, output->buffer);
      output->pos = output->buffer.first;

      if (output->buffer.first == output->buffer.second)
        return;
    }

    srcData += len;
    srcLength -= len;
  }
}

void
object_write_bencode_c_char(object_write_data_t* output, char src) {
  if (output->pos == output->buffer.second) {
    output->buffer = output->writeFunc(output->data, output->buffer);
    output->pos = output->buffer.first;

    if (output->buffer.first == output->buffer.second)
      return;
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

inline void
object_write_bencode_c_obj_value(object_write_data_t* output, int64_t value) {
  object_write_bencode_c_char(output, 'i');
  object_write_bencode_c_value(output, value);
  object_write_bencode_c_char(output, 'e');
}

inline void
object_write_bencode_c_obj_string(object_write_data_t* output, const char* data, uint32_t size) {
  object_write_bencode_c_value(output, size);
  object_write_bencode_c_char(output, ':');
  object_write_bencode_c_string(output, data, size);
}

inline void
object_write_bencode_c_obj_string(object_write_data_t* output, const std::string& str) {
  object_write_bencode_c_obj_string(output, str.c_str(), str.size());
}

void
object_write_bencode_c_object(object_write_data_t* output, const Object* object, uint32_t skip_mask) {
  switch (object->type()) {
  case Object::TYPE_NONE: break;
  case Object::TYPE_RAW_BENCODE:
  {
    raw_bencode raw = object->as_raw_bencode();
    object_write_bencode_c_string(output, raw.begin(), raw.size());
    break;
  }
  case Object::TYPE_RAW_STRING: 
  {
    raw_string raw = object->as_raw_string();
    object_write_bencode_c_value(output, raw.size());
    object_write_bencode_c_char(output, ':');
    object_write_bencode_c_string(output, raw.begin(), raw.size());
    break;
  }
  case Object::TYPE_RAW_LIST: 
  {
    raw_list raw = object->as_raw_list();
    object_write_bencode_c_char(output, 'l');
    object_write_bencode_c_string(output, raw.begin(), raw.size());
    object_write_bencode_c_char(output, 'e');
    break;
  }
  case Object::TYPE_RAW_MAP:
  {
    raw_map raw = object->as_raw_map();
    object_write_bencode_c_char(output, 'd');
    object_write_bencode_c_string(output, raw.begin(), raw.size());
    object_write_bencode_c_char(output, 'e');
    break;
  }
  case Object::TYPE_VALUE: object_write_bencode_c_obj_value(output, object->as_value()); break;
  case Object::TYPE_STRING: object_write_bencode_c_obj_string(output, object->as_string()); break;

  case Object::TYPE_LIST:
    object_write_bencode_c_char(output, 'l');

    for (Object::list_const_iterator itr = object->as_list().begin(), last = object->as_list().end(); itr != last; ++itr) {
      if (itr->is_empty() || itr->flags() & skip_mask)
        continue;

      object_write_bencode_c_object(output, &*itr, skip_mask);
    }

    object_write_bencode_c_char(output, 'e');
    break;

  case Object::TYPE_MAP:
    object_write_bencode_c_char(output, 'd');

    for (Object::map_const_iterator itr = object->as_map().begin(), last = object->as_map().end(); itr != last; ++itr) {
      if (itr->second.is_empty() || itr->second.flags() & skip_mask)
        continue;

      object_write_bencode_c_obj_string(output, itr->first);
      object_write_bencode_c_object(output, &itr->second, skip_mask);
    }

    object_write_bencode_c_char(output, 'e');
    break;
  case Object::TYPE_DICT_KEY:
    throw torrent::bencode_error("Cannot bencode internal dict_key type.");
    break;
  }
}

void
object_write_bencode(std::ostream* output, const Object* object, uint32_t skip_mask) {
  char buffer[1024];
  object_write_bencode_c(&object_write_to_stream, output, object_buffer_t(buffer, buffer + 1024), object, skip_mask);
}

object_buffer_t
object_write_bencode(char* first, char* last, const Object* object, uint32_t skip_mask) {
  object_buffer_t buffer = object_buffer_t(first, last);
  return object_write_bencode_c(&object_write_to_buffer, &buffer, buffer, object, skip_mask);
}

object_buffer_t
object_write_bencode_c(object_write_t writeFunc,
                       void* data,
                       object_buffer_t buffer,
                       const Object* object,
                       uint32_t skip_mask) {
  object_write_data_t output;
  output.writeFunc = writeFunc;
  output.data      = data;
  output.buffer    = buffer;
  output.pos       = buffer.first;

  if (!(object->flags() & skip_mask))
    object_write_bencode_c_object(&output, object, skip_mask);

  // Don't flush the buffer.
  if (output.pos == output.buffer.first)
    return output.buffer;

  return output.writeFunc(output.data, object_buffer_t(output.buffer.first, output.pos));
}

object_buffer_t
object_write_to_buffer(void* data, object_buffer_t buffer) {
  if (buffer.first == buffer.second)
    throw internal_error("object_write_to_buffer(...) buffer overflow.");

  // Hmm... this does something weird...
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

object_buffer_t
object_write_to_size(void* data, object_buffer_t buffer) {
  *reinterpret_cast<uint64_t*>(data) += std::distance(buffer.first, buffer.second);

  return buffer;
}

//
// static_map operations:
//

const char*
static_map_read_bencode_c(const char* first,
                         const char* last,
                         static_map_entry_type* entry_values,
                         const static_map_mapping_type* first_key,
                         const static_map_mapping_type* last_key) {
  // Temp hack... validate that we got valid bencode data...
//   {
//     torrent::Object obj;
//     if (object_read_bencode_c(first, last, &obj) != last) {
//       std::string escaped = rak::copy_escape_html(first, last);

//       char buffer[1024];
//       sprintf(buffer, "Verified wrong, %u, '%u', '%s'.", std::distance(first, last), (unsigned int)*first, escaped.c_str());

//       throw torrent::internal_error("Invalid bencode data.");
//     }
//   }

  if (first == last || *first++ != 'd')
    throw torrent::bencode_error("Invalid bencode data.");
  
  static_map_stack_type stack[8];
  static_map_stack_type* stack_itr = stack;
  stack_itr->clear();

  char current_key[static_map_mapping_type::max_key_size + 2] = "";

  while (first != last) {
    // End a dictionary/list or the whole stream.
    if (*first == 'e') {
      first++;

      if (stack_itr == stack)
        return first;

      stack_itr--;
      continue;
    }

    raw_string raw_key = object_read_bencode_c_string(first, last);
    first = raw_key.end();

    // Optimze this buy directly copying into 'current_key'.
    //
    // The max length of 'current_key' is one char more than the
    // mapping key so any bencode which exceeds that will always fail
    // to find a match.
    if (raw_key.size() >= static_map_mapping_type::max_key_size - stack_itr->next_key) {
      first = object_read_bencode_skip_c(first, last);
      continue;
    }

    memcpy(current_key + stack_itr->next_key, raw_key.data(), raw_key.size());
    current_key[stack_itr->next_key + raw_key.size()] = '\0';

    // Locate the right key. Optimize this by remembering previous
    // position...
    static_map_key_search_result key_search = find_key_match(first_key, last_key, current_key);    

    // We're not interest in this object, skip it.
    if (key_search.second == 0) {
      first = object_read_bencode_skip_c(first, last);
      continue;
    }

    // Check if we're interested in any dictionaries/lists entries
    // under this key.
    //
    // Note that 'find_key_match' only returns 'key_search.second !=
    // 0' for keys where the next characters are '\0', '::' or '[]'.
    switch (key_search.first->key[key_search.second]) {
    case '\0':
      first = object_read_bencode_c(first, last, &entry_values[key_search.first->index].object);
      first_key = key_search.first + 1;
      break;

    case '*':
      first = object_read_bencode_raw_c(first, last,
                                        &entry_values[key_search.first->index].object,
                                        key_search.first->key[key_search.second + 1]);
      first_key = key_search.first + 1;
      break;
    case ':':
      if (first == last)
        break;

      // The bencode object isn't a list. This should either skip it
      // or produce an error.
      if (*first++ != 'd') {
        first = object_read_bencode_skip_c(first - 1, last);
        break;
      }

      stack_itr++;
      stack_itr->set_key_index((stack_itr - 1)->next_key, key_search.second, 2);

      current_key[key_search.second] = ':';
      current_key[key_search.second + 1] = ':';
      break;

    case '[':
    {
      if (first == last)
        break;

      // The bencode object isn't a list. This should either skip it
      // or produce an error.
      if (*first++ != 'l') {
        first = object_read_bencode_skip_c(first - 1, last);
        break;
      }

      first_key = key_search.first;

      while (first != last) {
        if (*first == 'e') {
          first++;
          break;
        }

        if (first_key->key[key_search.second + 2] == '*') {
          first = object_read_bencode_raw_c(first, last,
                                            &entry_values[first_key->index].object,
                                            key_search.first->key[key_search.second + 1]);
        } else {
          first = object_read_bencode_c(first, last, &entry_values[first_key->index].object);
        }

        if (++first_key == last_key || strcmp(first_key->key, (first_key - 1)->key) != 0) {
          while (first != last) {
            if (*first == 'e') {
              first++;
              break;
            }

            first = object_read_bencode_skip_c(first, last);
          }

          break;
        }
      }

      break;
    }
    default:
      throw internal_error("static_map_read_bencode_c: key_search.first->key[base] returned invalid character.");
    };
  }
  
  throw torrent::bencode_error("Invalid bencode data.");
}

void
static_map_write_bencode_c_values(object_write_data_t* output,
                                 const static_map_entry_type* entry_values,
                                 const static_map_mapping_type* first_key,
                                 const static_map_mapping_type* last_key) {
  const char* prev_key = NULL;

  static_map_stack_type stack[8];
  static_map_stack_type* stack_itr = stack;
  stack_itr->clear();

  object_write_bencode_c_char(output, 'd');

  while (first_key != last_key) {
    if (entry_values[first_key->index].object.is_empty()) {
      first_key++;
      continue;
    }

    // Compare the keys to see if they are part of the same
    // dictionaries/lists.
    unsigned int base_size = rak::count_base(first_key->key, first_key->key + stack_itr->next_key,
                                             prev_key, prev_key + stack_itr->next_key);

    while (base_size < stack_itr->next_key) {
      object_write_bencode_c_char(output, 'e');
      stack_itr--;
    }

    const char* key_begin = first_key->key + stack_itr->next_key;

    do {
      const char* key_end = first_key->find_key_end(key_begin);

      if (stack_itr->obj_type == Object::TYPE_MAP)
        object_write_bencode_c_obj_string(output, key_begin, std::distance(key_begin, key_end));
    
      // Check if '::' or '[' were found...
      if (*key_end == ':' && *(key_end + 1) == ':') {
        (++stack_itr)->set_key_index(std::distance(first_key->key, key_begin),
                                     std::distance(first_key->key, key_end), 2);

        key_begin = key_end + 2;
        object_write_bencode_c_char(output, 'd');
        continue;
      }

      // Handle "foo[]..." entries. We iterate once for each "foo[]"
      // found in the key list.
      if (*key_end == '[' && *(key_end + 1) == ']') {
        (++stack_itr)->set_key_index(std::distance(first_key->key, key_begin),
                                     std::distance(first_key->key, key_end), 2,
                                     Object::TYPE_LIST);

        key_begin = key_end + 2;
        object_write_bencode_c_char(output, 'l');
        continue;
      }

      // We have a leaf object.
      if (*key_end != '\0' && *key_end != '*')
        throw internal_error("static_map_type key is invalid.");

      object_write_bencode_c_object(output, &entry_values[first_key->index].object, 0);

      break;
    } while (true);

    prev_key = (first_key++)->key;
  }

  do {
    object_write_bencode_c_char(output, 'e');
  } while (stack_itr-- != stack);
}

object_buffer_t
static_map_write_bencode_c_wrap(object_write_t writeFunc,
                               void* data,
                               object_buffer_t buffer,
                               const static_map_entry_type* entry_values,
                               const static_map_mapping_type* first_key,
                               const static_map_mapping_type* last_key) {
  object_write_data_t output;
  output.writeFunc = writeFunc;
  output.data      = data;
  output.buffer    = buffer;
  output.pos       = buffer.first;

  static_map_write_bencode_c_values(&output, entry_values, first_key, last_key);

  // DEBUG: Remove this.
//   {
//     torrent::Object obj;
//     if (object_read_bencode_c(output.buffer.first, output.pos, &obj) != output.pos) {
//       std::string escaped = rak::copy_escape_html(output.buffer.first, output.pos);

//       //char buffer[1024];
//       //      sprintf(buffer, "Verified wrong, %u, '%u', '%s'.", std::distanescaped.c_str());

//       throw torrent::internal_error("Invalid bencode data generated: '" + escaped + "'");
//     }
//   }

  // Don't flush the buffer.
  if (output.pos == output.buffer.first)
    return output.buffer;

  return output.writeFunc(output.data, object_buffer_t(output.buffer.first, output.pos));
}

}
