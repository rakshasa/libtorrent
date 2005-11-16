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

#ifndef LIBTORRENT_BENCODE_H
#define LIBTORRENT_BENCODE_H

#include <iosfwd>
#include <string>
#include <map>
#include <list>

namespace torrent {

// This class should very rarely change, so it doesn't matter that
// much of the implementation is visible. Though it really needs to be
// cleaned up.

class Bencode {
 public:
  typedef std::list<Bencode>             List;
  typedef std::map<std::string, Bencode> Map;

  enum Type {
    TYPE_NONE,
    TYPE_VALUE,
    TYPE_STRING,
    TYPE_LIST,
    TYPE_MAP
  };

  Bencode()                     : m_type(TYPE_NONE) {}
  Bencode(const int64_t v)      : m_type(TYPE_VALUE), m_value(v) {}
  Bencode(const char* s)        : m_type(TYPE_STRING), m_string(new std::string(s)) {}
  Bencode(const std::string& s) : m_type(TYPE_STRING), m_string(new std::string(s)) {}
  Bencode(const Bencode& b);

  explicit Bencode(Type t);
  
  ~Bencode()                              { clear(); }
  
  void                clear();

  Type                get_type() const    { return m_type; }

  bool                is_value() const    { return m_type == TYPE_VALUE; }
  bool                is_string() const   { return m_type == TYPE_STRING; }
  bool                is_list() const     { return m_type == TYPE_LIST; }
  bool                is_map() const      { return m_type == TYPE_MAP; }

  bool                has_key(const std::string& s) const;
  Bencode&            get_key(const std::string& k);
  const Bencode&      get_key(const std::string& k) const;

  Bencode&            insert_key(const std::string& s, const Bencode& b);
  void                erase_key(const std::string& s);

  int64_t&            as_value();
  std::string&        as_string();
  List&               as_list();
  Map&                as_map();

  int64_t             as_value() const;
  const std::string&  as_string() const;
  const List&         as_list() const;
  const Map&          as_map() const;

  // Unambigious const version of as_*
  int64_t             c_value() const     { return as_value(); }
  const std::string&  c_string() const    { return as_string(); }
  const List&         c_list() const      { return as_list(); }
  const Map&          c_map() const       { return as_map(); }

  Bencode&            operator = (const Bencode& b);

  friend std::istream& operator >> (std::istream& s, Bencode& b);
  friend std::ostream& operator << (std::ostream& s, const Bencode& b);

  std::string         compute_sha1() const;

 private:
  static bool         read_string(std::istream& s, std::string& str);

  Type                m_type;

  union {
    int64_t             m_value;
    std::string*        m_string;
    List*               m_list;
    Map*                m_map;
  };
};

}

#endif // LIBTORRENT_BENCODE_H
