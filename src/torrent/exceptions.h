// libTorrent - BitTorrent library
// Copyright (C) 2005, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

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

class program_error : public base_error {
public:
  program_error(const std::string& msg) : base_error(msg) {}
};

// The library or application did some borking it shouldn't have, bug tracking time!
class internal_error : public program_error {
public:
  internal_error(const std::string& msg) : program_error(msg) {}
};

class client_error : public program_error {
public:
  client_error(const std::string& msg) : program_error(msg) {}
};

// For some reason we couldn't talk with a protocol/tracker, migth be a
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

class storage_error : public local_error {
public:
  storage_error(const std::string& msg) : local_error(msg) {}
};

class input_error : public local_error {
public:
  input_error(const std::string& msg) : local_error(msg) {}
};

class bencode_error : public input_error {
public:
  bencode_error(const std::string& msg) : input_error(msg) {}
};

} // namespace torrent

#endif // LIBTORRENT_EXCEPTIONS_H
