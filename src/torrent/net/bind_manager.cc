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
  if (sa_is_default(sa)) {
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
make_bind_struct(const sockaddr* sa, int flags, uint16_t priority) {
  return bind_struct { flags, prepare_sockaddr(sa, flags), priority, 6980, 6999 };
}

static bool
validate_bind_flags(int flags) {
  if ((flags & bind_manager::flag_v4only) && (flags & bind_manager::flag_v6only))
    return false;

  return true;
}

bind_manager::bind_manager() {
  // TODO: Move this to a different place.
  // base_type::push_back(make_bind_struct(NULL, 0, 0));

  // base_type::push_back(make_bind_struct(NULL, flag_v6only, 0));
  base_type::push_back(make_bind_struct(NULL, flag_v4only, 0));
}

void
bind_manager::add_bind(const sockaddr* sa, int flags) {
  if (!sa_is_bindable(sa)) {
    LT_LOG("add bind failed, address is not bindable (flags:0x%x address:%s)",
           flags, sa_pretty_address_str(sa).c_str());

    // Throw here?
    return;
  }

  // TODO: Verify bind flags. Also pass sa to verify that sockaddr is
  // a valid sockaddr.

  LT_LOG("bind added (flags:0x%x address:%s)",
         flags, sa_pretty_address_str(sa).c_str());

  // TODO: Add a way to set the order.
  //base_type::push_back(make_bind_struct(sa, flags, 0));
  base_type::push_back(make_bind_struct(sa, flag_v4only, 0));
}

static int
attempt_open_connect(const bind_struct& itr, const sockaddr* sockaddr, fd_flags flags) {
  int fd = fd_open(flags);

  if (fd == -1) {
    LT_LOG_SOCKADDR_ERROR("open_connect open failed", itr.address.get(), flags);
    return -1;
  }

  if (!sa_is_default(itr.address.get()) && !fd_bind(fd, itr.address.get())) {
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
bind_manager::connect_socket(const sockaddr* connect_sockaddr, int flags) const {
  LT_LOG("connect_socket attempt (flags:0x%x address:%s)",
         flags, sa_pretty_address_str(connect_sockaddr).c_str());

  // TODO: Don't attempt connect if default sockaddr.

  for (auto& itr : *this) {
    const sockaddr* sa = connect_sockaddr;

    std::unique_ptr<sockaddr> sockaddr_ptr;

    fd_flags open_flags = fd_flag_stream | fd_flag_nonblock;

    if (!validate_bind_flags(itr.flags)) {
      // TODO: Warn here, do something.
      continue;
    }

    if ((itr.flags & flag_v4only)) {
      // if (sa_is_v4mapped(sa)) {
      //   sockaddr_ptr = sa_from_v4mapped(sa);
      //   sa = sockaddr_ptr.get();
      // }

      if (!sa_is_inet(sa))
        continue;

      open_flags = open_flags | fd_flag_v4only;
    }

    if ((itr.flags & flag_v6only)) {
      // TODO: Map v4 addresses here? Check a flag.

      if (!sa_is_inet6(sa))
        continue;

      open_flags = open_flags | fd_flag_v6only;
    }

    int fd = attempt_open_connect(itr, sa, open_flags);

    if (fd != -1)
      return fd;
  }

  LT_LOG("connect_socket failed (flags:0x%x address:%s)",
         flags, sa_pretty_address_str(connect_sockaddr).c_str());

  return -1;
}

static int
attempt_listen_open(std::unique_ptr<sockaddr>& sa, int bind_flags) {
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
    LT_LOG_SOCKADDR("listen open failed (flags:0x%x errno:%i message:'%s')",
                    sa.get(), open_flags, errno, std::strerror(errno));
    return -1;
  }

  LT_LOG_SOCKADDR("listen open successed (flags:0x%x)", sa.get(), open_flags);
  return fd;
}

static int
attempt_listen(const bind_struct& bind_itr, int backlog, bind_manager::listen_fd_type listen_fd) {
  std::unique_ptr<sockaddr> sa = sa_copy(bind_itr.address.get());

  int fd = attempt_listen_open(sa, bind_itr.flags);

  if (fd == -1)
    return -1;

  for (uint16_t port = bind_itr.listen_port_first; port - 1 < bind_itr.listen_port_last; port++) {
    sa_set_port(sa.get(), port);

    if (!fd_bind(fd, sa.get())) {
      if (errno == EADDRNOTAVAIL || errno == EAFNOSUPPORT) {
        LT_LOG_SOCKADDR("listen address not usable (fd:%i errno:%i message:'%s')",
                        sa.get(), fd, errno, std::strerror(errno));
        fd_close(fd);
        return -1;
      }

      continue;
    }

    if (!fd_listen(fd, backlog)) {
      LT_LOG_SOCKADDR("call to listen failed (fd:%i backlog:%i errno:%i message:'%s')",
                      sa.get(), fd, backlog, errno, std::strerror(errno));
      fd_close(fd);
      return -1;
    }

    listen_fd(fd, sa.get());

    LT_LOG_SOCKADDR("listen success (fd:%i)", sa.get(), fd);
    return fd;
  }

  LT_LOG_SOCKADDR("listen ports exhausted (fd:%i port_first:%" PRIu16 " port_last:%" PRIu16 ")",
                  sa.get(), fd, bind_itr.listen_port_first, bind_itr.listen_port_last);
  fd_close(fd);
  return -1;
}

int
bind_manager::listen_socket(int flags, int backlog, listen_fd_type listen_fd) const {
  LT_LOG("listen_socket attempt (flags:0x%x)", flags);

  for (auto& itr : *this) {
    int fd = attempt_listen(itr, backlog, listen_fd);

    if (fd != -1)
      return fd;
  }

  LT_LOG("listen_socket failed (flags:0x%x)", flags);
  return -1;
}

}
