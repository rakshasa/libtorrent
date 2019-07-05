#ifndef LIBTORRENT_NET_BIND_MANAGER_H
#define LIBTORRENT_NET_BIND_MANAGER_H

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <torrent/common.h>
#include <torrent/net/socket_address.h>

namespace torrent {

class socket_listen;

// Disable copy ctor, etc.
struct LIBTORRENT_EXPORT bind_struct {
  std::string name;

  int flags;
  c_sa_unique_ptr address;
  uint16_t priority;

  uint16_t listen_port_first;
  uint16_t listen_port_last;
  std::unique_ptr<socket_listen> listen;

  const sockaddr* listen_socket_address() const;
  uint16_t        listen_socket_address_port() const;
};

// TODO: Move listen to bind_struct.
struct listen_result_type {
  int fd;
  sa_unique_ptr address;
};

class LIBTORRENT_EXPORT bind_manager : private std::vector<bind_struct> {
public:
  typedef std::vector<bind_struct> base_type;
  typedef std::function<int ()>    alloc_fd_ftor;
  typedef std::function<void (int, const sockaddr*)> accepted_ftor;

  using base_type::iterator;
  using base_type::const_iterator;
  using base_type::reference;
  using base_type::const_reference;

  using base_type::value_type;
  using base_type::size_type;

  using base_type::empty;
  using base_type::size;

  // TODO: Add block_* flag that can override global block setting.
  // TODO: Add http bind support.
  // TODO: Flag to remove old entry if same name.
  enum flags_type {
    flag_v4only = 0x1,
    flag_v6only = 0x2,
    flag_use_listen_ports = 0x4,
    flag_listen_open = 0x8,
    flag_listen_closed = 0x10,
    flag_port_randomize = 0x20,
    flag_block_accept = 0x40,
    flag_block_connect = 0x80,
  };

  bind_manager();
  ~bind_manager();

  void clear();

  const_iterator begin() const;
  const_iterator end() const;

  const_reference front() const;
  const_reference back() const;
  const_reference operator[](size_type pos) const;

  const_iterator find_name(const std::string& name) const;
  const_iterator lower_bound_priority(uint16_t priority) const;
  const_iterator upper_bound_priority(uint16_t priority) const;

  int flags();

  bool is_listen_open() const;
  bool is_port_randomize() const;
  bool is_block_accept() const;
  bool is_block_connect() const;

  void set_port_randomize(bool flag);
  void set_block_accept(bool flag);
  void set_block_connect(bool flag);

  void add_bind(const std::string& name, uint16_t priority, const sockaddr* bind_sa, int flags);
  bool remove_bind(const std::string& name);
  void remove_bind_throw(const std::string& name);

  int connect_socket(const sockaddr* connect_sa, int flags) const;

  // TODO: Deprecate.
  listen_result_type listen_socket(int flags);

  // TODO: Open listen port count, error message in client if none open.

  void     listen_open_all();
  void     listen_close_all();

  bool     listen_open_bind(const std::string& name);

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
  iterator m_find_name(const std::string& name);
  iterator m_lower_bound_priority(uint16_t priority);
  iterator m_upper_bound_priority(uint16_t priority);

  bool     m_listen_open_bind(bind_struct& bind);

  // TODO: Deprecate.
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

inline bind_manager::const_reference bind_manager::front() const { return base_type::front(); }
inline bind_manager::const_reference bind_manager::back() const { return base_type::back(); }
inline bind_manager::const_reference bind_manager::operator[](bind_manager::size_type pos) const { return base_type::operator[](pos); }

inline int bind_manager::flags() { return m_flags; }

inline bool bind_manager::is_listen_open() const { return m_flags & flag_listen_open; }
inline bool bind_manager::is_port_randomize() const { return m_flags & flag_port_randomize; }
inline bool bind_manager::is_block_accept() const { return m_flags & flag_block_accept; }
inline bool bind_manager::is_block_connect() const { return m_flags & flag_block_connect; }

inline bind_manager::accepted_ftor& bind_manager::slot_accepted() { return m_slot_accepted; }

inline bind_manager::const_iterator
bind_manager::find_name(const std::string& name) const {
  return std::find_if(begin(), end(), [&name](const_reference itr)->bool { return itr.name == name; });
}

inline bind_manager::const_iterator
bind_manager::lower_bound_priority(uint16_t priority) const {
  return std::find_if(begin(), end(), [priority](const_reference itr)->bool { return itr.priority >= priority; });
}

inline bind_manager::const_iterator
bind_manager::upper_bound_priority(uint16_t priority) const {
  return std::find_if(begin(), end(), [priority](const_reference itr)->bool { return itr.priority > priority; });
}

inline bind_manager::iterator
bind_manager::m_find_name(const std::string& name) {
  return std::find_if(base_type::begin(), base_type::end(), [&name](reference itr)->bool { return itr.name == name; });
}

inline bind_manager::iterator
bind_manager::m_lower_bound_priority(uint16_t priority) {
  return std::find_if(base_type::begin(), base_type::end(), [priority](reference itr)->bool { return itr.priority >= priority; });
}

inline bind_manager::iterator
bind_manager::m_upper_bound_priority(uint16_t priority) {
  return std::find_if(base_type::begin(), base_type::end(), [priority](reference itr)->bool { return itr.priority > priority; });
}

}

#endif
