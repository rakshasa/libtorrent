#ifndef LIBTORRENT_DOWNLOAD_H
#define LIBTORRENT_DOWNLOAD_H

#include "bencode.h"
#include "service.h"
#include "download_state.h"

namespace torrent {

class TrackerControl;

class Download : public Service {
public:
  typedef std::list<Peer> Peers;
  typedef std::list<Download*> Downloads;

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

  DownloadState&  state()           { return m_state; }
  TrackerControl& tracker()         { return *m_tracker; }

  void add_peers(const Peers& p);

  static Download*  getDownload(const std::string& hash);
  static Downloads& downloads() { return m_downloads; }

private:
  Download();

  static Downloads m_downloads;
  
  DownloadState m_state;
  TrackerControl* m_tracker;

  std::string m_name;
  bool m_checked;
  bool m_started;
};

} // namespace torrent

#endif // LIBTORRENT_DOWNLOAD_H

