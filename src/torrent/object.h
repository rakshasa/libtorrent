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

// The Object class can hold various kinds of types like integer,
// string, list and map. By not using pointers to the list and map
// containers it avoids a layer of indirection. An alternate
// WeakObject class may contain either a reference or an instance,
// thus allowing for efficiently passing values in return statements.

#ifndef LIBTORRENT_OBJECT_H
#define LIBTORRENT_OBJECT_H

#include <inttypes.h>
#include <list>
#include <map>
#include <string>

#include <torrent/exceptions.h>

#define USE_OBJECT_INLINE inline

namespace torrent {

class Object;
class WeakObject;

// Need to mangle this class name.
class BaseObject {
public:
  // Ref, const char*?, etc.
  enum type_type {
    TYPE_NONE       = 0x00,
    TYPE_VALUE      = 0x01,
    TYPE_STRING     = 0x02,
    TYPE_MAP        = 0x03,
    TYPE_LIST       = 0x04,

    TYPE_STRING_REF = 0x12,
    TYPE_MAP_REF    = 0x13,
    TYPE_LIST_REF   = 0x14
  };

  typedef int64_t                       value_type;
  typedef std::string                   string_type;
  typedef std::map<std::string, Object> map_type;
  typedef std::list<Object>             list_type;

  typedef const string_type*            string_ref_type;
  typedef const map_type*               map_ref_type;
  typedef const list_type*              list_ref_type;

protected:
  
  BaseObject() {}

  void                construct_none()                      { m_type = TYPE_NONE; }
  void                construct_value()                     { m_type = TYPE_VALUE; new (m_data) value_type(); }
  void                construct_string()                    { m_type = TYPE_STRING; new (m_data) string_type(); }
  void                construct_map()                       { m_type = TYPE_MAP; new (m_data) map_type(); }
  void                construct_list()                      { m_type = TYPE_LIST; new (m_data) list_type(); }

  void                copy_value(value_type src)            { m_type = TYPE_VALUE; new (m_data) value_type(src); }
  void                copy_string(const string_type& src)   { m_type = TYPE_STRING; new (m_data) string_type(src); }
  void                copy_map(const map_type& src)         { m_type = TYPE_MAP; new (m_data) map_type(src); }
  void                copy_list(const list_type& src)       { m_type = TYPE_LIST; new (m_data) list_type(src); }

  void                copy_string_ref(string_ref_type src)  { m_type = TYPE_STRING_REF; ref_string_ref() = src; }
  void                copy_map_ref(map_ref_type src)        { m_type = TYPE_MAP_REF; ref_map_ref() = src; }
  void                copy_list_ref(list_ref_type src)      { m_type = TYPE_LIST_REF; ref_list_ref() = src; }

  void                copy(const BaseObject& src);
  void                weak_copy(const BaseObject& src);

  void                destroy();
  
  bool                is_ref() const                        { return m_type > 0x10; }

  // Find a better name.
  type_type           original_type() const                 { return m_type; }
  type_type           base_type() const                     { return (type_type)(m_type & 0x0f); }

  // Cast the m_data buffer to the preferred type.
  value_type&         ref_value()                           { return *reinterpret_cast<value_type*>(m_data); }
  string_type&        ref_string()                          { return *reinterpret_cast<string_type*>(m_data); }
  map_type&           ref_map()                             { return *reinterpret_cast<map_type*>(m_data); }
  list_type&          ref_list()                            { return *reinterpret_cast<list_type*>(m_data); }

  string_ref_type&    ref_string_ref()                      { return *reinterpret_cast<string_ref_type*>(m_data); }
  map_ref_type&       ref_map_ref()                         { return *reinterpret_cast<map_ref_type*>(m_data); }
  list_ref_type&      ref_list_ref()                        { return *reinterpret_cast<list_ref_type*>(m_data); }

  const value_type&   ref_value() const                     { return *reinterpret_cast<const value_type*>(m_data); }
  const string_type&  ref_string() const                    { return *reinterpret_cast<const string_type*>(m_data); }
  const map_type&     ref_map() const                       { return *reinterpret_cast<const map_type*>(m_data); }
  const list_type&    ref_list() const                      { return *reinterpret_cast<const list_type*>(m_data); }

  const string_ref_type& ref_string_ref() const             { return *reinterpret_cast<const string_ref_type*>(m_data); }
  const map_ref_type&    ref_map_ref() const                { return *reinterpret_cast<const map_ref_type*>(m_data); }
  const list_ref_type&   ref_list_ref() const               { return *reinterpret_cast<const list_ref_type*>(m_data); }

  void                check_type(type_type t) const         { if (t != m_type) throw torrent::bencode_error("Invalid Object type."); }

  // Returns true if the type is a reference.
  bool                check_weak_type(type_type t) const {
    if (t + 0x10 == m_type)
      return true;
    else if (t != m_type)
      throw torrent::bencode_error("Invalid Object type.");
    else
      return false;
  }

private:
  template <typename T, typename Next>
  struct msof {
    static const size_t value = sizeof(T) >= Next::value ? sizeof(T) : Next::value;
  };

  template <typename T>
  struct msof<T, void> {
    static const size_t value = sizeof(T);
  };

  static const size_t max_size = msof<value_type, msof<string_type, msof<map_type, msof<list_type, void> > > >::value;

  BaseObject(const BaseObject& src);
  BaseObject& operator = (const BaseObject& src);

  type_type           m_type;
  char                m_data[max_size];
};

// The main Object class, which always stores an instance of the type.
class Object : public BaseObject {
public:
  Object()                       { construct_none(); }
  Object(const Object& src);
  Object(const WeakObject& src);

  Object(value_type src)         { copy_value(src); }
  Object(const string_type& src) { copy_string(src); }
  Object(const map_type& src)    { copy_map(src); }

  // This needs to be inlined if we're to guess the type, though maybe
  // it would be best to not allow this kind of ctor?
  Object(type_type t);
  ~Object();

  Object&             operator = (const Object& src);
  Object&             operator = (const WeakObject& src);

  type_type           type() const         { return base_type(); }

  value_type&         as_value()           { check_type(TYPE_VALUE); return ref_value(); }
  string_type&        as_string()          { check_type(TYPE_STRING); return ref_string(); }
  map_type&           as_map()             { check_type(TYPE_MAP); return ref_map(); }

  const value_type&   as_value() const     { check_type(TYPE_VALUE); return ref_value(); }
  const string_type&  as_string() const    { check_type(TYPE_STRING); return ref_string(); }
  const map_type&     as_map() const       { check_type(TYPE_MAP); return ref_map(); }
};

class WeakObject : public BaseObject {
public:
  WeakObject()                       { construct_none(); }
  WeakObject(const Object& src);
  WeakObject(const WeakObject& src);

  WeakObject(value_type src)         { copy_value(src); }
  WeakObject(const string_type& src) { copy_string(src); }

  WeakObject(type_type t);
  ~WeakObject();

  WeakObject&         operator = (const Object& src);
  WeakObject&         operator = (const WeakObject& src);

  type_type           type() const         { return base_type(); }

  const value_type&   as_value() const     { check_type(TYPE_VALUE); return ref_value(); }
  const string_type&  as_string() const    { return check_weak_type(TYPE_STRING) ? *ref_string_ref() : ref_string(); }
};

USE_OBJECT_INLINE
Object::Object(const Object& src) { copy(src); }

USE_OBJECT_INLINE
Object::Object(const WeakObject& src) { copy(src); }

USE_OBJECT_INLINE
Object::~Object() { destroy(); }

USE_OBJECT_INLINE Object&
Object::operator = (const Object& src) { destroy(); copy(src); return *this; }

USE_OBJECT_INLINE Object&
Object::operator = (const WeakObject& src) { destroy(); copy(src); return *this; }

USE_OBJECT_INLINE
WeakObject::WeakObject(const Object& src) { weak_copy(src); }

USE_OBJECT_INLINE
WeakObject::WeakObject(const WeakObject& src) { weak_copy(src); }

USE_OBJECT_INLINE
WeakObject::~WeakObject() { destroy(); }

USE_OBJECT_INLINE WeakObject&
WeakObject::operator = (const Object& src) { destroy(); weak_copy(src); return *this; }

USE_OBJECT_INLINE WeakObject&
WeakObject::operator = (const WeakObject& src) { destroy(); weak_copy(src); return *this; }

}

#endif
  
