#ifndef LIBTORRENT_NET_DNS_BUFFER_H
#define LIBTORRENT_NET_DNS_BUFFER_H

// TODO: Review headers

#include <array>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "torrent/net/types.h"

namespace torrent::net {

// It is assume that 'requester' does very few simultaneous requests, so searching for pending
// queries is not too expensive.

struct DnsBufferRequester;

struct DnsBufferCallback {
  std::weak_ptr<DnsBufferRequester> requester;
  resolver_callback                 callback;
};

struct DnsBufferQuery {
  // Family set to '-1' indicates an empty query.
  int                            family{-1};
  std::string                    hostname;
  std::vector<DnsBufferCallback> callbacks;
};

struct DnsBufferRequester {
  unsigned int active_query_count{};
  unsigned int pending_query_count{};
};

class DnsBuffer {
public:
  constexpr static int max_requests = 8;

  ~DnsBuffer();

  // The 'fn' callback must do work in the originating thread using callbacks with 'requester'.
  void                resolve(void* requester, const std::string& hostname, int family, resolver_callback&& fn);

  // Calling thread must cancel callbacks with 'requester' afterwards.
  void                cancel_safe(void* requester);

  // TODO: Add a slot for completed queries so we can add the entry to the cache before calling the
  // list of callbacks.

private:
  using active_query_list = std::array<DnsBufferQuery, max_requests>;
  using requester_list    = std::map<void*, std::shared_ptr<DnsBufferRequester>>;

  void                activate_pending_query();
  void                activate_and_resolve_query(DnsBufferQuery query);

  void                process(unsigned int index, sin_shared_ptr result_sin, int error_sin, sin6_shared_ptr result_sin6, int error_sin6);
  void                process_callback(DnsBufferCallback& callback, sin_shared_ptr result_sin, int error_sin, sin6_shared_ptr result_sin6, int error_sin6);

  void*               requester_from_index(unsigned int index);

  unsigned int              m_active_query_count{};
  active_query_list         m_active_queries;
  std::list<DnsBufferQuery> m_pending_queries;

  std::mutex                m_requesters_mutex;
  requester_list            m_requesters;
};

} // namespace torrent::net

#endif // LIBTORRENT_NET_DNS_BUFFER_H
