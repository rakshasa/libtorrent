#ifndef LIBTORRENT_HANDSHAKE_MANAGER_H
#define LIBTORRENT_HANDSHAKE_MANAGER_H

#include <list>
#include <string>
#include <inttypes.h>
#include <sigc++/slot.h>

namespace torrent {

class PeerInfo;
class Handshake;

class HandshakeManager {
public:
  typedef std::list<Handshake*> HandshakeList;

  HandshakeManager() : m_size(0) {}
  ~HandshakeManager() { clear(); }

  void                add_incoming(int fd,
				   const std::string& dns,
				   uint16_t port);

  void                add_outgoing(const PeerInfo& p,
				   const std::string& infoHash,
				   const std::string& ourId);

  void                clear();

  uint32_t            get_size()                               { return m_size; }
  uint32_t            get_size_hash(const std::string& hash);

  bool                has_peer(const PeerInfo& p);

  // File descriptor
  // Info hash
  // Peer info
  typedef sigc::slot3<void, int, const std::string&, const PeerInfo&> SlotConnected;
  typedef sigc::slot1<std::string, const std::string&>                SlotDownloadId;

  void                slot_connected(SlotConnected s)          { m_slotConnected = s; }
  void                slot_download_id(SlotDownloadId s)       { m_slotDownloadId = s; }

  void                receive_connected(Handshake* h);
  void                receive_failed(Handshake* h);

  std::string         get_download_id(const std::string& hash) { return m_slotDownloadId(hash); }

private:

  void                remove(Handshake* h);

  HandshakeList       m_handshakes;
  uint32_t            m_size;

  SlotConnected       m_slotConnected;
  SlotDownloadId      m_slotDownloadId;
};

}

#endif

  
