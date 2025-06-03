#ifndef TORRENT_NET_HTTP_GET_H
#define TORRENT_NET_HTTP_GET_H

#include <functional>
#include <memory>
#include <thread>
#include <torrent/common.h>

namespace torrent::net {

class CurlGet;

class LIBTORRENT_EXPORT HttpGet {
public:
  HttpGet() = default;
  ~HttpGet() = default;

  // May be called from any thread.
  // void                resolve_both(void* requester, const std::string& hostname, int family, both_callback&& callback);

  // Must be called from the owning thread.
  // void                cancel(void* requester);

private:
  HttpGet(const HttpGet&) = delete;
  HttpGet& operator=(const HttpGet&) = delete;

  // utils::Thread* m_thread{nullptr};

  std::shared_ptr<CurlGet> m_curl_get;
};

} // namespace torrent::net

#endif // TORRENT_NET_HTTP_GET_H
