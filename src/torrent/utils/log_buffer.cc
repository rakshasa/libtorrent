#include "config.h"

#include "log_buffer.h"

#include "log.h"
#include "globals.h"

namespace torrent {

// Rename function/args?
log_buffer::const_iterator
log_buffer::find_older(int32_t older_than) {
  if (empty() || !back().is_younger_than(older_than))
    return end();

  return std::find_if(begin(), end(), std::bind(&log_entry::is_younger_or_same, std::placeholders::_1, older_than));
}

void
log_buffer::lock_and_push_log(const char* data, size_t length, int group) {
  if (group < 0)
    return;

  lock();

  if (size() >= max_size())
    base_type::pop_front();

  base_type::push_back(log_entry(cachedTime.seconds(), group % 6, std::string(data, length)));

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
  auto buffer = log_buffer_ptr(new log_buffer, std::bind(&log_buffer_deleter, std::placeholders::_1));

  log_open_output(name, std::bind(&log_buffer::lock_and_push_log, buffer.get(),
                                  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
  return buffer;
}

}
