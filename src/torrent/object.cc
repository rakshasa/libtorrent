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

#include <rak/functional.h>

#include "object.h"

namespace torrent {

Object::Object(const Object& b) :
  m_type(b.m_type)
{
  switch (m_type) {
  case TYPE_VALUE:
    m_value = b.m_value;
    break;
  case TYPE_STRING:
    m_string = new string_type(*b.m_string);
    break;
  case TYPE_LIST:
    m_list = new list_type(*b.m_list);
    break;
  case TYPE_MAP:
    m_map = new map_type(*b.m_map);
    break;
  default:
    break;
  }
}

void
Object::clear() {
  switch (m_type) {
  case TYPE_NONE:
  case TYPE_VALUE:
    break;
  case TYPE_STRING:
    delete m_string;
    break;
  case TYPE_LIST:
    delete m_list;
    break;
  case TYPE_MAP:
    delete m_map;
    break;
  default:
    break;
  }

  m_type = TYPE_NONE;
}

Object&
Object::operator = (const Object& b) {
  if (&b == this)
    return *this;

  clear();

  m_type = b.m_type;

  switch (m_type) {
  case TYPE_VALUE:
    m_value = b.m_value;
    break;
  case TYPE_STRING:
    m_string = new string_type(*b.m_string);
    break;
  case TYPE_LIST:
    m_list = new list_type(*b.m_list);
    break;
  case TYPE_MAP:
    m_map = new map_type(*b.m_map);
    break;
  default:
    break;
  }

  return *this;
}

Object&
Object::get_key(const std::string& k) {
  check_throw(TYPE_MAP);

  map_type::iterator itr = m_map->find(k);

  if (itr == m_map->end())
    throw bencode_error("Object operator [" + k + "] could not find element");

  return itr->second;
}


const Object&
Object::get_key(const std::string& k) const {
  check_throw(TYPE_MAP);

  map_type::const_iterator itr = m_map->find(k);

  if (itr == m_map->end())
    throw bencode_error("Object operator [" + k + "] could not find element");

  return itr->second;
}

// Add a parameter for how deep we do the merge?

Object&
Object::merge_recursive(const Object& object, uint32_t maxDepth) {
  if (maxDepth == 0 || !is_map() || !object.is_map())
    return (*this = object);

  map_type& dest = as_map();
  map_type::iterator destItr = dest.begin();

  map_type::const_iterator srcItr = object.as_map().begin();
  map_type::const_iterator srcLast = object.as_map().end();

  while (srcItr != srcLast) {
    destItr = std::find_if(destItr, dest.end(), rak::less_equal(srcItr->first, rak::mem_ptr_ref(&map_type::value_type::first)));

    if (srcItr->first < destItr->first) {
      // destItr remains valid and pointing to the next possible
      // position.
      dest.insert(destItr, *srcItr);

    } else {
      destItr->second.merge_recursive(srcItr->second, maxDepth - 1);
    }

    srcItr++;
  }

  return *this;
}

}
