#include "config.h"

#include "exceptions.h"
#include "file_chunk.h"

#include <sys/mman.h>

namespace torrent {

FileChunk::~FileChunk() {
  if (m_ptr &&
      munmap(m_ptr, m_begin - m_ptr + m_length) == -1)
    throw internal_error("FileChunk can't munmap");
}

}
