#include "config.h"

#include <cassert>
#include <iostream>
#include <ostream>

#include "socket_event.h"
#include "torrent/exceptions.h"

namespace torrent {

socket_event::~socket_event() {
#ifndef NDEBUG
  if (is_open()) {
    std::cerr << "Called socket_event::~socket_event while still open on type " << type_name() << std::endl;
    std::abort();
  }
#endif
}

void
socket_event::event_read() {
  throw internal_error("Called unsupported socket_event::event_read on type " + std::string(type_name()));
}

void
socket_event::event_write() {
  throw internal_error("Called unsupported socket_event::event_write on type " + std::string(type_name()));
}

void
socket_event::event_error() {
  throw internal_error("Called unsupported socket_event::event_error on type " + std::string(type_name()));
}

} // namespace torrent
