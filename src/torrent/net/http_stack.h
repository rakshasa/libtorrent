#ifndef TORRENT_NET_HTTP_GET_H
#define TORRENT_NET_HTTP_GET_H

#include <functional>
#include <memory>
#include <torrent/common.h>

namespace torrent::net {

class CurlStack;

class LIBTORRENT_EXPORT HttpStack {
public:
  HttpStack(utils::Thread* thread);
  ~HttpStack();

  void                shutdown();

  void                start_get(HttpGet& http_get);

  unsigned int        size() const;

  unsigned int        max_cache_connections() const;
  unsigned int        max_host_connections() const;
  unsigned int        max_total_connections() const;

  void                set_max_cache_connections(unsigned int value);
  void                set_max_host_connections(unsigned int value);
  void                set_max_total_connections(unsigned int value);

  std::string         user_agent() const;
  std::string         http_proxy() const;
  std::string         http_capath() const;
  std::string         http_cacert() const;

  void                set_user_agent(const std::string& s);
  void                set_http_proxy(const std::string& s);
  void                set_http_capath(const std::string& s);
  void                set_http_cacert(const std::string& s);

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

  std::unique_ptr<CurlStack> m_stack;
};

} // namespace torrent::net

#endif // TORRENT_NET_HTTP_GET_H
