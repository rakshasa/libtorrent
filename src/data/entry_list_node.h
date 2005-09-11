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

#ifndef LIBTORRENT_DATA_ENTRY_LIST_NODE_H
#define LIBTORRENT_DATA_ENTRY_LIST_NODE_H

#include "data/path.h"

namespace torrent {

class FileMeta;

class EntryListNode {
public:
  typedef std::pair<uint32_t, uint32_t> Range;

  EntryListNode();

  bool                is_valid() const                        { return m_fileMeta; }
  inline bool         is_valid_position(off_t p) const;

  FileMeta*           file_meta()                             { return m_fileMeta; }
  const FileMeta*     file_meta() const                       { return m_fileMeta; }
  void                set_file_meta(FileMeta* fm)             { m_fileMeta = fm; }

  Path*               path()                                  { return &m_path; }
  const Path*         path() const                            { return &m_path; }

  off_t               get_position() const                    { return m_position; }
  void                set_position(off_t pos)                 { m_position = pos; }

  off_t               get_size() const                        { return m_size; }
  void                set_size(off_t s)                       { m_size = s; }

  const Range&        get_range() const                       { return m_range; }
  void                set_range(const Range& r)               { m_range = r; }

  // Chunks completed.
  uint32_t            get_completed() const                   { return m_completed; }
  void                set_completed(uint32_t v)               { m_completed = v; }
  void                inc_completed()                         { m_completed++; }

  unsigned char       get_priority() const                    { return m_priority; }
  void                set_priority(unsigned char t)           { m_priority = t; }

  bool                resize_file() const;

private:
  FileMeta*           m_fileMeta;

  off_t               m_position;
  off_t               m_size;

  Path                m_path;
  Range               m_range;

  uint32_t            m_completed;
  unsigned char       m_priority;
};

inline bool
EntryListNode::is_valid_position(off_t p) const {
  return p >= m_position && p < m_position + m_size;
}

}

#endif
