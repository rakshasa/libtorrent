#include "config.h"

#include "torrent/net/network_config.h"

#include "torrent/exceptions.h"
#include "torrent/net/socket_address.h"
#include "torrent/utils/log.h"

// TODO: Add net category and add it to important/complete log outputs.

#define LT_LOG_NOTICE(log_fmt, ...)                                     \
  lt_log_print_subsystem(LOG_NOTICE, "net::network_config", log_fmt, __VA_ARGS__);

namespace torrent::net {

NetworkConfig::NetworkConfig() {
  m_bind_address = sa_make_unspec();
  m_local_address = sa_make_unspec();
  m_proxy_address = sa_make_unspec();
}

std::string
NetworkConfig::bind_address_str() const {
  auto guard = lock_guard();
  return sa_addr_str(m_bind_address.get());
}

void
NetworkConfig::set_bind_address(const sockaddr* sa) {
  if (sa->sa_family != AF_INET && sa->sa_family != AF_UNSPEC)
    throw input_error("Tried to set a bind address that is not an unspec/inet address.");

  if (sa_port(sa) != 0)
    throw input_error("Tried to set a bind address with a non-zero port.");

  auto guard = lock_guard();

  m_bind_address = sa_copy(sa);

  LT_LOG_NOTICE("bind address : %s", sa_pretty_str(m_bind_address.get()).c_str());

  // TODO: Warning if not matching block/prefer
}

std::string
NetworkConfig::local_address_str() const {
  auto guard = lock_guard();
  return sa_addr_str(m_local_address.get());
}

void
NetworkConfig::set_local_address(const sockaddr* sa) {
  if (sa->sa_family != AF_INET && sa->sa_family != AF_UNSPEC)
    throw input_error("Tried to set a local address that is not an unspec/inet address.");

  if (sa_port(sa) != 0)
    throw input_error("Tried to set a local address with a non-zero port.");

  auto guard = lock_guard();

  m_local_address = sa_copy(sa);

  LT_LOG_NOTICE("local address : %s", sa_pretty_str(m_local_address.get()).c_str());
}

std::string
NetworkConfig::proxy_address_str() const {
  auto guard = lock_guard();
  return sa_pretty_str(m_proxy_address.get());
}

void
NetworkConfig::set_proxy_address(const sockaddr* sa) {
  if (sa->sa_family != AF_INET && sa->sa_family != AF_UNSPEC)
    throw input_error("Tried to set a proxy address that is not an unspec/inet address.");

  if (sa_port(sa) == 0)
    throw input_error("Tried to set a proxy address with a zero port.");

  auto guard = lock_guard();

  m_proxy_address = sa_copy(sa);

  LT_LOG_NOTICE("proxy address : %s", sa_pretty_str(m_proxy_address.get()).c_str());
}

} // namespace torrent::net
