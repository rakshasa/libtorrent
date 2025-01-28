#include "config.h"

#include "torrent/tracker/tracker_manager.h"

namespace torrent {

// Add logging.

TrackerWrapper TrackerManager::add(Tracker* tracker) {
  std::lock_guard<std::mutex> guard(m_lock);

  auto result = base_type::insert(std::make_shared<Tracker>(tracker));

  if (!result.second)
    throw internal_error("TrackerManager::add(...) tracker already exists.");

  return TrackerWrapper(result.first);
}

  // void           remove(TrackerWrapper tracker);
void TrackerManager::remove(TrackerWrapper tracker) {
  std::lock_guard<std::mutex> guard(m_lock);

  base_type::erase(tracker.get());
}

} // namespace torrent
