#include "config.h"

#include "torrent/exceptions.h"
#include "handshake.h"
#include "handshake_manager.h"

namespace torrent {

Handshake::~Handshake() {
  if (m_fd >= 0)
    throw internal_error("Handshake dtor called but m_fd is still open");

  delete [] m_buf;
}

void
Handshake::close() {
  if (m_fd < 0)
    return;

  close_socket(m_fd);
  m_fd = -1;
}

void
Handshake::send_connected() {
  m_manager->receive_connected(this);
}

void
Handshake::send_failed() {
  m_manager->receive_failed(this);
}

bool
Handshake::recv1() {
  if (m_pos == 0 &&
      !read_buf(m_buf, 1, m_pos))
    return false;

  int l = (unsigned char)m_buf[0];

  if (!read_buf(m_buf + m_pos, l + 29, m_pos))
    return false;

  m_peer.ref_protocol() = std::string(&m_buf[1], l);
  m_peer.ref_options()  = std::string(&m_buf[1+l], 8);
  m_hash                = std::string(&m_buf[9+l], 20);

  if (m_peer.protocol() != "BitTorrent protocol") {
    throw communication_error("Peer returned wrong protocol identifier");
  } else {
    return true;
  }
}

bool
Handshake::recv2() {
  if (!read_buf(m_buf + m_pos, 20, m_pos))
    return false;

  m_peer.ref_id() = std::string(m_buf, 20);

  return true;
}  

}
