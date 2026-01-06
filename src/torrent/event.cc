#include "config.h"

#include "event.h"

#include <cassert>

#include "torrent/exceptions.h"
#include "torrent/net/fd.h"

namespace torrent {

Event::Event() = default;

Event::~Event() {
  assert(m_poll_event == nullptr && "Event::~Event() called with m_poll_event != nullptr.");
}

const char*
Event::type_name() const {
  throw internal_error("Event::type_name() must be overridden in derived class.");
}

std::string
Event::print_name_fd_str() const {
 return "name:" + std::string(type_name()) + " fd:" + std::to_string(file_descriptor());
}

void
Event::close_file_descriptor() {
  if (!is_open())
    throw internal_error("Tried to close already closed file descriptor on event type " + std::string(type_name()));

  fd_close(m_fileDesc);
  m_fileDesc = -1;
}

} // namespace torrent
