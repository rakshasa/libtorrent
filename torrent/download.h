#ifndef LIBTORRENT_DOWNLOAD_H
#define LIBTORRENT_DOWNLOAD_H

#include "bencode.h"
#include "bitfield.h"
#include "delegator.h"
#include "storage.h"
#include "service.h"
#include "settings.h"
#include "files.h"
#include "timer.h"
#include "peer.h"
#include "rate.h"
#include <list>

namespace torrent {

class Download : public Service {
  friend class PeerConnection;
  friend class PeerHandshake;
  friend class TrackerQuery;

public:
  typedef std::list<PeerConnection*> Connections;
  typedef std::list<Download*> Downloads;
  typedef std::list<Peer> Peers;

  enum ServiceState {
    HASH_COMPLETED = 0x1000,
    CHOKE_CYCLE = 0x1001
  };

  Download(const bencode& b);
  ~Download();

  void start();
  void stop();

  bool isStopped();

  void service(int type);

  std::string& name() { return m_name; }

  const Peer& me() const { return m_me; }

  Files&        files()          { return m_files; }
  Connections&  connections()    { return m_connections; }
  TrackerQuery& tracker()        { return *m_tracker; }
  Peers         availablePeers() { return m_availablePeers; }
  Delegator&    delegator()      { return m_delegator; }

  static Download*  getDownload(const std::string& hash);
  static Downloads& downloads() { return m_downloads; }

  uint64_t bytesDownloaded() const { return m_bytesDownloaded; }
  uint64_t bytesUploaded() const { return m_bytesUploaded; }

  Rate& rateUp() { return m_rateUp; }
  Rate& rateDown() { return m_rateDown; }

  int countConnections() const; 

  void connectPeers();

  // How many peers we can unchoke, negative means we must choke someone.
  void chokeBalance();
  
  DownloadSettings& settings() { return m_settings; }
  const DownloadSettings& settings() const { return m_settings; }

protected:

  // Adds a peer with the socket fd ready for communication.
  void addPeer(const Peer& p);
  void addConnection(int fd, const Peer& p);

  void removeConnection(PeerConnection* p);

  void chunkDone(Chunk& c);

  int canUnchoke();

  uint64_t m_bytesUploaded;
  uint64_t m_bytesDownloaded;

private:
  Download();

  static Downloads m_downloads;
  
  std::string m_name;

  Peer m_me;
  
  Files m_files;
  Delegator m_delegator;
  Connections m_connections;
  Peers m_availablePeers;

  TrackerQuery* m_tracker;

  bool m_checked;

  DownloadSettings m_settings;
  Rate m_rateUp;
  Rate m_rateDown;
};

} // namespace torrent

#endif // LIBTORRENT_DOWNLOAD_H

