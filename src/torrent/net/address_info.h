#ifndef LIBTORRENT_NET_ADDRESS_INFO_H
#define LIBTORRENT_NET_ADDRESS_INFO_H

#include <cstring>
#include <memory>
#include <string>
#include <netdb.h>
#include <torrent/common.h>

namespace torrent {

struct ai_deleter {
  void operator()(addrinfo* ai) const { freeaddrinfo(ai); }
};

typedef std::unique_ptr<addrinfo, ai_deleter> ai_unique_ptr;
typedef std::function<void (const sockaddr*)> ai_sockaddr_func;

inline void          ai_clear(addrinfo* ai);
inline ai_unique_ptr ai_make_hint(int family, int socktype);

int ai_getaddrinfo(const char* nodename, const char* servname, const addrinfo* hints, ai_unique_ptr& res) LIBTORRENT_EXPORT;

// Helper functions:
int ai_each_inet_inet6_first(const char* nodename, ai_sockaddr_func lambda) LIBTORRENT_EXPORT;

// Get all addrinfo's, iterate, etc.

// Implementation:

inline void
ai_clear(addrinfo* ai) {
  std::memset(ai, 0, sizeof(addrinfo));  
}

inline ai_unique_ptr
ai_make_hint(int family, int socktype) {
  ai_unique_ptr ai(new addrinfo);

  ai_clear(ai.get());
  ai->ai_family = family;
  ai->ai_socktype = socktype;

  return ai;
}

}

#endif
