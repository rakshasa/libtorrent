#ifndef LIBTORRENT_FILE_H
#define LIBTORRENT_FILE_H

#include <string>
#include <inttypes.h>
#include <sys/stat.h>

#include "file_chunk.h"

struct stat;

namespace torrent {

// Defaults to read only and nonblock.

class File {
 public:
  // Flags, not functions. See iostream
  static const unsigned int in             = 0x01;
  static const unsigned int out            = 0x02;
  static const unsigned int create         = 0x04;
  static const unsigned int truncate       = 0x08;
  static const unsigned int nonblock       = 0x10;
  static const unsigned int largefile      = 0x20;

  static const unsigned int type_regular   = 0x01;
  static const unsigned int type_directory = 0x02;
  static const unsigned int type_character = 0x04;
  static const unsigned int type_block     = 0x08;
  static const unsigned int type_fifo      = 0x10;
  static const unsigned int type_link      = 0x20;
  static const unsigned int type_socket    = 0x40;

  File() : m_fd(-1), m_flags(0), m_stat(NULL) {}
  ~File() { close(); }

  // Create only regular files for now.
  bool      open(const std::string& path,
		 unsigned int flags = in,
		 unsigned int mode = 0666);

  void      close();
  
  bool      is_open() const       { return m_fd != -1; }

  void      update_stats();

  bool      set_size(uint64_t v);
  uint64_t  get_size() const;

  int       get_flags() const     { return m_flags; }
  int       get_mode() const;
  int       get_type() const;

  time_t    get_mtime() const     { return m_stat->st_mtime; }

  bool      get_chunk(FileChunk& f,
		      uint64_t offset,
		      unsigned int length,
		      bool wr = false,
		      bool rd = true);
  
  int       fd()                  { return m_fd; }

  const std::string& path()       { return m_path; }

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

