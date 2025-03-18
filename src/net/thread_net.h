#ifndef LIBTORRENT_NET_THREAD_NET_H
#define LIBTORRENT_NET_THREAD_NET_H

#include "torrent/utils/thread.h"

namespace torrent {

class UdnsEvent;

class ThreadNet : public utils::Thread {
public:
  ThreadNet();
  ~ThreadNet() override = default;

  const char*     name() const override { return "rtorrent net"; }

  void            init_thread() override;

  void            request_resolve(void* requester, const char *name, int family, std::function<void (const sockaddr*, int)>&& callback);
  void            cancel_resolve(void* requester);

protected:
  friend class torrent::net::Resolver;

  void            call_events() override;
  int64_t         next_timeout_usec() override;

  std::unique_ptr<UdnsEvent> m_udns;
};

extern ThreadNet* thread_net;

} // namespace torrent

#endif // LIBTORRENT_NET_THREAD_NET_H
