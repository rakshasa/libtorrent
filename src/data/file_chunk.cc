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

  if (madvise(m_ptr + offset, length, advice))
    throw storage_error("System call madvise failed in FileChunk");
}

void
FileChunk::sync(uint32_t offset, uint32_t length, int flags) {
  if (!is_valid())
    throw internal_error("Called FileChunk::sync() on an invalid object");

  if (offset >= size() || length > size() || offset + length > size())
    internal_error("Tried to advise FileChunk with out of range parameters");

  if (length == 0)
    return;

  offset += page_align();

  length += offset % m_pagesize;
  offset -= offset % m_pagesize;
  
  if (msync(m_ptr + offset, length, flags))
    throw storage_error("System call msync failed in FileChunk");
}    

}
