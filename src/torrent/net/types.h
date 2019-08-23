#ifndef LIBTORRENT_NET_TYPES_H
#define LIBTORRENT_NET_TYPES_H

#include <memory>
#include <tuple>
#include <sys/socket.h>

struct sockaddr_in;
struct sockaddr_in6;
struct sockaddr_un;

namespace torrent {

typedef std::unique_ptr<sockaddr>     sa_unique_ptr;
typedef std::unique_ptr<sockaddr_in>  sin_unique_ptr;
typedef std::unique_ptr<sockaddr_in6> sin6_unique_ptr;
typedef std::unique_ptr<sockaddr_un>  sun_unique_ptr;

typedef std::unique_ptr<const sockaddr>     c_sa_unique_ptr;
typedef std::unique_ptr<const sockaddr_in>  c_sin_unique_ptr;
typedef std::unique_ptr<const sockaddr_in6> c_sin6_unique_ptr;
typedef std::unique_ptr<const sockaddr_un>  c_sun_unique_ptr;

typedef std::tuple<int, std::unique_ptr<sockaddr>> fd_sap_tuple;

struct listen_result_type {
  int fd;
  sa_unique_ptr address;
};

}

#endif
