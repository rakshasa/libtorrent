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
  HttpGet(const std::string& url, std::shared_ptr<std::ostream> stream);
  ~HttpGet();

  HttpGet(const HttpGet&) = default;
  HttpGet& operator=(const HttpGet&) = default;

  bool                is_valid() const { return m_curl_get != nullptr; }

  void                close();

  std::string         url() const;
  uint32_t            timeout() const;

  int64_t             size_done() const;
  int64_t             size_total() const;

  // Calling reset or add_*_slot is not allowed while the HttpGet is in the stack.
  //
  // Reset does not clear slots.
  void                reset(const std::string& url, std::shared_ptr<std::ostream> str);

  void                set_timeout(uint32_t seconds);

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
