#ifndef LIBTORRENT_TORRENT_SYSTEM_SPAWN_PROCESS_H
#define LIBTORRENT_TORRENT_SYSTEM_SPAWN_PROCESS_H

#include <torrent/system/common.h>

namespace torrent::system {

class LIBTORRENT_EXPORT SpawnProcess {
public:
  SpawnProcess() = default;
  ~SpawnProcess();

  void                set_background(bool background);
  void                set_capture_output(bool capture);

  void                set_log_fd(int fd);

  int                 execute(const char* path, char* const argv[]);

  std::string         capture();

private:
  void                close_fds();

  bool                m_background{};
  bool                m_capture_output{};

  int                 m_log_fd{-1};
  int                 m_parent_fd{-1};
  int                 m_child_fd{-1};

  pid_t               m_child_pid{};
};

inline void SpawnProcess::set_background(bool background)  { m_background = background; }
inline void SpawnProcess::set_capture_output(bool capture) { m_capture_output = capture; }
inline void SpawnProcess::set_log_fd(int fd)               { m_log_fd = fd; }

} // namespace torrent::system

#endif
