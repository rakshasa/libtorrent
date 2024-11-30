#include "config.h"

#include "choke_group.h"

#include <numeric>

#include "download/download_main.h"

namespace torrent {

choke_group::choke_group() :
    m_tracker_mode(TRACKER_MODE_NORMAL),
    m_down_queue(choke_queue::flag_unchoke_all_new),
    m_first(nullptr), m_last(nullptr) {}

uint64_t
choke_group::up_rate() const {
  return std::accumulate(m_first, m_last, (uint64_t)0, [](uint64_t i, auto r) { return i + r.up_rate()->rate(); });
}

uint64_t
choke_group::down_rate() const {
  return std::accumulate(m_first, m_last, (uint64_t)0, [](uint64_t i, auto r) { return i + r.down_rate()->rate(); });
}

} // namespace torrent
