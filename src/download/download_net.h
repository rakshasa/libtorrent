#ifndef LIBTORRENT_DOWNLOAD_NET_H
#define LIBTORRENT_DOWNLOAD_NET_H

#include <inttypes.h>
#include "content/delegator.h"

namespace torrent {

class Rate;
class DownloadSettings;

class DownloadNet {
public:
  DownloadNet(DownloadSettings* s = NULL) : m_settings(s), m_endgame(false) {}

  uint32_t          pipe_size(const Rate& r);

  bool              get_endgame()       { return m_endgame; }
  void              set_endgame(bool b) { m_endgame = b; }

  Delegator&        get_delegator()     { return m_delegator; }

private:
  DownloadSettings* m_settings;
  Delegator         m_delegator;

  bool              m_endgame;
};

}

#endif
