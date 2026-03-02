#ifndef LIBTORRENT_NET_THREAD_NET_H
#define LIBTORRENT_NET_THREAD_NET_H

#include "torrent/common.h"
#include "torrent/utils/thread.h"

namespace torrent {

namespace net {

class DnsBuffer;
class HttpStack;
class UdnsResolver;

} // namespace net

class LIBTORRENT_EXPORT ThreadNet : public utils::Thread {
public:
  ~ThreadNet() override;

  static void         create_thread();
  static void         destroy_thread();
  static ThreadNet*   thread_net();

  const char*         name() const override { return "rtorrent net"; }

  void                init_thread() override;
  void                init_thread_post_local() override;
  void                cleanup_thread() override;

protected:
  friend class ThreadNetInternal;
  friend class torrent::net::DnsBuffer;
  friend class torrent::net::HttpStack;
  friend class torrent::net::Resolver;

  ThreadNet() = default;

  static auto         internal_thread_net() { return m_thread_net; }

  void                      call_events() override;
  std::chrono::microseconds next_timeout() override;

  net::HttpStack*     http_stack() const   { return m_http_stack.get(); }
  net::DnsBuffer*     dns_buffer() const   { return m_dns_buffer.get(); }
  net::UdnsResolver*  dns_resolver() const { return m_dns_resolver.get(); }

private:
  static ThreadNet*   m_thread_net;

  std::unique_ptr<net::HttpStack>    m_http_stack;
  std::unique_ptr<net::DnsBuffer>    m_dns_buffer;
  std::unique_ptr<net::UdnsResolver> m_dns_resolver;
};

} // namespace torrent

#endif // LIBTORRENT_NET_THREAD_NET_H
