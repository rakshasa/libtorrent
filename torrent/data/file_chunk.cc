#include "config.h"

#include "exceptions.h"
#include "file_chunk.h"

#include <sys/mman.h>

namespace torrent {

void FileChunk::clear() {
  if (m_ptr &&
      munmap(m_ptr, m_begin - m_ptr + m_length) == -1)
    throw internal_error("FileChunk can't munmap");

  m_ptr = m_begin = NULL;
  m_length = 0;

  m_read = m_write = false;
}

FileChunk::FileChunk(const FileChunk&) {
  throw internal_error("FileChunk ctor used, but supposed to be disabled");
}

void FileChunk::operator = (const FileChunk&) {
  throw internal_error("FileChunk copy assignment used, but supposed to be disabled");
}

}
