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

  static constexpr priority_type iptos_default     = 0;
  static constexpr priority_type iptos_lowdelay    = IPTOS_LOWDELAY;
  static constexpr priority_type iptos_throughput  = IPTOS_THROUGHPUT;
  static constexpr priority_type iptos_reliability = IPTOS_RELIABILITY;

#if defined IPTOS_MINCOST
  static constexpr priority_type iptos_mincost     = IPTOS_MINCOST;
#elif defined IPTOS_LOWCOST
  static constexpr priority_type iptos_mincost     = IPTOS_LOWCOST;
#else
  static constexpr priority_type iptos_mincost     = iptos_throughput;
#endif

  static constexpr uint32_t encryption_none             = 0;
  static constexpr uint32_t encryption_allow_incoming   = (1 << 0);
  static constexpr uint32_t encryption_try_outgoing     = (1 << 1);
  static constexpr uint32_t encryption_require          = (1 << 2);
  static constexpr uint32_t encryption_require_RC4      = (1 << 3);
  static constexpr uint32_t encryption_enable_retry     = (1 << 4);
  static constexpr uint32_t encryption_prefer_plaintext = (1 << 5);

  // Internal to libtorrent.
  static constexpr uint32_t encryption_use_proxy        = (1 << 6);
  static constexpr uint32_t encryption_retrying         = (1 << 7);

  enum {
    handshake_incoming           = 1,
    handshake_outgoing           = 2,
    handshake_outgoing_encrypted = 3,
    handshake_outgoing_proxy     = 4,
    handshake_success            = 5,
    handshake_dropped            = 6,
    handshake_failed             = 7,
    handshake_retry_plaintext    = 8,
    handshake_retry_encrypted    = 9
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
  uint32_t            encryption_options() const              { return m_encryptionOptions; }

  void                set_max_size(size_type s)               { m_maxSize = s; }
  void                set_priority(priority_type p)           { m_priority = p; }
  void                set_send_buffer_size(uint32_t s);
  void                set_receive_buffer_size(uint32_t s);
  void                set_encryption_options(uint32_t options);

  uint32_t            filter(const sockaddr* sa);
  void                set_filter(const slot_filter_type& s)   { m_slot_filter = s; }

  bool                listen_open(port_type begin, port_type end);
  void                listen_close();

  void                set_listen_backlog(int backlog);

  // The slot returns a ThrottlePair to use for the given address, or
  // NULLs to use the default throttle.
  slot_throttle_type& address_throttle()  { return m_slot_address_throttle; }

  // For internal usage.
  Listen*             listen()            { return m_listen; }

  bool                is_block_ipv4() const    { return m_block_ipv4; }
  bool                is_block_ipv6() const    { return m_block_ipv6; }
  bool                is_block_ipv4in6() const { return m_block_ipv4in6; }
  bool                is_prefer_ipv6() const   { return m_prefer_ipv6; }

  void                set_block_ipv4(bool v)   { m_block_ipv4 = v; }
  void                set_block_ipv6(bool v)   { m_block_ipv6 = v; }
  void                set_block_ipv4in6(bool v) { m_block_ipv4in6 = v; }
  void                set_prefer_ipv6(bool v)  { m_prefer_ipv6 = v; }

private:
  size_type           m_size{0};
  size_type           m_maxSize{0};

  priority_type       m_priority{iptos_throughput};
  uint32_t            m_sendBufferSize{0};
  uint32_t            m_receiveBufferSize{0};
  int                 m_encryptionOptions{encryption_none};

  Listen*             m_listen;

  slot_filter_type    m_slot_filter;
  slot_throttle_type  m_slot_address_throttle;

  bool                m_block_ipv4{false};
  bool                m_block_ipv6{false};
  bool                m_block_ipv4in6{false};
  bool                m_prefer_ipv6{false};
};

} // namespace torrent

#endif

