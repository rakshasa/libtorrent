#include "config.h"

#include "torrent/exceptions.h"
#include "download_net.h"
#include "settings.h"
#include "rate.h"

namespace torrent {

uint32_t
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

void
DownloadNet::update_endgame() {
  if (!m_endgame)
    m_endgame = (m_slotChunksCompleted() + m_delegator.get_chunks().size() + m_settings->endgameBorder)
      >= m_slotChunksCount();

  m_delegator.set_aggressive(m_endgame);
}

// High stall count peers should request if we're *not* in endgame, or
// if we're in endgame and the download is too slow. Prefere not to request
// from high stall counts when we are doing decent speeds.
bool
DownloadNet::should_request(uint32_t stall) {
  if (!m_endgame)
    return true;
  else
    return !stall || m_rateDown.rate() < m_settings->endgameRate;
}

}
