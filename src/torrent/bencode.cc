// libTorrent - BitTorrent library
// Copyright (C) 2005, Jari Sundell
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <iostream>
#include <sstream>

#include "bencode.h"
#include "exceptions.h"
#include "utils/sha1.h"

namespace torrent {

Bencode::Bencode(const Bencode& b) :
  m_type(b.m_type)
{
  switch (m_type) {
  case TYPE_VALUE:
    m_value = b.m_value;
    break;
  case TYPE_STRING:
    m_string = new std::string(*b.m_string);
    break;
  case TYPE_LIST:
    m_list = new List(*b.m_list);
    break;
  case TYPE_MAP:
    m_map = new Map(*b.m_map);
    break;
  default:
    break;
  }
}

Bencode::Bencode(Type t) :
  m_type(t)
{
  switch (m_type) {
  case TYPE_VALUE:
    m_value = 0;
    break;
  case TYPE_STRING:
    m_string = new std::string();
    break;
  case TYPE_LIST:
    m_list = new List();
    break;
  case TYPE_MAP:
    m_map = new Map();
    break;
  default:
    break;
  }
}

void
Bencode::clear() {
  switch (m_type) {
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

Bencode&
Bencode::operator = (const Bencode& b) {
  clear();

  m_type = b.m_type;

  switch (m_type) {
  case TYPE_VALUE:
    m_value = b.m_value;
    break;
  case TYPE_STRING:
    m_string = new std::string(*b.m_string);
    break;
  case TYPE_LIST:
    m_list = new List(*b.m_list);
    break;
  case TYPE_MAP:
    m_map = new Map(*b.m_map);
    break;
  default:
    break;
  }

  return *this;
}

std::istream&
operator >> (std::istream& s, Bencode& b) {
  b.clear();

  if (s.peek() < 0) {
    s.setstate(std::istream::failbit);
    return s;
  }

  char c;

  switch (c = s.peek()) {
  case 'i':
    s.get();
    s >> b.m_value;

    if (s.fail() || s.get() != 'e')
      break;

    b.m_type = Bencode::TYPE_VALUE;

    return s;

  case 'l':
    s.get();

    b.m_list = new Bencode::List;
    b.m_type = Bencode::TYPE_LIST;

    while (s.good()) {
      if (s.peek() == 'e') {
	s.get();
	return s;
      }

      Bencode::List::iterator itr = b.m_list->insert(b.m_list->end(), Bencode());

      s >> *itr;
    }

    break;
  case 'd':
    s.get();

    b.m_map = new Bencode::Map;
    b.m_type = Bencode::TYPE_MAP;

    while (s.good()) {
      if (s.peek() == 'e') {
	s.get();
	return s;
      }

      std::string str;

      if (!Bencode::read_string(s, str))
	break;

      s >> (*b.m_map)[str];
    }

    break;
  default:
    if (c >= '0' && c <= '9') {
      b.m_string = new std::string();
      b.m_type = Bencode::TYPE_STRING;
      
      if (b.read_string(s, *b.m_string))
	return s;
    }

    break;
  }

  s.setstate(std::istream::failbit);
  b.clear();

  return s;
}

std::ostream&
operator << (std::ostream& s, const Bencode& b) {
  switch (b.m_type) {
  case Bencode::TYPE_VALUE:
    return s << 'i' << b.m_value << 'e';
  case Bencode::TYPE_STRING:
    return s << b.m_string->size() << ':' << *b.m_string;
  case Bencode::TYPE_LIST:
    s << 'l';

    for (Bencode::List::const_iterator itr = b.m_list->begin(); itr != b.m_list->end(); ++itr)
      s << *itr;

    return s << 'e';
  case Bencode::TYPE_MAP:
    s << 'd';

    for (Bencode::Map::const_iterator itr = b.m_map->begin(); itr != b.m_map->end(); ++itr)
      s << itr->first.size() << ':' << itr->first << itr->second;

    return s << 'e';
  default:
    break;
  }

  return s;
}
    
Bencode&
Bencode::operator [] (const std::string& k) {
  if (m_type != TYPE_MAP)
    throw bencode_error("Bencode operator [" + k + "] called on wrong type");

  Map::iterator itr = m_map->find(k);

  if (itr == m_map->end())
    throw bencode_error("Bencode operator [" + k + "] could not find element");

  return itr->second;
}


const Bencode&
Bencode::operator [] (const std::string& k) const {
  if (m_type != TYPE_MAP)
    throw bencode_error("Bencode operator [" + k + "] called on wrong type");

  Map::const_iterator itr = m_map->find(k);

  if (itr == m_map->end())
    throw bencode_error("Bencode operator [" + k + "] could not find element");

  return itr->second;
}

bool
Bencode::has_key(const std::string& s) const {
  if (m_type != TYPE_MAP)
    throw bencode_error("Bencode::has_key(" + s + ") called on wrong type");

  return m_map->find(s) != m_map->end();
}

Bencode&
Bencode::insert_key(const std::string& s, const Bencode& b) {
  if (m_type != TYPE_MAP)
    throw bencode_error("Bencode::insert_key(" + s + ", ...) called on wrong type");

  return (*m_map)[s] = b;
}

void
Bencode::erase_key(const std::string& s) {
  if (m_type != TYPE_MAP)
    throw bencode_error("Bencode::erase_key(" + s + ") called on wrong type");

  Map::iterator itr = m_map->find(s);

  if (itr != m_map->end())
    m_map->erase(itr);
}

bool
Bencode::read_string(std::istream& s, std::string& str) {
  int size;
  s >> size;

  if (s.fail() || s.get() != ':')
    return false;
  
  str.resize(size);

  for (std::string::iterator itr = str.begin(); itr != str.end() && s.good(); ++itr)
    *itr = s.get();
  
  return !s.fail();
}

int64_t&
Bencode::as_value() {
  if (!is_value())
    throw bencode_error("Bencode is not type value");

  return m_value;
}

int64_t
Bencode::as_value() const {
  if (!is_value())
    throw bencode_error("Bencode is not type value");

  return m_value;
}

std::string&
Bencode::as_string() {
  if (!is_string())
    throw bencode_error("Bencode is not type string");

  return *m_string;
}

const std::string&
Bencode::as_string() const {
  if (!is_string())
    throw bencode_error("Bencode is not type string");

  return *m_string;
}

Bencode::List&
Bencode::as_list() {
  if (!is_list())
    throw bencode_error("Bencode is not type list");

  return *m_list;
}

const Bencode::List&
Bencode::as_list() const {
  if (!is_list())
    throw bencode_error("Bencode is not type list");

  return *m_list;
}

Bencode::Map&
Bencode::as_map() {
  if (!is_map())
    throw bencode_error("Bencode is not type map");

  return *m_map;
}

const Bencode::Map&
Bencode::as_map() const {
  if (!is_map())
    throw bencode_error("Bencode is not type map");

  return *m_map;
}

// Would be nice to have a straigth stream to hash conversion.
std::string
Bencode::compute_sha1() const {
  std::stringstream str;
  str << *this;

  if (str.fail())
    throw bencode_error("Could not write bencode to stream");

  std::string s = str.str();
  Sha1 sha1;

  sha1.init();
  sha1.update(s.c_str(), s.size());

  return sha1.final();
}

} // namespace Torrent
