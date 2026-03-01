#ifndef LIBTORRENT_NET_DNS_BUFFER_H
#define LIBTORRENT_NET_DNS_BUFFER_H

#include <array>
#include <functional>
#include <list>
#include <map>
#include <string>
#include <vector>

#include "net/dns_types.h"
#include "torrent/net/types.h"

namespace torrent::net {

// TODO: Currently this is just a buffer for the resolver, however we also want a cache so depending
// on how cleanly it can be implemented we might deprecate this intermediate class.

struct DnsBufferQuery {
  int               family{-1};
  std::string       hostname;

  // Empty callbacks indicates an inactive query slot.
  std::vector<std::pair<void*, resolver_callback>> callbacks;
};

struct DnsBufferRequester {
  std::vector<unsigned int>                        active_queries;
  std::vector<std::list<DnsBufferQuery>::iterator> pending_queries;
};

class DnsBuffer {
public:
  constexpr static int max_requests = 8;

  DnsBuffer();
  ~DnsBuffer();

  void                resolve(void* requester, const std::string& hostname, int family, resolver_callback&& callback);

  void                cancel(void* requester);

  // TODO: Add a slot for completed queries so we can add the entry to the cache before calling the
  // list of callbacks.

private:
  using active_query_list = std::array<DnsBufferQuery, max_requests>;
  using requester_list    = std::map<void*, DnsBufferRequester>;

  unsigned int        activate_query(DnsBufferQuery query);
  void                activate_new_query(void* requester, DnsBufferQuery query);
  void                activate_pending_query();

  void*               requester_from_index(unsigned int index) const;

  void                process(unsigned int index, sin_shared_ptr result_sin, sin6_shared_ptr result_sin6, int error);

  unsigned int              m_active_query_count{};
  active_query_list         m_active_queries;
  std::list<DnsBufferQuery> m_pending_queries;

  requester_list            m_requesters;
};

} // namespace torrent::net

#endif // LIBTORRENT_NET_DNS_BUFFER_H
