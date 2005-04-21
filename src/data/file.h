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
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef LIBTORRENT_FILE_H
#define LIBTORRENT_FILE_H

#include <string>
#include <inttypes.h>
#include <fcntl.h>
#include <sys/types.h>

#include "memory_chunk.h"

namespace torrent {

class File {
 public:
  static const int o_create               = O_CREAT;
  static const int o_truncate             = O_TRUNC;
  static const int o_nonblock             = O_NONBLOCK;

  File() : m_fd(-1), m_prot(0), m_flags(0) {}
  ~File();

  // TODO: use proper mode type.
  bool                open(const std::string& path, int prot, int flags, mode_t mode = 0666);

  void                close();
  
  bool                is_open() const                                   { return m_fd != -1; }
  bool                is_readable() const                               { return m_prot & MemoryChunk::prot_read; }
  bool                is_writable() const                               { return m_prot & MemoryChunk::prot_write; }
  bool                is_nonblock() const                               { return m_flags & o_nonblock; }

  off_t               get_size() const;
  bool                set_size(off_t s) const;

  int                 get_prot() const                                  { return m_prot; }

  MemoryChunk         get_chunk(off_t offset, uint32_t length, int prot, int flags) const;
  
  int                 fd() const                                        { return m_fd; }

 private:
  // Use custom flags if stuff like file locking etc is implemented.

  File(const File&);
  void operator = (const File&);

  int                 m_fd;
  int                 m_prot;
  int                 m_flags;
};

}

#endif

