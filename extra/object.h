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

#ifndef LIBTORRENT_OBJECT_H
#define LIBTORRENT_OBJECT_H

#include <cstring>
#include <string>
#include <map>
#include <list>
#include <torrent/common.h>
#include <torrent/exceptions.h>

namespace torrent {

// Add support for the GCC move semantics.

class ObjectRefRef;

class LIBTORRENT_EXPORT Object {
public:
  typedef int64_t                         value_type;
  typedef std::string                     string_type;
  typedef std::list<Object>               list_type;
  typedef std::map<std::string, Object>   map_type;
  typedef map_type::key_type              key_type;

  // Figure a better name for this?
  typedef uint32_t                        state_type;

  enum type_type {
    TYPE_NONE,
    TYPE_VALUE,
    TYPE_STRING,
    TYPE_LIST,
    TYPE_MAP
  };

  // The users of the library should only ever set/get the bits in the
  // public mask, else risk waking dragons.
  //
  // Consider if part of the mask should be made to clear upon copy,
  // and do so for both private and public?

  static const state_type mask_private   = 0x0000ffff;
  static const state_type mask_public    = 0xffff0000;
  static const state_type mask_type      = 0x000000ff;
  
//   static const state_type flag_const   = 0x000000;
  static const state_type flag_reference = 0x100;

  static const state_type type_none      = TYPE_NONE;
  static const state_type type_value     = TYPE_VALUE;
  static const state_type type_string    = TYPE_STRING;
  static const state_type type_list      = TYPE_LIST;
  static const state_type type_map       = TYPE_MAP;

  // Add ctors that take a use_copy, use_move and use_internal_move
  // parameters.

  Object()                     : m_state(type_none) {}
  Object(const value_type v)   : m_state(type_value), m_value(v) {}
  Object(const char* s)        : m_state(type_string), m_string(new string_type(s)) {}
  Object(const string_type& s) : m_state(type_string), m_string(new string_type(s)) {}
  Object(const Object& b);

  // Hmm... reconsider this.
  explicit Object(type_type t);
  
  ~Object() { clear(); }

  void                clear();

  type_type           type() const                            { return (type_type)(m_state & mask_type); }

  bool                is_value() const                        { return type() == type_value; }
  bool                is_string() const                       { return type() == type_string; }
  bool                is_list() const                         { return type() == type_list; }
  bool                is_map() const                          { return type() == type_map; }

  bool                is_reference() const                    { return m_state & flag_reference; }

  // Add _mutable_ to non-const access.
  value_type&         as_value()                              { check_throw(type_value); return m_value; }
  const value_type&   as_value() const                        { check_throw(type_value); return m_value; }

  string_type&        as_string()                             { check_throw(type_string); return *m_string; }
  const string_type&  as_string() const                       { check_throw(type_string); return *m_string; }

  list_type&          as_list()                               { check_throw(type_list); return *m_list; }
  const list_type&    as_list() const                         { check_throw(type_list); return *m_list; }

  map_type&           as_map()                                { check_throw(type_map); return *m_map; }
  const map_type&     as_map() const                          { check_throw(type_map); return *m_map; }

  // Map operations:
  bool                has_key(const key_type& k) const        { check_throw(type_map); return m_map->find(k) != m_map->end(); }
  bool                has_key_value(const key_type& k) const  { check_throw(type_map); return check(m_map->find(k), type_value); }
  bool                has_key_string(const key_type& k) const { check_throw(type_map); return check(m_map->find(k), type_string); }
  bool                has_key_list(const key_type& k) const   { check_throw(type_map); return check(m_map->find(k), type_list); }
  bool                has_key_map(const key_type& k) const    { check_throw(type_map); return check(m_map->find(k), type_map); }

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

  Object&             insert_key(const key_type& k, const Object& b) { check_throw(type_map); return (*m_map)[k] = b; }
  void                erase_key(const key_type& k)                   { check_throw(type_map); m_map->erase(k); }

  // List operations:
  Object&             insert_front(const Object& b)                  { check_throw(type_list); return *m_list->insert(m_list->begin(), b); }
  Object&             insert_back(const Object& b)                   { check_throw(type_list); return *m_list->insert(m_list->end(), b); }

  // Copy and merge operations:
  Object&             move(Object& b);
  Object&             swap(Object& b);

  // Only map entries are merged.
  Object&             merge_move(Object object, uint32_t maxDepth = ~uint32_t());
  Object&             merge_copy(const Object& object, uint32_t maxDepth = ~uint32_t());

  Object&             operator = (const Object& b);

private:
  inline bool         check(map_type::const_iterator itr, state_type t) const;
  inline void         check_throw(state_type t) const;

  state_type          m_state;

  union {
    int64_t             m_value;
    string_type*        m_string;
    list_type*          m_list;
    map_type*           m_map;
  };
};

inline
Object::Object(type_type t) :
  m_state(t) {

  switch (t) {
  case TYPE_NONE:   break;
  case TYPE_VALUE:  m_value = value_type(); break;
  case TYPE_STRING: m_string = new string_type(); break;
  case TYPE_LIST:   m_list = new list_type(); break;
  case TYPE_MAP:    m_map = new map_type(); break;
  }
}

inline bool
Object::check(map_type::const_iterator itr, state_type t) const {
  return itr != m_map->end() && itr->second.type() == t;
}

inline void
Object::check_throw(state_type t) const {
  if (t != type())
    throw bencode_error("Wrong object type.");
}

}

#endif
