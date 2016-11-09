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

#include "bind_manager.h"

#include "net/socket_fd.h"
#include "rak/socket_address.h"
#include "torrent/utils/log.h"

#define LT_LOG(log_fmt, ...)                                            \
  lt_log_print(LOG_CONNECTION_BIND, "bind: " log_fmt, __VA_ARGS__);
#define LT_LOG_SA(sa, log_fmt, ...)                                     \
  lt_log_print(LOG_CONNECTION_BIND, "bind->%s: " log_fmt, (sa)->pretty_address_str().c_str(), __VA_ARGS__);

namespace torrent {

bool
bind_manager::connect_socket(int file_desc, const sockaddr* connect_sockaddr, int flags) const {
  SocketFd sock_fd(file_desc);

  if (!empty()) {
    // Do stuff to bind socket.
    // (bindAddress->is_bindable() && !fd.bind(*bindAddress))

    LT_LOG("bind foo (fd:%i)", file_desc);
  }

  if (connect_sockaddr != NULL) {
    const rak::socket_address* connect_socket_addr = rak::socket_address::cast_from(connect_sockaddr);

    if (!sock_fd.connect(*connect_socket_addr)) {
      LT_LOG("connect failed (fd:%i address:%s errno:%i message:'%s')",
             file_desc, connect_socket_addr->pretty_address_str().c_str(), errno, std::strerror(errno));

      errno = 0;
      return false;
    }

    LT_LOG("connect success (fd:%i address:%s)", file_desc, connect_socket_addr->pretty_address_str().c_str());
  }

  return true;
}

}
