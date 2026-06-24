#ifndef LIBTORRENT_NET_PROXY_PROXY_MANAGER_H
#define LIBTORRENT_NET_PROXY_PROXY_MANAGER_H

#include <torrent/common.h>

namespace torrent::net::proxy {

class Proxy;

class ProxyManager {
public:
  ProxyManager();
  ~ProxyManager() = default;

  // For now, only add one address, derive type using libcurl's uri schema names:
  //
  // Need to be thread-safe.

  std::string         address();
  void                set_address(const std::string& address);

  // TODO: Use atomic to avoid locking for this.
  std::unique_ptr<Proxy> create_proxy();

private:
  auto                lock_guard();

  std::mutex            m_mutex;

  std::string             m_address;
  std::function<Proxy*()> m_create_proxy;
};

auto ProxyManager::lock_guard() { return std::scoped_lock(m_mutex); }

} // namespace torrent::net::proxy

#endif
