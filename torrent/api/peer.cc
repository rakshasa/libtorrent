#include "config.h"

#include "peer.h"
#include "peer_connection.h"

namespace torrent {

std::string
Peer::get_id() {
  return ((PeerConnection*)m_ptr)->peer().id();
}

std::string
Peer::get_dns() {
  return ((PeerConnection*)m_ptr)->peer().dns();
}

uint16_t
Peer::get_port() {
  return ((PeerConnection*)m_ptr)->peer().port();
}

bool
Peer::get_local_choked() {
  return ((PeerConnection*)m_ptr)->up().c_choked();
}

bool
Peer::get_local_interested() {
  return ((PeerConnection*)m_ptr)->up().c_interested();
}

bool
Peer::get_remote_choked() {
  return ((PeerConnection*)m_ptr)->down().c_choked();
}

bool
Peer::get_remote_interested() {
  return ((PeerConnection*)m_ptr)->down().c_interested();
}

bool
Peer::get_choke_delayed() {
  return ((PeerConnection*)m_ptr)->chokeDelayed();
}

bool
Peer::get_snubbed() {
  return ((PeerConnection*)m_ptr)->throttle().get_snub();
}

// Bytes per second.
uint32_t
Peer::get_rate_down() {
  return ((PeerConnection*)m_ptr)->throttle().down().rate_quick();
}

uint32_t
Peer::get_rate_up() {
  return ((PeerConnection*)m_ptr)->throttle().up().rate_quick();
}

uint16_t
Peer::get_incoming_queue_size() {
  return ((PeerConnection*)m_ptr)->down().c_list().size();
}

uint16_t
Peer::get_outgoing_queue_size() {
  return ((PeerConnection*)m_ptr)->up().c_list().size();
}  

// Currently needs to copy the data once to a std::string. But 
// since gcc does ref counted std::string, you can inexpensively
// copy the resulting string. Will consider making BitField use a
// std::string.
std::string
Peer::get_bitfield() {
  return std::string(((PeerConnection*)m_ptr)->bitfield().data(),
		     ((PeerConnection*)m_ptr)->bitfield().sizeBytes());
}

void
Peer::set_snubbed(bool v) {
  ((PeerConnection*)m_ptr)->throttle().set_snub(v);

  if (v)
    ((PeerConnection*)m_ptr)->choke(true);
}

}
