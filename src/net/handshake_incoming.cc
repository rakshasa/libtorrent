#include "config.h"

#include "torrent/exceptions.h"

#include "handshake_incoming.h"
#include "handshake_manager.h"

namespace torrent {

HandshakeIncoming::HandshakeIncoming(int fd, HandshakeManager* m, const PeerInfo& p) :
  Handshake(fd, m),
  m_state(READ_HEADER1) {

  m_peer = p;

  insert_read();
  insert_except();
}

void
HandshakeIncoming::read() {
  try {

  switch (m_state) {
  case READ_HEADER1:
    if (!recv1())
      return;

    if ((m_id = m_manager->get_download_id(m_hash)).length() == 0)
      throw close_connection();
    
    m_buf[0] = 19;
    std::memcpy(&m_buf[1], "BitTorrent protocol", 19);
    std::memset(&m_buf[20], 0, 8);
    std::memcpy(&m_buf[28], m_hash.c_str(), 20);
    std::memcpy(&m_buf[48], m_id.c_str(), 20);

    m_pos = 0;
    m_state = WRITE_HEADER;

    remove_read();
    insert_write();

    return;

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
HandshakeIncoming::write() {
  try {
  switch (m_state) {
  case WRITE_HEADER:
    if (!write_buf(m_buf + m_pos, 68, m_pos))
      return;
 
    remove_write();
    insert_read();
 
    m_pos = 0;
    m_state = READ_HEADER2;
 
    return;

  default:
    throw internal_error("HandshakeOutgoing::write() called in wrong state");
  }

  } catch (network_error e) {
    m_manager->receive_failed(this);
  }
}

void
HandshakeIncoming::except() {
  m_manager->receive_failed(this);
}

}

