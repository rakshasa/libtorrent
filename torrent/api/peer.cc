#include "config.h"

#include "peer.h"
#include "peer_wrapper.h"
#include "peer_connection.h"

namespace torrent {

Peer::Peer() :
  m_wrapper(new PeerWrapper) {
  
  m_wrapper->download = NULL;
  m_wrapper->peer = NULL;
}

Peer::Peer(const Peer& p) :
  m_wrapper(new PeerWrapper) {

  m_wrapper->download = p.m_wrapper->download;
  m_wrapper->peer = p.m_wrapper->peer;
}

Peer::~Peer() {
  delete m_wrapper;
}

bool
Peer::is_valid() {
  if (m_wrapper->download == NULL || m_wrapper->peer == NULL)
    return false;

  // TODO: Fix this
}

std::string
Peer::get_id() {
  return m_wrapper->peer->peer().id();
}

std::string
Peer::get_dns() {
  return m_wrapper->peer->peer().dns();
}

uint16_t
Peer::get_port() {
  return m_wrapper->peer->peer().port();
}

bool
Peer::get_local_choked() {
  return m_wrapper->peer->up().c_choked();
}

bool
Peer::get_local_interested() {
  return m_wrapper->peer->up().c_interested();
}

bool
Peer::get_remote_choked() {
  return m_wrapper->peer->down().c_choked();
}

bool
Peer::get_remote_interested() {
  return m_wrapper->peer->down().c_interested();
}

bool
Peer::get_choke_delayed() {
  return m_wrapper->peer->chokeDelayed();
}

bool
Peer::get_snubbed() {
  return m_wrapper->peer->throttle().get_snub();
}

// Bytes per second.
uint32_t
Peer::get_rate_down() {
  return m_wrapper->peer->throttle().down().rate_quick();
}

uint32_t
Peer::get_rate_up() {
  return m_wrapper->peer->throttle().up().rate_quick();
}

uint16_t
Peer::get_incoming_queue_size() {
  return m_wrapper->peer->down().c_list().size();
}

uint16_t
Peer::get_outgoing_queue_size() {
  return m_wrapper->peer->up().c_list().size();
}  

// Currently needs to copy the data once to a std::string. But 
// since gcc does ref counted std::string, you can inexpensively
// copy the resulting string. Will consider making BitField use a
// std::string.
std::string
Peer::get_bitfield() {
  return std::string(m_wrapper->peer->bitfield().data(),
		     m_wrapper->peer->bitfield().sizeBytes());
}

void
Peer::set_snubbed(bool v) {
  m_wrapper->peer->throttle().set_snub(v);

  if (v)
    m_wrapper->peer->choke(true);
}

void
Peer::clear() {
  m_wrapper->download = NULL;
  m_wrapper->peer = NULL;
}

Peer&
Peer::operator = (const Peer& p) {
  m_wrapper->download = p.m_wrapper->download;
  m_wrapper->peer = p.m_wrapper->peer;

  return *this;
}

}
