#ifndef LIBTORRENT_NET_TYPES_H
#define LIBTORRENT_NET_TYPES_H

#include <memory>
#include <tuple>
#include <sys/socket.h>

struct sockaddr_in;
struct sockaddr_in6;
struct sockaddr_un;

namespace torrent {

using sa_unique_ptr   = std::unique_ptr<sockaddr>;
using sin_unique_ptr  = std::unique_ptr<sockaddr_in>;
using sin6_unique_ptr = std::unique_ptr<sockaddr_in6>;
using sun_unique_ptr  = std::unique_ptr<sockaddr_un>;

using c_sa_unique_ptr   = std::unique_ptr<const sockaddr>;
using c_sin_unique_ptr  = std::unique_ptr<const sockaddr_in>;
using c_sin6_unique_ptr = std::unique_ptr<const sockaddr_in6>;
using c_sun_unique_ptr  = std::unique_ptr<const sockaddr_un>;

using sa_shared_ptr   = std::shared_ptr<sockaddr>;
using sin_shared_ptr  = std::shared_ptr<sockaddr_in>;
using sin6_shared_ptr = std::shared_ptr<sockaddr_in6>;
using sun_shared_ptr  = std::shared_ptr<sockaddr_un>;

using c_sa_shared_ptr   = std::shared_ptr<const sockaddr>;
using c_sin_shared_ptr  = std::shared_ptr<const sockaddr_in>;
using c_sin6_shared_ptr = std::shared_ptr<const sockaddr_in6>;
using c_sun_shared_ptr  = std::shared_ptr<const sockaddr_un>;

using fd_sap_tuple = std::tuple<int, std::unique_ptr<sockaddr>>;

struct listen_result_type {
  int fd;
  sa_unique_ptr address;
};

}

#endif
