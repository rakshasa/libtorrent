#include "config.h"

#include "tracker_worker.h"

#include "torrent/exceptions.h"

namespace torrent {

TrackerWorker::TrackerWorker(TrackerInfo info, int flags)
  : m_info(info) {

  m_state.m_flags = flags;
}

TrackerWorker::~TrackerWorker() noexcept(false) {
  if (!m_state.is_deleted())
    throw internal_error("TrackerWorker destroyed without being marked as deleted.");
}

}  // namespace torrent
