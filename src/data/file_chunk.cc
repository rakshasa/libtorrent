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

#include "config.h"

#include "torrent/exceptions.h"
#include "file_chunk.h"

#include <sstream>
#include <unistd.h>

namespace torrent {

uint32_t FileChunk::m_pagesize = getpagesize();

void
FileChunk::clear() {
  if (m_ptr && munmap(m_ptr, m_end - m_ptr) == -1)
    throw internal_error("FileChunk can't munmap");

  m_ptr = m_begin = m_end = NULL;
  m_read = m_write = false;
}

void
FileChunk::incore(unsigned char* buf, uint32_t offset, uint32_t length) {
  if (!is_valid())
    throw internal_error("Called FileChunk::is_incore() on an invalid object");

  if (offset >= size() || length > size() || offset + length > size() || buf == NULL) {
    std::stringstream s;

    s << "Tried to check incore status in FileChunk with out of range parameters or a NULL buffer ("
      << std::hex << '(' << m_begin << ',' << m_end << ')';

    throw internal_error(s.str());
  }

  if (length == 0)
    return;

  offset += page_align();

  length += offset % m_pagesize;
  offset -= offset % m_pagesize;

#if USE_MINCORE_UNSIGNED
  if (mincore(m_ptr + offset, length, buf))
#else
  if (mincore(m_ptr + offset, length, (char*)buf))
#endif
    throw storage_error("System call mincore failed for FileChunk");
}

void
FileChunk::advise(uint32_t offset, uint32_t length, int advice) {
  if (!is_valid())
    throw internal_error("Called FileChunk::advise() on an invalid object");

  if (offset >= size() || length > size() || offset + length > size())
    internal_error("Tried to advise FileChunk with out of range parameters");

  if (length == 0)
    return;

  offset += page_align();

  length += offset % m_pagesize;
  offset -= offset % m_pagesize;

  if (madvise(m_ptr + offset, length, advice) == 0)
    return;

  else if (errno == EINVAL)
    throw storage_error("FileChunk::advise(...) received invalid input");
  else if (errno == ENOMEM)
    throw storage_error("FileChunk::advise(...) called on unmapped or out-of-range memory");
  else if (errno == EBADF)
    throw storage_error("FileChunk::advise(...) memory are not a file");
  else if (errno == EAGAIN)
    throw storage_error("FileChunk::advise(...) kernel resources temporary unavailable");
  else if (errno == EIO)
    throw storage_error("FileChunk::advise(...) EIO error");
  else
    throw storage_error("FileChunk::advise(...) failed");
}

bool
FileChunk::sync(uint32_t offset, uint32_t length, int flags) {
  if (!is_valid())
    throw internal_error("Called FileChunk::sync() on an invalid object");

  if (offset >= size() || length > size() || offset + length > size())
    internal_error("Tried to advise FileChunk with out of range parameters");

  if (length == 0)
    return true;

  offset += page_align();

  length += offset % m_pagesize;
  offset -= offset % m_pagesize;
  
  return !msync(m_ptr + offset, length, flags);
}    

}
