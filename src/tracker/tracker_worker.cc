#include "config.h"

#include "tracker_worker.h"

#include <netinet/in.h>

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

std::string
TrackerWorker::generate_error_message(int current_family, const std::string& current_msg, const std::string& last_msg) {
  constexpr auto not_resolved_msg = "Could not resolve hostname";

  std::string current_family_str = current_family == AF_INET ? "v4 : " : "v6 : ";
  std::string last_family_str    = current_family == AF_INET ? "v6 : " : "v4 : ";

  if (last_msg.empty())
    return current_family_str + current_msg;

  if (current_msg == not_resolved_msg && last_msg != not_resolved_msg)
    return current_family_str + current_msg;

  if (last_msg == not_resolved_msg && current_msg != not_resolved_msg)
    return last_family_str + last_msg;

  return current_family_str + current_msg + "  |  " + last_family_str + last_msg;
}

}  // namespace torrent
