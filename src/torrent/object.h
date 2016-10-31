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

#ifndef LIBTORRENT_OBJECT_H
#define LIBTORRENT_OBJECT_H

#include <string>
#include <map>
#include <vector>
#include <torrent/common.h>
#include <torrent/exceptions.h>
#include <torrent/object_raw_bencode.h>

namespace torrent {

class LIBTORRENT_EXPORT Object {
public:
  typedef int64_t                           value_type;
  typedef std::string                       string_type;
  typedef std::vector<Object>               list_type;
  typedef std::map<std::string, Object>     map_type;
  typedef map_type*                         map_ptr_type;
  typedef map_type::key_type                key_type;
  typedef std::pair<std::string, Object*>   dict_key_type;

  typedef list_type::iterator               list_iterator;
  typedef list_type::const_iterator         list_const_iterator;
  typedef list_type::reverse_iterator       list_reverse_iterator;
  typedef list_type::const_reverse_iterator list_const_reverse_iterator;

  typedef map_type::iterator                map_iterator;
  typedef map_type::const_iterator          map_const_iterator;
  typedef map_type::reverse_iterator        map_reverse_iterator;
  typedef map_type::const_reverse_iterator  map_const_reverse_iterator;

  typedef std::pair<map_iterator, bool>     map_insert_type;

  // Flags in the range of 0xffff0000 may be set by the user, however
  // 0x00ff0000 are reserved for keywords defined by libtorrent.
  static const uint32_t mask_type     = 0xff;
  static const uint32_t mask_flags    = ~mask_type;
  static const uint32_t mask_internal = 0xffff;
  static const uint32_t mask_public   = ~mask_internal;

  static const uint32_t flag_unordered    = 0x100;    // bencode dictionary was not sorted
  static const uint32_t flag_static_data  = 0x010000;  // Object does not change across sessions.
  static const uint32_t flag_session_data = 0x020000;  // Object changes between sessions.
  static const uint32_t flag_function     = 0x040000;  // A function object.
  static const uint32_t flag_function_q1  = 0x080000;  // A quoted function object.
  static const uint32_t flag_function_q2  = 0x100000;  // A double-quoted function object.

  static const uint32_t mask_function     = 0x1C0000;  // Mask for function objects.

  enum type_type {
    TYPE_NONE,
    TYPE_RAW_BENCODE,
    TYPE_RAW_STRING,
    TYPE_RAW_LIST,
    TYPE_RAW_MAP,
    TYPE_VALUE,
    TYPE_STRING,
    TYPE_LIST,
    TYPE_MAP,
    TYPE_DICT_KEY
  };

  Object()                     : m_flags(TYPE_NONE) {}
  Object(const value_type v)   : m_flags(TYPE_VALUE) { new (&_value()) value_type(v); }
  Object(const char* s)        : m_flags(TYPE_STRING) { new (&_string()) string_type(s); }
  Object(const string_type& s) : m_flags(TYPE_STRING) { new (&_string()) string_type(s); }
  Object(const raw_bencode& r) : m_flags(TYPE_RAW_BENCODE) { new (&_raw_bencode()) raw_bencode(r); }
  Object(const raw_string& r)  : m_flags(TYPE_RAW_STRING) { new (&_raw_string()) raw_string(r); }
  Object(const raw_list& r)    : m_flags(TYPE_RAW_LIST) { new (&_raw_list()) raw_list(r); }
  Object(const raw_map& r)     : m_flags(TYPE_RAW_MAP) { new (&_raw_map()) raw_map(r); }
  Object(const Object& b);

  ~Object() { clear(); }

  // TODO: Move this out of the class namespace, call them
  // make_object_. 
  static Object       create_empty(type_type t);
  static Object       create_value()  { return Object(value_type()); }
  static Object       create_string() { return Object(string_type()); }
  static Object       create_list()   { Object tmp; tmp.m_flags = TYPE_LIST; new (&tmp._list()) list_type(); return tmp; }
  static Object       create_map()    { Object tmp; tmp.m_flags = TYPE_MAP; tmp._map_ptr() = new map_type(); return tmp; }
  static Object       create_dict_key();

  static Object       create_raw_bencode(raw_bencode obj = raw_bencode());
  static Object       create_raw_string(raw_string obj = raw_string());
  static Object       create_raw_list(raw_list obj = raw_list());
  static Object       create_raw_map(raw_map obj = raw_map());

  template <typename ForwardIterator>
  static Object       create_list_range(ForwardIterator first, ForwardIterator last);

  static Object       from_list(const list_type& src) { Object tmp; tmp.m_flags = TYPE_LIST; new (&tmp._list()) list_type(src); return tmp; }

  // Clear should probably not be inlined due to size and not being
  // optimized away in pretty much any case. Might not work well in
  // cases where we pass constant rvalues.
  void                clear();

  type_type           type() const                            { return (type_type)(m_flags & mask_type); }
  uint32_t            flags() const                           { return m_flags & mask_flags; }

  void                set_flags(uint32_t f)                   { m_flags |= f & mask_public; }
  void                unset_flags(uint32_t f)                 { m_flags &= ~(f & mask_public); }

  void                set_internal_flags(uint32_t f)          { m_flags |= f & (mask_internal & ~mask_type); }
  void                unset_internal_flags(uint32_t f)        { m_flags &= ~(f & (mask_internal & ~mask_type)); }

  // Add functions for setting/clearing the public flags.

  bool                is_empty() const                        { return type() == TYPE_NONE; }
  bool                is_not_empty() const                    { return type() != TYPE_NONE; }
  bool                is_value() const                        { return type() == TYPE_VALUE; }
  bool                is_string() const                       { return type() == TYPE_STRING; }
  bool                is_string_empty() const                 { return type() != TYPE_STRING || _string().empty(); }
  bool                is_list() const                         { return type() == TYPE_LIST; }
  bool                is_map() const                          { return type() == TYPE_MAP; }
  bool                is_dict_key() const                     { return type() == TYPE_DICT_KEY; }
  bool                is_raw_bencode() const                  { return type() == TYPE_RAW_BENCODE; }
  bool                is_raw_string() const                   { return type() == TYPE_RAW_STRING; }
  bool                is_raw_list() const                     { return type() == TYPE_RAW_LIST; }
  bool                is_raw_map() const                      { return type() == TYPE_RAW_MAP; }

  value_type&         as_value()                              { check_throw(TYPE_VALUE); return _value(); }
  const value_type&   as_value() const                        { check_throw(TYPE_VALUE); return _value(); }
  string_type&        as_string()                             { check_throw(TYPE_STRING); return _string(); }
  const string_type&  as_string() const                       { check_throw(TYPE_STRING); return _string(); }
  const string_type&  as_string_c() const                     { check_throw(TYPE_STRING); return _string(); }
  list_type&          as_list()                               { check_throw(TYPE_LIST); return _list(); }
  const list_type&    as_list() const                         { check_throw(TYPE_LIST); return _list(); }
  map_type&           as_map()                                { check_throw(TYPE_MAP); return _map(); }
  const map_type&     as_map() const                          { check_throw(TYPE_MAP); return _map(); }
  string_type&        as_dict_key()                           { check_throw(TYPE_DICT_KEY); return _dict_key().first; }
  const string_type&  as_dict_key() const                     { check_throw(TYPE_DICT_KEY); return _dict_key().first; }
  Object&             as_dict_obj()                           { check_throw(TYPE_DICT_KEY); return *_dict_key().second; }
  const Object&       as_dict_obj() const                     { check_throw(TYPE_DICT_KEY); return *_dict_key().second; }
  raw_bencode&        as_raw_bencode()                        { check_throw(TYPE_RAW_BENCODE); return _raw_bencode(); }
  const raw_bencode&  as_raw_bencode() const                  { check_throw(TYPE_RAW_BENCODE); return _raw_bencode(); }
  raw_string&         as_raw_string()                         { check_throw(TYPE_RAW_STRING); return _raw_string(); }
  const raw_string&   as_raw_string() const                   { check_throw(TYPE_RAW_STRING); return _raw_string(); }
  raw_list&           as_raw_list()                           { check_throw(TYPE_RAW_LIST); return _raw_list(); }
  const raw_list&     as_raw_list() const                     { check_throw(TYPE_RAW_LIST); return _raw_list(); }
  raw_map&            as_raw_map()                            { check_throw(TYPE_RAW_MAP); return _raw_map(); }
  const raw_map&      as_raw_map() const                      { check_throw(TYPE_RAW_MAP); return _raw_map(); }

  bool                has_key(const key_type& k) const        { check_throw(TYPE_MAP); return _map().find(k) != _map().end(); }
  bool                has_key_value(const key_type& k) const  { check_throw(TYPE_MAP); return check(_map().find(k), TYPE_VALUE); }
  bool                has_key_string(const key_type& k) const { check_throw(TYPE_MAP); return check(_map().find(k), TYPE_STRING); }
  bool                has_key_list(const key_type& k) const   { check_throw(TYPE_MAP); return check(_map().find(k), TYPE_LIST); }
  bool                has_key_map(const key_type& k) const    { check_throw(TYPE_MAP); return check(_map().find(k), TYPE_MAP); }
  bool                has_key_raw_bencode(const key_type& k) const { check_throw(TYPE_MAP); return check(_map().find(k), TYPE_RAW_BENCODE); }
  bool                has_key_raw_string(const key_type& k) const { check_throw(TYPE_MAP); return check(_map().find(k), TYPE_RAW_STRING); }
  bool                has_key_raw_list(const key_type& k) const { check_throw(TYPE_MAP); return check(_map().find(k), TYPE_RAW_LIST); }
  bool                has_key_raw_map(const key_type& k) const { check_throw(TYPE_MAP); return check(_map().find(k), TYPE_RAW_MAP); }

  // Should have an interface for that returns pointer or something,
  // so we don't need to search twice.

  // Make these inline...

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

  Object&             insert_key(const key_type& k, const Object& b) { check_throw(TYPE_MAP); return _map()[k] = b; }
  Object&             insert_key_move(const key_type& k, Object& b)  { check_throw(TYPE_MAP); return _map()[k].move(b); }

  // 'insert_preserve_*' inserts the object 'b' if the key 'k' does
  // not exist, else it returns the old entry. The type specific
  // versions also require the old entry to be of the same type.
  //
  // Consider making insert_preserve_* return std::pair<Foo*,bool> or
  // something similar.
  map_insert_type     insert_preserve_any(const key_type& k, const Object& b) { check_throw(TYPE_MAP); return _map().insert(map_type::value_type(k, b)); }
  map_insert_type     insert_preserve_type(const key_type& k, Object& b);
  map_insert_type     insert_preserve_copy(const key_type& k, Object b) { return insert_preserve_type(k, b); }

  void                erase_key(const key_type& k)                   { check_throw(TYPE_MAP); _map().erase(k); }

  Object&             insert_front(const Object& b)                  { check_throw(TYPE_LIST); return *_list().insert(_list().begin(), b); }
  Object&             insert_back(const Object& b)                   { check_throw(TYPE_LIST); return *_list().insert(_list().end(), b); }

  // Copy and merge operations:
  Object&             move(Object& b);
  Object&             swap(Object& b);
  Object&             swap_same_type(Object& b);

  // Only map entries are merged.
  Object&             merge_move(Object& object, uint32_t maxDepth = ~uint32_t());
  Object&             merge_copy(const Object& object,
                                 uint32_t skip_mask = flag_static_data,
                                 uint32_t maxDepth = ~uint32_t());

  Object&             operator = (const Object& b);

  // Internal:
  void                swap_same_type(Object& left, Object& right);

 private:
  inline bool         check(map_type::const_iterator itr, type_type t) const { return itr != _map().end() && itr->second.type() == t; }
  inline void         check_throw(type_type t) const                         { if (t != type()) throw bencode_error("Wrong object type."); }

  uint32_t            m_flags;

#ifndef HAVE_STDCXX_0X
  value_type&         _value()             { return t_value; }
  const value_type&   _value() const       { return t_value; }
  string_type&        _string()            { return t_string; }
  const string_type&  _string() const      { return t_string; }
  list_type&          _list()              { return t_list; }
  const list_type&    _list() const        { return t_list; }
  map_type&           _map()               { return *t_map; }
  const map_type&     _map() const         { return *t_map; }
  map_ptr_type&       _map_ptr()           { return t_map; }
  const map_ptr_type& _map_ptr() const     { return t_map; }
  dict_key_type&       _dict_key()         { return t_dict_key; }
  const dict_key_type& _dict_key() const   { return t_dict_key; }
  raw_object&         _raw_object()        { return t_raw_object; }
  const raw_object&   _raw_object() const  { return t_raw_object; }
  raw_bencode&        _raw_bencode()       { return t_raw_bencode; }
  const raw_bencode&  _raw_bencode() const { return t_raw_bencode; }
  raw_string&         _raw_string()        { return t_raw_string; }
  const raw_string&   _raw_string() const  { return t_raw_string; }
  raw_list&           _raw_list()          { return t_raw_list; }
  const raw_list&     _raw_list() const    { return t_raw_list; }
  raw_map&            _raw_map()           { return t_raw_map; }
  const raw_map&      _raw_map() const     { return t_raw_map; }

  union pod_types {
    value_type    t_value;
    raw_object    t_raw_object;
    raw_bencode   t_raw_bencode;
    raw_string    t_raw_string;
    raw_list      t_raw_list;
    raw_map       t_raw_map;
  };

  union {
    pod_types     t_pod;

    value_type    t_value;
    string_type   t_string;
    list_type     t_list;
    map_type*     t_map;
    dict_key_type t_dict_key;
    raw_object    t_raw_object;
    raw_bencode   t_raw_bencode;
    raw_string    t_raw_string;
    raw_list      t_raw_list;
    raw_map       t_raw_map;
  };

#else
  // #error "WTF we're testing C++11 now."

  value_type&         _value()             { return reinterpret_cast<value_type&>(t_pod); }
  const value_type&   _value() const       { return reinterpret_cast<const value_type&>(t_pod); }
  string_type&        _string()            { return reinterpret_cast<string_type&>(t_string); }
  const string_type&  _string() const      { return reinterpret_cast<const string_type&>(t_string); }
  list_type&          _list()              { return reinterpret_cast<list_type&>(t_list); }
  const list_type&    _list() const        { return reinterpret_cast<const list_type&>(t_list); }
  map_type&           _map()               { return *reinterpret_cast<map_ptr_type&>(t_pod); }
  const map_type&     _map() const         { return *reinterpret_cast<const map_ptr_type&>(t_pod); }
  map_ptr_type&       _map_ptr()           { return reinterpret_cast<map_ptr_type&>(t_pod); }
  const map_ptr_type& _map_ptr() const     { return reinterpret_cast<const map_ptr_type&>(t_pod); }
  dict_key_type&       _dict_key()         { return reinterpret_cast<dict_key_type&>(t_pod); }
  const dict_key_type& _dict_key() const   { return reinterpret_cast<const dict_key_type&>(t_pod); }
  raw_object&         _raw_object()        { return reinterpret_cast<raw_object&>(t_pod); }
  const raw_object&   _raw_object() const  { return reinterpret_cast<const raw_object&>(t_pod); }
  raw_bencode&        _raw_bencode()       { return reinterpret_cast<raw_bencode&>(t_pod); }
  const raw_bencode&  _raw_bencode() const { return reinterpret_cast<const raw_bencode&>(t_pod); }
  raw_string&         _raw_string()        { return reinterpret_cast<raw_string&>(t_pod); }
  const raw_string&   _raw_string() const  { return reinterpret_cast<const raw_string&>(t_pod); }
  raw_list&           _raw_list()          { return reinterpret_cast<raw_list&>(t_pod); }
  const raw_list&     _raw_list() const    { return reinterpret_cast<const raw_list&>(t_pod); }
  raw_map&            _raw_map()           { return reinterpret_cast<raw_map&>(t_pod); }
  const raw_map&      _raw_map() const     { return reinterpret_cast<const raw_map&>(t_pod); }

  union pod_types {
    value_type    t_value;
    map_type*     t_map;
    char          t_raw_object[sizeof(raw_object)];
  };

  union {
    pod_types t_pod;
    char      t_string[sizeof(string_type)];
    char      t_list[sizeof(list_type)];
    char      t_dict_key[sizeof(dict_key_type)];
  };
#endif
};

inline
Object::Object(const Object& b) {
  m_flags = b.m_flags & (mask_type | mask_public);

  switch (type()) {
  case TYPE_NONE:
  case TYPE_RAW_BENCODE:
  case TYPE_RAW_STRING: 
  case TYPE_RAW_LIST: 
  case TYPE_RAW_MAP:
  case TYPE_VALUE:       t_pod = b.t_pod; break;
  case TYPE_STRING:      new (&_string()) string_type(b._string()); break;
  case TYPE_LIST:        new (&_list()) list_type(b._list()); break;
  case TYPE_MAP:         _map_ptr() = new map_type(b._map()); break;
  case TYPE_DICT_KEY:
    new (&_dict_key().first) string_type(b._dict_key().first);
    _dict_key().second = new Object(*b._dict_key().second); break;
  }
}

inline Object
Object::create_empty(type_type t) {
  switch (t) {
  case TYPE_RAW_BENCODE: return create_raw_bencode();
  case TYPE_RAW_STRING:  return create_raw_string();
  case TYPE_RAW_LIST:    return create_raw_list();
  case TYPE_RAW_MAP:     return create_raw_map();
  case TYPE_VALUE:       return create_value();
  case TYPE_STRING:      return create_string();
  case TYPE_LIST:        return create_list();
  case TYPE_MAP:         return create_map();
  case TYPE_DICT_KEY:    return create_dict_key();
  case TYPE_NONE:
  default: return torrent::Object();
  }
}

inline Object
object_create_raw_bencode_c_str(const char str[]) {
  return Object::create_raw_bencode(raw_bencode(str, strlen(str)));
}

// TODO: These do not preserve the flag...

Object object_create_normal(const raw_bencode& obj) LIBTORRENT_EXPORT;
Object object_create_normal(const raw_list& obj) LIBTORRENT_EXPORT;
Object object_create_normal(const raw_map& obj) LIBTORRENT_EXPORT;
inline Object object_create_normal(const raw_string& obj) { return torrent::Object(obj.as_string()); }

inline Object
Object::create_dict_key() {
  Object tmp;
  tmp.m_flags = TYPE_DICT_KEY;
  new (&tmp._dict_key()) dict_key_type();
  tmp._dict_key().second = new Object();
  return tmp;
}

inline Object
Object::create_raw_bencode(raw_bencode obj) {
  Object tmp; tmp.m_flags = TYPE_RAW_BENCODE; new (&tmp._raw_bencode()) raw_bencode(obj); return tmp;
}

inline Object
Object::create_raw_string(raw_string obj) {
  Object tmp; tmp.m_flags = TYPE_RAW_STRING; new (&tmp._raw_string()) raw_string(obj); return tmp;
}

inline Object
Object::create_raw_list(raw_list obj) {
  Object tmp; tmp.m_flags = TYPE_RAW_LIST; new (&tmp._raw_list()) raw_list(obj); return tmp;
}

inline Object
Object::create_raw_map(raw_map obj) {
  Object tmp; tmp.m_flags = TYPE_RAW_MAP; new (&tmp._raw_map()) raw_map(obj); return tmp;
}

inline Object
object_create_normal(const Object& obj) {
  switch (obj.type()) {
  case Object::TYPE_RAW_BENCODE: return object_create_normal(obj.as_raw_bencode());
  case Object::TYPE_RAW_STRING:  return object_create_normal(obj.as_raw_string());
  case Object::TYPE_RAW_LIST:    return object_create_normal(obj.as_raw_list());
  case Object::TYPE_RAW_MAP:     return object_create_normal(obj.as_raw_map());
  default: return obj;
  }
}

inline std::string
object_create_string(const torrent::Object& obj) {
  switch (obj.type()) {
  case Object::TYPE_RAW_BENCODE: return obj.as_raw_bencode().as_raw_string().as_string();
  case Object::TYPE_RAW_STRING:  return obj.as_raw_string().as_string();
  default: return obj.as_string();
  }
}

template <typename ForwardIterator>
inline Object
Object::create_list_range(ForwardIterator first, ForwardIterator last) {
  Object tmp; tmp.m_flags = TYPE_LIST; new (&tmp._list()) list_type(first, last); return tmp;
}

inline void
Object::clear() {
  switch (type()) {
  case TYPE_STRING:   _string().~string_type(); break;
  case TYPE_LIST:     _list().~list_type(); break;
  case TYPE_MAP:      delete _map_ptr(); break;
  case TYPE_DICT_KEY: delete _dict_key().second; _dict_key().~dict_key_type(); break;
  default: break;
  }

  // Only clear type?
  m_flags = TYPE_NONE;
}

inline void
Object::swap_same_type(Object& left, Object& right) {
  std::swap(left.m_flags, right.m_flags);

  switch (left.type()) {
  case Object::TYPE_STRING:   left._string().swap(right._string()); break;
  case Object::TYPE_LIST:     left._list().swap(right._list()); break;
  case Object::TYPE_DICT_KEY:
    std::swap(left._dict_key().first, right._dict_key().first);
    std::swap(left._dict_key().second, right._dict_key().second); break;
  default: std::swap(left.t_pod, right.t_pod); break;
  }
}

inline void swap(Object& left, Object& right) { left.swap(right); }

inline bool
object_equal(const Object& left, const Object& right) {
  if (left.type() != right.type())
    return false;

  switch (left.type()) {
  case Object::TYPE_NONE:        return true;
  case Object::TYPE_VALUE:       return left.as_value() == right.as_value();
  case Object::TYPE_STRING:      return left.as_string() == right.as_string();
  default: return false;
  }
}

}

#endif
