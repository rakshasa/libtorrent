#include "config.h"

#include "torrent/net/network_config.h"

#include "torrent/exceptions.h"
#include "torrent/net/http_stack.h"
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

bool
NetworkConfig::is_block_ipv4() const {
  auto guard = lock_guard();
  return m_block_ipv4;
}

bool
NetworkConfig::is_block_ipv6() const {
  auto guard = lock_guard();
  return m_block_ipv6;
}

bool
NetworkConfig::is_block_ipv4in6() const {
  auto guard = lock_guard();
  return m_block_ipv4in6;
}

bool
NetworkConfig::is_prefer_ipv6() const {
  auto guard = lock_guard();
  return m_prefer_ipv6;
}

void
NetworkConfig::set_block_ipv4(bool v) {
  auto guard = lock_guard();
  m_block_ipv4 = v;
}

void
NetworkConfig::set_block_ipv6(bool v) {
  auto guard = lock_guard();
  m_block_ipv6 = v;
}

void
NetworkConfig::set_block_ipv4in6(bool v) {
  auto guard = lock_guard();
  m_block_ipv4in6 = v;
}

void
NetworkConfig::set_prefer_ipv6(bool v) {
  auto guard = lock_guard();
  m_prefer_ipv6 = v;
}

c_sa_shared_ptr
NetworkConfig::bind_address() const {
  auto guard = lock_guard();
  return m_bind_address;
}

std::string
NetworkConfig::bind_address_str() const {
  auto guard = lock_guard();
  return sa_addr_str(m_bind_address.get());
}

// TODO: Move all management tasks here.

void
NetworkConfig::set_bind_address(const sockaddr* sa) {
  if (sa->sa_family != AF_INET && sa->sa_family != AF_UNSPEC)
    throw input_error("Tried to set a bind address that is not an unspec/inet address.");

  if (sa_port(sa) != 0)
    throw input_error("Tried to set a bind address with a non-zero port.");

  auto guard = lock_guard();

  LT_LOG_NOTICE("bind address : %s", sa_pretty_str(sa).c_str());

  m_bind_address = sa_copy(sa);

  // TODO: This should bind to inet/inet6 and only empty string on unspec.
  if (sa_is_any(sa))
    torrent::net_thread::http_stack()->set_bind_address(std::string());
  else
    torrent::net_thread::http_stack()->set_bind_address(sa_addr_str(sa).c_str());

  // TODO: Warning if not matching block/prefer
}

c_sa_shared_ptr
NetworkConfig::local_address() const {
  auto guard = lock_guard();
  return m_local_address;
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

  LT_LOG_NOTICE("local address : %s", sa_pretty_str(sa).c_str());

  m_local_address = sa_copy(sa);
}

c_sa_shared_ptr
NetworkConfig::proxy_address() const {
  auto guard = lock_guard();
  return m_proxy_address;
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

  LT_LOG_NOTICE("proxy address : %s", sa_pretty_str(sa).c_str());

  m_proxy_address = sa_copy(sa);
}

uint16_t
NetworkConfig::listen_port() const {
  auto guard = lock_guard();
  return m_listen_port;
}

uint16_t
NetworkConfig::listen_port_or_throw() const {
  auto guard = lock_guard();

  if (m_listen_port == 0)
    throw internal_error("Tried to get listen port but it is not set.");

  return m_listen_port;
}

int
NetworkConfig::listen_backlog() const {
  auto guard = lock_guard();
  return m_listen_backlog;
}

uint16_t
NetworkConfig::override_dht_port() const {
  auto guard = lock_guard();
  return m_override_dht_port;
}

void
NetworkConfig::set_override_dht_port(uint16_t port) {
  auto guard = lock_guard();
  m_override_dht_port = port;
}

void
NetworkConfig::set_listen_port(uint16_t port) {
  if (port == 0)
    throw input_error("Tried to set a listen port to zero.");

  auto guard = lock_guard();
  m_listen_port = port;
}

void
NetworkConfig::set_listen_backlog(int backlog) {
  if (backlog < 1)
    throw input_error("Tried to set a listen backlog less than 1.");

  if (backlog > (1 << 16))
    throw input_error("Tried to set a listen backlog greater than 65536.");

  auto guard = lock_guard();
  m_listen_backlog = backlog;
}

} // namespace torrent::net
