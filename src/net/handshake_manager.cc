#include "config.h"

#include <algo/algo.h>
#include <netinet/in.h>

#include "torrent/exceptions.h"
#include "handshake_manager.h"
#include "handshake_incoming.h"
#include "handshake_outgoing.h"
#include "peer_info.h"

using namespace algo;

namespace torrent {

void
HandshakeManager::add_incoming(int fd,
			       const std::string& dns,
			       uint16_t port) {
  SocketBase::close_socket(fd);
}
  
void
HandshakeManager::add_outgoing(const PeerInfo& p,
			       const std::string& infoHash,
			       const std::string& ourId) {
  try {
    sockaddr_in sa;
    SocketBase::make_sockaddr(p.dns(), p.port(), sa);

    m_handshakes.push_back(new HandshakeOutgoing(SocketBase::make_socket(sa), this, p, infoHash, ourId));
    m_size++;

  } catch (network_error e) {
  } catch (local_error e) {
  }
}

void
HandshakeManager::clear() {
  std::for_each(m_handshakes.begin(), m_handshakes.end(),
		branch(call_member(&Handshake::close), delete_on()));

  m_handshakes.clear();
}

uint32_t
HandshakeManager::get_size_hash(const std::string& hash) {
  uint32_t s = 0;

  std::for_each(m_handshakes.begin(), m_handshakes.end(),
		if_on(eq(call_member(&Handshake::get_hash), ref(hash)),

		      add_ref(s, value(1))));

  return s;
}

bool
HandshakeManager::has_peer(const PeerInfo& p) {
  return std::find_if(m_handshakes.begin(), m_handshakes.end(),
		      call_member(call_member(&Handshake::get_peer), &PeerInfo::is_same_host, ref(p)))
    != m_handshakes.end();
}

void
HandshakeManager::receive_connected(Handshake* h) {
  remove(h);

  m_slotConnected(h->fd(), h->get_hash(), h->get_peer());

  h->set_fd(-1);
  delete h;
}

void
HandshakeManager::receive_failed(Handshake* h) {
  remove(h);

  h->close();
  delete h;
}

void
HandshakeManager::remove(Handshake* h) {
  HandshakeList::iterator itr = std::find(m_handshakes.begin(), m_handshakes.end(), h);

  if (itr == m_handshakes.end())
    throw internal_error("HandshakeManager::remove(...) could not find Handshake");

  m_handshakes.erase(itr);
}

}
