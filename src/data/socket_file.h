#ifndef LIBTORRENT_SOCKET_FILE_H
#define LIBTORRENT_SOCKET_FILE_H

#include <string>
#include <cinttypes>
#include <fcntl.h>
#include <sys/types.h>

#include "memory_chunk.h"

namespace torrent {

class SocketFile {
public:
  using fd_type = int;

  static constexpr fd_type invalid_fd         = -1;

  static constexpr int o_create               = O_CREAT;
  static constexpr int o_truncate             = O_TRUNC;
  static constexpr int o_nonblock             = O_NONBLOCK;

  static constexpr int flag_fallocate          = (1 << 0);
  static constexpr int flag_fallocate_blocking = (1 << 1);

  SocketFile() = default;
  ~SocketFile() = default;
  SocketFile(fd_type fd) : m_fd(fd) {}

  bool                is_open() const                                   { return m_fd != invalid_fd; }

  bool                open(const std::string& path, int prot, int flags, mode_t mode = 0666);
  void                close();

  uint64_t            size() const;
  bool                set_size(uint64_t size) const;

  bool                allocate(uint64_t size, int flags = 0) const;

  static MemoryChunk  create_padding_chunk(uint32_t length, int prot, int flags);
  MemoryChunk         create_chunk(uint64_t offset, uint32_t length, int prot, int flags) const;

  fd_type             fd() const                                        { return m_fd; }

private:
  SocketFile(const SocketFile&) = delete;
  SocketFile& operator=(const SocketFile&) = delete;

  // Use custom flags if stuff like file locking etc is implemented.
  fd_type             m_fd{invalid_fd};
};

} // namespace torrent

#endif

