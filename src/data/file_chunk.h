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

  FileChunk() : m_ptr(NULL), m_begin(NULL), m_end(NULL), m_read(false), m_write(false) {}
  ~FileChunk() { clear(); }

  bool         is_valid()                                           { return m_ptr; }
  bool         is_read()                                            { return m_read; }
  bool         is_write()                                           { return m_write; }

  inline bool  is_incore(unsigned int offset, unsigned int length);

  char*        ptr()                                                { return m_ptr; }
  char*        begin()                                              { return m_begin; }
  char*        end()                                                { return m_end; }

  unsigned int length()                                             { return m_end - m_begin; }

  void         clear();

  void         incore(unsigned char* buf,
		      unsigned int offset,
		      unsigned int length);

  void         advise(unsigned int offset,
		      unsigned int length,
		      int advice);

  unsigned int        page_align()               { return m_begin - m_ptr; }
  unsigned int        page_align(unsigned int o) { return (o + page_align()) % m_pagesize; }

  // This won't return correct values for zero length.
  unsigned int        page_touched(unsigned int offset, unsigned int length) {
    return (length + (offset + page_align()) % m_pagesize + m_pagesize - 1) / m_pagesize;
  }

  static unsigned int page_size()                { return m_pagesize; }

 protected:
  // Ptr must not be NULL.
  void set(char* ptr,
	   char* begin,
	   char* end,
	   bool r,
	   bool w) {
    clear();

    m_ptr = ptr;
    m_begin = begin;
    m_end = end;
    m_read = r;
    m_write = w;
  }

private:
  FileChunk(const FileChunk&);
  void operator = (const FileChunk&);

  static unsigned int m_pagesize;

  char*        m_ptr;
  char*        m_begin;
  char*        m_end;

  bool         m_read;
  bool         m_write;
};

inline bool FileChunk::is_incore(unsigned int offset, unsigned int length) {
  unsigned int size = page_touched(offset, length);

  unsigned char buf[size];
  
  incore(buf, offset, length);

  for (unsigned int i = 0; i < size; ++i)
    if (!buf[i])
      return false;

  return true;
}

}

#endif
