#include "config.h"

#include "torrent/exceptions.h"
#include "file_chunk.h"

#include <sstream>
#include <unistd.h>
#include <sys/mman.h>

namespace torrent {

unsigned int FileChunk::m_pagesize = getpagesize();

void FileChunk::clear() {
  if (m_ptr &&
      munmap(m_ptr, m_end - m_ptr) == -1)
    throw internal_error("FileChunk can't munmap");

  m_ptr = m_begin = m_end = NULL;

  m_read = m_write = false;
}

void FileChunk::incore(unsigned char* buf, unsigned int offset, unsigned int len) {
  if (!is_valid())
    throw internal_error("Called FileChunk::is_incore() on an invalid object");

  if (offset >= length() ||
      len > length() ||
      offset + len > length() ||
      buf == NULL) {
    std::stringstream s;

    s << "Tried to check incore status in FileChunk with out of range parameters or a NULL buffer ("
      << std::hex << '(' << m_begin << ',' << m_end << ')';

    throw internal_error(s.str());
  }

  if (len == 0)
    return;

  offset += page_align();

  len    += offset % m_pagesize;
  offset -= offset % m_pagesize;

#if USE_MINCORE_UNSIGNED
  if (mincore(m_ptr + offset, len, buf))
#else
  if (mincore(m_ptr + offset, len, (char*)buf))
#endif
    throw storage_error("System call mincore failed for FileChunk");
}

void FileChunk::advise(unsigned int offset, unsigned int len, int advice) {
  if (!is_valid())
    throw internal_error("Called FileChunk::is_incore() on an invalid object");

  if (offset >= length() ||
      len > length() ||
      offset + len > length()) {
    std::stringstream s;

    s << "Tried to advise FileChunk with out of range parameters"
      << std::hex << '(' << m_begin << ',' << m_end << ',' << offset << ',' << len << ')';

    throw internal_error(s.str());
  }

  if (len == 0)
    return;

  offset += page_align();

  len    += offset % m_pagesize;
  offset -= offset % m_pagesize;

  int t;

  switch (advice) {
  case advice_normal:
    t = MADV_NORMAL;
    break;
  case advice_random:
    t = MADV_RANDOM;
    break;
  case advice_sequential:
    t = MADV_SEQUENTIAL;
    break;
  case advice_willneed:
    t = MADV_WILLNEED;
    break;
  case advice_dontneed:
    t = MADV_DONTNEED;
    break;
  default:
    throw internal_error("FileChunk::advise(...) received invalid advise");
  }

  if (madvise(m_ptr + offset, len, t))
    throw storage_error("System call madvise failed in FileChunk");
}

FileChunk::FileChunk(const FileChunk&) {
  throw internal_error("FileChunk ctor used, but supposed to be disabled");
}

void FileChunk::operator = (const FileChunk&) {
  throw internal_error("FileChunk copy assignment used, but supposed to be disabled");
}

}
