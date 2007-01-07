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

class SocketFile;

class LIBTORRENT_EXPORT File {
public:
  friend class FileList;

  typedef std::pair<uint32_t, uint32_t> range_type;

  File();
  ~File();

  bool                is_created() const;
  bool                is_open() const                          { return m_fd != -1; }

  bool                is_correct_size() const;
  bool                is_valid_position(uint64_t p) const;

  bool                has_permissions(int prot) const          { return !(prot & ~m_protection); }

//   bool                is_readable() const                               { return m_prot & MemoryChunk::prot_read; }
//   bool                is_writable() const                               { return m_prot & MemoryChunk::prot_write; }
//   bool                is_nonblock() const                               { return m_flags & o_nonblock; }

  uint64_t            offset() const                           { return m_offset; }

  uint64_t            size_bytes() const                       { return m_size; }
  uint32_t            size_chunks() const                      { return m_range.second - m_range.first; }

  uint32_t            completed_chunks() const                 { return m_completed; }

  const range_type&   range() const                            { return m_range; }
  uint32_t            range_first() const                      { return m_range.first; }
  uint32_t            range_second() const                     { return m_range.second; }

  priority_t          priority() const                         { return m_priority; }
  void                set_priority(priority_t t)               { m_priority = t; }

  Path*               path()                                   { return &m_path; }
  const Path*         path() const                             { return &m_path; }

  const std::string&  frozen_path() const                      { return m_frozenPath; }
  void                set_frozen_path(const std::string& path) { m_frozenPath = path; }

  uint32_t            match_depth_prev() const                 { return m_matchDepthPrev; }
  uint32_t            match_depth_next() const                 { return m_matchDepthNext; }

  // This should only be changed by libtorrent.
  int                 file_descriptor() const                  { return m_fd; }
  void                set_file_descriptor(int fd)              { m_fd = fd; }

  // Hmm...
  SocketFile*         socket_file()                            { return reinterpret_cast<SocketFile*>(&m_fd); }
  const SocketFile*   socket_file() const                      { return reinterpret_cast<const SocketFile*>(&m_fd); }

  // More hmm...
  bool                prepare(int prot, int flags = 0);

  int                 protection() const                       { return m_protection; }
  void                set_protection(int prot)                 { m_protection = prot; }

  uint64_t            last_touched() const                     { return m_lastTouched; }
  void                set_last_touched(uint64_t t)             { m_lastTouched = t; }

protected:
  void                set_offset(uint64_t off)                 { m_offset = off; }
  void                set_size_bytes(uint64_t size)            { m_size = size; }
  void                set_range(uint32_t chunkSize);

  void                set_completed(uint32_t v)                { m_completed = v; }
  void                inc_completed()                          { m_completed++; }

  void                set_match_depth_prev(uint32_t l)         { m_matchDepthPrev = l; }
  void                set_match_depth_next(uint32_t l)         { m_matchDepthNext = l; }

  bool                resize_file();

private:
  File(const File&);
  void operator = (const File&);

  Path                m_path;

  uint64_t            m_offset;
  uint64_t            m_size;

  range_type          m_range;

  uint32_t            m_completed;
  priority_t          m_priority;

  uint32_t            m_matchDepthPrev;
  uint32_t            m_matchDepthNext;

  // Move the below stuff up to the right places next incompatible API
  // change.
  int                 m_fd;
  std::string         m_frozenPath;

  int                 m_protection;

  uint64_t            m_lastTouched;
};

inline bool
File::is_valid_position(uint64_t p) const {
  return p >= m_offset && p < m_offset + m_size;
}

}

#endif
