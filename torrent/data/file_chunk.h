#ifndef LIBTORRENT_FILE_CHUNK_H
#define LIBTORRENT_FILE_CHUNK_H

namespace torrent {

class FileChunk {
 public:
  friend class File;

  static const int advice_normal     = 0x01;
  static const int advice_random     = 0x02;
  static const int advice_sequential = 0x04;
  static const int advice_willneed   = 0x08;
  static const int advice_dontneed   = 0x10;

  FileChunk() : m_ptr(NULL), m_begin(NULL), m_length(0), m_read(false), m_write(false) {}
  ~FileChunk() { clear(); }

  bool         is_valid()                                           { return m_ptr; }
  bool         is_read()                                            { return m_read; }
  bool         is_write()                                           { return m_write; }

  inline bool  is_incore(unsigned int offset, unsigned int length);

  char*        begin()                                              { return m_begin; }
  char*        end()                                                { return m_begin + m_length; }

  unsigned int length()                                             { return m_length; }

  void         clear();

  void         incore(unsigned char* buf,
		      unsigned int offset,
		      unsigned int length);

  void         advise(unsigned int offset,
		      unsigned int length,
		      int advice);

  unsigned int touched_pages(unsigned int offset, unsigned int length);

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

inline bool FileChunk::is_incore(unsigned int offset, unsigned int length) {
  unsigned int size = touched_pages(offset, length);

  unsigned char buf[size];
  
  incore(buf, offset, length);

  for (unsigned int i = 0; i < size; ++i)
    if (!buf[i])
      return false;

  return true;
}

}

#endif
