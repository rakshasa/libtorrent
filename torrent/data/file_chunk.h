#ifndef LIBTORRENT_FILE_CHUNK_H
#define LIBTORRENT_FILE_CHUNK_H

namespace torrent {

class FileChunk {
 public:
  friend class File;

  FileChunk() : m_ptr(NULL), m_begin(NULL), m_length(0), m_read(false), m_write(false) {}
  ~FileChunk() { clear(); }

  bool  is_valid()      { return m_ptr; }
  bool  is_read()       { return m_read; }
  bool  is_write()      { return m_write; }

  char* begin()         { return m_begin; }
  char* end()           { return m_begin + m_length; }

  unsigned int length() { return m_length; }

  void clear();

 protected:
  // Ptr must not be NULL.
  void set(char* ptr,
	   char* begin,
	   unsigned int length,
	   bool r,
	   bool w) {
    clear();

    m_ptr = ptr;
    m_begin = begin;
    m_length = length;
    m_read = r;
    m_write = w;
  }

private:
  FileChunk(const FileChunk&);
  void operator = (const FileChunk&);

  char*        m_ptr;
  char*        m_begin;
  unsigned int m_length;

  bool m_read;
  bool m_write;
};

}

#endif
