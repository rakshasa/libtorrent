#ifndef LIBTORRENT_STORAGE_BLOCK_H
#define LIBTORRENT_STORAGE_BLOCK_H

namespace torrent {

class StorageBlock {
public:
  struct Block {
    void* m_data;
    unsigned int m_length;
    unsigned int m_dataOffset;
    unsigned int m_dataLength;
    
    unsigned int m_refCount;
    bool m_write;
    Block** m_ptr;
  };

  StorageBlock();
  StorageBlock(Block* block);
  StorageBlock(const StorageBlock& src);

  ~StorageBlock();

  bool isValid() const;
  bool isWrite() const;
  unsigned int length() const;

  // Call this when Storage should do a new mmap when Storage::getBlock(...)
  // is called on this index.
  void unref();

  char* data();

  StorageBlock& operator = (const StorageBlock& src);

  // data must be non-NULL.
  static Block* create(void* data,
		       unsigned int length,
		       unsigned int dataOffset,
		       unsigned int dataLength,
		       bool write);

private:
  void cleanup();

  Block* m_block;
};

}

#endif // LIBTORRENT_STORAGE_BLOCK_H
