#include "config.h"

#include "exceptions.h"
#include "file_chunk.h"

#include <sys/mman.h>

namespace torrent {

FileChunk::~FileChunk() {
  if (m_data == NULL)
    throw internal_error("Deleted FileChunk twice or it was undefined");

  if (--m_data->m_ref)
    return;

  if (m_data->m_ptr &&
      munmap(m_data->m_ptr, m_data->m_begin - m_data->m_ptr + m_data->m_length) == -1)
    throw internal_error("FileChunk can't munmap");

  m_data = NULL;
}

}
