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

namespace {
c_sa_shared_ptr inet_any_value = sa_make_inet_any();
c_sa_shared_ptr inet6_any_value = sa_make_inet6_any();
}

NetworkConfig::NetworkConfig() {
  m_bind_inet_address = sa_make_unspec();
  m_bind_inet6_address = sa_make_unspec();
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
NetworkConfig::is_block_outgoing() const {
  auto guard = lock_guard();
  return m_block_outgoing;
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
NetworkConfig::set_block_outgoing(bool v) {
  auto guard = lock_guard();
  m_block_outgoing = v;
}

void
NetworkConfig::set_prefer_ipv6(bool v) {
  auto guard = lock_guard();
  m_prefer_ipv6 = v;
}

int
NetworkConfig::priority() const {
  auto guard = lock_guard();
  return m_priority;
}

void
NetworkConfig::set_priority(int p) {
  auto guard = lock_guard();
  m_priority = p;
}

// The caller must make sure the bind address family is not blocked before making a connection.
c_sa_shared_ptr
NetworkConfig::bind_address_best_match() const {
  auto guard = lock_guard();

  if (m_bind_inet_address->sa_family == AF_UNSPEC)
    return m_bind_inet6_address;

  if (m_bind_inet6_address->sa_family == AF_UNSPEC)
    return m_bind_inet_address;

  if (m_prefer_ipv6) {
    if (m_block_ipv6 && !m_block_ipv4)
      return m_bind_inet_address;

    return m_bind_inet6_address;
  }

  if (m_block_ipv4 && !m_block_ipv6)
    return m_bind_inet6_address;

  return m_bind_inet_address;
}

std::string
NetworkConfig::bind_address_best_match_str() const {
  return sa_addr_str(bind_address_best_match().get());
}

// TODO: There's a race condition if block/bind is changed after returning unspec, and the caller
// checks if ipv4/6 is blocked. Create a function that locks NC and sets up and binds fd.

c_sa_shared_ptr
NetworkConfig::bind_address_or_unspec_and_null() const {
  auto guard = lock_guard();

  if (m_block_ipv4 && m_block_ipv6)
    return nullptr;

  if (m_bind_inet_address->sa_family == AF_UNSPEC && m_bind_inet6_address->sa_family == AF_UNSPEC)
    return m_bind_inet_address;

  if (m_bind_inet_address->sa_family == AF_UNSPEC) {
    if (m_block_ipv6)
      return nullptr;

    return m_bind_inet6_address;
  }

  if (m_bind_inet6_address->sa_family == AF_UNSPEC) {
    if (m_block_ipv4)
      return nullptr;

    return m_bind_inet_address;
  }

  if (m_prefer_ipv6 && !m_block_ipv6)
    return m_bind_inet6_address;

  if (m_block_ipv4)
    return m_bind_inet6_address;

  return m_bind_inet_address;
}

c_sa_shared_ptr
NetworkConfig::bind_address_or_any_and_null() const {
  auto bind_address = bind_address_or_unspec_and_null();

  if (bind_address == nullptr)
    return nullptr;

  if (bind_address->sa_family != AF_UNSPEC)
    return bind_address;

  if (!m_block_ipv6)
    return inet6_any_value;

  if (!m_block_ipv4)
    return inet_any_value;

  throw internal_error("NetworkConfig::bind_address_or_any_and_null(): both ipv4 and ipv6 are blocked and returned unspec");
}

c_sa_shared_ptr
NetworkConfig::bind_address_for_connect(int family) const {
  auto guard = lock_guard();

  // if (m_block_outgoing)
  //   return nullptr;

  switch (family) {
  case AF_INET:
    if (m_block_ipv4)
      return nullptr;

    if (m_bind_inet_address->sa_family != AF_UNSPEC)
      return m_bind_inet_address;

    if (m_bind_inet6_address->sa_family != AF_UNSPEC) {
      if (m_block_ipv4in6)
        return nullptr;

      return m_bind_inet6_address;
    }

    return inet_any_value;

  case AF_INET6:
    if (m_block_ipv6)
      return nullptr;

    if (m_bind_inet6_address->sa_family != AF_UNSPEC)
      return m_bind_inet6_address;

    if (m_bind_inet_address->sa_family != AF_UNSPEC)
      return nullptr;

    return inet6_any_value;

  default:
    throw input_error("NetworkConfig::bind_address_for_connect() called with invalid address family");
  }
}

c_sa_shared_ptr
NetworkConfig::bind_inet_address() const {
  auto guard = lock_guard();
  return m_bind_inet_address;
}

std::string
NetworkConfig::bind_inet_address_str() const {
  auto guard = lock_guard();
  return sa_addr_str(m_bind_inet_address.get());
}

c_sa_shared_ptr
NetworkConfig::bind_inet6_address() const {
  auto guard = lock_guard();
  return m_bind_inet6_address;
}

std::string
NetworkConfig::bind_inet6_address_str() const {
  auto guard = lock_guard();
  return sa_addr_str(m_bind_inet6_address.get());
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

// TODO: Move all management tasks here.

// TODO: set_bind_address replace both bind_inet and bind_inet6.

void
NetworkConfig::set_bind_address(const sockaddr* sa) {
  if (sa->sa_family != AF_INET && sa->sa_family != AF_UNSPEC)
    throw input_error("Tried to set a bind address that is not an unspec/inet address.");

  if (sa_port(sa) != 0)
    throw input_error("Tried to set a bind address with a non-zero port.");

  auto guard = lock_guard();

  LT_LOG_NOTICE("bind address : %s", sa_pretty_str(sa).c_str());

  switch (sa->sa_family) {
  case AF_UNSPEC:
    m_bind_inet_address = sa_make_unspec();
    m_bind_inet6_address = sa_make_unspec();
    break;
  case AF_INET:
    m_bind_inet_address = sa_copy(sa);
    m_bind_inet6_address = sa_make_unspec();
    break;
  case AF_INET6:
    m_bind_inet_address = sa_make_unspec();
    m_bind_inet6_address = sa_copy(sa);
    break;
  default:
    throw internal_error("NetworkConfig::set_bind_address: invalid address family");
  }

  // TODO: This should bind to inet/inet6 and only empty string on unspec.
  if (sa_is_any(sa))
    torrent::net_thread::http_stack()->set_bind_address(std::string());
  else
    torrent::net_thread::http_stack()->set_bind_address(sa_addr_str(sa).c_str());

  // TODO: Warning if not matching block/prefer
}

// TODO: Add a separate bind setting for http stack.

void
NetworkConfig::set_bind_inet_address(const sockaddr* sa) {
  if (sa->sa_family != AF_INET && sa->sa_family != AF_UNSPEC)
    throw input_error("Tried to set a bind inet address that is not an unspec/inet address.");

  if (sa_port(sa) != 0)
    throw input_error("Tried to set a bind inet address with a non-zero port.");

  auto guard = lock_guard();

  LT_LOG_NOTICE("bind inet address : %s", sa_pretty_str(sa).c_str());

  m_bind_inet_address = sa_copy(sa);
}

void
NetworkConfig::set_bind_inet6_address(const sockaddr* sa) {
  if (sa->sa_family != AF_INET6 && sa->sa_family != AF_UNSPEC)
    throw input_error("Tried to set a bind inet6 address that is not an unspec/inet6 address.");

  if (sa_port(sa) != 0)
    throw input_error("Tried to set a bind inet6 address with a non-zero port.");

  auto guard = lock_guard();

  LT_LOG_NOTICE("bind inet6 address : %s", sa_pretty_str(sa).c_str());

  m_bind_inet6_address = sa_copy(sa);
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

uint32_t
NetworkConfig::encryption_options() const {
  auto guard = lock_guard();
  return m_encryption_options;
}

void
NetworkConfig::set_encryption_options(uint32_t opts) {
  auto guard = lock_guard();
  m_encryption_options = opts;
}

uint32_t
NetworkConfig::send_buffer_size() const {
  auto guard = lock_guard();
  return m_send_buffer_size;
}

uint32_t
NetworkConfig::receive_buffer_size() const {
  auto guard = lock_guard();
  return m_receive_buffer_size;
}

void
NetworkConfig::set_send_buffer_size(uint32_t s) {
  auto guard = lock_guard();
  m_send_buffer_size = s;
}

void
NetworkConfig::set_receive_buffer_size(uint32_t s) {
  auto guard = lock_guard();
  m_receive_buffer_size = s;
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
