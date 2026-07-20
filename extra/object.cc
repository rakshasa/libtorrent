#include "config.h"

#include <algorithm>

#include "object.h"

namespace torrent {

Object::Object(const Object& b) : m_state(b.type()) {
  switch (type()) {
  case type_none:   break;
  case type_value:  m_value = b.m_value; break;
  case type_string: m_string = new string_type(*b.m_string); break;
  case type_list:   m_list = new list_type(*b.m_list); break;
  case type_map:    m_map = new map_type(*b.m_map);  break;
  }
}

Object&
Object::operator = (const Object& src) {
  if (&src == this)
    return *this;

  clear();

  m_state = src.m_state;

  switch (type()) {
  case type_none:   break;
  case type_value:  m_value = src.m_value; break;
  case type_string: m_string = new string_type(*src.m_string); break;
  case type_list:   m_list = new list_type(*src.m_list); break;
  case type_map:    m_map = new map_type(*src.m_map);  break;
  }

  return *this;
}

void
Object::clear() {
  switch (type()) {
  case type_none:
  case type_value:  break;
  case type_string: delete m_string; break;
  case type_list:   delete m_list; break;
  case type_map:    delete m_map; break;
  }

  m_state = type_none;
}

Object&
Object::get_key(const std::string& k) {
  check_throw(type_map);

  map_type::iterator itr = m_map->find(k);

  if (itr == m_map->end())
    throw bencode_error("Object operator [" + k + "] could not find element");

  return itr->second;
}


const Object&
Object::get_key(const std::string& k) const {
  check_throw(type_map);

  map_type::const_iterator itr = m_map->find(k);

  if (itr == m_map->end())
    throw bencode_error("Object operator [" + k + "] could not find element");

  return itr->second;
}

Object&
Object::move(Object& src) {
  if (this == &src)
    return *this;

  clear();
  std::memcpy(this, &src, sizeof(Object));
  std::memset(&src, 0, sizeof(Object));

  return *this;
}

Object&
Object::swap(Object& src) {
  char tmp[sizeof(Object)];

  std::memcpy(tmp, &src, sizeof(Object));
  std::memcpy(&src, this, sizeof(Object));
  std::memcpy(this, tmp, sizeof(Object));

  return *this;
}

Object&
Object::merge_copy(const Object& object, uint32_t maxDepth) {
  if (maxDepth == 0)
    return (*this = object);

  if (object.is_map()) {
    if (!is_map())
      *this = Object(type_map);

    map_type& dest = as_map();
    map_type::iterator destItr = dest.begin();

    map_type::const_iterator srcItr = object.as_map().begin();
    map_type::const_iterator srcLast = object.as_map().end();

    while (srcItr != srcLast) {
      destItr = std::find_if(destItr, dest.end(), [&srcItr](const map_type::value_type& dest) {
          return srcItr->first <= dest.first;
        });

      if (srcItr->first < destItr->first)
        // destItr remains valid and pointing to the next possible
        // position.
        dest.insert(destItr, *srcItr);
      else
        destItr->second.merge_copy(srcItr->second, maxDepth - 1);

      srcItr++;
    }

  } else if (object.is_list()) {
    if (!is_list())
      *this = Object(type_list);

    list_type& dest = as_list();
    list_type::iterator destItr = dest.begin();

    list_type::const_iterator srcItr = object.as_list().begin();
    list_type::const_iterator srcLast = object.as_list().end();

    while (srcItr != srcLast) {
      if (destItr == dest.end())
        destItr = dest.insert(destItr, *srcItr);
      else
        destItr->merge_copy(*srcItr, maxDepth - 1);

      destItr++;
    }

  } else {
    *this = object;
  }

  return *this;
}

}
