#include "config.h"

#include <cstring>

#include "torrent/exceptions.h"
#include "handshake_outgoing.h"
#include "handshake_manager.h"

namespace torrent {

HandshakeOutgoing::HandshakeOutgoing(int fd,
				     HandshakeManager* m,
				     const PeerInfo& p,
				     const std::string& infoHash,
				     const std::string& ourId) :
  Handshake(fd, m),
  m_state(CONNECTING) {
  
  m_peer = p;
  m_id = ourId;
  m_local = infoHash;

  insert_write();
  insert_except();
 
  m_buf[0] = 19;
  std::memcpy(&m_buf[1], "BitTorrent protocol", 19);
  std::memset(&m_buf[20], 0, 8);
  std::memcpy(&m_buf[28], m_local.c_str(), 20);
  std::memcpy(&m_buf[48], m_id.c_str(), 20);
}
  
void
HandshakeOutgoing::read() {
  try {

  switch (m_state) {
  case READ_HEADER1:
    if (!recv1())
      return;

    if (m_hash != m_local)
      throw communication_error("Peer returned wrong download hash");

    m_pos = 0;
    m_state = READ_HEADER2;

  case READ_HEADER2:
    if (!recv2())
      return;

    m_manager->receive_connected(this);

    return;

  default:
    throw internal_error("HandshakeOutgoing::read() called in wrong state");
  }

  } catch (network_error e) {
    m_manager->receive_failed(this);
  }
}  

void
HandshakeOutgoing::write() {
  int error;

  try {

  switch (m_state) {
  case CONNECTING:
    error = get_socket_error(m_fd);
 
    if (error)
      throw connection_error("Could not connect to client");
 
    m_state = WRITE_HEADER;
 
  case WRITE_HEADER:
    if (!write_buf(m_buf + m_pos, 68, m_pos))
      return;
 
    remove_write();
    insert_read();
 
    m_pos = 0;
    m_state = READ_HEADER1;
 
    return;

  default:
    throw internal_error("HandshakeOutgoing::write() called in wrong state");
  }

  } catch (network_error e) {
    m_manager->receive_failed(this);
  }
}

void
HandshakeOutgoing::except() {
  m_manager->receive_failed(this);
}

}

