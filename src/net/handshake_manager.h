#ifndef LIBTORRENT_HANDSHAKE_MANAGER_H
#define LIBTORRENT_HANDSHAKE_MANAGER_H

#include <list>
#include <inttypes.h>

namespace torrent {

class Handshake;
class PeerInfo;

class HandshakeManager {
public:
  typedef std::list<Handshake*> HandshakeList;

  Handshakes() : m_size(0) {}
  ~Handshakes();

  void                add_incoming(int fd,
				   const std::string& dns,
				   uint16_t port);

  void                add_outgoing(const PeerInfo& p,
				   const std::string& ourId,
				   const std::string& infoHash);

  uint32_t            get_size()                       { return m_size; }

  typedef sigc::slot4<void, const PeerInfo&> SlotConnected;

  void                slot_connected(SlotConnected s)  { m_slotConnected = s; }

private:

  void                receive_connected(Handshake* h);
  void                receive_failed(Handshake* h);

  HandshakeList       m_handshakes;
  uint32_t            m_size;

  SlotConnected       m_slotConnected;
};

}

#endif

  
