// libTorrent - BitTorrent library
// Copyright (C) 2005-2007, Jari Sundell
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

  static const int flag_active             = (1 << 0);
  static const int flag_create_queued      = (1 << 1);
  static const int flag_resize_queued      = (1 << 2);
  static const int flag_previously_created = (1 << 3);

  File();
  ~File();

  bool                is_created() const;
  bool                is_open() const                          { return m_fd != -1; }

  bool                is_correct_size() const;
  bool                is_valid_position(uint64_t p) const;

  bool                is_create_queued() const                 { return m_flags & flag_create_queued; }
  bool                is_resize_queued() const                 { return m_flags & flag_resize_queued; }
  bool                is_previously_created() const            { return m_flags & flag_previously_created; }

  bool                has_flags(int flags)                     { return m_flags & flags; }

  void                set_flags(int flags);
  void                unset_flags(int flags);

  bool                has_permissions(int prot) const          { return !(prot & ~m_protection); }

  uint64_t            offset() const                           { return m_offset; }

  uint64_t            size_bytes() const                       { return m_size; }
  uint32_t            size_chunks() const                      { return m_range.second - m_range.first; }

  uint32_t            completed_chunks() const                 { return m_completed; }
  void                set_completed_chunks(uint32_t v);

  const range_type&   range() const                            { return m_range; }
  uint32_t            range_first() const                      { return m_range.first; }
  uint32_t            range_second() const                     { return m_range.second; }

  priority_t          priority() const                         { return m_priority; }
  void                set_priority(priority_t t)               { m_priority = t; }

  const Path*         path() const                             { return &m_path; }
  Path*               mutable_path()                           { return &m_path; }

  const std::string&  frozen_path() const                      { return m_frozenPath; }

  uint32_t            match_depth_prev() const                 { return m_matchDepthPrev; }
  uint32_t            match_depth_next() const                 { return m_matchDepthNext; }

  // This should only be changed by libtorrent.
  int                 file_descriptor() const                  { return m_fd; }
  void                set_file_descriptor(int fd)              { m_fd = fd; }

  // This might actually be wanted, as it would be nice to allow the
  // File to decide if it needs to try creating the underlying file or
  // not.
  bool                prepare(int prot, int flags = 0);

  int                 protection() const                       { return m_protection; }
  void                set_protection(int prot)                 { m_protection = prot; }

  uint64_t            last_touched() const                     { return m_lastTouched; }
  void                set_last_touched(uint64_t t)             { m_lastTouched = t; }

protected:
  void                set_flags_protected(int flags)           { m_flags |= flags; }
  void                unset_flags_protected(int flags)         { m_flags &= ~flags; }

  void                set_frozen_path(const std::string& path) { m_frozenPath = path; }

  void                set_offset(uint64_t off)                 { m_offset = off; }
  void                set_size_bytes(uint64_t size)            { m_size = size; }
  void                set_range(uint32_t chunkSize);

  void                set_completed_protected(uint32_t v)      { m_completed = v; }
  void                inc_completed_protected()                { m_completed++; }

  static void         set_match_depth(File* left, File* right);

  void                set_match_depth_prev(uint32_t l)         { m_matchDepthPrev = l; }
  void                set_match_depth_next(uint32_t l)         { m_matchDepthNext = l; }

private:
  File(const File&);
  void operator = (const File&);

  bool                resize_file();

  int                 m_fd;
  int                 m_protection;
  int                 m_flags;
  
  Path                m_path;
  std::string         m_frozenPath;

  uint64_t            m_offset;
  uint64_t            m_size;
  uint64_t            m_lastTouched;

  range_type          m_range;

  uint32_t            m_completed;
  priority_t          m_priority;

  uint32_t            m_matchDepthPrev;
  uint32_t            m_matchDepthNext;
};

inline bool
File::is_valid_position(uint64_t p) const {
  return p >= m_offset && p < m_offset + m_size;
}

inline void
File::set_flags(int flags) {
  set_flags_protected(flags & (flag_create_queued | flag_resize_queued));
}

inline void
File::unset_flags(int flags) {
  unset_flags_protected(flags & (flag_create_queued | flag_resize_queued));
}

inline void
File::set_completed_chunks(uint32_t v) {
  if (!has_flags(flag_active) && v <= size_chunks())
    m_completed = v;
}

}

#endif
