#ifndef LIBTORRENT_FILE_CHUNK_H
#define LIBTORRENT_FILE_CHUNK_H

namespace torrent {

class FileChunk {
 public:
  friend class File;

  struct Data {
    Data(char* ptr, char* begin, unsigned int length) :
      m_ref(1), m_begin(begin), m_length(length) {}

    int          m_ref;
    char*        m_ptr;
    char*        m_begin;
    unsigned int m_length;
  };

  FileChunk() : m_data(new Data(NULL, NULL, 0))      {}
  FileChunk(const FileChunk& fc) : m_data(fc.m_data) { m_data->m_ref++; }
  ~FileChunk();

  bool  is_valid()      { return m_data->m_ptr; }

  char* begin()         { return m_data->m_begin; }
  char* end()           { return m_data->m_begin + m_data->m_length; }

  unsigned int length() { return m_data->m_length; }

 protected:
  FileChunk(char* ptr, char* begin, unsigned int length) :
    m_data(new Data(ptr, begin, length)) {}

 private:
  Data* m_data;
};

}

#endif
