#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "storage_block.h"
#include "exceptions.h"
#include <sys/mman.h>

namespace torrent {

StorageBlock::StorageBlock() :
  m_block(NULL)
{
}

StorageBlock::StorageBlock(Block* block) :
  m_block(block)
{
  if (m_block)
    m_block->m_refCount++;
}

StorageBlock::StorageBlock(const StorageBlock& src) {
  m_block = src.m_block;
  
  if (m_block)
    m_block->m_refCount++;
}

StorageBlock::~StorageBlock() {
  cleanup();
}

bool StorageBlock::isValid() const {
  return m_block;
}

bool StorageBlock::isWrite() const {
  return m_block && m_block->m_write;
}

unsigned int StorageBlock::length() const {
  if (m_block &&
      m_block->m_dataOffset + m_block->m_dataLength != m_block->m_length)
    throw internal_error("StorageBlock::length bork");

  if (m_block)
    return m_block->m_dataLength;
  else
    return 0;
}

void StorageBlock::unref() {
  if (m_block == NULL ||
      m_block->m_ptr == NULL)
    return;

  *m_block->m_ptr = NULL;
  m_block->m_ptr = NULL;
}

StorageBlock& StorageBlock::operator = (const StorageBlock& src) {
  if (this == &src)
    throw internal_error("StorageBlock self assignment");

  cleanup();

  m_block = src.m_block;

  if (m_block)
    m_block->m_refCount++;

  return *this;
}

char* StorageBlock::data() {
  if (m_block && m_block->m_data == NULL)
    throw internal_error("StorageBlock::data bork bork bork");

  if (m_block)
    return (char*)m_block->m_data + m_block->m_dataOffset;
  else
    return NULL;
}

StorageBlock::Block* StorageBlock::create(void* data,
					  unsigned int length,
					  unsigned int dataOffset,
					  unsigned int dataLength,
					  bool write) {
  if (data == NULL)
    throw internal_error("Tried to create StorageBlock with NULL data");

  Block* b = new Block;

  b->m_data = data;
  b->m_length = length;
  b->m_dataOffset = dataOffset;
  b->m_dataLength = dataLength;
  b->m_refCount = 0;
  b->m_write = write;
  b->m_ptr = NULL;

  return b;
}

void StorageBlock::cleanup() {
  if (!m_block)
    return;

  m_block->m_refCount--;

  if (m_block->m_refCount > 0xfffffff)
    throw internal_error("StorageBlock::cleanup with negative ref count");

  if (m_block->m_refCount == 0) {
    if (m_block->m_ptr)
      *m_block->m_ptr = NULL;

    if (munmap(m_block->m_data, m_block->m_length))
      throw local_error("~StorageBlock() munmap failed");

    // Just to make sure we don't bork.
    m_block->m_data = NULL;

    delete m_block;
  }

  m_block = NULL;
}

} // namespace Torrent
