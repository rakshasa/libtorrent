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
  typedef std::function<void (const sockaddr* addr, int err)> result_func;

  Resolver() = default;
  ~Resolver() = default;
  Resolver(const Resolver&) = delete;
  Resolver& operator=(const Resolver&) = delete;

  void                resolve(void* requester, const char* host, int family, int socktype, result_func slot);
  void                cancel(void* requester);

protected:
  friend class torrent::utils::Thread;

  void                init();
  void                process();

private:
  struct Request {
    void*                requester;
    const char*          host;
    int                  family;
    int                  socktype;
    result_func          slot;
  };

  struct Result {
    void*                requester;
    const sockaddr*      addr;
    int                  err;
    result_func          slot;
  };

  std::thread::id      m_thread_id;
  unsigned int         m_signal_process_results{~0u};

  std::vector<Request> m_requests;
  std::vector<Result>  m_results;
};

}

#endif
