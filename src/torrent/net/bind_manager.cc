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
#include "rak/socket_address.h"
#include "torrent/net/socket_address.h"
#include "torrent/utils/log.h"

#define LT_LOG(log_fmt, ...)                                            \
  lt_log_print(LOG_CONNECTION_BIND, "bind: " log_fmt, __VA_ARGS__);
#define LT_LOG_SOCKADDR(log_fmt, sa, ...)                               \
  lt_log_print(LOG_CONNECTION_BIND, "bind->%s: " log_fmt, sa_pretty_address_str(sa).c_str(), __VA_ARGS__);

namespace torrent {

bind_struct
make_bind_struct(const sockaddr* a, int f, uint16_t priority) {
  auto addr = new rak::socket_address;

  if (a == NULL)
    addr->clear();
  else
    addr->copy_sockaddr(a);

  return bind_struct { f, std::unique_ptr<const sockaddr>(addr->c_sockaddr()), priority, 0, 0 };
}

inline const rak::socket_address*
bind_struct_cast_address(const bind_struct& bs) {
  return rak::socket_address::cast_from(bs.address.get());
}

bind_manager::bind_manager() {
  // TODO: Move this to a different place.
  base_type::push_back(make_bind_struct(NULL, 0, 0));
}

void
bind_manager::add_bind(const sockaddr* bind_sockaddr, int flags) {
  if (!sa_is_bindable(bind_sockaddr)) {
    LT_LOG("add bind failed, address is not bindable (flags:0x%x address:%s)",
           flags, sa_pretty_address_str(bind_sockaddr).c_str());

    // Throw here?
    return;
  }

  LT_LOG("bind added (flags:0x%x address:%s)",
         flags, sa_pretty_address_str(bind_sockaddr).c_str());

  // TODO: Add a way to set the order.
  base_type::push_back(make_bind_struct(bind_sockaddr, flags, 0));
}

static int
attempt_connect(const sockaddr* connect_sockaddr, const sockaddr* bind_sockaddr, bind_manager::alloc_fd_ftor alloc_fd) {
  int file_desc = alloc_fd();

  if (file_desc == -1) {
    LT_LOG("open failed (address:%s errno:%i message:'%s')",
           sa_pretty_address_str(connect_sockaddr).c_str(), errno, std::strerror(errno));

    return -1;
  }

  SocketFd socket_fd(file_desc);

  if (!sa_is_default(bind_sockaddr) && !socket_fd.bind_sa(bind_sockaddr)) {
    LT_LOG_SOCKADDR("bind failed (fd:%i address:%s errno:%i message:'%s')",
                    bind_sockaddr, file_desc, sa_pretty_address_str(connect_sockaddr).c_str(), errno, std::strerror(errno));

    socket_fd.close();
    return -1;
  }

  if (!socket_fd.connect_sa(connect_sockaddr)) {
    LT_LOG_SOCKADDR("connect failed (fd:%i address:%s errno:%i message:'%s')",
                    bind_sockaddr, file_desc, sa_pretty_address_str(connect_sockaddr).c_str(), errno, std::strerror(errno));

    socket_fd.close();
    return -1;
  }

  LT_LOG_SOCKADDR("connect success (fd:%i address:%s)",
                  bind_sockaddr, file_desc, sa_pretty_address_str(connect_sockaddr).c_str());

  return file_desc;
}

int
bind_manager::connect_socket(const sockaddr* connect_sockaddr, int flags, alloc_fd_ftor alloc_fd) const {
  // Santitize flags.

  LT_LOG("connect_socket attempt (flags:0x%x address:%s)",
         flags, sa_pretty_address_str(connect_sockaddr).c_str());

  for (auto& itr : *this) {
    int file_desc = attempt_connect(connect_sockaddr, itr.address.get(), alloc_fd);

    if (file_desc != -1) {
      LT_LOG("connect_socket succeeded (flags:0x%x address:%s fd:%i)",
             flags, sa_pretty_address_str(connect_sockaddr).c_str(), file_desc);

      return file_desc;
    }
  }

  LT_LOG("connect_socket failed (flags:0x%x address:%s)",
         flags, sa_pretty_address_str(connect_sockaddr).c_str());

  return -1;
}

static int
attempt_listen(const sockaddr* bind_sockaddr, uint16_t port_first, uint16_t port_last, bind_manager::alloc_fd_ftor alloc_fd, bind_manager::listen_fd_type listen_fd) {
  // TODO: Move up.
  int file_desc = alloc_fd();

  if (file_desc == -1) {
    LT_LOG_SOCKADDR("open failed (errno:%i message:'%s')",
                    bind_sockaddr, errno, std::strerror(errno));

    return -1;
  }

  SocketFd socket_fd(file_desc);
  rak::socket_address sa;

  if (!sa_is_default(bind_sockaddr)) {
    sa.copy_sockaddr(bind_sockaddr);
  } else {
    // TODO: This will need to handle ipv4/6 difference.
    // sa.sa_inet()->clear();
    sa.sa_inet6()->clear();
  }

  for (uint16_t port = port_first; port != port_last; port++) {
    sa.set_port(port);

    if (socket_fd.bind(sa)) {
      if (!listen_fd(file_desc, port)) {
        LT_LOG_SOCKADDR("call to listen failed (fd:%i backlog:%i errno:%i message:'%s')",
                        sa.c_sockaddr(), file_desc, 128, errno, std::strerror(errno));
        break;
      }

      LT_LOG_SOCKADDR("listen success (fd:%i backlog:%i)", sa.c_sockaddr(), file_desc, 128);

      return socket_fd.get_fd();
    }
  }

  LT_LOG_SOCKADDR("listen failed (fd:%i port_first:%" PRIu16 " port_last:%" PRIu16 ")",
                  sa.c_sockaddr(), file_desc, port_first, port_last);

  socket_fd.close();
  return -1;
}

int
bind_manager::listen_socket(uint16_t port_first, uint16_t port_last, int flags, alloc_fd_ftor alloc_fd, listen_fd_type listen_fd) const {
  // TODO: Remove this log message.
  LT_LOG("listen_socket attempt (flags:0x%x port_first:%" PRIu16 " port_last:%" PRIu16 ")",
         flags, port_first, port_last);

  for (auto& itr : *this) {
    int file_desc = attempt_listen(itr.address.get(), port_first, port_last, alloc_fd, listen_fd);

    if (file_desc != -1) {
      LT_LOG("listen_socket succeeded (flags:0x%x fd:%i port_first:%" PRIu16 " port_last:%" PRIu16 ")",
             flags, file_desc, port_first, port_last);

      return file_desc;
    }
  }

  LT_LOG("listen_socket failed (flags:0x%x port_first:%" PRIu16 " port_last:%" PRIu16 ")",
         flags, port_first, port_last);

  return -1;
}

}
