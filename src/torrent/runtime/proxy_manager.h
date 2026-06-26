#ifndef LIBTORRENT_TORRENT_RUNTIME_PROXY_MANAGER_H
#define LIBTORRENT_TORRENT_RUNTIME_PROXY_MANAGER_H

#include <torrent/net/types.h>

namespace torrent::runtime {

class ProxyManager {
public:
  ProxyManager();
  ~ProxyManager() = default;

  // For now, only add one address, derive type using libcurl's uri schema names:
  //
  // Need to be thread-safe.

  bool                is_udp_blocked() const;

  std::string         proxy_uri();
  std::string         http_proxy_uri();

  void                set_proxy_uri(const std::string& uri);

  net::proxy_ptr      create_proxy(const sockaddr* connect_sa);

private:
  using create_proxy_func = std::function<net::proxy::Proxy*(const sockaddr*)>;

  auto                lock_guard();

  std::atomic<bool>   m_has_proxy{};
  std::atomic<bool>   m_blocks_udp{};

  align_cacheline std::mutex m_mutex;

  std::string         m_proxy_uri;
  create_proxy_func   m_create_proxy;
};

ProxyManager* proxy_manager() LIBTORRENT_EXPORT;

bool ProxyManager::is_udp_blocked() const { return m_blocks_udp; }
auto ProxyManager::lock_guard()           { return std::scoped_lock(m_mutex); }

} // namespace torrent::net::proxy

#endif
