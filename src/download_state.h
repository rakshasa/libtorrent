#ifndef LIBTORRENT_DOWNLOAD_STATE_H
#define LIBTORRENT_DOWNLOAD_STATE_H

#include "bitfield_counter.h"
#include "peer_info.h"
#include "rate.h"
#include "settings.h"
#include "content/content.h"
#include "data/hash_torrent.h"
#include "torrent/peer.h"

#include <list>
#include <sigc++/signal.h>

namespace torrent {

extern HashQueue hashQueue;
extern HashTorrent hashTorrent;

class PeerConnection;
class DownloadNet;

// Here goes all those things that Peer* and Delegator needs.
class DownloadState {
 public:
  typedef std::list<PeerInfo>        Peers;
  typedef std::list<PeerConnection*> Connections;

  DownloadState();
  ~DownloadState();

  PeerInfo&     me()              { return m_me; }
  std::string&  hash()            { return m_hash; }

  Content&      content()         { return m_content; }
  Connections&  connections()     { return m_connections; }
  Peers&        available_peers() { return m_availablePeers; }

  uint64_t& bytesDownloaded() { return m_bytesDownloaded; }
  uint64_t& bytesUploaded() { return m_bytesUploaded; }

  Rate& rateUp() { return m_rateUp; }
  Rate& rateDown() { return m_rateDown; }

  DownloadSettings& settings() { return m_settings; }
  const DownloadSettings& settings() const { return m_settings; }

  int canUnchoke();
  void chokeBalance();

  void chunk_done(unsigned int index);

  BitFieldCounter& bfCounter() { return m_bfCounter; }

  int countConnections() const; 

  void addConnection(int fd, const PeerInfo& p);
  void removeConnection(PeerConnection* p);

  void download_stats(uint64_t& down, uint64_t& up, uint64_t& left);

  void connect_peers();

  void set_net(DownloadNet* net) { m_net = net; }

  // Incoming signals.
  void receive_hashdone(std::string id, Storage::Chunk c, std::string hash);

  typedef sigc::signal1<void, Peer>     SignalPeerConnected;
  typedef sigc::signal1<void, Peer>     SignalPeerDisconnected;
  typedef sigc::signal1<void, uint32_t> SignalChunkPassed;
  typedef sigc::signal1<void, uint32_t> SignalChunkFailed;

  SignalPeerConnected&                  signal_peer_connected()    { return m_signalPeerConnected; }
  SignalPeerConnected&                  signal_peer_disconnected() { return m_signalPeerDisconnected; }
  SignalChunkPassed&                    signal_chunk_passed()      { return m_signalChunkPassed; }
  SignalChunkFailed&                    signal_chunk_failed()      { return m_signalChunkFailed; }

private:
  // Disable
  DownloadState(const DownloadState&);
  void operator = (const DownloadState&);

  uint64_t m_bytesUploaded;
  uint64_t m_bytesDownloaded;

  PeerInfo m_me;
  std::string m_hash;
  
  Content m_content;
  Connections m_connections;
  Peers m_availablePeers;

  DownloadNet* m_net;
  DownloadSettings m_settings;
  Rate m_rateUp;
  Rate m_rateDown;

  BitFieldCounter m_bfCounter;

  SignalPeerConnected    m_signalPeerConnected;
  SignalPeerDisconnected m_signalPeerDisconnected;
  SignalChunkPassed      m_signalChunkPassed;
  SignalChunkFailed      m_signalChunkFailed;
};

}

#endif
