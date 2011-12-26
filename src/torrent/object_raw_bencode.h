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

#ifndef LIBTORRENT_OBJECT_RAW_BENCODE_H
#define LIBTORRENT_OBJECT_RAW_BENCODE_H

#include <algorithm>
#include <string>
#include <cstring>
#include <torrent/common.h>

namespace torrent {

class raw_bencode;
class raw_string;
class raw_list;
class raw_map;

// The base class for static constant version of Objects. This class
// should never be used directly.
class raw_object {
public:
  typedef const char  value_type;
  typedef const char* iterator;
  typedef const char* const_iterator;
  typedef uint32_t    size_type;

  raw_object() : m_data(), m_size() {}
  raw_object(value_type* src_data, size_type src_size) : m_data(src_data), m_size(src_size) {}

  bool        empty() const { return m_size == 0; }
  size_type   size() const { return m_size; }

  iterator    begin() const { return m_data; }
  iterator    end() const { return m_data + m_size; }

  value_type* data() const { return m_data; }

  bool operator == (const raw_object& rhs) const { return m_size == rhs.m_size && std::memcmp(m_data, rhs.m_data, m_size) == 0; }
  bool operator != (const raw_object& rhs) const { return m_size != rhs.m_size || std::memcmp(m_data, rhs.m_data, m_size) != 0; }

protected:
  iterator  m_data;
  size_type m_size;
};

#define RAW_BENCODE_SET_USING                   \
  using raw_object::value_type;                 \
  using raw_object::iterator;                   \
  using raw_object::const_iterator;             \
  using raw_object::size_type;                  \
  using raw_object::empty;                      \
  using raw_object::size;                       \
  using raw_object::begin;                      \
  using raw_object::end;                        \
  using raw_object::data;                       \
                                                \
  bool operator == (const this_type& rhs) const { return raw_object::operator==(rhs); } \
  bool operator != (const this_type& rhs) const { return raw_object::operator!=(rhs); } \

// A raw_bencode object shall always contain valid bencode data or be
// empty.
class raw_bencode : protected raw_object {
public:
  typedef raw_bencode this_type;
  RAW_BENCODE_SET_USING

  raw_bencode() : raw_object() {}
  raw_bencode(value_type* src_data, size_type src_size) : raw_object(src_data, src_size) {}

  bool        is_empty() const      { return m_size == 0; }
  bool        is_value() const      { return m_size >= 3 && m_data[0] >= 'i'; }
  bool        is_raw_string() const { return m_size >= 2 && m_data[0] >= '0' && m_data[0] <= '9'; }
  bool        is_raw_list() const   { return m_size >= 2 && m_data[0] >= 'l'; }
  bool        is_raw_map() const    { return m_size >= 2 && m_data[0] >= 'd'; }

  std::string as_value_string() const;
  raw_string  as_raw_string() const;
  raw_list    as_raw_list() const;
  raw_map     as_raw_map() const;

  static raw_bencode from_c_str(const char* str) { return raw_bencode(str, std::strlen(str)); }
};

class raw_string : protected raw_object {
public:
  typedef raw_string this_type;
  RAW_BENCODE_SET_USING

  raw_string() {}
  raw_string(value_type* src_data, size_type src_size) : raw_object(src_data, src_size) {}

  std::string as_string() const { return std::string(m_data, m_size); }

  static raw_string from_c_str(const char* str) { return raw_string(str, std::strlen(str)); }
  static raw_string from_string(const std::string& str) { return raw_string(str.data(), str.size()); }
};

class raw_list : protected raw_object {
public:
  typedef raw_list this_type;
  RAW_BENCODE_SET_USING

  raw_list() {}
  raw_list(value_type* src_data, size_type src_size) : raw_object(src_data, src_size) {}

  static raw_list from_c_str(const char* str) { return raw_list(str, std::strlen(str)); }
};

class raw_map : protected raw_object {
public:
  typedef raw_map this_type;
  RAW_BENCODE_SET_USING

  raw_map() {}
  raw_map(value_type* src_data, size_type src_size) : raw_object(src_data, src_size) {}
};

//
//
//

inline std::string
raw_bencode::as_value_string() const {
  if (!is_value())
    throw bencode_error("Wrong object type.");

  return std::string(data() + 1, size() - 2);
}

inline raw_string
raw_bencode::as_raw_string() const {
  if (!is_raw_string())
    throw bencode_error("Wrong object type.");

  const_iterator itr = std::find(begin(), end(), ':');

  if (itr == end())
    throw internal_error("Invalid bencode in raw_bencode.");

  return raw_string(itr + 1, std::distance(itr + 1, end()));
}

inline raw_list
raw_bencode::as_raw_list() const {
  if (!is_raw_list())
    throw bencode_error("Wrong object type.");

  return raw_list(m_data + 1, m_size - 2);
}

inline raw_map
raw_bencode::as_raw_map() const {
  if (!is_raw_map())
    throw bencode_error("Wrong object type.");

  return raw_map(m_data + 1, m_size - 2);
}

//
// Redo...
//

inline bool
raw_bencode_equal(const raw_bencode& left, const raw_bencode& right) {
  return left.size() == right.size() && std::memcmp(left.begin(), right.begin(), left.size()) == 0;
}

template <typename tmpl_raw_object>
inline bool
raw_bencode_equal(const tmpl_raw_object& left, const char* right, size_t right_size) {
  return left.size() == right_size && std::memcmp(left.begin(), right, right_size) == 0;
}

template <typename tmpl_raw_object>
inline bool
raw_bencode_equal_c_str(const tmpl_raw_object& left, const char* right) {
  return raw_bencode_equal(left, right, strlen(right));
}

}

#endif
