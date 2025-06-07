#ifndef TORRENT_NET_RESOLVER_H
#define TORRENT_NET_RESOLVER_H

#include <functional>
#include <thread>

#include <torrent/common.h>
#include <torrent/net/types.h>

namespace torrent::net {

class LIBTORRENT_EXPORT Resolver {
public:
  using both_callback   = std::function<void(c_sin_shared_ptr, c_sin6_shared_ptr, int)>;
  using single_callback = std::function<void(c_sa_shared_ptr, int)>;

  Resolver() = default;
  ~Resolver() = default;

  // May be called from any thread.
  void                resolve_both(void* requester, const std::string& hostname, int family, both_callback&& callback);
  void                resolve_preferred(void* requester, const std::string& hostname, int family, int preferred, single_callback&& callback);
  void                resolve_specific(void* requester, const std::string& hostname, int family, single_callback&& callback);

  // Must be called from the owning thread.
  void                cancel(void* requester);

protected:
  friend class utils::Thread;

  void                init();

private:
  Resolver(const Resolver&) = delete;
  Resolver& operator=(const Resolver&) = delete;

  utils::Thread* m_thread{nullptr};
};

} // namespace torrent::net

#endif
