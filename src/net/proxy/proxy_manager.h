#ifndef LIBTORRENT_NET_PROXY_PROXY_MANAGER_H
#define LIBTORRENT_NET_PROXY_PROXY_MANAGER_H

#include <net/proxy/proxy.h>
#include <torrent/common.h>

namespace torrent::net::proxy {

class ProxyManager {
public:
  ProxyManager();
  ~ProxyManager() = default;

  // For now, only add one address, derive type using libcurl's uri schema names:
  //
  // Need to be thread-safe.

  bool                is_udp_blocked() const;

  std::string         proxy_uri();
  void                set_proxy_uri(const std::string& uri);

  auto                create_proxy(const sockaddr* connect_sa);

private:
  using create_proxy_func = std::function<Proxy*(const sockaddr*)>;

  auto                lock_guard();

  std::unique_ptr<Proxy> create_proxy_safe(const sockaddr* connect_sa);

  // TODO: Add atomic flags for has proxy and blocks udp.
  std::atomic<bool>   m_has_proxy{};
  std::atomic<bool>   m_blocks_udp{};

  align_cacheline std::mutex m_mutex;

  std::string         m_proxy_uri;
  create_proxy_func   m_create_proxy;
};

bool ProxyManager::is_udp_blocked() const { return m_blocks_udp; }
auto ProxyManager::lock_guard()           { return std::scoped_lock(m_mutex); }

auto
ProxyManager::create_proxy(const sockaddr* connect_sa) {
  if (!m_has_proxy)
    return std::unique_ptr<Proxy>{};

  return create_proxy_safe(connect_sa);
}

} // namespace torrent::net::proxy

#endif
