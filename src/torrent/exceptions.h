// libTorrent - BitTorrent library
// Copyright (C) 2005-2007, Jari Sundell
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

namespace torrent {

// All exceptions inherit from std::exception to make catching
// everything libtorrent related at the root easier.
class LIBTORRENT_EXPORT base_error : public std::exception {
public:
  virtual ~base_error() throw() {}
};

// The library or application did some borking it shouldn't have, bug
// tracking time!
class LIBTORRENT_EXPORT internal_error : public base_error {
public:
  internal_error(const char* msg);
  internal_error(const std::string& msg);
  virtual ~internal_error() throw() {}

  virtual const char* what() const throw() { return m_msg.c_str(); }

private:
  std::string m_msg;
};

// For some reason we couldn't talk with a protocol/tracker, migth be a
// library bug, connection problem or bad input.
class LIBTORRENT_EXPORT network_error : public base_error {
public:
  virtual ~network_error() throw() {}
};

class LIBTORRENT_EXPORT communication_error : public network_error {
public:
  communication_error(const char* msg);
  communication_error(const std::string& msg);
  virtual ~communication_error() throw() {}

  virtual const char* what() const throw() { return m_msg.c_str(); }

private:
  std::string m_msg;
};

class LIBTORRENT_EXPORT connection_error : public network_error {
public:
  connection_error(int err) : m_errno(err) {}
  virtual ~connection_error() throw() {}

  virtual const char* what() const throw();

private:
  int m_errno;
};

class LIBTORRENT_EXPORT close_connection : public network_error {
public:
  virtual ~close_connection() throw() {}
};

class LIBTORRENT_EXPORT blocked_connection : public network_error {
public:
  virtual ~blocked_connection() throw() {}
};

// Stuff like bad torrent file, disk space and permissions.
class LIBTORRENT_EXPORT local_error : public base_error {
public:
  virtual ~local_error() throw() {}
};

class LIBTORRENT_EXPORT storage_error : public local_error {
public:
  storage_error(const char* msg);
  storage_error(const std::string& msg);
  virtual ~storage_error() throw() {}

  virtual const char* what() const throw() { return m_msg.c_str(); }

private:
  std::string m_msg;
};

class LIBTORRENT_EXPORT resource_error : public local_error {
public:
  resource_error(const char* msg);
  resource_error(const std::string& msg);
  virtual ~resource_error() throw() {}

  virtual const char* what() const throw() { return m_msg.c_str(); }

private:
  std::string m_msg;
};

class LIBTORRENT_EXPORT input_error : public local_error {
public:
  input_error(const char* msg);
  input_error(const std::string& msg);
  virtual ~input_error() throw() {}

  virtual const char* what() const throw() { return m_msg.c_str(); }

private:
  std::string m_msg;
};

class LIBTORRENT_EXPORT bencode_error : public input_error {
public:
  bencode_error(const char* msg) : input_error(msg) {}
  bencode_error(const std::string& msg) : input_error(msg) {}

  virtual ~bencode_error() throw() {}
};

}

#endif
