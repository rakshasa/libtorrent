#ifndef LIBTORRENT_DOWNLOAD_NET_H
#define LIBTORRENT_DOWNLOAD_NET_H

#include <vector>
#include <inttypes.h>

#include "content/delegator.h"
#include "torrent/peer.h"

#include "rate.h"
#include "peer_info.h"

namespace torrent {

class DownloadSettings;
class PeerConnection;

class DownloadNet {
public:
  typedef std::list<PeerInfo>          PeerList;
  typedef std::list<PeerConnection*> ConnectionList;

  DownloadNet() : m_settings(NULL), m_endgame(false) {}
  ~DownloadNet();

  uint32_t          pipe_size(const Rate& r);

  // Set endgame mode if we're close enough to the end.
  // Move this to download_state or whatever. Make a slot.
  void              update_endgame();

  bool              should_request(uint32_t stall);

  bool              get_endgame()                            { return m_endgame; }
  void              set_endgame(bool b)                      { m_endgame = b; }

  void              set_settings(DownloadSettings* s)        { m_settings = s; }

  Delegator&        get_delegator()                          { return m_delegator; }

  Rate&             get_rate_up()                            { return m_rateUp; }
  Rate&             get_rate_down()                          { return m_rateDown; }

  void              send_have_chunk(uint32_t index);

  // Peer connections management:

  ConnectionList&   get_connections()                        { return m_connections; }
  PeerList&         get_available_peers()                    { return m_availablePeers; }

  void              add_connection(int fd, const PeerInfo& p);
  void              remove_connection(PeerConnection* p);

  int               can_unchoke();
  void              choke_balance();
  void              connect_peers();

  int               count_connections() const; 

  // Deprecate these
  typedef sigc::slot0<uint32_t> SlotChunksCompleted;
  typedef sigc::slot0<uint32_t> SlotChunksCount;

  void slot_chunks_completed(SlotChunksCompleted s)          { m_slotChunksCompleted = s; }
  void slot_chunks_count(SlotChunksCount s)                  { m_slotChunksCount = s; }

  // Signals and slots.

  typedef sigc::signal1<void, Peer>                          SignalPeerConnected;
  typedef sigc::signal1<void, Peer>                          SignalPeerDisconnected;

  typedef sigc::slot2<PeerConnection*, int, const PeerInfo&> SlotCreateConnection;
  typedef sigc::slot1<void, const PeerInfo&>                 SlotStartHandshake;
  typedef sigc::slot0<uint32_t>                              SlotCountHandshakes;

  SignalPeerConnected& signal_peer_connected()               { return m_signalPeerConnected; }
  SignalPeerConnected& signal_peer_disconnected()            { return m_signalPeerDisconnected; }

  void slot_create_connection(SlotCreateConnection s)        { m_slotCreateConnection = s; }
  void slot_start_handshake(SlotStartHandshake s)            { m_slotStartHandshake = s; }
  void slot_count_handshakes(SlotCountHandshakes s)          { m_slotCountHandshakes = s; }

private:
  DownloadNet(const DownloadNet&);
  void operator = (const DownloadNet&);

  DownloadSettings*      m_settings;
  Delegator              m_delegator;
  ConnectionList         m_connections;
  PeerList               m_availablePeers;

  bool                   m_endgame;
  
  Rate                   m_rateUp;
  Rate                   m_rateDown;

  SlotChunksCompleted    m_slotChunksCompleted;
  SlotChunksCount        m_slotChunksCount;

  SignalPeerConnected    m_signalPeerConnected;
  SignalPeerDisconnected m_signalPeerDisconnected;

  SlotCreateConnection   m_slotCreateConnection;
  SlotStartHandshake     m_slotStartHandshake;
  SlotCountHandshakes    m_slotCountHandshakes;
};

}

#endif
