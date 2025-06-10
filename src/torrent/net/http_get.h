#ifndef LIBTORRENT_TORRENT_NET_HTTP_GET_H
#define LIBTORRENT_TORRENT_NET_HTTP_GET_H

#include <functional>
#include <iosfwd>
#include <memory>
#include <string>
#include <thread>
#include <torrent/common.h>

namespace torrent::net {

class CurlGet;
class CurlStack;
class HttpStack;

class LIBTORRENT_EXPORT HttpGet {
public:
  HttpGet();
  HttpGet(const std::string& url, std::iostream* s = nullptr);
  ~HttpGet() = default;

  HttpGet(const HttpGet&) = default;
  HttpGet& operator=(const HttpGet&) = default;

  bool                is_valid() const { return m_curl_get != nullptr; }

  void                start(HttpStack* stack);
  void                close();

  std::string         url() const;
  std::iostream*      stream();
  uint32_t            timeout() const;

  void                set_url(std::string url);
  void                set_stream(std::iostream* str);
  void                set_timeout(uint32_t seconds);

  int64_t             size_done() const;
  int64_t             size_total() const;

  void                add_done_slot(const std::function<void()>& slot);
  void                add_failed_slot(const std::function<void(const std::string&)>& slot);

  bool                operator<(const HttpGet& other) const;
  bool                operator==(const HttpGet& other) const;

protected:
  friend class HttpStack;

  auto                curl_get() { return m_curl_get; }

private:
  std::shared_ptr<CurlGet> m_curl_get;
};

inline bool
HttpGet::operator<(const HttpGet& other) const {
  return m_curl_get < other.m_curl_get;
}

inline bool
HttpGet::operator==(const HttpGet& other) const {
  return m_curl_get == other.m_curl_get;
}

} // namespace torrent::net

#endif // TORRENT_NET_HTTP_GET_H
