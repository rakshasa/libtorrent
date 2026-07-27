#ifndef LIBTORRENT_TORRENT_SYSTEM_IPC_FACTORY_H
#define LIBTORRENT_TORRENT_SYSTEM_IPC_FACTORY_H

#include <memory>
#include <torrent/common.h>
#include <torrent/system/ipc/router.h>
#include <torrent/system/ipc/segment.h>

// Holds the everything needed to create a Router.

namespace torrent::system::ipc {

class Channel;
class Router;
class Segment;

class RouterFactory {
public:
  RouterFactory() = default;
  ~RouterFactory() = default;

  void                    initialize(uint32_t segment_size);

  std::unique_ptr<Router> create_parent_router();
  std::unique_ptr<Router> create_child_router();

private:
  int                 m_control_fd_1{};
  int                 m_control_fd_2{};

  int                 m_keepalive_fd_1{};
  int                 m_keepalive_fd_2{};

  // TODO: Copy move these to router.
  std::unique_ptr<Segment> m_segment_1;
  std::unique_ptr<Segment> m_segment_2;
};

} // namespace torrent::system::ipc

#endif
