#ifndef LIBTORRENT_SETTINGS_H
#define LIBTORRENT_SETTINGS_H

#include <string>

namespace torrent {

class Settings {
 public:
  static std::string httpName;
  static std::string peerName;

  static int filesCheckWait;
  static int filesMode;
  static int dirMode;

  static int rateStart;
  static int rateWindow;
  static int rateQuick;
};

class DownloadSettings {
 public:
  DownloadSettings();

  int minPeers;
  int maxPeers;

  int maxUploads;

  int chokeCycle;
  int chokeGracePeriod;

  static DownloadSettings& global() { return *m_global; }

 private:
  static DownloadSettings* m_global;
};

// When pipelining of requests is implemented, add bounds so it can never
// overflow peer_connection's writeBuf.

}

#endif // LIBTORRENT_SETTINGS_H
