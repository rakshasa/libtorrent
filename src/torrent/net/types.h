#ifndef LIBTORRENT_NET_TYPES_H
#define LIBTORRENT_NET_TYPES_H

#include <functional>
#include <memory>
#include <tuple>
#include <netinet/in.h>
#include <sys/socket.h>
#include <torrent/common.h>

struct sockaddr_in;
struct sockaddr_in6;
struct sockaddr_un;

namespace torrent {

namespace net {

void sa_free(const sockaddr* sa) LIBTORRENT_EXPORT;

struct sockaddr_deleter {
  constexpr sockaddr_deleter() noexcept = default;

  void operator()(const sockaddr* sa) const { net::sa_free(sa); }
};

} // namespace torrent::net

using sa_unique_ptr   = std::unique_ptr<sockaddr, net::sockaddr_deleter>;
using sin_unique_ptr  = std::unique_ptr<sockaddr_in>;
using sin6_unique_ptr = std::unique_ptr<sockaddr_in6>;
using sun_unique_ptr  = std::unique_ptr<sockaddr_un>;

using c_sa_unique_ptr   = std::unique_ptr<const sockaddr, net::sockaddr_deleter>;
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

using fd_sap_tuple      = std::tuple<int, sa_unique_ptr>;
using sin46_shared_pair = std::pair<sin_shared_ptr, sin6_shared_ptr>;
using resolver_callback = std::function<void(sin_shared_ptr, int, sin6_shared_ptr, int)>;

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

namespace net {

namespace proxy {

class Proxy;

} // namespace torrent::net::proxy

using proxy_ptr = std::unique_ptr<proxy::Proxy>;

} // namespace torrent::net

//
// Helper functions:
//

// TODO: Move to a separate header file.
// TODO: Rename sa_lookup_address.

c_sa_shared_ptr                sa_lookup_address(const std::string& address_str, int family) LIBTORRENT_EXPORT;
std::tuple<sa_unique_ptr,bool> sa_lookup_numeric(const std::string& address_str, int family) LIBTORRENT_EXPORT;

sin46_shared_pair   try_lookup_numeric(const std::string& hostname, int family) LIBTORRENT_EXPORT;

// TODO: Rename to family_enum and add family_enum_str.
const char*         family_str(int family) LIBTORRENT_EXPORT;
inline std::string  family_enum_str(int family) { return family_str(family); }

namespace net {

bool                                verify_url_guess_scheme(const std::string& url) LIBTORRENT_EXPORT;
bool                                verify_no_path_query_fragment(const std::string& url) LIBTORRENT_EXPORT;

std::string                         parse_uri_scheme(const std::string& url) LIBTORRENT_EXPORT;
std::pair<std::string, uint16_t>    parse_uri_host_port(const std::string& uri) LIBTORRENT_EXPORT;
std::pair<std::string, std::string> parse_uri_user_password(const std::string& uri) LIBTORRENT_EXPORT;

} // namespace net

} // namespace torrent

#endif
