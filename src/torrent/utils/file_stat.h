#ifndef TORRENT_UTILS_FILE_STAT_H
#define TORRENT_UTILS_FILE_STAT_H

#include <string>
#include <cinttypes>
#include <sys/stat.h>

namespace torrent::utils {

class FileStat {
public:

  bool                update(int fd)                           { return fstat(fd, &m_stat) == 0; }
  bool                update(const char* filename)             { return stat(filename, &m_stat) == 0; }
  bool                update(const std::string& filename)      { return update(filename.c_str()); }

  bool                update_link(const char* filename)        { return lstat(filename, &m_stat) == 0; }
  bool                update_link(const std::string& filename) { return update_link(filename.c_str()); }

  bool                is_regular() const                       { return S_ISREG(m_stat.st_mode); }
  bool                is_directory() const                     { return S_ISDIR(m_stat.st_mode); }
  bool                is_character() const                     { return S_ISCHR(m_stat.st_mode); }
  bool                is_block() const                         { return S_ISBLK(m_stat.st_mode); }
  bool                is_fifo() const                          { return S_ISFIFO(m_stat.st_mode); }
  bool                is_link() const                          { return S_ISLNK(m_stat.st_mode); }
  bool                is_socket() const                        { return S_ISSOCK(m_stat.st_mode); }

  off_t               size() const                             { return m_stat.st_size; }

  time_t              access_time() const                      { return m_stat.st_atime; }
  time_t              change_time() const                      { return m_stat.st_ctime; }
  time_t              modified_time() const                    { return m_stat.st_mtime; }

private:
  struct stat         m_stat;
};

} // namespace torrent::utils

#endif // TORRENT_UTILS_FILE_STAT_H
