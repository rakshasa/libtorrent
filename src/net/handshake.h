#ifndef LIBTORRENT_HANDSHAKE_H
#define LIBTORRENT_HANDSHAKE_H

#include <inttypes.h>

#include "socket_base.h"
#include "peer_info.h"

namespace torrent {

class HandshakeManager;

class Handshake : public SocketBase {
public:
  Handshake(int fd, HandshakeManager* m) :
    SocketBase(fd),
    m_manager(m),
    m_buf(new char[256 + 48]),
    m_pos(0) {}

  virtual ~Handshake();

  const PeerInfo&     get_peer() { return m_peer; }
  const std::string&  get_hash() { return m_hash; }
  const std::string&  get_id()   { return m_id; }

  void                set_manager(HandshakeManager* m) { m_manager = m; }
  void                set_fd(int fd)                   { m_fd = fd; }

  void                close();

protected:
  
  // Make sure you don't touch anything in the class after calling these,
  // return immidiately.
  void send_connected();
  void send_failed();

  bool recv1();
  bool recv2();

  PeerInfo          m_peer;
  std::string       m_hash;
  std::string       m_id;

  HandshakeManager* m_manager;

  char*             m_buf;
  uint32_t          m_pos;
};

}

#endif

