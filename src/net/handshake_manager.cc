#include "config.h"

#include "handshake_manager.h"
#include "handshake_incoming.h"
#include "handshake_outgoing.h"

#include "peer_info.h"

namespace torrent {

void
add_incoming(int fd,
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

