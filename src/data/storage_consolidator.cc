#include "config.h"

#include <algo/algo.h>

#include "torrent/exceptions.h"
#include "file.h"
#include "storage_consolidator.h"

using namespace algo;

namespace torrent {

StorageConsolidator::~StorageConsolidator() {
  close();
}

void StorageConsolidator::add_file(File* file, uint64_t size) {
  if (sizeof(off_t) != 8)
    throw internal_error("sizeof(off_t) != 8");

  if (size + m_size < m_size)
    throw internal_error("Sum of files added to StorageConsolidator overflowed 64bit");

  if (file == NULL)
    throw internal_error("StorageConsolidator::add_file received a File NULL pointer");

  m_files.push_back(StorageFile(file, m_size, size));
  m_size += size;
}

bool StorageConsolidator::resize() {
  return std::find_if(m_files.begin(), m_files.end(),
		      bool_not(call_member(call_member(&StorageFile::file),
					   &File::set_size,
					   call_member(&StorageFile::size))))
    == m_files.end();
}
					   
bool
StorageConsolidator::sync() {
  bool result = true;

  for (FileList::iterator itr = m_files.begin(); itr != m_files.end(); ++itr) {
    FileChunk f;
    uint64_t pos = 0;

    while (pos != itr->size()) {
      uint32_t length = std::min<uint64_t>(itr->size() - pos, (128 << 20));

      if (!itr->file()->get_chunk(f, pos, length, true)) {
	result = false;
	break;
      }

      f.sync(0, length, FileChunk::sync_async);
      pos += length;
    }
  }

  return result;
}

void StorageConsolidator::close() {
  std::for_each(m_files.begin(), m_files.end(),
		on(call_member(&StorageFile::file), delete_on()));

  m_files.clear();
  m_size = 0;
}

void StorageConsolidator::set_chunksize(uint32_t size) {
  if (size == 0)
    throw internal_error("Tried to set StorageConsolidator's chunksize to zero");

  m_chunksize = size;
}

bool StorageConsolidator::get_chunk(StorageChunk& chunk, uint32_t b, bool wr, bool rd) {
  chunk.clear();

  uint64_t pos = b * (uint64_t)m_chunksize;
  uint64_t end = std::min((b + 1) * (uint64_t)m_chunksize, m_size);

  if (pos >= m_size)
    throw internal_error("Tried to access chunk out of range in StorageConsolidator");

  FileList::iterator itr = std::find_if(m_files.begin(), m_files.end(),
					lt(value(pos),
					   add<uint64_t>(call_member(&StorageFile::position),
							 call_member(&StorageFile::size))));

  while (pos != end) {
    if (itr == m_files.end())
      throw internal_error("StorageConsolidator could not find a valid file for chunk");

    uint64_t offset = pos - itr->position();
    uint32_t length = std::min(end - pos, itr->size() - offset);

    if (length == 0)
      throw internal_error("StorageConsolidator::get_chunk caught a piece with 0 lenght");

    if (length > m_chunksize)
      throw internal_error("StorageConsolidator::get_chunk caught an excessively large piece");

    FileChunk& f = chunk.add_file(length);

    if (!itr->file()->get_chunk(f, offset, length, wr, rd)) {
      chunk.clear();

      return false;
    }

    pos += length;
    ++itr;
  }

  if (chunk.get_size() != end - b * (uint64_t)m_chunksize)
    throw internal_error("StorageConsolidator::get_chunk didn't get a chunk with the correct size");

  return true;
}

}

