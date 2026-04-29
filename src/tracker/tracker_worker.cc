#include "config.h"

#include "tracker_worker.h"

namespace torrent {

TrackerWorker::TrackerWorker(TrackerInfo info, int flags)
  : m_info(info) {

  m_state.m_flags = flags;
}

TrackerWorker::~TrackerWorker() = default;

}  // namespace torrent
