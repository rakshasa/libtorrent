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

#include "memory_chunk.h"

namespace torrent {

// Defaults to read only and nonblock.

class File {
 public:
  // Flags, not functions. See iostream
  static const unsigned int in             = 0x01;
  static const unsigned int out            = 0x02;
  static const unsigned int create         = 0x04;
  static const unsigned int truncate       = 0x08;
  static const unsigned int nonblock       = 0x10;
  static const unsigned int largefile      = 0x20;

  File() : m_fd(-1), m_flags(0) {}
  ~File() { close(); }

  // Create only regular files for now.
  bool                open(const std::string& path,
			   unsigned int flags = in,
			   unsigned int mode = 0666);

  void                close();
  
  bool                is_open() const                 { return m_fd != -1; }

  off_t               get_size() const;
  bool                set_size(uint64_t v);

  int                 get_flags() const               { return m_flags; }

  MemoryChunk         get_chunk(uint64_t offset, uint32_t length, bool wr = false, bool rd = true);
  
  int                 fd() const                      { return m_fd; }

 private:
  File(const File&);
  void operator = (const File&);

  int                 m_fd;
  int                 m_flags;
};

}

#endif

