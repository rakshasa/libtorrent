#ifndef LIBTORRENT_DOWNLOAD_NET_H
#define LIBTORRENT_DOWNLOAD_NET_H

#include <inttypes.h>

namespace torrent {

class Rate;
class DownloadSettings;

class DownloadNet {
public:
  DownloadNet(DownloadSettings* s = NULL) : m_settings(s), m_endgame(false) {}

  uint32_t          pipe_size(const Rate& r);

  bool              get_endgame()       { return m_endgame; }
  void              set_endgame(bool b) { m_endgame = b; }

private:
  DownloadSettings* m_settings;

  bool              m_endgame;
};

}

#endif
