#ifndef LIBTORRENT_TORRENT_NET_NETWORK_CONFIG_H
#define LIBTORRENT_TORRENT_NET_NETWORK_CONFIG_H

#include <mutex>
#include <torrent/net/types.h>

namespace torrent::net {

class LIBTORRENT_EXPORT NetworkConfig {
public:
  NetworkConfig();

  // TODO: Calls using these should be thread safe, and a connection manager should be responsible
  // for binding, etc, fds.
  //
  // When we change the * addresses, we should lock the connection manager until both pre- and
  // post-change handling is done. This would include closing and reopening listening sockets,
  // resetting connections, restarting dht, etc.

  // TODO: Http bind address should be moved here.
  // TODO: Move helper functions in rtorrent manager here.

  auto                bind_address() const                    { return m_bind_address; }
  void                set_bind_address(const sockaddr* sa);

  auto                local_address() const                   { return m_local_address; }
  void                set_local_address(const sockaddr* sa);

  auto                proxy_address() const                   { return m_proxy_address; }
  void                set_proxy_address(const sockaddr* sa);

protected:
  // TODO: Lock guard and unsafe access to the addresses.

  void                lock() const                    { m_mutex.lock(); }
  auto                lock_guard() const              { return std::lock_guard(m_mutex); }
  void                unlock() const                  { m_mutex.unlock(); }
  auto&               mutex() const                   { return m_mutex; }

private:
  mutable std::mutex  m_mutex;

  c_sa_shared_ptr     m_bind_address;
  c_sa_shared_ptr     m_local_address;
  c_sa_shared_ptr     m_proxy_address;
};

} // namespace torrent::net

#endif
