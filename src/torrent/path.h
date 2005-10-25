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

#ifndef LIBTORRENT_PATH_H
#define LIBTORRENT_PATH_H

#include <string>
#include <list>

namespace torrent {

// Use a blank first path to get root and "." to get current dir.

// Consider using std::vector.

class Path : private std::list<std::string> {
public:
  typedef std::list<std::string> Base;

  typedef Base::value_type value_type;
  typedef Base::pointer pointer;
  typedef Base::const_pointer const_pointer;
  typedef Base::reference reference;
  typedef Base::const_reference const_reference;
  typedef Base::size_type size_type;
  typedef Base::difference_type difference_type;
  typedef Base::allocator_type allocator_type;

  typedef Base::iterator iterator;
  typedef Base::reverse_iterator reverse_iterator;
  typedef Base::const_iterator const_iterator;
  typedef Base::const_reverse_iterator const_reverse_iterator;

  using Base::clear;
  using Base::empty;
  using Base::size;
  //using Base::reserve;

  using Base::front;
  using Base::back;
  using Base::begin;
  using Base::end;
  using Base::rbegin;
  using Base::rend;

  using Base::push_front;
  using Base::push_back;

  Path() {}
  Path(const std::string& path, const std::string& enc) { insert_path(end(), path); m_encoding = enc; }

  void               insert_path(iterator pos, const std::string& path);

  // Return the path as a string with '/' deliminator. The deliminator
  // is only inserted between path elements.
  std::string        as_string();

  const std::string  encoding() const                     { return m_encoding; }
  void               set_encoding(const std::string& enc) { m_encoding = enc; }

  Base&              get_base()                           { return *this; }

private:
  std::string        m_encoding;
};

}

#endif

