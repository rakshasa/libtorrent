#ifndef LIBTORRENT_NET_THREAD_NET_H
#define LIBTORRENT_NET_THREAD_NET_H

#include "torrent/common.h"
#include "torrent/utils/thread.h"

namespace torrent {

class UdnsResolver;

namespace net {
class HttpStack;
} // namespace net

class LIBTORRENT_EXPORT ThreadNet : public utils::Thread {
public:
  ~ThreadNet() override;

  static void         create_thread();
  static ThreadNet*   thread_net();

  const char*         name() const override { return "rtorrent net"; }

  void                init_thread() override;
  void                cleanup_thread() override;

protected:
  friend class ThreadNetInternal;
  friend class torrent::net::HttpStack;
  friend class torrent::net::Resolver;

  ThreadNet() = default;

  static auto         internal_thread_net() { return m_thread_net; }

  void                      call_events() override;
  std::chrono::microseconds next_timeout() override;

  const auto&         http_stack() const { return m_http_stack; }
  const auto&         udns() const       { return m_udns; }

private:
  static ThreadNet*   m_thread_net;

  std::unique_ptr<net::HttpStack> m_http_stack;
  std::unique_ptr<UdnsResolver>   m_udns;
};

} // namespace torrent

#endif // LIBTORRENT_NET_THREAD_NET_H
