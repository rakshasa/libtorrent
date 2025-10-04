// Note that some of the exception classes below are strictly speaking
// _NOT DOING THE RIGHT THING_. One is really not supposed to use any
// calls to malloc/new in an exception's ctor.
//
// Will be fixed next API update.

#ifndef LIBTORRENT_EXCEPTIONS_H
#define LIBTORRENT_EXCEPTIONS_H

#include <exception>
#include <string>
#include <torrent/common.h>
#include <torrent/hash_string.h>

namespace torrent {

// All exceptions inherit from std::exception to make catching
// everything libtorrent related at the root easier.
class LIBTORRENT_EXPORT base_error : public std::exception {
};

// The library or application did some borking it shouldn't have, bug
// tracking time!
class LIBTORRENT_EXPORT internal_error : public base_error {
public:
  internal_error(const char* msg)        { initialize(msg); }
  internal_error(const char* msg, const std::string& context) {
    initialize(std::string(msg) + " [" + context + "]"); }
  internal_error(const char* msg, const HashString& hash) {
    initialize(std::string(msg) + " [#" + hash_string_to_hex_str(hash) + "]"); }
  internal_error(const std::string& msg) { initialize(msg); }

  const char*        what() const noexcept override;
  const std::string& backtrace() const noexcept { return m_backtrace; }

  void               push_front(const std::string& msg) { m_msg = msg + m_msg; }

private:
  // Use this function for breaking on throws.
  void initialize(const std::string& msg);

  std::string m_msg;
  std::string m_backtrace;
};

// For some reason we couldn't talk with a protocol/tracker, migth be a
// library bug, connection problem or bad input.
class LIBTORRENT_EXPORT network_error : public base_error {
};

class LIBTORRENT_EXPORT communication_error : public network_error {
public:
  communication_error(const char* msg)        { initialize(msg); }
  communication_error(const std::string& msg) { initialize(msg); }

  const char* what() const noexcept override;

private:
  // Use this function for breaking on throws.
  void initialize(const std::string& msg);

  std::string m_msg;
};

class LIBTORRENT_EXPORT connection_error : public network_error {
public:
  connection_error(int err) : m_errno(err) {}

  const char* what() const noexcept override;

  int get_errno() const { return m_errno; }

private:
  int m_errno;
};

class LIBTORRENT_EXPORT address_info_error : public network_error {
public:
  address_info_error(int err) : m_errno(err) {}

  const char* what() const noexcept override;

  int get_errno() const { return m_errno; }

private:
  int m_errno;
};

class LIBTORRENT_EXPORT close_connection : public network_error {
};

class LIBTORRENT_EXPORT blocked_connection : public network_error {
};

// Stuff like bad torrent file, disk space and permissions.
class LIBTORRENT_EXPORT local_error : public base_error {
};

class LIBTORRENT_EXPORT storage_error : public local_error {
public:
  storage_error(const char* msg)       { initialize(msg); }
  storage_error(const std::string& msg) { initialize(msg); }

  const char* what() const noexcept override;

private:
  // Use this function for breaking on throws.
  void initialize(const std::string& msg);

  std::string m_msg;
};

class LIBTORRENT_EXPORT resource_error : public local_error {
public:
  resource_error(const char* msg) { initialize(msg); }
  resource_error(const std::string& msg) { initialize(msg); }

  const char* what() const noexcept override;

private:
  // Use this function for breaking on throws.
  void initialize(const std::string& msg);

  std::string m_msg;
};

class LIBTORRENT_EXPORT input_error : public local_error {
public:
  input_error(const char* msg) { initialize(msg); }
  input_error(const std::string& msg) { initialize(msg); }

  const char* what() const noexcept override;

private:
  // Use this function for breaking on throws.
  void initialize(const std::string& msg);

  std::string m_msg;
};

class LIBTORRENT_EXPORT bencode_error : public input_error {
public:
  using input_error::input_error;
};

class LIBTORRENT_EXPORT shutdown_exception : public base_error {
};

} // namespace torrent

#endif
