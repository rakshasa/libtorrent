#include "config.h"

#include "torrent/exceptions.h"
#include "download_net.h"
#include "settings.h"
#include "rate.h"

namespace torrent {

unsigned int
DownloadNet::pipe_size(const Rate& r) {
  float s = r.rate();

  if (!m_endgame)
    if (s < 50000.0f)
      return std::max(2, (int)((s + 2000.0f) / 2000.0f));
    else
      return std::min(80, (int)((s + 160000.0f) / 8000.0f));

  else
    if (s < 4000.0f)
      return 1;
    else
      return std::min(80, (int)((s + 32000.0f) / 16000.0f));
}

}
