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
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef LIBTORRENT_FILE_CHUNK_H
#define LIBTORRENT_FILE_CHUNK_H

#include <inttypes.h>
#include <sys/mman.h>

namespace torrent {

// Class for making accessing of file chunks easy. It does not care
// about page alignment, but the length of the chunk is limited to
// 2^31 - page_align.

class FileChunk {
 public:
  friend class File;

  static const int advice_normal     = MADV_NORMAL;
  static const int advice_random     = MADV_RANDOM;
  static const int advice_sequential = MADV_SEQUENTIAL;
  static const int advice_willneed   = MADV_WILLNEED;
  static const int advice_dontneed   = MADV_DONTNEED;
  static const int sync_sync         = MS_SYNC;
  static const int sync_async        = MS_ASYNC;
  static const int sync_invalidate   = MS_INVALIDATE;

  FileChunk() : m_ptr(NULL), m_begin(NULL), m_end(NULL), m_read(false), m_write(false) {}
  ~FileChunk() { clear(); }

  bool               is_valid()                               { return m_ptr; }
  bool               is_read()                                { return m_read; }
  bool               is_write()                               { return m_write; }

  char*              ptr()                                    { return m_ptr; }
  char*              begin()                                  { return m_begin; }
  char*              end()                                    { return m_end; }

  uint32_t           size()                                   { return m_end - m_begin; }

  void               clear();

  void               incore(unsigned char* buf, uint32_t offset, uint32_t length);
  void               advise(uint32_t offset, uint32_t length, int advice);
  bool               sync(uint32_t offset, uint32_t length, int flags);

  uint32_t           page_align()                             { return m_begin - m_ptr; }
  uint32_t           page_align(uint32_t o)                   { return (o + page_align()) % m_pagesize; }

  // This won't return correct values for zero length.
  uint32_t           page_touched(uint32_t offset, uint32_t length) {
    return (length + (offset + page_align()) % m_pagesize + m_pagesize - 1) / m_pagesize;
  }

  inline bool        is_incore(uint32_t offset, uint32_t length);

  static uint32_t    page_size()                              { return m_pagesize; }

 protected:
  // Ptr must not be NULL.
  void               set(char* ptr, char* begin, char* end, bool r, bool w) {
    clear();

    m_ptr = ptr;
    m_begin = begin;
    m_end = end;
    m_read = r;
    m_write = w;
  }

private:
  FileChunk(const FileChunk&);
  void operator = (const FileChunk&);

  static uint32_t m_pagesize;

  char*     m_ptr;
  char*     m_begin;
  char*     m_end;

  bool      m_read;
  bool      m_write;
};

inline bool
FileChunk::is_incore(uint32_t offset, uint32_t length) {
  uint32_t size = page_touched(offset, length);

  unsigned char buf[size];
  
  incore(buf, offset, length);

  for (uint32_t i = 0; i < size; ++i)
    if (!buf[i])
      return false;

  return true;
}

}

#endif
