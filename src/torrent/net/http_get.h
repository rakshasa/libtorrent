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
  HttpGet(std::string url, std::shared_ptr<std::ostream> stream);
  ~HttpGet();

  HttpGet(const HttpGet&) = default;
  HttpGet& operator=(const HttpGet&) = default;

  bool                is_valid() const { return m_curl_get != nullptr; }

  // close_and_keep_callbacks() and close_and_cancel_callbacks() do not immediately close as they
  // use callbacks to thread_net. Some functions throw internal_error if they don't wait for close
  // to finish.
  void                close_and_keep_callbacks();
  void                close_and_cancel_callbacks(utils::Thread* callback_thread);

  // Always call before reset() if the HttpGet is being reused.
  void                wait_for_close();
  bool                try_wait_for_close();

  // Calling reset is not allowed while the HttpGet is in the stack. Does not clear slots or callbacks.
  void                reset(const std::string& url, std::shared_ptr<std::ostream> str);

  std::string         url() const;
  uint32_t            timeout() const;

  int64_t             size_done() const;
  int64_t             size_total() const;

  void                set_timeout(uint32_t seconds);

  void                use_ipv4();
  void                use_ipv6();
  void                prefer_ipv4();
  void                prefer_ipv6();

  // The slots add callbacks to the calling thread when triggered, and all slots will remain in the
  // thread's callback queue even if the underlying CurlGet is closed or deleted.
  //
  // Calling add_*_slot is not allowed while the HttpGet is in the stack.
  void                add_done_slot(const std::function<void()>& slot);
  void                add_failed_slot(const std::function<void(const std::string&)>& slot);
  // TODO: Add a closed_slot.

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
