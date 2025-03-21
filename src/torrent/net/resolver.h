#ifndef TORRENT_NET_RESOLVER_H
#define TORRENT_NET_RESOLVER_H

#include <functional>
#include <thread>
#include <vector>

#include "torrent/common.h"

namespace torrent::net {

// TODO: Remove obsolete resolver in connection_manager.

class Resolver {
public:
  typedef std::function<void (const sockaddr* addr, int err)> resolver_callback;

  Resolver() = default;
  ~Resolver() = default;

  void                resolve(void* requester, const char* host, int family, resolver_callback&& slot);
  void                cancel(void* requester);

protected:
  friend class torrent::utils::Thread;

  void                init();
  void                process_callback(void* requester, const sockaddr* addr, int err, resolver_callback&& slot);

private:
  Resolver(const Resolver&) = delete;
  Resolver& operator=(const Resolver&) = delete;

  torrent::utils::Thread* m_thread{nullptr};
};

}

#endif
