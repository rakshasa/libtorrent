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

#ifndef LIBTORRENT_FILE_H
#define LIBTORRENT_FILE_H

#include <torrent/common.h>
#include <torrent/path.h>

namespace torrent {

class LIBTORRENT_EXPORT File {
public:
  friend class FileList;

  typedef std::pair<uint32_t, uint32_t> range_type;

  File();
  ~File();

  bool                is_created() const;
  bool                is_correct_size() const;
  inline bool         is_valid_position(uint64_t p) const;

  Path*               path()                                  { return &m_path; }
  const Path*         path() const                            { return &m_path; }

  uint64_t            position() const                        { return m_position; }

  uint64_t            size_bytes() const                      { return m_size; }
  uint32_t            size_chunks() const                     { return m_range.second - m_range.first; }

  uint32_t            completed_chunks() const                { return m_completed; }

  const range_type&   range() const                           { return m_range; }

  priority_t          priority() const                        { return m_priority; }
  void                set_priority(priority_t t)              { m_priority = t; }

protected:
  void                set_position(uint64_t pos)              { m_position = pos; }
  void                set_size_bytes(uint64_t s)              { m_size = s; }
  void                set_range(const range_type& range)      { m_range = range; }

  void                set_completed(uint32_t v)               { m_completed = v; }
  void                inc_completed()                         { m_completed++; }

  bool                resize_file();

  FileMeta*           file_meta()                             { return m_fileMeta; }
  const FileMeta*     file_meta() const                       { return m_fileMeta; }

private:
  File(const File&);
  void operator = (const File&);

  FileMeta*           m_fileMeta;
  Path                m_path;

  uint64_t            m_position;
  uint64_t            m_size;

  range_type          m_range;

  uint32_t            m_completed;
  priority_t          m_priority;
};

inline bool
File::is_valid_position(uint64_t p) const {
  return p >= m_position && p < m_position + m_size;
}

}

#endif
