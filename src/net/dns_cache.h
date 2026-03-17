#ifndef LIBTORRENT_NET_DNS_CACHE_H
#define LIBTORRENT_NET_DNS_CACHE_H

#include <array>
#include <deque>
#include <functional>
#include <list>
#include <string>

// #include "net/dns_types.h"
#include "torrent/net/types.h"

namespace torrent::net {

struct DnsCacheEntry;

using DnsCacheEntries   = std::unordered_map<std::string, DnsCacheEntry>;
using DnsCacheStaleness = std::list<std::reference_wrapper<DnsCacheEntry>>;

struct DnsCacheInfo {
  bool                 updating{};
  bool                 no_record{};
  std::chrono::minutes last_updated{};
  std::chrono::minutes last_failed_update{};

  DnsCacheEntries::iterator   cache_itr{};
  DnsCacheStaleness::iterator staleness_itr{};
};

struct DnsCacheEntry {
  sin_shared_ptr  sin_addr;
  DnsCacheInfo    sin_info;

  sin6_shared_ptr sin6_addr;
  DnsCacheInfo    sin6_info;
};

class DnsCache {
public:
  // TODO: Add different types of resolve, e.g. force, in_error, etc.

  void                resolve(void* requester, std::string hostname, int family, resolver_callback&& callback);

protected:
  friend class DnsBuffer;

  void                process_success(const std::string& hostname, int family, sin_shared_ptr result_sin, sin6_shared_ptr result_sin6);
  void                process_failure(const std::string& hostname, int family, int error);

private:
  void                cull_stale_entries();

  void                reset_updating(DnsCacheEntry& entry, int family);

  void                reset_sin_updated(DnsCacheEntry& entry, std::chrono::minutes current_time);
  void                reset_sin6_updated(DnsCacheEntry& entry, std::chrono::minutes current_time);
  void                reset_sin_failed(DnsCacheEntry& entry, std::chrono::minutes current_time);
  void                reset_sin6_failed(DnsCacheEntry& entry, std::chrono::minutes current_time);

  DnsCacheEntries     m_entries;
  DnsCacheStaleness   m_sin_staleness;
  DnsCacheStaleness   m_sin6_staleness;
};

} // namespace torrent::net

#endif // LIBTORRENT_NET_DNS_CACHE_H
