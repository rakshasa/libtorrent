#ifndef LIBTORRENT_TORRENT_UTILS_PROCESS_H
#define LIBTORRENT_TORRENT_UTILS_PROCESS_H

#include <sys/types.h>
#include <torrent/common.h>

// TODO: Add to common.h
namespace torrent::shm {

class Router;
class RouterFactory;

} // namespace torrent::shm

namespace torrent::system {

class ProcessInternal;

class LIBTORRENT_EXPORT Process {
public:
  Process();
  ~Process();

  pid_t               process_id() const;

  shm::Router*        router();

  void                start(std::function<void()> fn);

protected:
  pid_t               m_pid{};

  std::unique_ptr<shm::Router>        m_router;
  std::unique_ptr<shm::RouterFactory> m_router_factory;
};

inline pid_t        Process::process_id() const { return m_pid; }
inline shm::Router* Process::router()           { return m_router.get(); }

} // namespace torrent::utils

#endif // LIBTORRENT_TORRENT_UTILS_PROCESS_H
