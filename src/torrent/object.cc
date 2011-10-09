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

#include <algorithm>
#include <cstring>
#include <rak/functional.h>

#include "object.h"
#include "object_stream.h"

namespace torrent {

Object&
Object::get_key(const std::string& k) {
  check_throw(TYPE_MAP);
  map_type::iterator itr = _map().find(k);

  if (itr == _map().end())
    throw bencode_error("Object operator [" + k + "] could not find element");

  return itr->second;
}


const Object&
Object::get_key(const std::string& k) const {
  check_throw(TYPE_MAP);
  map_type::const_iterator itr = _map().find(k);

  if (itr == _map().end())
    throw bencode_error("Object operator [" + k + "] could not find element");

  return itr->second;
}

Object&
Object::get_key(const char* k) {
  check_throw(TYPE_MAP);
  map_type::iterator itr = _map().find(std::string(k));

  if (itr == _map().end())
    throw bencode_error("Object operator [" + std::string(k) + "] could not find element");

  return itr->second;
}

const Object&
Object::get_key(const char* k) const {
  check_throw(TYPE_MAP);
  map_type::const_iterator itr = _map().find(std::string(k));

  if (itr == _map().end())
    throw bencode_error("Object operator [" + std::string(k) + "] could not find element");

  return itr->second;
}

Object::map_insert_type
Object::insert_preserve_type(const key_type& k, Object& b) {
  check_throw(TYPE_MAP);
  map_insert_type result = _map().insert(map_type::value_type(k, b));

  if (!result.second && result.first->second.type() != b.type()) {
    result.first->second.move(b);
    result.second = true;
  }

  return result;
}

Object&
Object::move(Object& src) {
  if (this == &src)
    return *this;

  *this = create_empty(src.type());
  swap_same_type(*this, src);

  return *this;
}

Object&
Object::swap(Object& src) {
  if (this == &src)
    return *this;

  if (type() != src.type()) {
    torrent::Object tmp = create_empty(src.type());
    swap_same_type(tmp, src);
    src = create_empty(this->type());
    swap_same_type(src, *this);
    *this = create_empty(tmp.type());
    swap_same_type(*this, tmp);

  } else {
    swap_same_type(*this, src);
  }

  return *this;
}

Object&
Object::merge_copy(const Object& object, uint32_t skip_mask, uint32_t maxDepth) {
  if (maxDepth == 0 || m_flags & skip_mask)
    return (*this = object);

  if (object.is_map()) {
    if (!is_map())
      *this = create_map();

    map_type& dest = as_map();
    map_type::iterator destItr = dest.begin();

    map_type::const_iterator srcItr = object.as_map().begin();
    map_type::const_iterator srcLast = object.as_map().end();

    while (srcItr != srcLast) {
      destItr = std::find_if(destItr, dest.end(), rak::less_equal(srcItr->first, rak::mem_ref(&map_type::value_type::first)));

      if (srcItr->first < destItr->first)
        // destItr remains valid and pointing to the next possible
        // position.
        dest.insert(destItr, *srcItr);
      else
        destItr->second.merge_copy(srcItr->second, maxDepth - 1);

      srcItr++;
    }

//   } else if (object.is_list()) {
//     if (!is_list())
//       *this = create_list();

//     list_type& dest = as_list();
//     list_type::iterator destItr = dest.begin();

//     list_type::const_iterator srcItr = object.as_list().begin();
//     list_type::const_iterator srcLast = object.as_list().end();
    
//     while (srcItr != srcLast) {
//       if (destItr == dest.end())
//         destItr = dest.insert(destItr, *srcItr);
//       else
//         destItr->merge_copy(*srcItr, maxDepth - 1);

//       destItr++;
//     }

  } else {
    *this = object;
  }

  return *this;
}

Object&
Object::operator = (const Object& src) {
  if (&src == this)
    return *this;

  clear();

  // Need some more magic here?
  m_flags = src.m_flags & (mask_type | mask_public);

  switch (type()) {
  case TYPE_STRING:   new (&_string()) string_type(src._string()); break;
  case TYPE_LIST:     new (&_list()) list_type(src._list()); break;
  case TYPE_MAP:      _map_ptr() = new map_type(src._map()); break;
  case TYPE_DICT_KEY: new (&_dict_key()) dict_key_type(src._dict_key()); _dict_key().second = new Object(*src._dict_key().second); break;
  default: t_pod = src.t_pod; break;
  }

  return *this;
}

Object object_create_normal(const raw_bencode& obj) {
  torrent::Object result;

  if (object_read_bencode_c(obj.begin(), obj.end(), &result, 128) != obj.end())
    throw bencode_error("Invalid bencode data.");

  return result;
}

Object object_create_normal(const raw_list& obj) {
  torrent::Object result = Object::create_list();

  raw_list::iterator first = obj.begin();
  raw_list::iterator last = obj.end();

  while (first != last) {
    Object::list_iterator new_entry = result.as_list().insert(result.as_list().end(), Object());

    first = object_read_bencode_c(first, last, &*new_entry, 128);

    // The unordered flag is inherited also from list elements who
    // have been marked as unordered, though e.g. unordered strings
    // in the list itself does not cause this flag to be set.
    if (new_entry->flags() & Object::flag_unordered)
      result.set_internal_flags(Object::flag_unordered);
  }

  return result;
}

Object object_create_normal(const raw_map& obj) {
  torrent::Object result = Object::create_map();

  raw_list::iterator first = obj.begin();
  raw_list::iterator last = obj.end();

  Object::string_type prev;

  while (first != last) {
    raw_string raw_str = object_read_bencode_c_string(first, last);
    first = raw_str.end();

    Object::string_type key_str = raw_str.as_string();

    // We do not set flag_unordered if the first key was zero
    // length, while multiple zero length keys will trigger the
    // unordered_flag.
    if (key_str <= prev && !result.as_map().empty())
      result.set_internal_flags(Object::flag_unordered);

    Object* value = &result.as_map()[key_str];
    first = object_read_bencode_c(first, last, value, 128);

    if (value->flags() & Object::flag_unordered)
      result.set_internal_flags(Object::flag_unordered);

    key_str.swap(prev);
  }

  return result;
}


}
