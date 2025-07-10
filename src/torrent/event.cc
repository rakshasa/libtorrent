#include "config.h"

#include "event.h"

#include "torrent/exceptions.h"
#include "torrent/net/fd.h"

namespace torrent {

const char*
Event::type_name() const {
  return "default";
}

void
Event::close_file_descriptor() {
  if (!is_open())
    throw internal_error("Tried to close already closed file descriptor on event type " + std::string(type_name()));

  fd_close(m_fileDesc);
  m_fileDesc = -1;
}

} // namespace torrent
