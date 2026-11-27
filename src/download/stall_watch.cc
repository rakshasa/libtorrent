#include "config.h"

#include "download/stall_watch.h"

#include "torrent/common.h"

namespace torrent {

bool
StallWatch::update(uint64_t total) {
  auto now = this_thread::cached_seconds();

  if (m_last_update == std::chrono::seconds{} || total < m_last_total) {
    m_last_update = now;
    m_last_total  = total;
    m_starved     = 0;
    return false;
  }

  auto elapsed = static_cast<uint64_t>((now - m_last_update).count());

  if (elapsed == 0)
    return is_stalled();

  uint64_t received = total - m_last_total;
  uint64_t rate     = received / elapsed;
  uint64_t average  = m_history.rate() * 1024;

  m_last_update = now;
  m_last_total  = total;
  m_history.insert(received / 1024);

  if (rate < floor_rate || rate * ratio < average)
    m_starved++;
  else
    m_starved = 0;

  return is_stalled();
}

} // namespace torrent
