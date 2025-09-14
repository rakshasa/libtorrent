#ifndef LIBTORRENT_NET_TYPES_H
#define LIBTORRENT_NET_TYPES_H

#include <memory>
#include <tuple>
#include <netinet/in.h>
#include <sys/socket.h>
#include <torrent/common.h>

struct sockaddr_in;
struct sockaddr_in6;
struct sockaddr_un;

namespace torrent {

void sa_free(const sockaddr* sa) LIBTORRENT_EXPORT;

struct sockaddr_deleter {
  constexpr sockaddr_deleter() noexcept = default;

  void operator()(const sockaddr* sa) const { sa_free(sa); }
};

using sa_unique_ptr   = std::unique_ptr<sockaddr, sockaddr_deleter>;
using sin_unique_ptr  = std::unique_ptr<sockaddr_in>;
using sin6_unique_ptr = std::unique_ptr<sockaddr_in6>;
using sun_unique_ptr  = std::unique_ptr<sockaddr_un>;

using c_sa_unique_ptr   = std::unique_ptr<const sockaddr, sockaddr_deleter>;
using c_sin_unique_ptr  = std::unique_ptr<const sockaddr_in>;
using c_sin6_unique_ptr = std::unique_ptr<const sockaddr_in6>;
using c_sun_unique_ptr  = std::unique_ptr<const sockaddr_un>;

// Shared pointer objects of sockaddr type must use sa_free as deleter.

using sa_shared_ptr   = std::shared_ptr<sockaddr>;
using sin_shared_ptr  = std::shared_ptr<sockaddr_in>;
using sin6_shared_ptr = std::shared_ptr<sockaddr_in6>;
using sun_shared_ptr  = std::shared_ptr<sockaddr_un>;

using c_sa_shared_ptr   = std::shared_ptr<const sockaddr>;
using c_sin_shared_ptr  = std::shared_ptr<const sockaddr_in>;
using c_sin6_shared_ptr = std::shared_ptr<const sockaddr_in6>;
using c_sun_shared_ptr  = std::shared_ptr<const sockaddr_un>;

using fd_sap_tuple = std::tuple<int, sa_unique_ptr>;

struct listen_result_type {
  int fd;
  sa_unique_ptr address;
};

union sa_inet_union {
  // Ensure the largest member is first as it used for default list-initialization.
  sockaddr_in6 inet6;
  sockaddr_in  inet;
  sockaddr     sa;
};

} // namespace torrent

#endif
