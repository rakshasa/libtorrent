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
#include "torrent/exceptions.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

namespace torrent {

bool
File::open(const std::string& path,
		unsigned int flags,
		unsigned int mode) {
  close();

  int fd = ::open(path.c_str(), 
		  
		  (flags & in && flags & out ? O_RDWR :
		   (flags & in  ? O_RDONLY : 0) |
		   (flags & out ? O_WRONLY : 0)) |
		  
#ifdef O_LARGEFILE
		  (flags & largefile ? O_LARGEFILE : 0) |
#endif

		  (flags & create    ? O_CREAT     : 0) |
		  (flags & truncate  ? O_TRUNC     : 0) |
		  (flags & nonblock  ? O_NONBLOCK  : 0),
		  
		  mode);
  
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

bool
File::set_size(uint64_t v) {
  if (!is_open())
    return false;

  return ftruncate(m_fd, v) == 0;
}

bool
File::get_chunk(FileChunk& f,
		uint64_t offset,
		uint32_t length,
		bool wr,
		bool rd) {
  if (!is_open()) {
    f.clear();

    return false;
  }

  uint64_t align = offset % getpagesize();

  char* ptr = (char*)mmap(NULL, length + align,
			  (m_flags & in ? PROT_READ : 0) | (m_flags & out ? PROT_WRITE : 0),
			  MAP_SHARED, m_fd, offset - align);

  
  if (ptr == MAP_FAILED)
    f.clear();
  else
    f.set(ptr, ptr + align, ptr + align + length, m_flags & in, m_flags & out);

  return f.is_valid();
}

}

