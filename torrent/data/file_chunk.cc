#include "config.h"

#include "exceptions.h"
#include "file_chunk.h"

#include <unistd.h>
#include <sys/mman.h>

namespace torrent {

// m_ptr is guaranteed to be on the boundary of a page.

void FileChunk::clear() {
  if (m_ptr &&
      munmap(m_ptr, m_begin - m_ptr + m_length) == -1)
    throw internal_error("FileChunk can't munmap");

  m_ptr = m_begin = NULL;
  m_length = 0;

  m_read = m_write = false;
}

bool FileChunk::is_incore(unsigned int offset, unsigned int length) {
  if (!is_valid())
    throw internal_error("Called FileChunk::is_incore() on an invalid object");

  if (offset >= m_length ||
      length > m_length ||
      offset + length > m_length)
    throw internal_error("Tried to check incore status in FileChunk with out of range parameters");

  if (length == 0)
    return true;

  offset += m_begin - m_ptr;

  length += offset % getpagesize();
  offset -= offset % getpagesize();

  unsigned int size = (length + getpagesize() - 1) / getpagesize();

  unsigned char buf[size];

  if (mincore(m_ptr + offset, length, buf))
    throw storage_error("System call mincore failed for FileChunk");

  for (unsigned int i = 0; i < size; ++i)
    if (!buf[i])
      return false;

  return true;
}

void FileChunk::advise(unsigned int offset, unsigned int length, int advice) {
  if (!is_valid())
    throw internal_error("Called FileChunk::is_incore() on an invalid object");

  if (offset >= m_length ||
      length > m_length ||
      offset + length > m_length)
    throw internal_error("Tried to check incore status in FileChunk with out of range parameters");

  if (length == 0)
    return;

  offset += m_begin - m_ptr;

  length += offset % getpagesize();
  offset -= offset % getpagesize();

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

  if (madvise(m_ptr + offset, length, t))
    throw storage_error("System call madvise failed in FileChunk");
}

FileChunk::FileChunk(const FileChunk&) {
  throw internal_error("FileChunk ctor used, but supposed to be disabled");
}

void FileChunk::operator = (const FileChunk&) {
  throw internal_error("FileChunk copy assignment used, but supposed to be disabled");
}

}
