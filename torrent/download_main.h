#ifndef LIBTORRENT_DOWNLOAD_MAIN_H
#define LIBTORRENT_DOWNLOAD_MAIN_H

#include "bencode.h"
#include "service.h"
#include "download_state.h"

namespace torrent {

class TrackerControl;

class DownloadMain : public Service {
public:
  typedef std::list<PeerInfo> Peers;
  typedef std::list<DownloadMain*> Downloads;

  enum ServiceState {
    CHOKE_CYCLE = 0x1001
  };

  DownloadMain(const bencode& b);
  ~DownloadMain();

  void start();
  void stop();

  bool is_active() { return m_started; }
  bool isStopped();

  void service(int type);

  std::string& name() { return m_name; }

  DownloadState&  state()           { return m_state; }
  TrackerControl& tracker()         { return *m_tracker; }

  void add_peers(const Peers& p);

  static DownloadMain*  getDownload(const std::string& hash);
  static Downloads& downloads() { return m_downloads; }

private:
  DownloadMain();

  void receive_initial_hash(const std::string& id);

  static Downloads m_downloads;
  
  DownloadState m_state;
  TrackerControl* m_tracker;

  std::string m_name;
  bool m_checked;
  bool m_started;
};

} // namespace torrent

#endif // LIBTORRENT_DOWNLOAD_H

