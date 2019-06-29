#include "config.h"

#include <cerrno>
#include <cstring>
#include <cinttypes>

#include "bind_manager.h"

#include "net/socket_listen.h"
#include "torrent/exceptions.h"
#include "torrent/net/fd.h"
#include "torrent/utils/log.h"
#include "torrent/utils/random.h"

#define LT_LOG(log_fmt, ...)                                            \
  lt_log_print(LOG_CONNECTION_BIND, "bind: " log_fmt, __VA_ARGS__);
#define LT_LOG_SOCKADDR(log_fmt, sa, ...)                               \
  lt_log_print(LOG_CONNECTION_BIND, "bind->%s: " log_fmt, sa_pretty_str(sa).c_str(), __VA_ARGS__);
#define LT_LOG_SOCKADDR_ERROR(log_fmt, bind_sa, flags, address)         \
  LT_LOG_SOCKADDR(log_fmt " (flags:0x%x fd:%i address:%s errno:%i message:'%s')", \
                  bind_sa, flags, fd, sa_pretty_str(address).c_str(), errno, std::strerror(errno));

#define LT_ASSERT_INPUT(message, condition)                             \
  { if (!(condition)) { lt_log_print(LOG_CONNECTION_BIND, "bind: %s", std::string(message).c_str()); \
                        throw input_error(message); }};
#define LT_ASSERT_INTERNAL(message, condition)                          \
  { if (!(condition)) { lt_log_print(LOG_CONNECTION_BIND, "bind: %s", std::string(message).c_str()); \
                        throw internal_error(message); }};
#define LT_ASSERT_INTERNAL_F_SA(message, flags, sa, condition)          \
  { if (!(condition)) { lt_log_print(LOG_CONNECTION_BIND, "bind: %s (flags:0x%x address:%s)", \
                                     std::string(message).c_str(), flags, torrent::sa_pretty_str(sa).c_str()); \
                        throw internal_error(message); }};

namespace torrent {

const sockaddr*
bind_struct::listen_socket_address() const {
  return listen != nullptr ? listen->socket_address() : nullptr;
}

bind_struct
make_bind_struct(const std::string& name, c_sa_unique_ptr&& sap, int flags, uint16_t priority, std::unique_ptr<socket_listen>&& listen) {
  if ((flags & bind_manager::flag_v4only) && !sap_is_inet(sap))
    throw internal_error("(flags & flag_v4only) && !sa_is_inet(sa)");

  if ((flags & bind_manager::flag_v6only) && !sap_is_inet6(sap))
    throw internal_error("(flags & flag_v6only) && !sa_is_inet(sa)");

  return bind_struct{ name, flags, std::move(sap), priority, 0, 0, std::move(listen) };
}

static bool
validate_bind_flags(int flags) {
  if ((flags & bind_manager::flag_v4only) && (flags & bind_manager::flag_v6only))
    return false;

  // TODO: Detect unknown flags.

  return true;
}

bind_manager::bind_manager() :
  m_flags(flag_port_randomize),
  m_listen_backlog(SOMAXCONN),
  m_listen_port(0),
  m_listen_port_first(6881),
  m_listen_port_last(6999)
{
}

bind_manager::~bind_manager() {
}

void
bind_manager::clear() {
  LT_LOG("cleared all entries", 0);

  base_type::clear();
}

void
bind_manager::set_port_randomize(bool flag) {
  if (flag) {
    LT_LOG("randomized listen port by default", 0);
    m_flags |= flag_port_randomize;
  } else {
    LT_LOG("sequential listen port by default", 0);
    m_flags &= ~flag_port_randomize;
  }
}

void
bind_manager::set_block_accept(bool flag) {
  if (flag) {
    LT_LOG("block accept for all binds", 0);
    m_flags |= flag_block_accept;
  } else {
    LT_LOG("allow accept for all binds", 0);
    m_flags &= ~flag_block_accept;
  }
}

void
bind_manager::set_block_connect(bool flag) {
  if (flag) {
    LT_LOG("block connect for all binds", 0);
    m_flags |= flag_block_connect;
  } else {
    LT_LOG("allow connect for all binds", 0);
    m_flags &= ~flag_block_connect;
  }
}

void
bind_manager::add_bind(const std::string& name, uint16_t priority, const sockaddr* bind_sa, int flags) {
  LT_ASSERT_INTERNAL("add_bind with " + sa_pretty_str(bind_sa) + " sockaddr",
                     bind_sa != nullptr && (sa_is_inet(bind_sa) || sa_is_inet6(bind_sa)));

  auto sap = sa_convert(bind_sa);
  LT_ASSERT_INPUT("add_bind with duplicate name", find_name(name) == end());
  LT_ASSERT_INPUT("add_bind with broadcast address", !sap_is_broadcast(sap));
  LT_ASSERT_INPUT("add_bind with non-zero port", sap_is_port_any(sap));

  if (sap_is_inet(sap)) {
    LT_ASSERT_INPUT("add_bind with " + sap_pretty_str(sap) + " and incompatible v6only flag", !(flags & flag_v6only));
    flags |= flag_v4only;
  }

  if (sap_is_inet6(sap)) {
    LT_ASSERT_INPUT("add_bind with " + sap_pretty_str(sap) + " and incompatible v4only flag", !(flags & flag_v4only));

    if (!sap_is_any(sap))
      flags |= flag_v6only;
  }

  LT_LOG("added binding (name:%s priority:%" PRIu16 " flags:0x%x address:%s)", name.c_str(), priority, flags, sap_pretty_str(sap).c_str());

  // TODO: Verify passed flags, etc.
  // TODO: Sanitize the flags.

  std::unique_ptr<socket_listen> listen(new socket_listen);
  listen->set_backlog(m_listen_backlog);

  base_type::insert(m_upper_bound_priority(priority),
                    make_bind_struct(name, std::move(sap), flags, priority, std::move(listen)));
}

bool
bind_manager::remove_bind(const std::string& name) {
  auto itr = m_find_name(name);

  if (itr == end()) {
    LT_LOG("remove binding failed, name not found (name:%s)", name.c_str());
    return false;
  }

  LT_LOG("removed binding (name:%s)", name.c_str());

  base_type::erase(itr);
  return true;
}

void
bind_manager::remove_bind_throw(const std::string& name) {
  if (!remove_bind(name))
    throw input_error("remove binding failed, name not found: '" + name + "'");
}

static int
attempt_open_connect(const bind_struct& bs, const sockaddr* sockaddr, fd_flags flags) {
  int fd = fd_open(flags);

  if (fd == -1) {
    LT_LOG_SOCKADDR_ERROR("open_connect open failed", bs.address.get(), flags, sockaddr);
    return -1;
  }

  if (!sap_is_any(bs.address) && !fd_bind(fd, bs.address.get())) {
    LT_LOG_SOCKADDR_ERROR("open_connect bind failed", bs.address.get(), flags, sockaddr);
    fd_close(fd);
    return -1;
  }

  if (!fd_connect(fd, sockaddr)) {
    LT_LOG_SOCKADDR_ERROR("open_connect connect failed", bs.address.get(), flags, sockaddr);
    fd_close(fd);
    return -1;
  }

  LT_LOG_SOCKADDR("open_connect succeeded (flags:0x%x fd:%i address:%s)",
                  bs.address.get(), flags, fd, sa_pretty_str(sockaddr).c_str());
  return fd;
}

int
bind_manager::connect_socket(const sockaddr* connect_sa, int flags) const {
  LT_ASSERT_INTERNAL_F_SA("connect_socket called with invalid sockaddr", flags, connect_sa,
                          sa_is_inet(connect_sa) || sa_is_inet6(connect_sa));
  LT_ASSERT_INTERNAL_F_SA("connect_socket called with invalid sockaddr", flags, connect_sa,
                          !sa_is_any(connect_sa) && !sa_is_broadcast(connect_sa) && sa_port(connect_sa) != 0);

  if ((m_flags & flag_block_connect)) {
    // LT_LOG("connect_socket failed, all outgoing connections blocked (flags:0x%x address:%s)",
    //        flags, sa_pretty_str(connect_sa).c_str());
    return -1;
  }

  LT_LOG("connect_socket attempt (flags:0x%x address:%s)", flags, sa_pretty_str(connect_sa).c_str());

  for (auto& itr : *this) {
    if (itr.flags & flag_block_connect)
      continue;

    if (!validate_bind_flags(itr.flags))
      continue; // TODO: Warn here, do something.

    fd_flags open_flags = fd_flag_stream | fd_flag_nonblock;
    sa_unique_ptr current_sap = sa_convert(connect_sa);

    if ((itr.flags & flag_v4only)) {
      if (!sap_is_inet(current_sap))
        continue;
      open_flags |= fd_flag_v4only;

    } else if ((itr.flags & flag_v6only)) {
      if (!sap_is_inet6(current_sap))
        continue;
      open_flags |= fd_flag_v6only;

    } else if (sap_is_inet(current_sap)) {
      open_flags |= fd_flag_v4only;
    }

    int fd = attempt_open_connect(itr, current_sap.get(), open_flags);

    if (fd != -1)
      return fd;
  }

  LT_LOG("connect_socket failed (flags:0x%x address:%s)", flags, sa_pretty_str(connect_sa).c_str());
  return -1;
}

static int
attempt_listen_open(const sockaddr* bind_sa, int bind_flags) {
  if (!validate_bind_flags(bind_flags)) {
    // TODO: Warn here, do something.
    return -1;
  }

  // TODO: Move open_flags context to a helper method.
  fd_flags open_flags = fd_flag_stream | fd_flag_nonblock | fd_flag_reuse_address;

  // TODO: Validate bind sa is v4 or v6 respectively.
  if ((bind_flags & bind_manager::flag_v4only))
    open_flags |= fd_flag_v4only;
  if ((bind_flags & bind_manager::flag_v6only))
    open_flags |= fd_flag_v6only;

  int fd = fd_open(open_flags);

  if (fd == -1) {
    LT_LOG_SOCKADDR("listen open failed (flags:0x%x errno:%i message:'%s')", bind_sa, open_flags, errno, std::strerror(errno));
    return -1;
  }

  LT_LOG_SOCKADDR("listen open successed (flags:0x%x)", bind_sa, open_flags);
  return fd;
}

static std::tuple<uint16_t, uint16_t, uint16_t>
get_bind_ports(const bind_manager* manager, const bind_struct& itr) {
  uint16_t port_first = (itr.flags & bind_manager::flag_use_listen_ports) ? itr.listen_port_first : manager->listen_port_first();
  uint16_t port_last = (itr.flags & bind_manager::flag_use_listen_ports) ? itr.listen_port_last : manager->listen_port_last();
  uint16_t port_itr;

  if (port_first == 0 || port_last == 0 || port_first > port_last)
    throw internal_error("port_first == 0 || port_last == 0 || port_first > port_last");

  // TODO: Check itr flag first.
  // TODO: Test that we can get the last port number.
  if (!manager->is_port_randomize() || (port_last - port_first) == 0)
    port_itr = port_first;
  else
    port_itr = random_uniform_uint16(port_first, port_last);

  if (port_itr == 0 || port_itr < port_first || port_itr > port_last)
    throw internal_error("port_itr == 0 || port_itr < port_first || port_itr > port_last");

  return std::make_tuple(port_first, port_last, port_itr);
}

listen_result_type
bind_manager::attempt_listen(const bind_struct& bs) const {
  sa_unique_ptr sa = sap_copy(bs.address);
  int fd = attempt_listen_open(sa.get(), bs.flags);

  if (fd == -1)
    return listen_result_type{-1, NULL};

  uint16_t port_first, port_last, port_itr;
  std::tie(port_first, port_last, port_itr) = get_bind_ports(this, bs);
  uint16_t port_stop = port_itr;

  if (port_first == 0) {
    LT_LOG_SOCKADDR("listen got invalid port numbers (flags:0x%x)", sa.get(), bs.flags);
    return listen_result_type{-1, NULL};
  }

  do {
    sap_set_port(sa, port_itr);

    if (!fd_bind(fd, sa.get())) {
      if (errno == EADDRNOTAVAIL || errno == EAFNOSUPPORT) {
        LT_LOG_SOCKADDR("listen address not usable (fd:%i errno:%i message:'%s')", sa.get(), fd, errno, std::strerror(errno));
        fd_close(fd);
        return listen_result_type{-1, NULL};
      }

      if (port_itr == port_last)
        port_itr = port_first;
      else
        port_itr++;

      continue;
    }

    if (!fd_listen(fd, m_listen_backlog)) {
      LT_LOG_SOCKADDR("call to listen failed (fd:%i backlog:%i errno:%i message:'%s')",
                      sa.get(), fd, m_listen_backlog, errno, std::strerror(errno));
      fd_close(fd);
      return listen_result_type{-1, NULL};
    }

    LT_LOG_SOCKADDR("listen success (fd:%i)", sa.get(), fd);
    return listen_result_type{fd, sa_unique_ptr(sa.release())};

  } while (port_itr != port_stop);

  LT_LOG_SOCKADDR("listen ports exhausted (fd:%i port_first:%" PRIu16 " port_last:%" PRIu16 ")",
                  sa.get(), fd, port_first, port_last);
  fd_close(fd);
  return listen_result_type{-1, NULL};
}

listen_result_type
bind_manager::listen_socket(int flags) {
  LT_LOG("listen_socket attempt (flags:0x%x)", flags);

  for (auto& itr : *this) {
    listen_result_type result = attempt_listen(itr);

    if (result.fd != -1) {
      // TODO: Needs to be smarter.
      m_listen_port = sap_port(result.address);

      return result;
    }
  }

  LT_LOG("listen_socket failed (flags:0x%x)", flags);
  return listen_result_type{-1, NULL};
}

void
bind_manager::listen_open_all() {
  if (is_listen_open())
    return;

  m_flags |= flag_listen_open;
}

void
bind_manager::listen_close_all() {
  if (!is_listen_open())
    return;

  m_flags &= flag_listen_open;
}

void
bind_manager::set_listen_backlog(int backlog) {
  if (backlog < 0) {
    LT_LOG("could not set listen backlog, value less than zero (backlog:%i)", backlog);
    throw input_error("Could not set listen backlog, negative value not valid");
  }

  if (backlog > SOMAXCONN) {
    LT_LOG("could not set listen backlog, value greater than SOMAXCONN (backlog:%i SOMAXCONN:%i)",
           backlog, SOMAXCONN);
    throw input_error("Could not set listen backlog, value greater than SOMAXCONN");
  }

  m_listen_backlog = backlog;
}

void
bind_manager::set_listen_port_range(uint16_t port_first, uint16_t port_last, int flags) {
  if (port_first > port_last) {
    LT_LOG("could not set listen port range, invalid port range (port_first:%" PRIu16 " port_last:%" PRIu16 " flags:%i",
           port_first, port_last, flags);
    throw input_error("Could not set listen port range, invalid port range");
  }

  m_listen_port_first = port_first;
  m_listen_port_last = port_last;

  // Update based on flags.
  // Open listen ports. (try the same port as before, when multiple share same)
}

const sockaddr*
bind_manager::local_v6_address() const {
  for (auto& itr : *this) {
    if ((itr.flags & flag_v4only))
      continue;

    // TODO: Do two for loops?
    if (sap_is_any(itr.address)) {
      // TODO: Find suitable IPv6.
      LT_LOG_SOCKADDR("local_v6_address has not implemented ipv6 lookup for any address", itr.address.get(), 0);
      continue;
    }

    if (sap_is_inet(itr.address)) {
      LT_LOG_SOCKADDR("local_v6_address skipping bound v4 socket entry with no v4only, v6 lookup for this not implemented", itr.address.get(), 0);
      continue;
    }

    if (sap_is_inet6(itr.address)) {
      LT_LOG_SOCKADDR("local_v6_address found ipv6 address", itr.address.get(), 0);
      return itr.address.get();
    }
  }

  // TODO: How do we handle ipv6 lookup?

  LT_LOG("local_v6_address could not find any ipv6 address", 0);
  return NULL;
}

bool
bind_manager::m_listen_open_bind(bind_struct& bind) {
  
}

}
