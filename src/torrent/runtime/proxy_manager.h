#ifndef LIBTORRENT_TORRENT_RUNTIME_PROXY_MANAGER_H
#define LIBTORRENT_TORRENT_RUNTIME_PROXY_MANAGER_H

#include <torrent/net/types.h>

namespace torrent::runtime {

class LIBTORRENT_EXPORT ProxyManager {
public:
  // Currently not needed, use if we allow hostnames in the future.
  // static constexpr uint32_t max_proxy/connect_url_length = 256;

  ProxyManager();
  ~ProxyManager() = default;

  // For now, only add one address, derive type using libcurl's url schema names:
  //
  // Need to be thread-safe.

  bool                is_udp_blocked() const;

  std::string         proxy_url();
  void                set_proxy_url(const std::string& url);

  std::string         http_proxy_url();
  void                set_http_proxy_url(const std::string& url);

  net::proxy_ptr      create_proxy(const sockaddr* connect_sa);

private:
  using create_proxy_func = std::function<net::proxy::Proxy*(const sockaddr*)>;

  auto                lock_guard();

  std::atomic<bool>   m_has_proxy{};
  std::atomic<bool>   m_blocks_udp{};

  align_cacheline std::mutex m_mutex;

  std::string         m_proxy_url;
  std::string         m_http_proxy_url;

  create_proxy_func   m_create_proxy;
};

ProxyManager* proxy_manager() LIBTORRENT_EXPORT;

inline bool        ProxyManager::is_udp_blocked() const { return m_blocks_udp; }
inline std::string ProxyManager::proxy_url()            { return m_proxy_url; }
inline std::string ProxyManager::http_proxy_url()       { return m_http_proxy_url; }
inline auto        ProxyManager::lock_guard()           { return std::scoped_lock(m_mutex); }

} // namespace torrent::net::proxy

#endif
