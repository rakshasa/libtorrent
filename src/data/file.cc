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

#include "config.h"

#include "file.h"
#include "file_stat.h"
#include "torrent/exceptions.h"

#include <unistd.h>
#include <sys/mman.h>

namespace torrent {

bool
File::open(const std::string& path, int flags, mode_t mode) {
  close();

#ifdef O_LARGEFILE
  int fd = ::open(path.c_str(), flags | O_LARGEFILE, mode);
#else
  int fd = ::open(path.c_str(), flags, mode);
#endif
  
  if (fd == -1)
    return false;

  m_fd = fd;
  m_flags = flags;

  return true;
}

void
File::close() {
  if (!is_open())
    return;

  ::close(m_fd);

  m_fd = -1;
  m_flags = 0;
}

off_t
File::get_size() const {
  if (!is_open())
    throw internal_error("File::get_size() called on a closed file");

  return FileStat(m_fd).get_size();
}  

bool
File::set_size(off_t s) const {
  if (!is_open())
    throw internal_error("File::set_size() called on a closed file");

  return ftruncate(m_fd, s) == 0;
}

MemoryChunk
File::get_chunk(off_t offset, uint32_t length, int prot, int flags) const {
  if (!is_open())
    throw internal_error("File::get_chunk() called on a closed file");

  if ((prot & MemoryChunk::prot_read && !is_readable()) ||
      (prot & MemoryChunk::prot_write && !is_writable()))
    throw internal_error("File::get_chunk() permission denied");

  // For some reason mapping beyond the extent of the file does not
  // cause mmap to complain, so we need to check manually here.
  if ((off_t)offset + length > get_size())
    return MemoryChunk();

  off_t align = offset % getpagesize();

  char* ptr = (char*)mmap(NULL, length + align, prot, flags, m_fd, offset - align);
  
  if (ptr == MAP_FAILED)
    return MemoryChunk();

  return MemoryChunk(ptr, ptr + align, ptr + align + length, prot, flags);
}

}

