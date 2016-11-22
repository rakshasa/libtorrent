#ifndef LIBTORRENT_NET_UDNSEVENT_H
#define LIBTORRENT_NET_UDNSEVENT_H

#include lt_tr1_functional

#include <list>
#include <inttypes.h>

#include <rak/priority_queue_default.h>
#include "torrent/event.h"
#include "torrent/connection_manager.h"

struct dns_ctx;
struct dns_query;

namespace torrent {

struct udns_query {
    ::dns_query *a4_query;
    ::dns_query *a6_query;
    resolver_callback  *callback;
    int                 error;
};

class UdnsEvent : public Event {
public:

  typedef std::vector<udns_query*> query_list_type;

  UdnsEvent();
  ~UdnsEvent();

  virtual void        event_read();
  virtual void        event_write();
  virtual void        event_error();
  virtual const char* type_name();

  // wraps udns's dns_submit_a[46] functions. they and it return control immediately,
  // without either sending outgoing UDP packets or executing callbacks:
  udns_query*         enqueue_resolve(const char *name, int family, resolver_callback *callback);
  // wraps the dns_timeouts function. it sends packets and can execute arbitrary
  // callbacks:
  void                flush_resolves();
  // wraps the dns_cancel function:
  void                cancel(udns_query *query);

protected:
  void                process_timeouts();

  ::dns_ctx*             m_ctx;
  rak::priority_item     m_taskTimeout;
  query_list_type        m_malformed_queries;
};

}

#endif
