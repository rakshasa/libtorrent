#include "config.h"

#include <algo/algo.h>
#include "exceptions.h"
#include "storage_consolidator.h"

using namespace algo;

namespace torrent {

StorageConsolidator::~StorageConsolidator() {
  close();

  std::for_each(m_files.begin(), m_files.end(), delete_on(&Node::file));
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
}

void StorageConsolidator::set_chunksize(unsigned int size) {
  if (size == 0)
    throw internal_error("Tried to set StorageConsolidator's blocksize to zero");

  m_chunksize = size;
}

bool StorageConsolidator::get_chunk(StorageChunk& chunk, unsigned int b) {
}

}

