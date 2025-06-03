#ifndef LIBTORRENT_CONNECTION_MANAGER_H
#define LIBTORRENT_CONNECTION_MANAGER_H

#include <functional>
#include <list>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <torrent/common.h>

namespace torrent {

// Standard pair of up/down throttles.
// First element is upload throttle, second element is download throttle.
using ThrottlePair = std::pair<Throttle*, Throttle*>;

class LIBTORRENT_EXPORT ConnectionManager {
public:
  using size_type     = uint32_t;
  using port_type     = uint16_t;
  using priority_type = uint8_t;

  enum iptos : priority_type {
    def         = 0,
    lowdelay    = IPTOS_LOWDELAY,
    throughput  = IPTOS_THROUGHPUT,
    reliability = IPTOS_RELIABILITY,

#if defined IPTOS_MINCOST
    mincost     = IPTOS_MINCOST,
#elif defined IPTOS_LOWCOST
    mincost     = IPTOS_LOWCOST,
#else
    mincost     = iptos::throughput,
#endif
  };

  enum encryption : uint32_t {
    none             = 0,
    allow_incoming   = (1 << 0),
    try_outgoing     = (1 << 1),
    require          = (1 << 2),
    require_RC4      = (1 << 3),
    enable_retry     = (1 << 4),
    prefer_plaintext = (1 << 5),

  // Internal to libtorrent.
    use_proxy        = (1 << 6),
    retrying         = (1 << 7),
  };

  enum {
    handshake_incoming           = 1,
    handshake_outgoing           = 2,
    handshake_outgoing_encrypted = 3,
    handshake_outgoing_proxy     = 4,
    handshake_success            = 5,
    handshake_dropped            = 6,
    handshake_failed             = 7,
    handshake_retry_plaintext    = 8,
    handshake_retry_encrypted    = 9,
  };

  using slot_filter_type   = std::function<uint32_t(const sockaddr*)>;
  using slot_throttle_type = std::function<ThrottlePair(const sockaddr*)>;

  ConnectionManager();
  ~ConnectionManager();
  ConnectionManager(const ConnectionManager&) = delete;
  ConnectionManager& operator=(const ConnectionManager&) = delete;

  // Check that we have not surpassed the max number of open sockets
  // and that we're allowed to connect to the socket address.
  //
  // Consider only checking max number of open sockets.
  bool                can_connect() const;

  // Call this to keep the socket count up to date.
  void                inc_socket_count()                      { m_size++; }
  void                dec_socket_count()                      { m_size--; }

  size_type           size() const                            { return m_size; }
  size_type           max_size() const                        { return m_maxSize; }

  priority_type       priority() const                        { return m_priority; }
  uint32_t            send_buffer_size() const                { return m_sendBufferSize; }
  uint32_t            receive_buffer_size() const             { return m_receiveBufferSize; }
  uint32_t            encryption_options()                    { return m_encryptionOptions; }

  void                set_max_size(size_type s)               { m_maxSize = s; }
  void                set_priority(priority_type p)           { m_priority = p; }
  void                set_send_buffer_size(uint32_t s);
  void                set_receive_buffer_size(uint32_t s);
  void                set_encryption_options(uint32_t options);

  // Setting the addresses creates a copy of the address.
  const sockaddr*     bind_address() const                    { return m_bindAddress; }
  const sockaddr*     local_address() const                   { return m_localAddress; }
  const sockaddr*     proxy_address() const                   { return m_proxyAddress; }

  void                set_bind_address(const sockaddr* sa);
  void                set_local_address(const sockaddr* sa);
  void                set_proxy_address(const sockaddr* sa);

  uint32_t            filter(const sockaddr* sa);
  void                set_filter(const slot_filter_type& s)   { m_slot_filter = s; }

  bool                listen_open(port_type begin, port_type end);
  void                listen_close();

  // Since trackers need our port number, it doesn't get cleared after
  // 'listen_close()'. The client may change the reported port number,
  // but do note that it gets overwritten after 'listen_open(...)'.
  port_type           listen_port() const                     { return m_listen_port; }
  int                 listen_backlog() const                  { return m_listen_backlog; }
  void                set_listen_port(port_type p)            { m_listen_port = p; }
  void                set_listen_backlog(int v);

  // The slot returns a ThrottlePair to use for the given address, or
  // NULLs to use the default throttle.
  slot_throttle_type& address_throttle()  { return m_slot_address_throttle; }

  // For internal usage.
  Listen*             listen()            { return m_listen; }

  bool                is_block_ipv4() const  { return m_block_ipv4; }
  void                set_block_ipv4(bool v) { m_block_ipv4 = v; }

  bool                is_block_ipv6() const  { return m_block_ipv6; }
  void                set_block_ipv6(bool v) { m_block_ipv6 = v; }

  bool                is_prefer_ipv6() const  { return m_prefer_ipv6; }
  void                set_prefer_ipv6(bool v) { m_prefer_ipv6 = v; }

private:
  size_type           m_size{0};
  size_type           m_maxSize{0};

  priority_type       m_priority{iptos::throughput};
  uint32_t            m_sendBufferSize{0};
  uint32_t            m_receiveBufferSize{0};
  int                 m_encryptionOptions{encryption::none};

  sockaddr*           m_bindAddress;
  sockaddr*           m_localAddress;
  sockaddr*           m_proxyAddress;

  Listen*             m_listen;
  port_type           m_listen_port{0};
  uint32_t            m_listen_backlog{SOMAXCONN};

  slot_filter_type    m_slot_filter;
  slot_throttle_type  m_slot_address_throttle;

  bool                m_block_ipv4{false};
  bool                m_block_ipv6{false};
  bool                m_prefer_ipv6{false};
};

}

#endif

