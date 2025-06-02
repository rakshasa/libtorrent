#include "config.h"

#include "log_buffer.h"

#include <algorithm>

#include "log.h"

namespace torrent {

// Rename function/args?
log_buffer::const_iterator
log_buffer::find_older(int32_t older_than) {
  if (empty() || !back().is_younger_than(older_than))
    return end();

  return std::find_if(begin(), end(), [older_than](const auto& entry) { return entry.is_younger_or_same(older_than); });
}

void
log_buffer::lock_and_push_log(const char* data, size_t length, int group) {
  if (group < 0)
    return;

  lock();

  if (size() >= max_size())
    base_type::pop_front();

  base_type::push_back(log_entry(this_thread::cached_seconds().count(), group % 6, std::string(data, length)));

  if (m_slot_update)
    m_slot_update();

  unlock();
}

static void
log_buffer_deleter(log_buffer* lb) {
  delete lb;
}

log_buffer_ptr
log_open_log_buffer(const char* name) {
  // TODO: Deregister when deleting.
  auto buffer = log_buffer_ptr(new log_buffer, &log_buffer_deleter);

  log_open_output(name, [b = buffer.get()](auto d, auto l, auto g) { b->lock_and_push_log(d, l, g); });
  return buffer;
}

} // namespace torrent
