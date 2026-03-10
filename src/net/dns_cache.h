#ifndef LIBTORRENT_NET_DNS_CACHE_H
#define LIBTORRENT_NET_DNS_CACHE_H

#include <array>
#include <deque>
#include <functional>
#include <string>

#include "net/dns_types.h"
#include "torrent/net/types.h"

namespace torrent::net {

// TODO: Break this into sin/sin6 cache entries.

struct DnsCacheEntry {
  sin_shared_ptr  sin;
  sin6_shared_ptr sin6;

  // TODO: Add error mode, where we do not attempt o resolve for a while.
  bool            updating{};
  bool            no_record{};

  std::chrono::minutes last_updated{};
  std::chrono::minutes last_failed_update{};
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

  std::unordered_map<DnsKey, DnsCacheEntry> m_cache;
};

} // namespace torrent::net

#endif // LIBTORRENT_NET_DNS_CACHE_H
