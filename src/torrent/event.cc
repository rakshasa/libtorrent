#include "config.h"

#include "event.h"

#include <cassert>

#include "torrent/exceptions.h"
#include "torrent/net/fd.h"
#include "torrent/net/socket_address.h"

namespace torrent {

Event::~Event() {
  assert(m_poll_event == nullptr && "Event::~Event() called with m_poll_event != nullptr.");
}

int
Event::socket_type_or_zero() const {
  if (!is_open())
    throw internal_error("Event::socket_type_or_zero() called on closed event.");

  int socktype{};

  if (!fd_get_type(m_fileDesc, &socktype)) {
    if (errno != EBADF)
      throw internal_error("Event::socket_type_or_zero() error getting socket type");

    return 0;
  }

  return socktype;
}

const char*
Event::type_name() const {
  throw internal_error("Event::type_name() must be overridden in derived class.");
}

std::string
Event::print_name_fd_str() const {
 return "name:" + std::string(type_name()) + " fd:" + std::to_string(file_descriptor());
}

// TODO: Add "check_socket_valid_and_same".

bool
Event::update_socket_address() {
  if (!is_open())
    throw internal_error("Event::update_socket_address() called on closed event.");

  int socktype = socket_type_or_zero();

  if (socktype == 0 || (socktype != SOCK_STREAM && socktype != SOCK_DGRAM))
    return false;

  auto address = fd_get_socket_name(m_fileDesc);

  if (address == nullptr) {
    if (errno != EBADF && errno != ENOTCONN && errno != EINVAL)
      throw internal_error("Event::update_socket_address() error getting socket address");

    return false;
  }

  m_socket_address = std::move(address);
  return true;
}

bool
Event::update_peer_address() {
  if (!is_open())
    throw internal_error("Event::update_peer_address() called on closed event.");

  int socktype = socket_type_or_zero();

  if (socktype == 0 || (socktype != SOCK_STREAM && socktype != SOCK_DGRAM))
    return false;

  auto address = fd_get_peer_name(m_fileDesc);

  if (address == nullptr) {
    if (errno != EBADF && errno != ENOTCONN && errno != EINVAL)
      throw internal_error("Event::update_peer_address() error getting peer address");

    return false;
  }

  m_peer_address = std::move(address);
  return true;
}

bool
Event::update_and_verify_socket_address() {
  auto old_address = std::move(m_socket_address);

  if (!update_socket_address()) {
    m_socket_address = std::move(old_address);

    return m_socket_address == nullptr;
  }

  if (old_address == nullptr)
    return true;

  if (!sap_equal(old_address, m_socket_address)) {
    m_socket_address = std::move(old_address);
    return false;
  }

  return true;
}

bool
Event::update_and_verify_peer_address() {
  auto old_address = std::move(m_peer_address);

  if (!update_peer_address()) {
    m_peer_address = std::move(old_address);

    return m_peer_address == nullptr;
  }

  if (old_address == nullptr)
    return true;

  if (!sap_equal(old_address, m_peer_address)) {
    m_peer_address = std::move(old_address);
    return false;
  }

  return true;
}

} // namespace torrent
