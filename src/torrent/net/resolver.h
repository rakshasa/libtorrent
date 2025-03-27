#ifndef TORRENT_NET_RESOLVER_H
#define TORRENT_NET_RESOLVER_H

#include <functional>
#include <thread>

#include <torrent/common.h>
#include <torrent/net/types.h>

namespace torrent::net {

class LIBTORRENT_EXPORT Resolver {
public:
  typedef std::function<void (c_sin_shared_ptr, c_sin6_shared_ptr, int)> both_callback;
  typedef std::function<void (c_sa_shared_ptr, int)>                     single_callback;

  Resolver() = default;
  ~Resolver() = default;

  // May be called from any thread.
  void                resolve_both(void* requester, const std::string& hostname, int family, both_callback&& callback);
  void                resolve_preferred(void* requester, const std::string& hostname, int family, int preferred, single_callback&& callback);
  void                resolve_specific(void* requester, const std::string& hostname, int family, single_callback&& callback);

  // Must be called from the owning thread.
  void                cancel(void* requester);

protected:
  friend class torrent::utils::Thread;

  void                init();

private:
  Resolver(const Resolver&) = delete;
  Resolver& operator=(const Resolver&) = delete;

  torrent::utils::Thread* m_thread{nullptr};
};

}

#endif
