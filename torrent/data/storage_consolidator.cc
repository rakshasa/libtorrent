#include "config.h"

#include <algo/algo.h>
#include "exceptions.h"
#include "storage_consolidator.h"

using namespace algo;

namespace torrent {

StorageConsolidator::~StorageConsolidator() {
  close();
}

void StorageConsolidator::add_file(File* file, uint64_t length) {
  if (length + m_size < m_size)
    throw internal_error("Sum of files added to StorageConsolidator overflowed 64bit");

  if (file == NULL)
    throw internal_error("StorageConsolidator::add_file received a File NULL pointer");

  m_files.push_back(Node(file, m_size, length));
  m_size += length;
}

bool StorageConsolidator::resize() {
  return std::find_if(m_files.begin(), m_files.end(),
		      bool_not(call_member(member(&Node::file),
					   &File::set_size,
					   member(&Node::length))))
    == m_files.end();
}
					   
void StorageConsolidator::close() {
  std::for_each(m_files.begin(), m_files.end(),
		call_member(member(&Node::file),
			    &File::close));

  std::for_each(m_files.begin(), m_files.end(), delete_on(&Node::file));

  m_files.clear();
}

void StorageConsolidator::set_chunksize(unsigned int size) {
  if (size == 0)
    throw internal_error("Tried to set StorageConsolidator's blocksize to zero");

  m_chunksize = size;
}

bool StorageConsolidator::get_chunk(StorageChunk& chunk, unsigned int b, bool wr, bool rd) {
  chunk.clear();

  uint64_t pos = b * (uint64_t)m_chunksize;
  uint64_t end = std::min((b + 1) * (uint64_t)m_chunksize, m_size);

  if (pos >= m_size)
    throw internal_error("Tried to access chunk out of range in StorageConsolidator");

  Files::iterator itr = std::find_if(m_files.begin(), m_files.end(),
				     lt(value(pos),
					add<uint64_t>(member(&Node::position),
						      member(&Node::length))));

  while (pos != end) {
    if (itr == m_files.end())
      throw internal_error("StorageConsolidator could not find a valid file for chunk");

    unsigned int offset = pos - itr->position;
    unsigned int length = std::min(end - pos, itr->length - offset);

    if (length == 0)
      throw internal_error("StorageConsolidator::get_chunk caught a piece with 0 lenght");

    if (length > m_chunksize)
      throw internal_error("StorageConsolidator::get_chunk caught an excessively large piece");

    FileChunk& f = chunk.add_file(length);

    if (!itr->file->get_chunk(f, offset, length, wr, rd)) {
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

