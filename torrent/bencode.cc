#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "bencode.h"
#include "exceptions.h"
#include <iostream>

namespace torrent {

bencode::bencode() :
  m_type(TYPE_NONE)
{
}

bencode::bencode(const bencode& b) :
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

bencode::bencode(const int64_t v) :
  m_type(TYPE_VALUE),
  m_value(v)
{
}

bencode::bencode(const std::string& s) :
  m_type(TYPE_STRING),
  m_string(new std::string(s))
{
}

bencode::bencode(const char* s) :
  m_type(TYPE_STRING),
  m_string(new std::string(s)) {
}

void bencode::clear() {
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

bencode::~bencode() {
  clear();
}
  
bencode& bencode::operator = (const bencode& b) {
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

std::istream& operator >> (std::istream& s, bencode& b) {
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

    b.m_type = bencode::TYPE_VALUE;

    return s;

  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
    b.m_string = new std::string(c, '\0');
    b.m_type = bencode::TYPE_STRING;

    if (b.readString(s, b.m_string))
      return s;
    else
      break;

  case 'l':
    s.get();

    b.m_list = new bencode::List;
    b.m_type = bencode::TYPE_LIST;

    while (s.good()) {
      if (s.peek() == 'e') {
	s.get();
	return s;
      }

      bencode::List::iterator itr = b.m_list->insert(b.m_list->end(), bencode());

      s >> *itr;
    }

    break;
  case 'd':
    s.get();

    b.m_map = new bencode::Map;
    b.m_type = bencode::TYPE_MAP;

    while (s.good()) {
      if (s.peek() == 'e') {
	s.get();
	return s;
      }

      std::string str;

      if (!bencode::readString(s, &str))
	break;

      s >> (*b.m_map)[str];
    }

    break;
  default:
    break;
  }

  s.setstate(std::istream::failbit);
  b.clear();

  return s;
}

std::ostream& operator << (std::ostream& s, const bencode& b) {
  switch (b.m_type) {
  case bencode::TYPE_VALUE:
    return s << 'i' << b.m_value << 'e';
  case bencode::TYPE_STRING:
    return s << b.m_string->size() << ':' << *b.m_string;
  case bencode::TYPE_LIST:
    s << 'l';

    for (bencode::List::const_iterator itr = b.m_list->begin(); itr != b.m_list->end(); ++itr)
      s << *itr;

    return s << 'e';
  case bencode::TYPE_MAP:
    s << 'd';

    for (bencode::Map::const_iterator itr = b.m_map->begin(); itr != b.m_map->end(); ++itr)
      s << itr->first.size() << ':' << itr->first << itr->second;

    return s << 'e';
  default:
    break;
  }

  return s;
}
    
bencode& bencode::operator [] (const std::string& k) {
  if (m_type != TYPE_MAP)
    throw bencode_error("bencode operator [" + k + "] called on wrong type");

  Map::iterator itr = m_map->find(k);

  if (itr == m_map->end())
    throw bencode_error("bencode operator [" + k + "] could not find element");

  return itr->second;
}


const bencode& bencode::operator [] (const std::string& k) const {
  if (m_type != TYPE_MAP)
    throw bencode_error("bencode operator [" + k + "] called on wrong type");

  Map::const_iterator itr = m_map->find(k);

  if (itr == m_map->end())
    throw bencode_error("bencode operator [" + k + "] could not find element");

  return itr->second;
}

bool bencode::hasKey(const std::string& s) const {
  return m_map->find(s) != m_map->end();
}

bool bencode::readString(std::istream& s, std::string* str) {
  int size;
  s >> size;

  if (s.fail() || s.get() != ':')
    return false;
  
  str->resize(size);

  for (std::string::iterator itr = str->begin(); itr != str->end() && s.good(); ++itr)
    *itr = s.get();
  
  return !s.fail();
}

int64_t& bencode::asValue() {
  if (!isValue())
    throw bencode_error("Bencode is not type value");

  return m_value;
}

int64_t bencode::asValue() const {
  if (!isValue())
    throw bencode_error("Bencode is not type value");

  return m_value;
}

std::string& bencode::asString() {
  if (!isString())
    throw bencode_error("Bencode is not type string");

  return *m_string;
}

const std::string& bencode::asString() const {
  if (!isString())
    throw bencode_error("Bencode is not type string");

  return *m_string;
}

bencode::List& bencode::asList() {
  if (!isList())
    throw bencode_error("Bencode is not type list");

  return *m_list;
}

const bencode::List& bencode::asList() const {
  if (!isList())
    throw bencode_error("Bencode is not type list");

  return *m_list;
}

bencode::Map& bencode::asMap() {
  if (!isMap())
    throw bencode_error("Bencode is not type map");

  return *m_map;
}

const bencode::Map& bencode::asMap() const {
  if (!isMap())
    throw bencode_error("Bencode is not type map");

  return *m_map;
}

} // namespace Torrent
