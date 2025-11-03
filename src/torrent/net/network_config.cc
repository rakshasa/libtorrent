#include "config.h"

#include "torrent/net/network_config.h"

#include "torrent/exceptions.h"
#include "torrent/net/http_stack.h"
#include "torrent/net/network_manager.h"
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
  m_local_inet_address = sa_make_unspec();
  m_local_inet6_address = sa_make_unspec();
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
  runtime::network_manager()->restart_listen();
}

void
NetworkConfig::set_block_ipv6(bool v) {
  auto guard = lock_guard();

  m_block_ipv6 = v;
  runtime::network_manager()->restart_listen();
}

void
NetworkConfig::set_block_ipv4in6(bool v) {
  auto guard = lock_guard();

  m_block_ipv4in6 = v;
  runtime::network_manager()->restart_listen();
}

void
NetworkConfig::set_block_outgoing(bool v) {
  auto guard = lock_guard();

  m_block_outgoing = v;
  runtime::network_manager()->restart_listen();
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

c_sa_shared_ptr
NetworkConfig::bind_address_best_match() const {
  return generic_address_best_match(m_bind_inet_address, m_bind_inet6_address);
}

std::string
NetworkConfig::bind_address_best_match_str() const {
  return generic_address_best_match_str(m_bind_inet_address, m_bind_inet6_address);
}

c_sa_shared_ptr
NetworkConfig::bind_address_or_unspec_and_null() const {
  return generic_address_or_unspec_and_null(m_bind_inet_address, m_bind_inet6_address);
}

c_sa_shared_ptr
NetworkConfig::bind_address_or_any_and_null() const {
  return generic_address_or_any_and_null(m_bind_inet_address, m_bind_inet6_address);
}

c_sa_shared_ptr
NetworkConfig::bind_address_for_connect(int family) const {
  return generic_address_for_connect(family, m_bind_inet_address, m_bind_inet6_address);
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

std::tuple<c_sa_shared_ptr, c_sa_shared_ptr>
NetworkConfig::bind_addresses_or_null() const {
  auto guard = lock_guard();

  auto inet_addr  = m_bind_inet_address;
  auto inet6_addr = m_bind_inet6_address;

  if (inet_addr->sa_family != AF_UNSPEC && inet6_addr->sa_family == AF_UNSPEC)
    inet6_addr = nullptr;
  else if (inet6_addr->sa_family != AF_UNSPEC && inet_addr->sa_family == AF_UNSPEC)
    inet_addr = nullptr;

  if (m_block_ipv4)
    inet_addr = nullptr;

  if (m_block_ipv6)
    inet6_addr = nullptr;

  return std::make_tuple(inet_addr, inet6_addr);
}

std::tuple<std::string, std::string>
NetworkConfig::bind_addresses_str() const {
  auto guard = lock_guard();
  return std::make_tuple(sa_addr_str(m_bind_inet_address.get()), sa_addr_str(m_bind_inet6_address.get()));
}

c_sa_shared_ptr
NetworkConfig::local_address_best_match() const {
  return generic_address_best_match(m_local_inet_address, m_local_inet6_address);
}

std::string
NetworkConfig::local_address_best_match_str() const {
  return generic_address_best_match_str(m_local_inet_address, m_local_inet6_address);
}

c_sa_shared_ptr
NetworkConfig::local_address_or_unspec_and_null() const {
  return generic_address_or_unspec_and_null(m_local_inet_address, m_local_inet6_address);
}

c_sa_shared_ptr
NetworkConfig::local_address_or_any_and_null() const {
  return generic_address_or_any_and_null(m_local_inet_address, m_local_inet6_address);
}

c_sa_shared_ptr
NetworkConfig::local_inet_address() const {
  auto guard = lock_guard();
  return m_local_inet_address;
}

c_sa_shared_ptr
NetworkConfig::local_inet_address_or_null() const {
  auto guard = lock_guard();

  if (m_block_ipv4 || m_local_inet_address->sa_family == AF_UNSPEC)
    return nullptr;

  return m_local_inet_address;
}

std::string
NetworkConfig::local_inet_address_str() const {
  auto guard = lock_guard();
  return sa_addr_str(m_local_inet_address.get());
}

c_sa_shared_ptr
NetworkConfig::local_inet6_address() const {
  auto guard = lock_guard();
  return m_local_inet6_address;
}

c_sa_shared_ptr
NetworkConfig::local_inet6_address_or_null() const {
  auto guard = lock_guard();

  if (m_block_ipv6 || m_local_inet6_address->sa_family == AF_UNSPEC)
    return nullptr;

  return m_local_inet6_address;
}

std::string
NetworkConfig::local_inet6_address_str() const {
  auto guard = lock_guard();
  return sa_addr_str(m_local_inet6_address.get());
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

void
NetworkConfig::set_bind_address(const sockaddr* sa) {
  auto guard = lock_guard();

  set_generic_address_unsafe("bind", m_bind_inet_address, m_bind_inet6_address, sa);
  runtime::network_manager()->restart_listen();
}

void
NetworkConfig::set_bind_address_str(const std::string& addr) {
  set_bind_address(sa_lookup_address(addr, AF_UNSPEC).get());
}

void
NetworkConfig::set_bind_inet_address(const sockaddr* sa) {
  auto guard = lock_guard();

  set_generic_inet_address_unsafe("bind", m_bind_inet_address, sa);
  runtime::network_manager()->restart_listen();
}

void
NetworkConfig::set_bind_inet_address_str(const std::string& addr) {
  set_bind_inet_address(sa_lookup_address(addr, AF_INET).get());
}

void
NetworkConfig::set_bind_inet6_address(const sockaddr* sa) {
  auto guard = lock_guard();

  set_generic_inet6_address_unsafe("bind", m_bind_inet6_address, sa);
  runtime::network_manager()->restart_listen();
}

void
NetworkConfig::set_bind_inet6_address_str(const std::string& addr) {
  set_bind_inet6_address(sa_lookup_address(addr, AF_INET6).get());
}

void
NetworkConfig::set_local_address(const sockaddr* sa) {
  if (sa_is_any(sa))
    throw input_error("Tried to set local address to an any address.");

  auto guard = lock_guard();
  set_generic_address_unsafe("local", m_local_inet_address, m_local_inet6_address, sa);
}

void
NetworkConfig::set_local_address_str(const std::string& addr) {
  set_local_address(sa_lookup_address(addr, AF_UNSPEC).get());
}

void
NetworkConfig::set_local_inet_address(const sockaddr* sa) {
  if (sa_is_any(sa))
    throw input_error("Tried to set local inet address to an any address.");

  auto guard = lock_guard();
  set_generic_inet_address_unsafe("local", m_local_inet_address, sa);
}

void
NetworkConfig::set_local_inet_address_str(const std::string& addr) {
  set_local_inet_address(sa_lookup_address(addr, AF_INET).get());
}

void
NetworkConfig::set_local_inet6_address(const sockaddr* sa) {
  if (sa_is_any(sa))
    throw input_error("Tried to set local inet6 address to an any address.");

  auto guard = lock_guard();
  set_generic_inet6_address_unsafe("local", m_local_inet6_address, sa);
}

void
NetworkConfig::set_local_inet6_address_str(const std::string& addr) {
  set_local_inet6_address(sa_lookup_address(addr, AF_INET6).get());
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

int
NetworkConfig::listen_backlog() const {
  auto guard = lock_guard();
  return m_listen_backlog;
}

void
NetworkConfig::set_listen_backlog(int backlog) {
  if (backlog < 1)
    throw input_error("Tried to set a listen backlog less than 1.");

  if (backlog > (1 << 16))
    throw input_error("Tried to set a listen backlog greater than 65536.");

  auto guard = lock_guard();

  m_listen_backlog = backlog;
  runtime::network_manager()->restart_listen();
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
NetworkConfig::send_buffer_size() const {
  auto guard = lock_guard();
  return m_send_buffer_size;
}

void
NetworkConfig::set_send_buffer_size(uint32_t s) {
  auto guard = lock_guard();
  m_send_buffer_size = s;
}

uint32_t
NetworkConfig::receive_buffer_size() const {
  auto guard = lock_guard();
  return m_receive_buffer_size;
}

void
NetworkConfig::set_receive_buffer_size(uint32_t s) {
  auto guard = lock_guard();
  m_receive_buffer_size = s;
}

NetworkConfig::listen_addresses
NetworkConfig::listen_addresses_unsafe() {
  auto inet_address  = m_block_ipv4 ? nullptr : m_bind_inet_address;
  auto inet6_address = m_block_ipv6 ? nullptr : m_bind_inet6_address;

  if (inet_address == nullptr && inet6_address == nullptr)
    return {};

  if (inet_address == nullptr) {
    if (inet6_address->sa_family == AF_UNSPEC)
      return {nullptr, inet6_any_value, m_block_ipv4in6};

    return {nullptr, inet6_address, m_block_ipv4in6};
  }

  if (inet6_address == nullptr) {
    if (inet_address->sa_family == AF_UNSPEC)
      return {inet_any_value, nullptr, false};

    return {inet_address, nullptr, false};
  }

  if (inet_address->sa_family == AF_UNSPEC && inet6_address->sa_family == AF_UNSPEC) {
    if (m_block_ipv4in6)
      return {inet_any_value, inet6_any_value, true};

    return {nullptr, inet6_any_value, false};
  }

  if (inet_address->sa_family != AF_UNSPEC && inet6_address->sa_family != AF_UNSPEC)
    return {inet_address, inet6_address, m_block_ipv4in6};

  if (inet_address->sa_family != AF_UNSPEC)
    return {inet_address, nullptr, false};

  if (inet6_address->sa_family != AF_UNSPEC)
    return {nullptr, inet6_address, m_block_ipv4in6};

  throw internal_error("NetworkConfig::listen_addresses_unsafe(): reached unreachable code.");
}

int
NetworkConfig::listen_backlog_unsafe() const {
  return m_listen_backlog;
}

//
// Helper Functions:
//

// The caller must make sure the address family is not blocked before making a connection.
c_sa_shared_ptr
NetworkConfig::generic_address_best_match(const c_sa_shared_ptr& inet_address, const c_sa_shared_ptr& inet6_address) const {
  auto guard = lock_guard();

  if (inet_address->sa_family == AF_UNSPEC)
    return inet6_address;

  if (inet6_address->sa_family == AF_UNSPEC)
    return inet_address;

  if (m_prefer_ipv6) {
    if (m_block_ipv6 && !m_block_ipv4)
      return inet_address;

    return inet6_address;
  }

  if (m_block_ipv4 && !m_block_ipv6)
    return inet6_address;

  return inet_address;
}

std::string
NetworkConfig::generic_address_best_match_str(const c_sa_shared_ptr& inet_address, const c_sa_shared_ptr& inet6_address) const {
  return sa_addr_str(generic_address_best_match(inet_address, inet6_address).get());
}

// TODO: There's a race condition if block/local is changed after returning unspec, and the caller
// checks if ipv4/6 is blocked. Create a function that locks NC and sets up and locals fd.

c_sa_shared_ptr
NetworkConfig::generic_address_or_unspec_and_null(const c_sa_shared_ptr& inet_address, const c_sa_shared_ptr& inet6_address) const {
  auto guard = lock_guard();

  if (m_block_ipv4 && m_block_ipv6)
    return nullptr;

  if (inet_address->sa_family == AF_UNSPEC && inet6_address->sa_family == AF_UNSPEC)
    return inet_address;

  if (inet_address->sa_family == AF_UNSPEC) {
    if (m_block_ipv6)
      return nullptr;

    return inet6_address;
  }

  if (inet6_address->sa_family == AF_UNSPEC) {
    if (m_block_ipv4)
      return nullptr;

    return inet_address;
  }

  if (m_prefer_ipv6 && !m_block_ipv6)
    return inet6_address;

  if (m_block_ipv4)
    return inet6_address;

  return inet_address;
}

c_sa_shared_ptr
NetworkConfig::generic_address_or_any_and_null(const c_sa_shared_ptr& inet_address, const c_sa_shared_ptr& inet6_address) const {
  auto address = generic_address_or_unspec_and_null(inet_address, inet6_address);

  if (address == nullptr)
    return nullptr;

  if (address->sa_family != AF_UNSPEC)
    return address;

  if (!m_block_ipv6)
    return inet6_any_value;

  if (!m_block_ipv4)
    return inet_any_value;

  throw internal_error("NetworkConfig::generic_address_or_any_and_null(): both ipv4 and ipv6 are blocked and returned unspec");
}

c_sa_shared_ptr
NetworkConfig::generic_address_for_connect(int family, const c_sa_shared_ptr& inet_address, const c_sa_shared_ptr& inet6_address) const {
  auto guard = lock_guard();

  // if (m_block_outgoing)
  //   return nullptr;

  switch (family) {
  case AF_INET:
    if (m_block_ipv4)
      return nullptr;

    if (inet_address->sa_family != AF_UNSPEC)
      return inet_address;

    if (inet6_address->sa_family != AF_UNSPEC) {
      if (m_block_ipv4in6)
        return nullptr;

      return inet6_address;
    }

    return inet_any_value;

  case AF_INET6:
    if (m_block_ipv6)
      return nullptr;

    if (inet6_address->sa_family != AF_UNSPEC)
      return inet6_address;

    if (inet_address->sa_family != AF_UNSPEC)
      return nullptr;

    return inet6_any_value;

  default:
    throw input_error("NetworkConfig::generic_address_for_connect() called with invalid address family");
  }
}

// TODO: Move all management tasks here.

void
NetworkConfig::set_generic_address_unsafe(const char* category, c_sa_shared_ptr& inet_address, c_sa_shared_ptr& inet6_address, const sockaddr* sa) {
  if (sa->sa_family != AF_INET && sa->sa_family != AF_UNSPEC)
    throw input_error("Tried to set a " + std::string(category) + " address that is not an unspec/inet address.");

  if (sa_port(sa) != 0)
    throw input_error("Tried to set a " + std::string(category) + " address with a non-zero port.");

  LT_LOG_NOTICE("%s address : %s", category, sa_pretty_str(sa).c_str());

  switch (sa->sa_family) {
  case AF_UNSPEC:
    inet_address = sa_make_unspec();
    inet6_address = sa_make_unspec();
    break;
  case AF_INET:
    inet_address = sa_copy(sa);
    inet6_address = sa_make_unspec();
    break;
  case AF_INET6:
    inet_address = sa_make_unspec();
    inet6_address = sa_copy(sa);
    break;
  default:
    throw internal_error("NetworkConfig::set_" + std::string(category) + "_address: invalid address family");
  }

  // TODO: Warning if not matching block/prefer
}

void
NetworkConfig::set_generic_inet_address_unsafe(const char* category, c_sa_shared_ptr& inet_address, const sockaddr* sa) {
  if (sa->sa_family != AF_INET && sa->sa_family != AF_UNSPEC)
    throw input_error("Tried to set a " + std::string(category) + " inet address that is not an unspec/inet address.");

  if (sa_port(sa) != 0)
    throw input_error("Tried to set a " + std::string(category) + " inet address with a non-zero port.");

  LT_LOG_NOTICE("%s inet address : %s", category, sa_pretty_str(sa).c_str());

  inet_address = sa_copy(sa);
}

void
NetworkConfig::set_generic_inet6_address_unsafe(const char* category, c_sa_shared_ptr& inet6_address, const sockaddr* sa) {
  if (sa->sa_family != AF_INET6 && sa->sa_family != AF_UNSPEC)
    throw input_error("Tried to set a " + std::string(category) + " inet6 address that is not an unspec/inet6 address.");

  if (sa_port(sa) != 0)
    throw input_error("Tried to set a " + std::string(category) + " inet6 address with a non-zero port.");

  LT_LOG_NOTICE("%s inet6 address : %s", category, sa_pretty_str(sa).c_str());

  inet6_address = sa_copy(sa);
}

} // namespace torrent::net
