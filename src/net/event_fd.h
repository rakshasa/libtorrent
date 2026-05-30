#ifndef LIBTORRENT_NET_EVENT_FD_H
#define LIBTORRENT_NET_EVENT_FD_H

#include "torrent/event.h"

namespace torrent::net {

class EventFd : public Event {
public:
  EventFd() = default;

  const char*         type_name() const override { return "eventfd"; }

  void                add_to_poll();
  void                remove_from_poll(system::Poll* poll);

  void                send_signal();

protected:
  void                event_read() override;
  void                event_write() override;
  void                event_error() override;
};

} // namespace torrent::net

#endif
