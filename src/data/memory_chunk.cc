// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
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

#include "config.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <rak/error_number.h>

#include "torrent/exceptions.h"
#include "memory_chunk.h"

#ifdef __sun__
extern "C" int madvise(void *, size_t, int);
//#include <sys/mman.h>
//Should be the include line instead, but Solaris
//has an annoying bug wherein it doesn't declare
//madvise for C++.
#endif

namespace torrent {

uint32_t MemoryChunk::m_pagesize = getpagesize();

const int MemoryChunk::sync_sync;
const int MemoryChunk::sync_async;
const int MemoryChunk::sync_invalidate;

const int MemoryChunk::prot_exec;
const int MemoryChunk::prot_read;
const int MemoryChunk::prot_write;
const int MemoryChunk::prot_none;
const int MemoryChunk::map_shared;

const int MemoryChunk::advice_normal;
const int MemoryChunk::advice_random;
const int MemoryChunk::advice_sequential;
const int MemoryChunk::advice_willneed;
const int MemoryChunk::advice_dontneed;

inline void
MemoryChunk::align_pair(uint32_t* offset, uint32_t* length) const {
  *offset += page_align();
  
  *length += *offset % m_pagesize;
  *offset -= *offset % m_pagesize;
}

MemoryChunk::MemoryChunk(char* ptr, char* begin, char* end, int prot, int flags) :
  m_ptr(ptr),
  m_begin(begin),
  m_end(end),
  m_prot(prot),
  m_flags(flags) {

  if (ptr == NULL)
    throw internal_error("MemoryChunk::MemoryChunk(...) received ptr == NULL");

  if (page_align() >= m_pagesize)
    throw internal_error("MemoryChunk::MemoryChunk(...) received an page alignment >= page size");

  if ((std::ptrdiff_t)ptr % m_pagesize)
    throw internal_error("MemoryChunk::MemoryChunk(...) is not aligned to a page");
}

void
MemoryChunk::unmap() {
  if (!is_valid())
    throw internal_error("MemoryChunk::unmap() called on an invalid object");

  if (munmap(m_ptr, m_end - m_ptr) != 0)
    throw internal_error("MemoryChunk::unmap() system call failed: " + std::string(rak::error_number::current().c_str()));
}  

void
MemoryChunk::incore(char* buf, uint32_t offset, uint32_t length) {
  if (!is_valid())
    throw internal_error("Called MemoryChunk::incore(...) on an invalid object");

  if (!is_valid_range(offset, length))
    throw internal_error("MemoryChunk::incore(...) received out-of-range input");

  align_pair(&offset, &length);

#if USE_MINCORE

#if USE_MINCORE_UNSIGNED
  if (mincore(m_ptr + offset, length, (unsigned char*)buf))
#else
  if (mincore(m_ptr + offset, length, (char*)buf))
#endif
    throw storage_error("System call mincore failed: " + std::string(rak::error_number::current().c_str()));

#else // !USE_MINCORE
  // Pretend all pages are in memory.
  memset(buf, 1, pages_touched(offset, length));

#endif
}

bool
MemoryChunk::advise(uint32_t offset, uint32_t length, int advice) {
  if (!is_valid())
    throw internal_error("Called MemoryChunk::advise() on an invalid object");

  if (!is_valid_range(offset, length))
    throw internal_error("MemoryChunk::advise(...) received out-of-range input");

#if USE_MADVISE
  align_pair(&offset, &length);

  if (madvise(m_ptr + offset, length, advice) == 0)
    return true;

  else if (errno == EINVAL || (errno == ENOMEM && advice != advice_willneed) || errno == EBADF)
    throw internal_error("MemoryChunk::advise(...) " + std::string(strerror(errno)));
  
  else
    return false;

#else
  return true;

#endif
}

bool
MemoryChunk::sync(uint32_t offset, uint32_t length, int flags) {
  if (!is_valid())
    throw internal_error("Called MemoryChunk::sync() on an invalid object");

  if (!is_valid_range(offset, length))
    throw internal_error("MemoryChunk::sync(...) received out-of-range input");

  align_pair(&offset, &length);
  
  return msync(m_ptr + offset, length, flags) == 0;
}    

}
