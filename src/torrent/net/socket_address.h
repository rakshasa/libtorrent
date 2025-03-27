#ifndef LIBTORRENT_NET_SOCKET_ADDRESS_H
#define LIBTORRENT_NET_SOCKET_ADDRESS_H

#include <memory>
#include <string>
#include <netinet/in.h>
#include <sys/socket.h>
#include <torrent/common.h>
#include <torrent/net/types.h>

namespace torrent {

bool sa_is_unspec(const sockaddr* sa) LIBTORRENT_EXPORT;
bool sa_is_inet(const sockaddr* sa) LIBTORRENT_EXPORT;
bool sa_is_inet6(const sockaddr* sa) LIBTORRENT_EXPORT;
bool sa_is_inet_inet6(const sockaddr* sa) LIBTORRENT_EXPORT;

bool sa_is_any(const sockaddr* sa) LIBTORRENT_EXPORT;
bool sin_is_any(const sockaddr_in* sa) LIBTORRENT_EXPORT;
bool sin6_is_any(const sockaddr_in6* sa) LIBTORRENT_EXPORT;

bool sa_is_broadcast(const sockaddr* sa) LIBTORRENT_EXPORT;
bool sin_is_broadcast(const sockaddr_in* sa) LIBTORRENT_EXPORT;

bool sa_is_v4mapped(const sockaddr* sa) LIBTORRENT_EXPORT;
bool sin6_is_v4mapped(const sockaddr_in6* sa) LIBTORRENT_EXPORT;

bool sa_is_port_any(const sockaddr* sa) LIBTORRENT_EXPORT;

size_t sa_length(const sockaddr* sa) LIBTORRENT_EXPORT;

sa_unique_ptr   sa_make_unspec() LIBTORRENT_EXPORT;
sa_unique_ptr   sa_make_inet() LIBTORRENT_EXPORT;
sa_unique_ptr   sa_make_inet6() LIBTORRENT_EXPORT;
sa_unique_ptr   sa_make_unix(const std::string& pathname) LIBTORRENT_EXPORT;

sa_unique_ptr   sa_convert(const sockaddr* sa) LIBTORRENT_EXPORT;

sa_unique_ptr   sa_copy(const sockaddr* sa) LIBTORRENT_EXPORT;
sa_unique_ptr   sa_copy_in(const sockaddr_in* sa) LIBTORRENT_EXPORT;
sa_unique_ptr   sa_copy_in6(const sockaddr_in6* sa) LIBTORRENT_EXPORT;
sa_unique_ptr   sa_copy_addr(const sockaddr* sa, uint16_t port = 0) LIBTORRENT_EXPORT;
sa_unique_ptr   sa_copy_addr_in(const sockaddr_in* sa, uint16_t port = 0) LIBTORRENT_EXPORT;
sa_unique_ptr   sa_copy_addr_in6(const sockaddr_in6* sa, uint16_t port = 0) LIBTORRENT_EXPORT;
sin_unique_ptr  sin_copy(const sockaddr_in* sa) LIBTORRENT_EXPORT;
sin6_unique_ptr sin6_copy(const sockaddr_in6* sa) LIBTORRENT_EXPORT;

sin_unique_ptr  sin_make() LIBTORRENT_EXPORT;
sin6_unique_ptr sin6_make() LIBTORRENT_EXPORT;

sa_unique_ptr   sa_from_v4mapped(const sockaddr* sa) LIBTORRENT_EXPORT;
sa_unique_ptr   sa_to_v4mapped(const sockaddr* sa) LIBTORRENT_EXPORT;
sa_unique_ptr   sa_from_v4mapped_in6(const sockaddr_in6* sin6) LIBTORRENT_EXPORT;
sa_unique_ptr   sa_to_v4mapped_in(const sockaddr_in* sin) LIBTORRENT_EXPORT;
sin_unique_ptr  sin_from_v4mapped_in6(const sockaddr_in6* sin6) LIBTORRENT_EXPORT;
sin6_unique_ptr sin6_to_v4mapped_in(const sockaddr_in* sin) LIBTORRENT_EXPORT;

sa_unique_ptr     sa_from_in(sin_unique_ptr&& sinp) LIBTORRENT_EXPORT;
c_sa_unique_ptr   sa_from_in(c_sin_unique_ptr&& sinp) LIBTORRENT_EXPORT;
sa_unique_ptr     sa_from_in6(sin6_unique_ptr&& sin6p) LIBTORRENT_EXPORT;
c_sa_unique_ptr   sa_from_in6(c_sin6_unique_ptr&& sin6p) LIBTORRENT_EXPORT;
sin_unique_ptr    sin_from_sa(sa_unique_ptr&& sap) LIBTORRENT_EXPORT;
sin6_unique_ptr   sin6_from_sa(sa_unique_ptr&& sap) LIBTORRENT_EXPORT;
c_sin_unique_ptr  sin_from_c_sa(c_sa_unique_ptr&& sap) LIBTORRENT_EXPORT;
c_sin6_unique_ptr sin6_from_c_sa(c_sa_unique_ptr&& sap) LIBTORRENT_EXPORT;

void        sa_clear_inet6(sockaddr_in6* sa) LIBTORRENT_EXPORT;

uint16_t    sa_port(const sockaddr* sa) LIBTORRENT_EXPORT;
void        sa_set_port(sockaddr* sa, uint16_t port) LIBTORRENT_EXPORT;

bool        sa_equal(const sockaddr* lhs, const sockaddr* rhs) LIBTORRENT_EXPORT;
bool        sin_equal(const sockaddr_in* lhs, const sockaddr_in* rhs) LIBTORRENT_EXPORT;
bool        sin6_equal(const sockaddr_in6* lhs, const sockaddr_in6* rhs) LIBTORRENT_EXPORT;

bool        sa_equal_addr(const sockaddr* lhs, const sockaddr* rhs) LIBTORRENT_EXPORT;
bool        sin_equal_addr(const sockaddr_in* lhs, const sockaddr_in* rhs) LIBTORRENT_EXPORT;
bool        sin6_equal_addr(const sockaddr_in6* lhs, const sockaddr_in6* rhs) LIBTORRENT_EXPORT;

std::string sa_addr_str(const sockaddr* sa) LIBTORRENT_EXPORT;
std::string sin_addr_str(const sockaddr_in* sa) LIBTORRENT_EXPORT;
std::string sin6_addr_str(const sockaddr_in6* sa) LIBTORRENT_EXPORT;

std::string sa_pretty_str(const sockaddr* sa) LIBTORRENT_EXPORT;
std::string sin_pretty_str(const sockaddr_in* sa) LIBTORRENT_EXPORT;
std::string sin6_pretty_str(const sockaddr_in6* sa) LIBTORRENT_EXPORT;

// Rename/replace:
void sa_inet_mapped_inet6(const sockaddr_in* sa, sockaddr_in6* mapped) LIBTORRENT_EXPORT;

std::string sa_pretty_address_str(const sockaddr* sa) LIBTORRENT_EXPORT;

//
// Tuples:
//

bool fd_sap_equal(const fd_sap_tuple& lhs, const fd_sap_tuple& rhs) LIBTORRENT_EXPORT;

//
// Safe conversion from unique_ptr arguments:
//

inline bool sap_is_unspec(const sa_unique_ptr& sap) { return sa_is_unspec(sap.get()); }
inline bool sap_is_unspec(const c_sa_unique_ptr& sap) { return sa_is_unspec(sap.get()); }
inline bool sap_is_inet(const sa_unique_ptr& sap) { return sa_is_inet(sap.get()); }
inline bool sap_is_inet(const c_sa_unique_ptr& sap) { return sa_is_inet(sap.get()); }
inline bool sap_is_inet6(const sa_unique_ptr& sap) { return sa_is_inet6(sap.get()); }
inline bool sap_is_inet6(const c_sa_unique_ptr& sap) { return sa_is_inet6(sap.get()); }
inline bool sap_is_inet_inet6(const sa_unique_ptr& sap) { return sa_is_inet_inet6(sap.get()); }
inline bool sap_is_inet_inet6(const c_sa_unique_ptr& sap) { return sa_is_inet_inet6(sap.get()); }

inline bool sap_is_any(const sa_unique_ptr& sap) { return sa_is_any(sap.get()); }
inline bool sap_is_any(const c_sa_unique_ptr& sap) { return sa_is_any(sap.get()); }
inline bool sinp_is_any(const sin_unique_ptr& sinp) { return sin_is_any(sinp.get()); }
inline bool sinp_is_any(const c_sin_unique_ptr& sinp) { return sin_is_any(sinp.get()); }
inline bool sinp6_is_any(const sin6_unique_ptr& sin6p) { return sin6_is_any(sin6p.get()); }
inline bool sinp6_is_any(const c_sin6_unique_ptr& sin6p) { return sin6_is_any(sin6p.get()); }

inline bool sap_is_broadcast(const sa_unique_ptr& sap) { return sa_is_broadcast(sap.get()); }
inline bool sap_is_broadcast(const c_sa_unique_ptr& sap) { return sa_is_broadcast(sap.get()); }
inline bool sinp_is_broadcast(const sin_unique_ptr& sap) { return sin_is_broadcast(sap.get()); }
inline bool sinp_is_broadcast(const c_sin_unique_ptr& sap) { return sin_is_broadcast(sap.get()); }

inline bool sap_is_v4mapped(const sa_unique_ptr& sap) { return sa_is_v4mapped(sap.get()); }
inline bool sap_is_v4mapped(const c_sa_unique_ptr& sap) { return sa_is_v4mapped(sap.get()); }
inline bool sinp6_is_v4mapped(const sin6_unique_ptr& sin6p) { return sin6_is_v4mapped(sin6p.get()); }
inline bool sinp6_is_v4mapped(const c_sin6_unique_ptr& sin6p) { return sin6_is_v4mapped(sin6p.get()); }

inline bool sap_is_port_any(const sa_unique_ptr& sap) { return sa_is_port_any(sap.get()); }
inline bool sap_is_port_any(const c_sa_unique_ptr& sap) { return sa_is_port_any(sap.get()); }

inline size_t sap_length(const sa_unique_ptr& sap) { return sa_length(sap.get()); }
inline size_t sap_length(const c_sa_unique_ptr& sap) { return sa_length(sap.get()); }

inline sa_unique_ptr sap_copy(const sa_unique_ptr& sap) { return sa_copy(sap.get()); }
inline sa_unique_ptr sap_copy(const c_sa_unique_ptr& sap) { return sa_copy(sap.get()); }
inline sa_unique_ptr sap_copy_addr(const sa_unique_ptr& sap, uint16_t port = 0) { return sa_copy_addr(sap.get(), port); }
inline sa_unique_ptr sap_copy_addr(const c_sa_unique_ptr& sap, uint16_t port = 0) { return sa_copy_addr(sap.get(), port); }
inline sa_unique_ptr sap_copy_in(const sin_unique_ptr& sinp) { return sa_copy_in(sinp.get()); }
inline sa_unique_ptr sap_copy_in(const c_sin_unique_ptr& sinp) { return sa_copy_in(sinp.get()); }
inline sa_unique_ptr sap_copy_in6(const sin6_unique_ptr& sin6p) { return sa_copy_in6(sin6p.get()); }
inline sa_unique_ptr sap_copy_in6(const c_sin6_unique_ptr& sin6p) { return sa_copy_in6(sin6p.get()); }

inline sa_unique_ptr   sap_from_v4mapped(const sa_unique_ptr& sap) { return sa_from_v4mapped(sap.get()); }
inline sa_unique_ptr   sap_from_v4mapped(const c_sa_unique_ptr& sap) { return sa_from_v4mapped(sap.get()); }
inline sa_unique_ptr   sap_to_v4mapped(const sa_unique_ptr& sap) { return sa_to_v4mapped(sap.get()); }
inline sa_unique_ptr   sap_to_v4mapped(const c_sa_unique_ptr& sap) { return sa_to_v4mapped(sap.get()); }
inline sin_unique_ptr  sinp_from_v4mapped_in6(const sin6_unique_ptr& sin6p) { return sin_from_v4mapped_in6(sin6p.get()); }
inline sin_unique_ptr  sinp_from_v4mapped_in6(const c_sin6_unique_ptr& sin6p) { return sin_from_v4mapped_in6(sin6p.get()); }
inline sin6_unique_ptr sin6p_to_v4mapped_in(const sin_unique_ptr& sinp) { return sin6_to_v4mapped_in(sinp.get()); }
inline sin6_unique_ptr sin6p_to_v4mapped_in(const c_sin_unique_ptr& sinp) { return sin6_to_v4mapped_in(sinp.get()); }

inline uint16_t sap_port(const sa_unique_ptr& sap) { return sa_port(sap.get()); }
inline uint16_t sap_port(const c_sa_unique_ptr& sap) { return sa_port(sap.get()); }
inline void sap_set_port(const sa_unique_ptr& sap, uint16_t port) { sa_set_port(sap.get(), port); }

inline bool sap_equal(const sa_unique_ptr& lhs, const sa_unique_ptr& rhs) { return sa_equal(lhs.get(), rhs.get()); }
inline bool sap_equal(const sa_unique_ptr& lhs, const c_sa_unique_ptr& rhs) { return sa_equal(lhs.get(), rhs.get()); }
inline bool sap_equal(const c_sa_unique_ptr& lhs, const sa_unique_ptr& rhs) { return sa_equal(lhs.get(), rhs.get()); }
inline bool sap_equal(const c_sa_unique_ptr& lhs, const c_sa_unique_ptr& rhs) { return sa_equal(lhs.get(), rhs.get()); }
inline bool sinp_equal(const sin_unique_ptr& lhs, const sin_unique_ptr& rhs) { return sin_equal(lhs.get(), rhs.get()); }
inline bool sinp_equal(const sin_unique_ptr& lhs, const c_sin_unique_ptr& rhs) { return sin_equal(lhs.get(), rhs.get()); }
inline bool sinp_equal(const c_sin_unique_ptr& lhs, const sin_unique_ptr& rhs) { return sin_equal(lhs.get(), rhs.get()); }
inline bool sinp_equal(const c_sin_unique_ptr& lhs, const c_sin_unique_ptr& rhs) { return sin_equal(lhs.get(), rhs.get()); }
inline bool sin6p_equal(const sin6_unique_ptr& lhs, const sin6_unique_ptr& rhs) { return sin6_equal(lhs.get(), rhs.get()); }
inline bool sin6p_equal(const sin6_unique_ptr& lhs, const c_sin6_unique_ptr& rhs) { return sin6_equal(lhs.get(), rhs.get()); }
inline bool sin6p_equal(const c_sin6_unique_ptr& lhs, const sin6_unique_ptr& rhs) { return sin6_equal(lhs.get(), rhs.get()); }
inline bool sin6p_equal(const c_sin6_unique_ptr& lhs, const c_sin6_unique_ptr& rhs) { return sin6_equal(lhs.get(), rhs.get()); }

inline bool sap_equal_addr(const sa_unique_ptr& lhs, const sa_unique_ptr& rhs) { return sa_equal_addr(lhs.get(), rhs.get()); }
inline bool sap_equal_addr(const sa_unique_ptr& lhs, const c_sa_unique_ptr& rhs) { return sa_equal_addr(lhs.get(), rhs.get()); }
inline bool sap_equal_addr(const c_sa_unique_ptr& lhs, const sa_unique_ptr& rhs) { return sa_equal_addr(lhs.get(), rhs.get()); }
inline bool sap_equal_addr(const c_sa_unique_ptr& lhs, const c_sa_unique_ptr& rhs) { return sa_equal_addr(lhs.get(), rhs.get()); }
inline bool sinp_equal_addr(const sin_unique_ptr& lhs, const sin_unique_ptr& rhs) { return sin_equal_addr(lhs.get(), rhs.get()); }
inline bool sinp_equal_addr(const sin_unique_ptr& lhs, const c_sin_unique_ptr& rhs) { return sin_equal_addr(lhs.get(), rhs.get()); }
inline bool sinp_equal_addr(const c_sin_unique_ptr& lhs, const sin_unique_ptr& rhs) { return sin_equal_addr(lhs.get(), rhs.get()); }
inline bool sinp_equal_addr(const c_sin_unique_ptr& lhs, const c_sin_unique_ptr& rhs) { return sin_equal_addr(lhs.get(), rhs.get()); }
inline bool sin6p_equal_addr(const sin6_unique_ptr& lhs, const sin6_unique_ptr& rhs) { return sin6_equal_addr(lhs.get(), rhs.get()); }
inline bool sin6p_equal_addr(const sin6_unique_ptr& lhs, const c_sin6_unique_ptr& rhs) { return sin6_equal_addr(lhs.get(), rhs.get()); }
inline bool sin6p_equal_addr(const c_sin6_unique_ptr& lhs, const sin6_unique_ptr& rhs) { return sin6_equal_addr(lhs.get(), rhs.get()); }
inline bool sin6p_equal_addr(const c_sin6_unique_ptr& lhs, const c_sin6_unique_ptr& rhs) { return sin6_equal_addr(lhs.get(), rhs.get()); }

inline std::string sap_addr_str(const sa_unique_ptr& sap) { return sa_addr_str(sap.get()); }
inline std::string sap_addr_str(const c_sa_unique_ptr& sap) { return sa_addr_str(sap.get()); }
inline std::string sap_pretty_str(const sa_unique_ptr& sap) { return sa_pretty_str(sap.get()); }
inline std::string sap_pretty_str(const c_sa_unique_ptr& sap) { return sa_pretty_str(sap.get()); }

//
// Implementations:
//

inline sa_unique_ptr
sa_from_v4mapped_in6(const sockaddr_in6* sin6) {
  return sa_from_in(sin_from_v4mapped_in6(sin6));
}

inline sa_unique_ptr
sa_to_v4mapped_in(const sockaddr_in* sin) {
  return sa_from_in6(sin6_to_v4mapped_in(sin));
}

inline sa_unique_ptr
sa_from_in(sin_unique_ptr&& sinp) {
  return sa_unique_ptr(reinterpret_cast<sockaddr*>(sinp.release()));
}

inline c_sa_unique_ptr
sa_from_in(c_sin_unique_ptr&& sinp) {
  return c_sa_unique_ptr(reinterpret_cast<const sockaddr*>(sinp.release()));
}

inline sa_unique_ptr
sa_from_in6(sin6_unique_ptr&& sin6p) {
  return sa_unique_ptr(reinterpret_cast<sockaddr*>(sin6p.release()));
}

inline c_sa_unique_ptr
sa_from_in6(c_sin6_unique_ptr&& sin6p) {
  return c_sa_unique_ptr(reinterpret_cast<const sockaddr*>(sin6p.release()));
}

inline bool
fd_sap_equal(const fd_sap_tuple& lhs, const fd_sap_tuple& rhs) {
  return std::get<0>(lhs) == std::get<0>(rhs) && sap_equal(std::get<1>(lhs), std::get<1>(rhs));
}

}

#endif
