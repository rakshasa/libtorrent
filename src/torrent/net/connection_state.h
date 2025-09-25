#ifndef LIBTORRENT_TORRENT_NET_CONNECTION_STATE_H
#define LIBTORRENT_TORRENT_NET_CONNECTION_STATE_H

#include <torrent/net/types.h>

namespace torrent::net {

class LIBTORRENT_EXPORT ConnectionState {
public:

  int                 current_family() const                  { return m_current_family; }
  int                 last_failure_family() const            { return m_last_failure_family; }

  void                set_current_family(int family);
  void                set_last_failure_family(int family);

  int                 success_inet() const                    { return m_success_inet; }
  int                 success_inet6() const                   { return m_success_inet6; }

  int                 failures_inet() const                   { return m_failures_inet; }
  int                 failures_inet6() const                  { return m_failures_inet6; }

private:
  int                 m_current_family{AF_UNSPEC};
  int                 m_last_failure_family{AF_UNSPEC};

  int                 m_success_inet{0};
  int                 m_success_inet6{0};

  int                 m_failures_inet{0};
  int                 m_failures_inet6{0};



  // c_sa_shared_ptr     m_bind_address;

  // has connected to inet/inet6

  // int                 m_attempts{0};
  // int                 m_successes{0};
  // int                 m_failures{0};

  // int                 m_last_family{AF_UNSPEC};

  // bool                m_failed{false};

};

int select_next_address_family(ConnectionState& state);

} // namespace torrent::net

#endif
