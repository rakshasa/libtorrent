#ifndef LIBTORRENT_NET_DNS_CACHE_H
#define LIBTORRENT_NET_DNS_CACHE_H

#include <array>
#include <deque>
#include <functional>
#include <string>

#include "torrent/net/types.h"

namespace torrent::net {

// TODO: Currently this is just a cache for the resolver, however we also want a cache so depending
// on how cleanly it can be implemented we might deprecate this intermediate class.

struct DnsCacheEntry {
  void*             requester{};
  std::string       hostname;
  int               family{};

  // Null callback indicates an inactive query slot.
  resolver_callback callback;
};

class DnsCache {
public:
  DnsCache();
  ~DnsCache();

  void                resolve(void* requester, const std::string& hostname, int family, resolver_callback&& callback);

  // TODO: Remove this, and only allow complete cancellation of all requests. (once we have a cache)
  void                cancel(void* requester);

private:
  void                activate_query(DnsCacheQuery query);

  void                process(unsigned int index, sin_shared_ptr result_sin, sin6_shared_ptr result_sin6, int error);

  // utils::Thread*      m_thread{};

  std::unordered_map<DnsCacheKey, Dns
};

} // namespace torrent::net

#endif // LIBTORRENT_NET_DNS_CACHE_H
