#ifndef LIBTORRENT_EXCEPTIONS_H
#define LIBTORRENT_EXCEPTIONS_H

#include <exception>
#include <string>

namespace torrent {

// all exceptions inherit from runtime_error to make catching everything
// at the root easier.

class base_error : public std::exception {
public:
  base_error(const std::string& msg) : m_msg(msg) {}
  virtual ~base_error() throw() {}

  virtual const char* what() const throw() { return m_msg.c_str(); }

  void set(const std::string& msg) { m_msg = msg; }

private:
  std::string m_msg;
};

// The library or application did some borking it shouldn't have, bug tracking time!
class internal_error : public base_error {
public:
  internal_error(const std::string& msg) : base_error(msg) {}
};

// For some reason we couldn't talk with a peer/tracker, migth be a
// library bug, connection problem or bad input.
class network_error : public base_error {
public:
  network_error(const std::string& msg) : base_error(msg) {}
};

class communication_error : public network_error {
public:
  communication_error(const std::string& msg) : network_error(msg) {}
};

class connection_error : public network_error {
public:
  connection_error(const std::string& msg) : network_error(msg) {}
};

class close_connection : public network_error {
public:
  close_connection() : network_error("") {}

  close_connection(const std::string& msg) : network_error(msg) {}
};

// Stuff like bad torrent file, disk space and permissions.
class local_error : public base_error {
public:
  local_error(const std::string& msg) : base_error(msg) {}
};

class input_error : public local_error {
public:
  input_error(const std::string& msg) : local_error(msg) {}
};

class storage_error : public local_error {
public:
  storage_error(const std::string& msg) : local_error(msg) {}
};

class bencode_error : public local_error {
public:
  bencode_error(const std::string& msg) : local_error(msg) {}
};

} // namespace torrent

#endif // LIBTORRENT_EXCEPTIONS_H
