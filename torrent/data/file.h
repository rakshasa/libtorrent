#ifndef LIBTORRENT_FILE_H
#define LIBTORRENT_FILE_H

#include <string>
#include <inttypes.h>

#include "file_chunk.h"

struct stat;

namespace torrent {

// Defaults to read only and nonblock.

class File {
 public:
  // Flags, not functions. See iostream
  static const int in             = 0x01;
  static const int out            = 0x02;
  static const int create         = 0x04;
  static const int truncate       = 0x08;
  static const int nonblock       = 0x10;
  static const int largefile      = 0x20;

  static const int type_regular   = 0x01;
  static const int type_directory = 0x02;
  static const int type_character = 0x04;
  static const int type_block     = 0x08;
  static const int type_fifo      = 0x10;
  static const int type_link      = 0x20;
  static const int type_socket    = 0x40;

  File() : m_fd(-1), m_flags(0), m_stat(NULL) {}
  ~File() { close(); }

  // Create only regular files for now.
  bool     open(const std::string& path,
		  int flags = in,
		  int mode = 0666);

  void     close();
  
  bool     is_open() const       { return m_fd != -1; }

  bool     set_size(uint64_t v);

  int      get_flags() const     { return m_flags; }
  int      get_mode() const;
  int      get_type() const;
  uint64_t get_size() const;

  bool     get_chunk(FileChunk& f,
		     uint64_t offset,
		     unsigned int length,
		     bool wr = false,
		     bool rd = true);
  
  int      fd()                  { return m_fd; }

  const std::string& path()      { return m_path; }

 private:
  File(const File&);
  void operator = (const File&);

  int          m_fd;
  int          m_flags;

  struct stat* m_stat;

  std::string  m_path;
};

}

#endif

