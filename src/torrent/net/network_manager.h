#ifndef LIBTORRENT_TORRENT_NET_NETWORK_MANAGER_H
#define LIBTORRENT_TORRENT_NET_NETWORK_MANAGER_H

#include <mutex>
#include <torrent/net/types.h>

namespace torrent {

class Manager;

} // namespace torrent

namespace torrent::net {

class LIBTORRENT_EXPORT NetworkManager {
public:
  NetworkManager();
  ~NetworkManager();

  // TODO: Change to have network_config hold the port range / random info.

  bool                listen_open(uint16_t first, uint16_t last);
  void                listen_close();

  int                 listen_backlog() const;
  void                set_listen_backlog(int backlog);

protected:
  friend class torrent::Manager;

  void                lock() const                    { m_mutex.lock(); }
  auto                lock_guard() const              { return std::lock_guard(m_mutex); }
  void                unlock() const                  { m_mutex.unlock(); }
  auto&               mutex() const                   { return m_mutex; }

  auto                listen_inet_unsafe()            { return m_listen_inet.get(); }
  auto                listen_inet6_unsafe()           { return m_listen_inet6.get(); }

private:
  bool                open_listen_on_address(c_sa_shared_ptr& bind_address, uint16_t begin, uint16_t end);

  mutable std::mutex  m_mutex;

  std::unique_ptr<Listen> m_listen_inet;
  std::unique_ptr<Listen> m_listen_inet6;
  int                     m_listen_backlog{SOMAXCONN};
};

} // namespace torrent::net

#endif // LIBTORRENT_TORRENT_NET_NETWORK_MANAGER_H
