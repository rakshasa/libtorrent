#include "config.h"

#include "exceptions.h"
#include "file_chunk.h"

#include <unistd.h>
#include <sys/mman.h>

namespace torrent {

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
      buf == NULL)
    throw internal_error("Tried to check incore status in FileChunk with out of range parameters or a NULL buffer");

  if (len == 0)
    return;

  offset += page_align();

  len    += offset % m_pagesize;
  offset -= offset % m_pagesize;

  if (mincore(m_ptr + offset, len, buf))
    throw storage_error("System call mincore failed for FileChunk");
}

void FileChunk::advise(unsigned int offset, unsigned int len, int advice) {
  if (!is_valid())
    throw internal_error("Called FileChunk::is_incore() on an invalid object");

  if (offset >= length() ||
      len > length() ||
      offset + len > length())
    throw internal_error("Tried to check incore status in FileChunk with out of range parameters");

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
