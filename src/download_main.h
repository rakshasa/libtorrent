#ifndef LIBTORRENT_DOWNLOAD_MAIN_H
#define LIBTORRENT_DOWNLOAD_MAIN_H

#include "bencode.h"
#include "service.h"
#include "download_state.h"
#include "download/download_net.h"

#include <sigc++/connection.h>

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
  DownloadNet&    net()             { return m_net; }
  TrackerControl& tracker()         { return *m_tracker; }

  void add_peers(const Peers& p);

  static DownloadMain*  getDownload(const std::string& hash);
  static Downloads&     downloads() { return m_downloads; }

  static std::string get_download_id(const std::string& hash);

  static void receive_connection(int fd, const std::string& hash, const PeerInfo& peer);

  // Modifying signals
  typedef sigc::signal0<void>              SignalTrackerSucceded;

  SignalTrackerSucceded& signal_tracker_succeded() { return m_signalTrackerSucceded; }

private:
  DownloadMain();

  void receive_initial_hash(const std::string& id);
  void receive_download_done();

  void setup_delegator();
  void setup_net();
  void setup_tracker(const bencode& b);

  void setup_start();
  void setup_stop();

  static Downloads m_downloads;
  
  DownloadState m_state;
  DownloadNet   m_net;
  TrackerControl* m_tracker;

  std::string m_name;
  bool m_checked;
  bool m_started;

  SignalTrackerSucceded m_signalTrackerSucceded;

  sigc::connection m_connectionChunkPassed;
  sigc::connection m_connectionChunkFailed;
};

} // namespace torrent

#endif // LIBTORRENT_DOWNLOAD_H

