#ifndef LIBTORRENT_PROTOCOL_WEBTORRENT_MANAGER_H
#define LIBTORRENT_PROTOCOL_WEBTORRENT_MANAGER_H

#include "config.h"

#include <memory>
#include <vector>

#include "protocol/webtorrent/rtc_signaling.h"

namespace torrent {

class DownloadMain;

namespace webtorrent {

class WebtorrentSession;

class WebtorrentManager {
public:
  explicit WebtorrentManager(DownloadMain* download);
  ~WebtorrentManager();

  WebtorrentManager(const WebtorrentManager&) = delete;
  WebtorrentManager& operator=(const WebtorrentManager&) = delete;

  void start();
  void stop();
  void close();
  void tick();

#ifdef USE_WEBTORRENT
  void receive_stream(RtcStream stream);
#endif

private:
  DownloadMain* m_download{};
  bool m_active{false};

#ifdef USE_WEBTORRENT
  std::vector<std::shared_ptr<WebtorrentSession>> m_sessions;
#endif
};

} // namespace webtorrent

} // namespace torrent

#endif
