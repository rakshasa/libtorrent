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

#ifndef LIBTORRENT_NET_SOCKET_STREAM_H
#define LIBTORRENT_NET_SOCKET_STREAM_H

#include "socket_base.h"

namespace torrent {

class SocketStream : public SocketBase {
public:
  int                 read_stream(void* buf, uint32_t length);
  int                 write_stream(const void* buf, uint32_t length);

  // Returns the number of bytes read, or zero if the socket is
  // blocking. On errors or closed sockets it will throw an
  // appropriate exception.
  uint32_t            read_stream_throws(void* buf, uint32_t length);
  uint32_t            write_stream_throws(const void* buf, uint32_t length);

  // Handles all the error catching etc. Returns true if the buffer is
  // finished reading/writing.
  bool                read_buffer(void* buf, uint32_t length, uint32_t& pos);
  bool                write_buffer(const void* buf, uint32_t length, uint32_t& pos);

  uint32_t            ignore_stream_throws(uint32_t length) { return read_stream_throws(m_nullBuffer, length); }
};

inline bool
SocketStream::read_buffer(void* buf, uint32_t length, uint32_t& pos) {
  pos += read_stream_throws(buf, length - pos);

  return pos == length;
}

inline bool
SocketStream::write_buffer(const void* buf, uint32_t length, uint32_t& pos) {
  pos += write_stream_throws(buf, length - pos);

  return pos == length;
}

}

#endif
