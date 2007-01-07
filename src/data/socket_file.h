// libTorrent - BitTorrent library
// Copyright (C) 2005-2006, Jari Sundell
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

#ifndef LIBTORRENT_SOCKET_FILE_H
#define LIBTORRENT_SOCKET_FILE_H

#include <string>
#include <inttypes.h>
#include <fcntl.h>
#include <sys/types.h>

#include "memory_chunk.h"

namespace torrent {

// Inherit from SocketBase?

class SocketFile {
public:
  typedef int fd_type;

  static const fd_type invalid_fd         = -1;

  static const int o_create               = O_CREAT;
  static const int o_truncate             = O_TRUNC;
  static const int o_nonblock             = O_NONBLOCK;

  SocketFile() : m_fd(invalid_fd) {}
  ~SocketFile();

  bool                open(const std::string& path, int prot, int flags, mode_t mode = 0666);

  void                close();
  
  // Reserve the space on disk if a system call is defined. 'length'
  // of zero indicates to the end of the file.
  bool                reserve(uint64_t offset = 0, uint64_t length = 0);

  bool                is_open() const                                   { return m_fd != invalid_fd; }

  uint64_t            size() const;
  bool                set_size(uint64_t s) const;

  MemoryChunk         create_chunk(uint64_t offset, uint32_t length, int prot, int flags) const;
  
  fd_type             fd() const                                        { return m_fd; }

private:
  // Use custom flags if stuff like file locking etc is implemented.

  SocketFile(const SocketFile&);
  void operator = (const SocketFile&);

  fd_type             m_fd;
};

}

#endif

