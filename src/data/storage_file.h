#ifndef LIBTORRENT_STORAGE_FILE_H
#define LIBTORRENT_STORAGE_FILE_H

namespace torrent {

class File;

class StorageFile {
public:
  StorageFile() : m_file(NULL), m_position(0), m_size(0) {}
  StorageFile(File* f, uint64_t p, uint64_t s) : m_file(f), m_position(p), m_size(s) {}

  bool        is_valid()        { return m_file; }

  File*       file()            { return m_file; }
  uint64_t    position()        { return m_position; }
  uint64_t    size()            { return m_size; }

  File const* c_file() const    { return m_file; }

private:
  File* m_file;
  uint64_t m_position;
  uint64_t m_size;
};

}

#endif
