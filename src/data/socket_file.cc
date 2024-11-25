// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
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

#include "config.h"

#include "socket_file.h"
#include "torrent/exceptions.h"

#include <fcntl.h>
#include <unistd.h>
#include <rak/error_number.h>
#include <rak/file_stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>

#ifdef HAVE_FALLOCATE
#define _GNU_SOURCE
#include <linux/falloc.h>
#endif

namespace torrent {

bool
SocketFile::open(const std::string& path, int prot, int flags, mode_t mode) {
  close();

  if (prot & MemoryChunk::prot_read &&
      prot & MemoryChunk::prot_write)
    flags |= O_RDWR;
  else if (prot & MemoryChunk::prot_read)
    flags |= O_RDONLY;
  else if (prot & MemoryChunk::prot_write)
    flags |= O_WRONLY;
  else
    throw internal_error("torrent::SocketFile::open(...) Tried to open file with no protection flags");

#ifdef O_LARGEFILE
  fd_type fd = ::open(path.c_str(), flags | O_LARGEFILE, mode);
#else
  fd_type fd = ::open(path.c_str(), flags, mode);
#endif
  
  if (fd == invalid_fd)
    return false;

  m_fd = fd;
  return true;
}

void
SocketFile::close() {
  if (!is_open())
    return;

  ::close(m_fd);

  m_fd = invalid_fd;
}

uint64_t
SocketFile::size() const {
  if (!is_open())
    throw internal_error("SocketFile::size() called on a closed file");

  rak::file_stat fs;

  return fs.update(m_fd) ? fs.size() : 0;
}  

bool
SocketFile::set_size(uint64_t size, int flags) const {
  if (!is_open())
    throw internal_error("SocketFile::set_size() called on a closed file");

#ifdef HAVE_FALLOCATE
  if (flags & flag_fallocate && fallocate(m_fd, 0, 0, size) == 0 && ftruncate(m_fd, size) == 0)
    return true;
#endif

#ifdef USE_POSIX_FALLOCATE
  if (flags & flag_fallocate &&
      flags & flag_fallocate_blocking &&
      posix_fallocate(m_fd, 0, size) == 0 && ftruncate(m_fd, size) == 0)
    return true;
#endif

#ifdef SYS_DARWIN
  if (flags & flag_fallocate) {
    fstore_t fstore;
    fstore.fst_flags = F_ALLOCATECONTIG;
    fstore.fst_posmode = F_PEOFPOSMODE;
    fstore.fst_offset = 0;
    fstore.fst_length = size;
    fstore.fst_bytesalloc = 0;

    // Hmm... this shouldn't really be something we fail the set_size
    // on...
    //
    // Yet is somehow fails with ENOSPC...
//     if (fcntl(m_fd, F_PREALLOCATE, &fstore) == -1)
//       throw internal_error("hack: fcntl failed" + std::string(strerror(errno)));

    fcntl(m_fd, F_PREALLOCATE, &fstore); // Ignore result for now...
  }
#endif

  if (ftruncate(m_fd, size) == 0)
    return true;
  
  // Use workaround to resize files on vfat. It causes the whole
  // client to block while it is resizing the files, this really
  // should be in a seperate thread.
  if (size != 0 &&
      lseek(m_fd, size - 1, SEEK_SET) == (off_t)(size - 1) &&
      write(m_fd, &size, 1) == 1)
    return true;

  return false;
}

MemoryChunk
SocketFile::create_chunk(uint64_t offset, uint32_t length, int prot, int flags) const {
  if (!is_open())
    throw internal_error("SocketFile::get_chunk() called on a closed file");

  // For some reason mapping beyond the extent of the file does not
  // cause mmap to complain, so we need to check manually here.
  if (length == 0 || offset > size() || offset + length > size())
    return MemoryChunk();

  uint64_t align = offset % MemoryChunk::page_size();

  char* ptr = (char*)mmap(NULL, length + align, prot, flags, m_fd, offset - align);
  
  if (ptr == MAP_FAILED)
    return MemoryChunk();

  return MemoryChunk(ptr, ptr + align, ptr + align + length, prot, flags);
}

}

