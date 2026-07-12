#include "config.h"

#include "torrent/runtime/client_config.h"

#include "torrent/exceptions.h"

namespace torrent::runtime {

void
ClientConfig::set_listen_port_range(uint16_t first, uint16_t last) {
  if (first == 0 || first > last)
    throw input_error("Invalid listen port range.");

  auto guard = lock_guard();

  m_listen_port_first = first;
  m_listen_port_last  = last;
}

} // namespace torrent::runtime
