#ifndef LIBTORRENT_NET_UDNSEVENT_H
#define LIBTORRENT_NET_UDNSEVENT_H

#include <map>

#include "rak/priority_queue_default.h"
#include "torrent/event.h"
#include "torrent/net/types.h"

struct dns_ctx;
struct dns_query;
struct dns_rr_a4;
struct dns_rr_a6;

namespace torrent {

// TODO: Rename to UdnsResolver.

class UdnsEvent : public Event {
public:
  typedef std::function<void (sin_shared_ptr, sin6_shared_ptr, int)> resolver_callback;

  struct Query {
    void*             requester;
    std::string       hostname;
    int               family;
    resolver_callback callback;

    UdnsEvent*        parent;
    bool              canceled{false};
    bool              deleted{false};
    ::dns_query*      a4_query{nullptr};
    ::dns_query*      a6_query{nullptr};

    sin_shared_ptr    result_sin;
    sin6_shared_ptr   result_sin6;
    int               error{0};
  };

  typedef std::multimap<void*, std::unique_ptr<Query>> query_map;

  UdnsEvent();
  ~UdnsEvent();

  const char*         type_name() const override { return "udns"; }

  // Callback must happen in thread_net and cannot call back into the resolver.
  void                resolve(void* requester, const std::string& hostname, int family, resolver_callback&& callback);

  // Cancel may block if the resolver received the response and is calling the callback.
  void                cancel(void* requester);

  void                flush();

  void                event_read() override;
  void                event_write() override;
  void                event_error() override;

protected:
  std::unique_ptr<Query> erase_query(query_map::iterator itr);
  query_map::iterator    find_query(Query* query);
  query_map::iterator    find_malformed_query(Query* query);

  void                process_canceled();
  void                process_timeouts();

  static void         a4_callback_wrapper(struct ::dns_ctx *ctx, ::dns_rr_a4 *result, void *data);
  static void         a6_callback_wrapper(struct ::dns_ctx *ctx, ::dns_rr_a6 *result, void *data);
  static void         process_result(query_map::iterator itr);

  static bool         m_initialized;

  ::dns_ctx*          m_ctx{nullptr};
  rak::priority_item  m_taskTimeout;

  std::mutex          m_mutex;
  query_map           m_queries;
  query_map           m_malformed_queries;
};

} // namespace torrent

#endif // LIBTORRENT_NET_UDNSEVENT_H
