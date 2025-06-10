#ifndef TORRENT_NET_HTTP_GET_H
#define TORRENT_NET_HTTP_GET_H

#include <functional>
#include <memory>
#include <thread>
#include <torrent/common.h>

namespace torrent::net {

class CurlStack;

class LIBTORRENT_EXPORT HttpStack {
public:
  HttpStack(utils::Thread* thread);
  ~HttpStack();

  unsigned int        active() const;
  unsigned int        max_active() const;
  void                set_max_active(unsigned int a);

  std::string         user_agent() const;
  std::string         http_proxy() const;
  std::string         bind_address() const;
  std::string         http_capath() const;
  std::string         http_cacert() const;

  void                set_user_agent(std::string s);
  void                set_http_proxy(std::string s);
  void                set_bind_address(std::string s);
  void                set_http_capath(std::string s);
  void                set_http_cacert(std::string s);

  bool                ssl_verify_host() const;
  bool                ssl_verify_peer() const;

  void                set_ssl_verify_host(bool s);
  void                set_ssl_verify_peer(bool s);

  long                dns_timeout() const;
  void                set_dns_timeout(long timeout);

protected:
  friend class HttpGet;

  CurlStack*          curl_stack() { return m_stack.get(); }

private:
  HttpStack() = delete;
  HttpStack(const HttpStack&) = delete;
  HttpStack& operator=(const HttpStack&) = delete;

  utils::Thread*             m_thread{nullptr};
  std::unique_ptr<CurlStack> m_stack;
};

} // namespace torrent::net

#endif // TORRENT_NET_HTTP_GET_H
