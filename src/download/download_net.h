#ifndef LIBTORRENT_DOWNLOAD_NET_H
#define LIBTORRENT_DOWNLOAD_NET_H

#include <deque>
#include <inttypes.h>

#include "content/delegator.h"
#include "torrent/peer.h"

#include "utils/rate.h"
#include "peer_info.h"

namespace torrent {

class DownloadSettings;
class PeerConnection;

class DownloadNet {
public:
  typedef std::deque<PeerInfo>                    PeerContainer;
  typedef std::list<PeerInfo>                     PeerList;
  typedef std::list<PeerConnection*>              ConnectionList;

  DownloadNet() : m_settings(NULL), m_endgame(false) {}
  ~DownloadNet();

  uint32_t          pipe_size(const Rate& r);

  bool              should_request(uint32_t stall);

  bool              get_endgame()                            { return m_endgame; }
  void              set_endgame(bool b);

  void              set_settings(DownloadSettings* s)        { m_settings = s; }

  Delegator&        get_delegator()                          { return m_delegator; }

  Rate&             get_rate_up()                            { return m_rateUp; }
  Rate&             get_rate_down()                          { return m_rateDown; }

  void              send_have_chunk(uint32_t index);

  // Peer connections management:

  ConnectionList&   get_connections()                        { return m_connections; }
  PeerContainer&    get_available_peers()                    { return m_availablePeers; }

  bool              add_connection(int fd, const PeerInfo& p);
  void              remove_connection(PeerConnection* p);

  void              add_available_peers(const PeerList& p);

  int               can_unchoke();
  void              choke_balance();
  void              connect_peers();

  int               count_connections() const; 

  // Signals and slots.

  typedef sigc::signal1<void, Peer>                            SignalPeer;
  typedef sigc::signal1<void, const std::string&>              SignalString;

  typedef sigc::slot2<PeerConnection*, int, const PeerInfo&>   SlotCreateConnection;
  typedef sigc::slot1<void, const PeerInfo&>                   SlotStartHandshake;
  typedef sigc::slot1<bool, const PeerInfo&>                   SlotHasHandshake;
  typedef sigc::slot0<uint32_t>                                SlotCountHandshakes;

  SignalPeer&   signal_peer_connected()                        { return m_signalPeerConnected; }
  SignalPeer&   signal_peer_disconnected()                     { return m_signalPeerDisconnected; }

  SignalString& signal_network_log()                           { return m_signalNetworkLog; }

  void          slot_create_connection(SlotCreateConnection s) { m_slotCreateConnection = s; }
  void          slot_start_handshake(SlotStartHandshake s)     { m_slotStartHandshake = s; }
  void          slot_has_handshake(SlotHasHandshake s)         { m_slotHasHandshake = s; }
  void          slot_count_handshakes(SlotCountHandshakes s)   { m_slotCountHandshakes = s; }

private:
  DownloadNet(const DownloadNet&);
  void operator = (const DownloadNet&);

  DownloadSettings*      m_settings;
  Delegator              m_delegator;
  ConnectionList         m_connections;
  PeerContainer          m_availablePeers;

  bool                   m_endgame;
  
  Rate                   m_rateUp;
  Rate                   m_rateDown;

  SignalPeer             m_signalPeerConnected;
  SignalPeer             m_signalPeerDisconnected;
  SignalString           m_signalNetworkLog;

  SlotCreateConnection   m_slotCreateConnection;
  SlotStartHandshake     m_slotStartHandshake;
  SlotHasHandshake       m_slotHasHandshake;
  SlotCountHandshakes    m_slotCountHandshakes;
};

}

#endif
