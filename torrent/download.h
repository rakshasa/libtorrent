#ifndef LIBTORRENT_DOWNLOAD_H
#define LIBTORRENT_DOWNLOAD_H

#include "bencode.h"
#include "service.h"
#include "download_state.h"

namespace torrent {

class Download : public Service {
  friend class TrackerQuery;

public:
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

  TrackerQuery& tracker()        { return *m_tracker; }

  static Download*  getDownload(const std::string& hash);
  static Downloads& downloads() { return m_downloads; }

  DownloadState& state() { return m_state; }

private:
  Download();

  static Downloads m_downloads;
  
  DownloadState m_state;
  TrackerQuery* m_tracker;

  std::string m_name;
  bool m_checked;
};

} // namespace torrent

#endif // LIBTORRENT_DOWNLOAD_H

