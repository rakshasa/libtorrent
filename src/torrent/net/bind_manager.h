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

// Add some helpfull words here.

#ifndef LIBTORRENT_NET_BIND_MANAGER_H
#define LIBTORRENT_NET_BIND_MANAGER_H

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <torrent/common.h>

namespace torrent {

struct bind_struct {
  std::string name;

  int flags;
  std::unique_ptr<const sockaddr> address;

  uint16_t priority;
  uint16_t listen_port_first;
  uint16_t listen_port_last;
};

// Move listen to bind_struct.
struct listen_result_type {
  int fd;
  std::unique_ptr<struct sockaddr> address;
};

class LIBTORRENT_EXPORT bind_manager : private std::vector<bind_struct> {
public:
  typedef std::vector<bind_struct> base_type;
  typedef std::function<int ()>    alloc_fd_ftor;
  typedef std::function<void (int, const sockaddr*)> accepted_ftor;

  using base_type::iterator;
  using base_type::const_iterator;
  using base_type::value_type;

  using base_type::back;
  using base_type::front;
  using base_type::empty;
  using base_type::size;

  // TODO: Add block_* flag that can override global block setting.
  // TODO: Add http bind support.
  enum flags_type {
    flag_v4only = 0x1,
    flag_v6only = 0x2,
    flag_use_listen_ports = 0x4,
    flag_listen_open = 0x8,
    flag_port_randomize = 0x10,
    flag_block_accept = 0x20,
    flag_block_connect = 0x40,
  };

  bind_manager();

  void clear();

  const_iterator begin() const;
  const_iterator end() const;

  const_iterator find_name(const std::string& name) const;

  int flags();

  bool is_listen_open() const;
  bool is_port_randomize() const;
  bool is_block_accept() const;
  bool is_block_connect() const;

  void set_port_randomize(bool flag);
  void set_block_accept(bool flag);
  void set_block_connect(bool flag);

  // TODO: Rename.
  void add_bind(const std::string& name, uint16_t priority, const sockaddr* sa, int flags);

  int connect_socket(const sockaddr* sock_addr, int flags) const;

  listen_result_type listen_socket(int flags);

  void     listen_open();
  void     listen_close();
  //void     listen_close_all();

  int      listen_backlog() const { return m_listen_backlog; }
  uint16_t listen_port() const { return m_listen_port; }
  //uint16_t listen_port_on_sockaddr() const;
  uint16_t listen_port_first() const { return m_listen_port_first; }
  uint16_t listen_port_last() const { return m_listen_port_last; }

  void     set_listen_backlog(int backlog);
  void     set_listen_port_range(uint16_t port_first, uint16_t port_last, int flags = 0);

  const sockaddr* local_v6_address() const;

  accepted_ftor&  slot_accepted();

private:
  listen_result_type attempt_listen(const bind_struct& bind_itr) const;

  int      m_flags;

  int      m_listen_backlog;
  uint16_t m_listen_port;
  uint16_t m_listen_port_first;
  uint16_t m_listen_port_last;

  accepted_ftor m_slot_accepted;
};

// Implementation:

inline bind_manager::const_iterator begin(const bind_manager& b) { return b.begin(); }
inline bind_manager::const_iterator end(const bind_manager& b) { return b.end(); }

inline bind_manager::const_iterator bind_manager::begin() const { return base_type::begin(); }
inline bind_manager::const_iterator bind_manager::end() const { return base_type::end(); }

inline int bind_manager::flags() { return m_flags; }

inline bool bind_manager::is_listen_open() const { return m_flags & flag_listen_open; }
inline bool bind_manager::is_port_randomize() const { return m_flags & flag_port_randomize; }
inline bool bind_manager::is_block_accept() const { return m_flags & flag_block_accept; }
inline bool bind_manager::is_block_connect() const { return m_flags & flag_block_connect; }

inline bind_manager::accepted_ftor& bind_manager::slot_accepted() { return m_slot_accepted; }

inline bind_manager::const_iterator
bind_manager::find_name(const std::string& name) const {
  return std::find_if(begin(), end(), [&name](const value_type& itr)->bool { return itr.name == name; });
}

}

#endif
