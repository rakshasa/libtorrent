// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#include "config.h"

#include <cerrno>
#include <cstring>
#include <cinttypes>

#include "bind_manager.h"

#include "net/socket_fd.h"
#include "torrent/exceptions.h"
#include "torrent/net/fd.h"
#include "torrent/net/socket_address.h"
#include "torrent/utils/log.h"

#define LT_LOG(log_fmt, ...)                                            \
  lt_log_print(LOG_CONNECTION_BIND, "bind: " log_fmt, __VA_ARGS__);
#define LT_LOG_SOCKADDR(log_fmt, sa, ...)                               \
  lt_log_print(LOG_CONNECTION_BIND, "bind->%s: " log_fmt, sa_pretty_address_str(sa).c_str(), __VA_ARGS__);
#define LT_LOG_SOCKADDR_ERROR(log_fmt, bind_sa, flags)                  \
  LT_LOG_SOCKADDR(log_fmt " (flags:0x%x fd:%i address:%s errno:%i message:'%s')", \
                  bind_sa, flags, fd, sa_pretty_address_str(sockaddr).c_str(), errno, std::strerror(errno));

namespace torrent {

static std::unique_ptr<sockaddr>
prepare_sockaddr(const sockaddr* sa, int flags) {
  if (sa_is_any(sa)) {
    if ((flags & bind_manager::flag_v4only))
      return sa_make_inet();

    return sa_make_inet6();
  }

  if ((flags & bind_manager::flag_v4only) && !sa_is_inet(sa))
    throw internal_error("(flags & flag_v4only) && !sa_is_inet(sa)");

  if ((flags & bind_manager::flag_v6only) && !sa_is_inet6(sa))
    throw internal_error("(flags & flag_v6only) && !sa_is_inet(sa)");

  return sa_copy(sa);
}

bind_struct
make_bind_struct(const std::string& name, const sockaddr* sa, int flags, uint16_t priority) {
  return bind_struct { name, flags, prepare_sockaddr(sa, flags), priority, 0, 0 };
}

static bool
validate_bind_flags(int flags) {
  if ((flags & bind_manager::flag_v4only) && (flags & bind_manager::flag_v6only))
    return false;

  return true;
}

bind_manager::bind_manager() :
  m_flags(flag_port_randomize),
  m_listen_backlog(SOMAXCONN),
  m_listen_port_first(6881),
  m_listen_port_last(6999)
{
  base_type::push_back(make_bind_struct("default", NULL, 0, 0));
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
bind_manager::add_bind(const std::string& name, uint16_t priority, const sockaddr* sa, int flags) {
  if (!sa_is_bindable(sa)) {
    LT_LOG("add bind failed, address is not bindable (flags:0x%x address:%s)",
           flags, sa_pretty_address_str(sa).c_str());

    // Throw here?
    return;
  }

  // TODO: Verify passed flags, etc.
  // TODO: Verify bind flags. Also pass sa to verify that sockaddr is
  // a valid sockaddr.

  if (sa_is_inet(sa))
    flags |= flag_v4only;

  // if inet6, can't be v4only.

  LT_LOG("bind added (flags:0x%x address:%s)",
         flags, sa_pretty_address_str(sa).c_str());

  // TODO: Sanitize the flags.
  // TODO: Add a way to set the order.
  // TODO: Verify unique name.

  base_type::push_back(make_bind_struct(name, sa, flags, priority));
}

static int
attempt_open_connect(const bind_struct& itr, const sockaddr* sockaddr, fd_flags flags) {
  int fd = fd_open(flags);

  if (fd == -1) {
    LT_LOG_SOCKADDR_ERROR("open_connect open failed", itr.address.get(), flags);
    return -1;
  }

  if (!sa_is_any(itr.address.get()) && !fd_bind(fd, itr.address.get())) {
    LT_LOG_SOCKADDR_ERROR("open_connect bind failed", itr.address.get(), flags);
    fd_close(fd);
    return -1;
  }

  if (!fd_connect(fd, sockaddr)) {
    LT_LOG_SOCKADDR_ERROR("open_connect connect failed", itr.address.get(), flags);
    fd_close(fd);
    return -1;
  }

  LT_LOG_SOCKADDR("open_connect succeeded (flags:0x%x fd:%i address:%s)",
                  itr.address.get(), flags, fd, sa_pretty_address_str(sockaddr).c_str());
  return fd;
}

int
bind_manager::connect_socket(const sockaddr* connect_sa, int flags) const {
  if (sa_is_any(connect_sa)) {
    LT_LOG("connect_socket called with invalid sockaddr (flags:0x%x address:%s)", flags, sa_pretty_address_str(connect_sa).c_str());
    return -1;
  }

  if (m_flags & flag_block_connect) {
    LT_LOG("connect_socket blocked (flags:0x%x address:%s)", flags, sa_pretty_address_str(connect_sa).c_str());
    return -1;
  }

  LT_LOG("connect_socket attempt (flags:0x%x address:%s)", flags, sa_pretty_address_str(connect_sa).c_str());

  for (auto& itr : *this) {
    if (itr.flags & flag_block_connect)
      continue;

    std::unique_ptr<sockaddr> tmp_sa;

    const sockaddr* sa = connect_sa;
    fd_flags open_flags = fd_flag_stream | fd_flag_nonblock;

    if (!validate_bind_flags(itr.flags))
      continue; // TODO: Warn here, do something.

    if ((itr.flags & flag_v4only)) {
      if (sa_is_v4mapped(sa))
        sa = (tmp_sa = sa_from_v4mapped(sa)).get();

      if (!sa_is_inet(sa))
        continue;

      open_flags = open_flags | fd_flag_v4only;

    } else if ((itr.flags & flag_v6only)) {
      if (!sa_is_inet6(sa))
        continue;

      open_flags = open_flags | fd_flag_v6only;

    } else {
      if (sa_is_inet(sa))
        sa = (tmp_sa = sa_to_v4mapped(sa)).get();

      if (!sa_is_inet6(sa))
        continue; // TODO: Exception here?
    }

    int fd = attempt_open_connect(itr, sa, open_flags);

    if (fd != -1) {
      return fd;
    }
  }

  LT_LOG("connect_socket failed (flags:0x%x address:%s)", flags, sa_pretty_address_str(connect_sa).c_str());
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
    open_flags = open_flags | fd_flag_v4only;

  if ((bind_flags & bind_manager::flag_v6only))
    open_flags = open_flags | fd_flag_v6only;

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
    return std::make_tuple(0, 0, 0);

  // TODO: Check itr flag first.
  // TODO: Test that we can get the last port number.
  if (!manager->is_port_randomize() || (port_last - port_first) == 0)
    port_itr = port_first;
  else
    port_itr = port_first + rand() % (port_last - port_first + 1);

  if (port_itr == 0 || port_itr < port_first || port_itr > port_last)
    return std::make_tuple(0, 0, 0);

  return std::make_tuple(port_first, port_last, port_itr);
}

listen_result_type
bind_manager::attempt_listen(const bind_struct& bind_itr) const {
  std::unique_ptr<sockaddr> sa = sa_copy(bind_itr.address.get());
  int fd = attempt_listen_open(sa.get(), bind_itr.flags);

  if (fd == -1)
    return listen_result_type{-1, NULL};

  uint16_t port_first, port_last, port_itr;
  std::tie(port_first, port_last, port_itr) = get_bind_ports(this, bind_itr);
  uint16_t port_stop = port_itr;

  if (port_first == 0) {
    LT_LOG_SOCKADDR("listen got invalid port numbers (flags:0x%x)", sa.get(), bind_itr.flags);
    return listen_result_type{-1, NULL};
  }

  do {
    sa_set_port(sa.get(), port_itr);

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
    return listen_result_type{fd, std::unique_ptr<struct sockaddr>(sa.release())};

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
      m_listen_port = sa_port(result.sockaddr.get());

      return result;
    }
  }

  LT_LOG("listen_socket failed (flags:0x%x)", flags);
  return listen_result_type{-1, NULL};
}

void
bind_manager::listen_open() {
  if (m_flags & flag_listen_open)
    return;

  m_flags |= flag_listen_open;
}

void
bind_manager::listen_close() {
  if (m_flags | flag_listen_open)
    return;

  m_flags &= flag_listen_open;
}

void
bind_manager::set_listen_port_range(uint16_t port_first, uint16_t port_last, int flags) {
  if (port_first > port_last) {
    LT_LOG("could not set listen port range, invalid port range: %" PRIu16 "-%" PRIu16, 0);
    throw input_error("could not set listen port range, invalid port range");
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
    if (sa_is_any(itr.address.get())) {
      // TODO: Find suitable IPv6.
      LT_LOG_SOCKADDR("local_v6_address has not implemented ipv6 lookup for any address", itr.address.get(), 0);
      continue;
    }

    if (sa_is_inet(itr.address.get())) {
      LT_LOG_SOCKADDR("local_v6_address skipping bound v4 socket entry with no v4only, v6 lookup for this not implemented", itr.address.get(), 0);
      continue;
    }

    if (sa_is_inet6(itr.address.get())) {
      LT_LOG_SOCKADDR("local_v6_address found ipv6 address", itr.address.get(), 0);
      return itr.address.get();
    }
  }

  // TODO: How do we handle ipv6 lookup?

  LT_LOG("local_v6_address could not find any ipv6 address", 0);
  return NULL;
}

}
