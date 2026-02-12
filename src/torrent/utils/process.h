#ifndef LIBTORRENT_TORRENT_UTILS_PROCESS_H
#define LIBTORRENT_TORRENT_UTILS_PROCESS_H

#include <sys/types.h>
#include <torrent/common.h>

// TODO: Add to common.h
namespace torrent::shm {
class Channel;
class Router;
class Segment;
} // namespace torrent::shm

namespace torrent::utils {

class ProcessInternal;

class LIBTORRENT_EXPORT Process {
public:
  Process();
  virtual ~Process();

  pid_t               process_id() const;

  void                start_process();

protected:
  pid_t               m_pid{};

  std::unique_ptr<shm::Segment> m_read_segment;
  std::unique_ptr<shm::Router>  m_read_router;

  std::unique_ptr<shm::Segment> m_write_segment;
  std::unique_ptr<shm::Router>  m_write_router;
};

inline pid_t Process::process_id() const { return m_pid; }

} // namespace torrent::utils

#endif // LIBTORRENT_TORRENT_UTILS_PROCESS_H
