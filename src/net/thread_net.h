#ifndef LIBTORRENT_NET_THREAD_NET_H
#define LIBTORRENT_NET_THREAD_NET_H

#include "torrent/utils/thread.h"

namespace torrent {

class UdnsResolver;

class ThreadNet : public utils::Thread {
public:
  ThreadNet();
  ~ThreadNet() override = default;

  const char*         name() const override { return "rtorrent net"; }

  void                init_thread() override;

protected:
  friend class torrent::net::Resolver;

  void                call_events() override;
  int64_t             next_timeout_usec() override;

  UdnsResolver*       udns() const { return m_udns.get(); }

private:
  std::unique_ptr<UdnsResolver> m_udns;
};

extern ThreadNet* thread_net;

} // namespace torrent

#endif // LIBTORRENT_NET_THREAD_NET_H
