#include "config.h"

#include "tracker_worker.h"

#include "torrent/exceptions.h"
#include "torrent/system/callbacks.h"

namespace torrent {

TrackerWorker::TrackerWorker(TrackerInfo info, int flags)
  : m_callback_id(system::make_callback_id()),
    m_info(info) {

  m_state.m_flags = flags;
}

TrackerWorker::~TrackerWorker() noexcept(false) {
  if (!m_state.is_deleted())
    throw internal_error("TrackerWorker destroyed without being marked as deleted.");
}

void
TrackerWorker::mark_starting_request() {
  if (type() == TRACKER_DHT)
    return;

  auto guard = lock_guard();
  m_state.m_flags |= tracker::TrackerState::flag_starting_request;
}

void
TrackerWorker::remove_events() {
  system::cancel_callback_and_wait(m_callback_id, main_thread::thread(), tracker_thread::thread());
}

}  // namespace torrent
