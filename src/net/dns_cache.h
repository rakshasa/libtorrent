#ifndef LIBTORRENT_NET_DNS_CACHE_H
#define LIBTORRENT_NET_DNS_CACHE_H

#include <array>
#include <deque>
#include <functional>
#include <string>

// #include "net/dns_types.h"
#include "torrent/net/types.h"

namespace torrent::net {

struct DnsCacheInfo {
  void reset_state();
  void reset_updated(std::chrono::minutes current_time);
  void reset_failed(std::chrono::minutes current_time);

  bool                 updating{};
  bool                 no_record{};
  std::chrono::minutes last_updated{};
  std::chrono::minutes last_failed_update{};
};

struct DnsCacheEntry {
  void reset_updating(int family);

  sin_shared_ptr  sin_addr;
  DnsCacheInfo    sin_info;

  sin6_shared_ptr sin6_addr;
  DnsCacheInfo    sin6_info;
};

class DnsCache {
public:
  // TODO: Add different types of resolve, e.g. force, in_error, etc.

  void                resolve(void* requester, const std::string& hostname, int family, resolver_callback&& callback);

protected:
  friend class DnsBuffer;

  void                process_success(const std::string& hostname, int family, sin_shared_ptr result_sin, sin6_shared_ptr result_sin6);
  void                process_failure(const std::string& hostname, int family, int error);

private:
  std::unordered_map<std::string, DnsCacheEntry> m_cache;
};

} // namespace torrent::net

#endif // LIBTORRENT_NET_DNS_CACHE_H
