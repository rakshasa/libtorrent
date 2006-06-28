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

#ifndef LIBTORRENT_OBJECT_H
#define LIBTORRENT_OBJECT_H

#include <string>
#include <map>
#include <list>
#include <inttypes.h>
#include <torrent/exceptions.h>

namespace torrent {

// Look into making a custom comp and allocator classes for the
// map_type which use a const char* for key_type.

class Object {
public:
  typedef int64_t                         value_type;
  typedef std::string                     string_type;
  typedef std::list<Object>               list_type;
  typedef std::map<std::string, Object>   map_type;
  typedef map_type::key_type              key_type;

  enum type_type {
    TYPE_NONE,
    TYPE_VALUE,
    TYPE_STRING,
    TYPE_LIST,
    TYPE_MAP
  };

  Object()                     : m_type(TYPE_NONE) {}
  Object(const int64_t v)      : m_type(TYPE_VALUE), m_value(v) {}
  Object(const char* s)        : m_type(TYPE_STRING), m_string(new string_type(s)) {}
  Object(const string_type& s) : m_type(TYPE_STRING), m_string(new string_type(s)) {}
  Object(const Object& b);

  explicit Object(type_type t);
  
  ~Object() { clear(); }

  void                clear();

  type_type           type() const                            { return m_type; }

  bool                is_value() const                        { return m_type == TYPE_VALUE; }
  bool                is_string() const                       { return m_type == TYPE_STRING; }
  bool                is_list() const                         { return m_type == TYPE_LIST; }
  bool                is_map() const                          { return m_type == TYPE_MAP; }

  value_type&         as_value()                              { check_throw(TYPE_VALUE); return m_value; }
  const value_type&   as_value() const                        { check_throw(TYPE_VALUE); return m_value; }

  string_type&        as_string()                             { check_throw(TYPE_STRING); return *m_string; }
  const string_type&  as_string() const                       { check_throw(TYPE_STRING); return *m_string; }

  list_type&          as_list()                               { check_throw(TYPE_LIST); return *m_list; }
  const list_type&    as_list() const                         { check_throw(TYPE_LIST); return *m_list; }

  map_type&           as_map()                                { check_throw(TYPE_MAP); return *m_map; }
  const map_type&     as_map() const                          { check_throw(TYPE_MAP); return *m_map; }

  bool                has_key(const key_type& k) const        { check_throw(TYPE_MAP); return m_map->find(k) != m_map->end(); }
  bool                has_key_value(const key_type& k) const  { check_throw(TYPE_MAP); return check(m_map->find(k), TYPE_VALUE); }
  bool                has_key_string(const key_type& k) const { check_throw(TYPE_MAP); return check(m_map->find(k), TYPE_STRING); }
  bool                has_key_list(const key_type& k) const   { check_throw(TYPE_MAP); return check(m_map->find(k), TYPE_LIST); }
  bool                has_key_map(const key_type& k) const    { check_throw(TYPE_MAP); return check(m_map->find(k), TYPE_MAP); }

  // Should have an interface for that returns pointer or something,
  // so we don't need to search twice.

  Object&             get_key(const key_type& k);
  const Object&       get_key(const key_type& k) const;

  value_type&         get_key_value(const key_type& k)               { return get_key(k).as_value(); }
  const value_type&   get_key_value(const key_type& k) const         { return get_key(k).as_value(); }

  string_type&        get_key_string(const key_type& k)              { return get_key(k).as_string(); }
  const string_type&  get_key_string(const key_type& k) const        { return get_key(k).as_string(); }

  list_type&          get_key_list(const key_type& k)                { return get_key(k).as_list(); }
  const list_type&    get_key_list(const key_type& k) const          { return get_key(k).as_list(); }

  map_type&           get_key_map(const key_type& k)                 { return get_key(k).as_map(); }
  const map_type&     get_key_map(const key_type& k) const           { return get_key(k).as_map(); }

  Object&             insert_key(const key_type& k, const Object& b) { check_throw(TYPE_MAP); return (*m_map)[k] = b; }
  void                erase_key(const key_type& k)                   { check_throw(TYPE_MAP); m_map->erase(k); }

  Object&             operator = (const Object& b);

 private:
  inline bool         check(map_type::const_iterator itr, type_type t) const { return itr != m_map->end() && itr->second.m_type == t; }
  inline void         check_throw(type_type t) const                         { if (t != m_type) throw bencode_error("Wrong object type."); }

  type_type           m_type;

  union {
    int64_t             m_value;
    string_type*        m_string;
    list_type*          m_list;
    map_type*           m_map;
  };
};

inline
Object::Object(type_type t) :
  m_type(t) {

  switch (m_type) {
  case TYPE_NONE:
    break;
  case TYPE_VALUE:
    m_value = value_type();
    break;
  case TYPE_STRING:
    m_string = new string_type();
    break;
  case TYPE_LIST:
    m_list = new list_type();
    break;
  case TYPE_MAP:
    m_map = new map_type();
    break;
  }
}

}

#endif
