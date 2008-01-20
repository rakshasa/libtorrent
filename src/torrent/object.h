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

#include <string>
#include <map>
#include <list>
#include <torrent/common.h>
#include <torrent/exceptions.h>

namespace torrent {

// Look into making a custom comp and allocator classes for the
// map_type which use a const char* for key_type.

class LIBTORRENT_EXPORT Object {
public:
  typedef int64_t                           value_type;
  typedef std::string                       string_type;
  typedef std::list<Object>                 list_type;
  typedef std::map<std::string, Object>     map_type;
  typedef map_type::key_type                key_type;

  typedef list_type::iterator               list_iterator;
  typedef list_type::const_iterator         list_const_iterator;
  typedef list_type::reverse_iterator       list_reverse_iterator;
  typedef list_type::const_reverse_iterator list_const_reverse_iterator;

  typedef map_type::iterator                map_iterator;
  typedef map_type::const_iterator          map_const_iterator;
  typedef map_type::reverse_iterator        map_reverse_iterator;
  typedef map_type::const_reverse_iterator  map_const_reverse_iterator;

  typedef std::pair<map_iterator, bool>     map_insert_type;

  static const uint32_t mask_type     = 0xff;
  static const uint32_t mask_internal = 0xffff;
  static const uint32_t mask_public   = ~mask_internal;

  enum type_type {
    TYPE_NONE,
    TYPE_VALUE,
    TYPE_STRING,
    TYPE_LIST,
    TYPE_MAP
  };

  Object()                     : m_flags(TYPE_NONE) {}
  Object(const value_type v)   : m_flags(TYPE_VALUE), m_value(v) {}
  Object(const char* s)        : m_flags(TYPE_STRING), m_string(new string_type(s)) {}
  Object(const string_type& s) : m_flags(TYPE_STRING), m_string(new string_type(s)) {}
  Object(const Object& b);

  ~Object() { clear(); }

  // Move this out of the class namespace, call them create_object_.
  static Object       create_value()  { return Object(value_type()); }
  static Object       create_string() { return Object(string_type()); }
  static Object       create_list()   { Object tmp; tmp.m_flags = TYPE_LIST; tmp.m_list = new list_type(); return tmp; }
  static Object       create_map()    { Object tmp; tmp.m_flags = TYPE_MAP;  tmp.m_map  = new map_type();  return tmp; }

  // Clear should probably not be inlined due to size and not being
  // optimized away in pretty much any case. Might not work well in
  // cases where we pass constant rvalues.
  void                clear();

  type_type           type() const                            { return (type_type)(m_flags & mask_type); }

  // Add functions for setting/clearing the public flags.

  bool                is_value() const                        { return type() == TYPE_VALUE; }
  bool                is_string() const                       { return type() == TYPE_STRING; }
  bool                is_list() const                         { return type() == TYPE_LIST; }
  bool                is_map() const                          { return type() == TYPE_MAP; }

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
  Object&             get_key(const char* k);
  const Object&       get_key(const char* k) const;

  template <typename T> value_type&        get_key_value(const T& k)        { return get_key(k).as_value(); }
  template <typename T> const value_type&  get_key_value(const T& k) const  { return get_key(k).as_value(); }

  template <typename T> string_type&       get_key_string(const T& k)       { return get_key(k).as_string(); }
  template <typename T> const string_type& get_key_string(const T& k) const { return get_key(k).as_string(); }

  template <typename T> list_type&         get_key_list(const T& k)         { return get_key(k).as_list(); }
  template <typename T> const list_type&   get_key_list(const T& k) const   { return get_key(k).as_list(); }

  template <typename T> map_type&          get_key_map(const T& k)          { return get_key(k).as_map(); }
  template <typename T> const map_type&    get_key_map(const T& k) const    { return get_key(k).as_map(); }

  Object&             insert_key(const key_type& k, const Object& b) { check_throw(TYPE_MAP); return (*m_map)[k] = b; }

  // 'insert_preserve_*' inserts the object 'b' if the key 'k' does
  // not exist, else it returns the old entry. The type specific
  // versions also require the old entry to be of the same type.
  //
  // Consider making insert_preserve_* return std::pair<Foo*,bool> or
  // something similar.
  map_insert_type     insert_preserve_any(const key_type& k, const Object& b) { check_throw(TYPE_MAP); return m_map->insert(map_type::value_type(k, b)); }
  map_insert_type     insert_preserve_type(const key_type& k, Object& b);
  map_insert_type     insert_preserve_copy(const key_type& k, Object b) { return insert_preserve_type(k, b); }

  void                erase_key(const key_type& k)                   { check_throw(TYPE_MAP); m_map->erase(k); }

  Object&             insert_front(const Object& b)                  { check_throw(TYPE_LIST); return *m_list->insert(m_list->begin(), b); }
  Object&             insert_back(const Object& b)                   { check_throw(TYPE_LIST); return *m_list->insert(m_list->end(), b); }

  // Copy and merge operations:
  Object&             move(Object& b);
  Object&             swap(Object& b);

  // Only map entries are merged.
  Object&             merge_move(Object object, uint32_t maxDepth = ~uint32_t());
  Object&             merge_copy(const Object& object, uint32_t maxDepth = ~uint32_t());

  Object&             operator = (const Object& b);

 private:
  // TMP to kill bad uses.
  explicit Object(type_type t);

  inline bool         check(map_type::const_iterator itr, type_type t) const { return itr != m_map->end() && itr->second.type() == t; }
  inline void         check_throw(type_type t) const                         { if (t != type()) throw bencode_error("Wrong object type."); }

  uint32_t            m_flags;

  union {
    int64_t             m_value;
    string_type*        m_string;
    list_type*          m_list;
    map_type*           m_map;
  };
};

inline
Object::Object(const Object& b) : m_flags(b.type()) {
  switch (type()) {
  case TYPE_NONE:   break;
  case TYPE_VALUE:  m_value = b.m_value; break;
  case TYPE_STRING: m_string = new string_type(*b.m_string); break;
  case TYPE_LIST:   m_list = new list_type(*b.m_list); break;
  case TYPE_MAP:    m_map = new map_type(*b.m_map);  break;
  }
}

inline void
Object::clear() {
  switch (type()) {
  case TYPE_NONE:
  case TYPE_VALUE:  break;
  case TYPE_STRING: delete m_string; break;
  case TYPE_LIST:   delete m_list; break;
  case TYPE_MAP:    delete m_map; break;
  }

  // Only clear type?
  m_flags = TYPE_NONE;
}

}

#endif
