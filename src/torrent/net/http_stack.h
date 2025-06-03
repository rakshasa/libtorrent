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

  // May be called from any thread.
  // void                resolve_both(void* requester, const std::string& hostname, int family, both_callback&& callback);

  // Must be called from the owning thread.
  // void                cancel(void* requester);

private:
  HttpStack() = delete;
  HttpStack(const HttpStack&) = delete;
  HttpStack& operator=(const HttpStack&) = delete;

  utils::Thread*             m_thread{nullptr};
  std::unique_ptr<CurlStack> m_stack;
};

} // namespace torrent::net

#endif // TORRENT_NET_HTTP_GET_H
