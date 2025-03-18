#ifndef LIBTORRENT_NET_UDNSEVENT_H
#define LIBTORRENT_NET_UDNSEVENT_H

#include <map>

#include "rak/priority_queue_default.h"
#include "torrent/event.h"

struct dns_ctx;
struct dns_query;
struct dns_rr_a4;
struct dns_rr_a6;

namespace torrent {

class UdnsEvent : public Event {
public:
  typedef std::function<void (const sockaddr*, int)> resolver_callback;

  struct Query {
    void*             requester;
    UdnsEvent*        parent;
    ::dns_query*      a4_query;
    ::dns_query*      a6_query;
    resolver_callback callback;
    int               error;
  };

  typedef std::map<void*, std::unique_ptr<Query>> query_map;

  UdnsEvent();
  ~UdnsEvent();

  const char*         type_name() const override { return "udns"; }

  void                enqueue_resolve(void* requester, const char *name, int family, resolver_callback&& callback);
  void                cancel(void* requester);

  void                flush();

  void                event_read() override;
  void                event_write() override;
  void                event_error() override;

protected:
  void                process_timeouts();

  static void         a4_callback_wrapper(struct ::dns_ctx *ctx, ::dns_rr_a4 *result, void *data);
  static void         a6_callback_wrapper(struct ::dns_ctx *ctx, ::dns_rr_a6 *result, void *data);

  static bool         m_initialized;

  ::dns_ctx*          m_ctx{nullptr};
  rak::priority_item  m_taskTimeout;

  std::mutex          m_mutex;
  query_map           m_queries;
  query_map           m_malformed_queries;
};

} // namespace torrent

#endif // LIBTORRENT_NET_UDNSEVENT_H
