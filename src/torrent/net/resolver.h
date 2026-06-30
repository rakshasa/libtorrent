#ifndef TORRENT_NET_RESOLVER_H
#define TORRENT_NET_RESOLVER_H

#include <functional>
#include <thread>

#include <torrent/common.h>
#include <torrent/net/types.h>

namespace RTORRENT_EXPORT torrent {

namespace net {

class Resolver {
public:
  using both_callback   = std::function<void(c_sin_shared_ptr, int, c_sin6_shared_ptr, int)>;
  using single_callback = std::function<void(c_sa_shared_ptr, int)>;

  Resolver() = default;
  ~Resolver() = default;

  void                resolve_both(system::callback_id& id, const std::string& hostname, int family, both_callback&& callback);
  void                resolve_preferred(system::callback_id& id, const std::string& hostname, int family, int preferred, single_callback&& callback);
  void                resolve_specific(system::callback_id& id, const std::string& hostname, int family, single_callback&& callback);

  // Must be called from the owning thread.
  void                cancel(system::callback_id& callback_id);

protected:
  friend class system::Thread;

  void                init();

private:
  Resolver(const Resolver&) = delete;
  Resolver& operator=(const Resolver&) = delete;

  system::Thread* m_thread{nullptr};
};

} // namespace torrent::net

} // namespace torrent

#endif
