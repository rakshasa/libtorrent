#include "config.h"

#include "socket_event.h"

#include "torrent/exceptions.h"

namespace torrent {

socket_event::~socket_event() {
  if (is_open())
    throw internal_error("Called socket_event::~socket_event while still open on type " + std::string(type_name()));
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

}
