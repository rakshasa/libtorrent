#ifndef LIBTORRENT_NET_UDNSEVENT_H
#define LIBTORRENT_NET_UDNSEVENT_H

#include <list>
#include <inttypes.h>
#include lt_tr1_functional

#include <udns.h>

#include <rak/priority_queue_default.h>
#include "torrent/common.h"
#include "torrent/event.h"

namespace torrent {

class LIBTORRENT_EXPORT UdnsEvent : public Event {
public:

  // TODO clean up original typedef slot_resolver_result_type in connection_manager.h
  typedef std::function<void (const sockaddr*, int)> a4_callback_type;

  UdnsEvent();
  ~UdnsEvent();

  virtual void        event_read();
  virtual void        event_write();
  virtual void        event_error();
  virtual const char* type_name();

  struct ::dns_query *resolve_in4(const char *name, a4_callback_type *callback);
  void process_timeouts();
  void cancel(struct ::dns_query *query);

protected:
  dns_ctx*            m_ctx;
  rak::priority_item  m_taskTimeout;
};

}

#endif
