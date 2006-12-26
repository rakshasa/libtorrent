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

#ifndef LIBTORRENT_FILE_LIST_ITERATOR_H
#define LIBTORRENT_FILE_LIST_ITERATOR_H

#include <torrent/common.h>
#include <torrent/data/file_list.h>

namespace torrent {

class File;

// A special purpose iterator class for iterating through FileList as
// a dired structure.
class LIBTORRENT_EXPORT FileListIterator {
public:
  typedef FileList::iterator iterator;
  typedef File*              reference;
  typedef File**             pointer;

  FileListIterator() {}
  explicit FileListIterator(iterator pos, uint32_t depth = 0) : m_position(pos), m_depth(depth) {}

  bool                is_file() const;
  bool                is_empty() const;

  bool                is_entering() const;
  bool                is_leaving() const  { return m_depth < 0; }

  uint32_t            depth() const       { return std::abs(m_depth); }

  iterator            base() const        { return m_position; }
  reference           file() const        { return *m_position; }

  reference           operator *() const  { return *m_position; }
  pointer             operator ->() const { return &*m_position; }
  
  FileListIterator&   operator ++();
  FileListIterator&   operator --();

  FileListIterator    operator ++(int);
  FileListIterator    operator --(int);

  friend bool         operator == (const FileListIterator& left, const FileListIterator& right);
  friend bool         operator != (const FileListIterator& left, const FileListIterator& right);

private:
  iterator            m_position;
  int32_t             m_depth;
};

inline FileListIterator
FileListIterator::operator ++(int) {
  FileListIterator tmp = *this;
  ++(*this);
  return tmp;
}

inline FileListIterator
FileListIterator::operator --(int) {
  FileListIterator tmp = *this;
  --(*this);
  return tmp;
}

inline bool
operator == (const FileListIterator& left, const FileListIterator& right) {
  return left.m_position == right.m_position && left.m_depth == right.m_depth;
}

inline bool
operator != (const FileListIterator& left, const FileListIterator& right) {
  return left.m_position != right.m_position || left.m_depth != right.m_depth;
}

}

#endif
