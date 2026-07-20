#ifndef LIBTORRENT_NET_ADDRESS_INFO_H
#define LIBTORRENT_NET_ADDRESS_INFO_H

#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <netdb.h>
#include <torrent/common.h>
#include <torrent/net/socket_address.h>

namespace torrent {

struct ai_deleter {
  void operator()(addrinfo* ai) const { if (ai != nullptr) ::freeaddrinfo(ai); }
};

using ai_unique_ptr    = std::unique_ptr<addrinfo, ai_deleter>;
using c_ai_unique_ptr  = std::unique_ptr<const addrinfo, ai_deleter>;
using ai_sockaddr_func = std::function<void(const sockaddr*)>;

inline std::unique_ptr<addrinfo> ai_make_hint(int flags, int family, int socktype);

int ai_get_addrinfo(const char* nodename, const char* servname, const addrinfo* hints, ai_unique_ptr& res) LIBTORRENT_EXPORT;

// Helper functions:

// TODO: Consider servname "0".
// TODO: ai_get_first_sa_err that returns a tuple?
sa_unique_ptr ai_get_first_sa(const char* nodename, const char* servname = nullptr, const addrinfo* hints = nullptr) LIBTORRENT_EXPORT;

int ai_each_inet_inet6_first(const char* nodename, const ai_sockaddr_func& lambda) LIBTORRENT_EXPORT;

// Get all addrinfo's, iterate, etc.

//
// Safe conversion from unique_ptr arguments:
//

inline void aip_clear(ai_unique_ptr& aip) { *aip = addrinfo{}; }

inline int aip_get_addrinfo(const char* nodename, const char* servname, const ai_unique_ptr& hints, ai_unique_ptr& res) { return ai_get_addrinfo(nodename, servname, hints.get(), res); }
inline int aip_get_addrinfo(const char* nodename, const char* servname, const c_ai_unique_ptr& hints, ai_unique_ptr& res) { return ai_get_addrinfo(nodename, servname, hints.get(), res); }

//
// Implementations:
//

inline std::unique_ptr<addrinfo>
ai_make_hint(int flags, int family, int socktype) {
  auto aip = std::make_unique<addrinfo>();

  *aip = addrinfo{};
  aip->ai_flags = flags;
  aip->ai_family = family;
  aip->ai_socktype = socktype;

  return aip;
}

} // namespace torrent

#endif
