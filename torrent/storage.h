#ifndef LIBTORRENT_STORAGE_H
#define LIBTORRENT_STORAGE_H

#include "storage_block.h"
#include <vector>
#include <string>
#include <stdint.h>
#include <sys/stat.h>

namespace torrent {

class Storage {
  typedef std::vector<StorageBlock::Block*> BlockVector;

public:
  Storage();
  ~Storage();

  bool open(const std::string& filename,
	    unsigned int blockSize, unsigned int offset,
	    bool writeAccess, bool createFile, unsigned int mask = 0644);
  void close();
  
  // Don't touch.
  int fd() { return m_fd; }

  // Call this to make sure you can write, open's rw arg
  // does not guarantee write access.
  bool isWrite() const;
  bool isOpen() const;

  uint64_t fileSize() const;
  uint64_t blockSize() const;
  unsigned int blockCount() const;

  StorageBlock getBlock(unsigned int index, bool write = false);

  void resize(uint64_t size);

private:
  Storage(const Storage&);
  Storage& operator = (const Storage&);

  void clear();

  int m_fd;
  unsigned int m_blockSize;
  unsigned int m_offset;

  struct stat m_stat;
  bool m_write;

  BlockVector m_blocks;
};

} // namespace Torrent

#endif // LIBTORRENT_STORAGE_H
