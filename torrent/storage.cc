#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "storage.h"
#include "storage_block.h"
#include "exceptions.h"

#include <sstream>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <iostream>
#include <cassert>

namespace torrent {

Storage::Storage() :
  m_fd(-1),
  m_blockSize(0),
  m_offset(0) {
  clear();
}

Storage::~Storage() {
  close();
}

bool Storage::open(const std::string& filename,
		   unsigned int blockSize, unsigned int offset,
		   bool writeAccess, bool createFile, unsigned int mask) {
  assert(sizeof(off_t) == 8);

  if (offset >= blockSize ||
      blockSize == 0)
    throw internal_error("Storage recived bad block size or offset");

  close();

  m_blockSize = blockSize;
  m_offset = offset;
  m_write = writeAccess;

  m_fd = ::open(filename.c_str(),
	      O_LARGEFILE |
	      (writeAccess ? O_RDWR : O_RDONLY) |
	      (createFile ? O_CREAT : 0),
	      mask);

  if (m_fd == -1)
    return writeAccess ? open(filename, blockSize, offset, false, createFile, mask) : false;

  fstat(m_fd, &m_stat);
  m_blocks = BlockVector(blockCount(), NULL);

  return true;
}

void Storage::close() {
  if (m_fd == -1)
    return;
  
  ::close(m_fd);
  clear();
  m_blocks.resize(0);

  m_fd = -1;
}

// Call this to make sure you can write, open's rw arg
// does not guarantee write access.
bool Storage::isWrite() const {
  return m_write;
}

bool Storage::isOpen() const {
  return m_fd != -1;
}

uint64_t Storage::fileSize() const {
  return m_stat.st_size;
}

uint64_t Storage::blockSize() const {
  return m_blockSize;
}

unsigned int Storage::blockCount() const {
  return fileSize() ? (fileSize() - 1 + m_offset) / m_blockSize + 1 : 0;
}

StorageBlock Storage::getBlock(unsigned int index, bool write) {
  // TODO: We're forcing RW on all blocks;
  //write = true;

  if (m_fd == -1)
    throw storage_error("Storage::read(...) called on a closed file");

  if (index >= m_blocks.size())
    throw storage_error("Storage::read(...) index out of range");

  if (write && !isWrite())
    throw storage_error("Attempted to get writeable block on a read-only file");

  if (write && m_blocks[index] && !m_blocks[index]->m_write) {
    //throw internal_error("Storage: We're not supposed to go here unless the TODO is fixed");

    // reallocate as read/write
    m_blocks[index]->m_ptr = NULL;
    m_blocks[index] = NULL;
  }

  if (!m_blocks[index]) {
    off_t start, length;

    if (index == 0) {
      start = 0;
      length = index < blockCount() - 1 ? m_blockSize - m_offset : m_stat.st_size;

    } else if (index == blockCount() - 1) {
      start  = index * m_blockSize - m_offset;
      length = m_stat.st_size - start;

    } else {
      start = index * m_blockSize - m_offset;
      length = m_blockSize;
    }

    off_t pageOffset = start % getpagesize();

    void* data = mmap(NULL,
		      length + pageOffset,
		      (write ? PROT_WRITE  : 0) | PROT_READ, MAP_SHARED, m_fd,
		      start - pageOffset);
    
    if (data == MAP_FAILED)
      throw storage_error("Storage::getBlock(...) mmap failed");

    m_blocks[index] = StorageBlock::create(data, length + pageOffset, pageOffset, length, write);
    m_blocks[index]->m_ptr = &m_blocks[index];
  }

  if (m_blocks[index]->m_data == NULL)
    throw internal_error("Storage::getBlock found a block without data");

  return StorageBlock(m_blocks[index]);
}

void Storage::resize(uint64_t size) {
  if (size == fileSize())
    return;

  fstat(m_fd, &m_stat);

  uint64_t prev = fileSize();

  if (ftruncate(m_fd, size))
    throw storage_error("Could not resize file");

  fstat(m_fd, &m_stat);

  if (size == 0) {
    throw internal_error("We're zeroing the size of a file, bork atm");

    clear();
    m_blocks.resize(blockCount());

  } else if (prev == 0) {
    m_blocks.resize(blockCount(), NULL);

  } else if (size < prev) {
    throw internal_error("We're decreasing the size of a file, bork atm");

    for (BlockVector::iterator itr = m_blocks.begin() + (size - 1) / m_blockSize;
	 itr != m_blocks.end(); ++itr)
      if (*itr) {
	// TODO: Make sure we bork them, so we don't write outside the file.
	(*itr)->m_ptr = NULL;
	*itr = NULL;
      }

    m_blocks.resize(blockCount());

  } else {
    if (blockCount() <= m_blocks.size())
      throw internal_error("Storage::resize W00t1");

    if (prev % m_blockSize)
      if (m_blocks[m_blocks.size() - 1]) {
	m_blocks[m_blocks.size() - 1]->m_ptr = NULL;
	m_blocks[m_blocks.size() - 1] = NULL;
      }
    
    m_blocks.resize(blockCount(), NULL);
  }
}

void Storage::clear() {
  for (BlockVector::iterator itr = m_blocks.begin(); itr != m_blocks.end(); ++itr)
    if (*itr) {
      (*itr)->m_ptr = NULL;
      *itr = NULL;
    }
}

} // namespace Torrent
