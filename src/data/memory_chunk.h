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

#ifndef LIBTORRENT_DATA_MEMORY_CHUNK_H
#define LIBTORRENT_DATA_MEMORY_CHUNK_H

#include <inttypes.h>
#include <sys/mman.h>

namespace torrent {

class MemoryChunk {
 public:
  // Consider information about whetever the memory maps to a file or
  // not, since mincore etc can only be called on files.

  static const int prot_exec              = PROT_EXEC;
  static const int prot_read              = PROT_READ;
  static const int prot_write             = PROT_WRITE;
  static const int prot_none              = PROT_NONE;
  static const int map_shared             = MAP_SHARED;

  static const int advice_normal          = MADV_NORMAL;
  static const int advice_random          = MADV_RANDOM;
  static const int advice_sequential      = MADV_SEQUENTIAL;
  static const int advice_willneed        = MADV_WILLNEED;
  static const int advice_dontneed        = MADV_DONTNEED;
  static const int sync_sync              = MS_SYNC;
  static const int sync_async             = MS_ASYNC;
  static const int sync_invalidate        = MS_INVALIDATE;

  MemoryChunk() { clear(); }
  ~MemoryChunk() { clear(); }

  // Doesn't allow ptr == NULL, use the default ctor instead.
  MemoryChunk(char* ptr, char* begin, char* end, int prot, int flags);

  bool                is_valid() const                                     { return m_ptr; }
  bool                is_readable() const                                  { return m_prot & PROT_READ; }
  bool                is_writable() const                                  { return m_prot & PROT_WRITE; }
  bool                is_exec() const                                      { return m_prot & PROT_EXEC; }

  inline bool         is_valid_range(uint32_t offset, uint32_t length) const;

  bool                has_permissions(int prot) const                      { return !(prot & ~m_prot); }

  char*               ptr()                                                { return m_ptr; }
  char*               begin()                                              { return m_begin; }
  char*               end()                                                { return m_end; }

  int                 get_prot() const                                     { return m_prot; }

  uint32_t            size() const                                         { return m_end - m_begin; }
  inline void         clear();
  void                unmap();

  // Use errno and strerror if you want to know why these failed.
  void                incore(char* buf, uint32_t offset, uint32_t length);
  bool                advise(uint32_t offset, uint32_t length, int advice);
  void                sync(uint32_t offset, uint32_t length, int flags);

  bool                is_incore(uint32_t offset, uint32_t length);

  // Helper functions for aligning offsets and ranges to page boundaries.

  uint32_t            page_align() const                                   { return m_begin - m_ptr; }
  uint32_t            page_align(uint32_t o) const                         { return (o + page_align()) % m_pagesize; }

  // This won't return correct values if length == 0.
  inline uint32_t     pages_touched(uint32_t offset, uint32_t length) const;

  static uint32_t     page_size()                                          { return m_pagesize; }

private:
  inline void         align_pair(uint32_t& offset, uint32_t& length) const;

  static uint32_t     m_pagesize;

  char*               m_ptr;
  char*               m_begin;
  char*               m_end;

  int                 m_prot;
  int                 m_flags;
};

inline bool
MemoryChunk::is_valid_range(uint32_t offset, uint32_t length) const {
  return length != 0 && (off_t)offset + length <= size();
}

inline void
MemoryChunk::clear() {
  m_ptr = m_begin = m_end = NULL; m_flags = PROT_NONE;
}

inline uint32_t
MemoryChunk::pages_touched(uint32_t offset, uint32_t length) const {
  return (length + page_align(offset) + m_pagesize - 1) / m_pagesize;
}

}

#endif
