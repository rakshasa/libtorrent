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

#include "config.h"

#include "file.h"
#include "torrent/exceptions.h"

#include <fcntl.h>
#include <unistd.h>
#include <rak/file_stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>

#ifdef USE_XFS
#include <xfs/libxfs.h>
#endif

namespace torrent {

File::~File() {
  // Temporary test case to make sure we close files properly.
  if (is_open())
    throw internal_error("Destroyed a File that is open");
}

bool
File::open(const std::string& path, int prot, int flags, mode_t mode) {
  close();

  if (prot & MemoryChunk::prot_read &&
      prot & MemoryChunk::prot_write)
    flags |= O_RDWR;
  else if (prot & MemoryChunk::prot_read)
    flags |= O_RDONLY;
  else if (prot & MemoryChunk::prot_write)
    flags |= O_WRONLY;
  else
    throw internal_error("torrent::File::open(...) Tried to open file with no protection flags");

#ifdef O_LARGEFILE
  fd_type fd = ::open(path.c_str(), flags | O_LARGEFILE, mode);
#else
  fd_type fd = ::open(path.c_str(), flags, mode);
#endif
  
  if (fd == invalid_fd)
    return false;

  m_fd = fd;
  m_flags = flags;
  m_prot = prot;

  return true;
}

void
File::close() {
  if (!is_open())
    return;

  ::close(m_fd);

  m_fd = invalid_fd;
  m_prot = 0;
  m_flags = 0;
}

// Reserve the space on disk if a system call is defined. 'length'
// of zero indicates to the end of the file.
#if !defined(USE_XFS) && !defined(USE_POSIX_FALLOCATE)
#define RESERVE_PARAM __UNUSED
#else
#define RESERVE_PARAM
#endif

bool
File::reserve(RESERVE_PARAM off_t offset, RESERVE_PARAM off_t length) {
#ifdef USE_XFS
  struct xfs_flock64 flock;

  flock.l_whence = SEEK_SET;
  flock.l_start = offset;
  flock.l_len = length;

  if (ioctl(m_fd, XFS_IOC_RESVSP64, &flock) >= 0)
    return true;
#endif

#ifdef USE_POSIX_FALLOCATE
  return !posix_fallocate(m_fd, offset, length);
#else
  return true;
#endif
}

#undef RESERVE_PARAM

off_t
File::size() const {
  if (!is_open())
    throw internal_error("File::size() called on a closed file");

  rak::file_stat fs;

  return fs.update(m_fd) ? fs.size() : 0;
}  

bool
File::set_size(off_t size) const {
  if (!is_open())
    throw internal_error("File::set_size() called on a closed file");

  if (ftruncate(m_fd, size) == 0)
    return true;

  // Use workaround to resize files on vfat. It causes the whole
  // client to block while it is resizing the files, this really
  // should be in a seperate thread.
  if (size != 0 &&
      lseek(m_fd, size - 1, SEEK_SET) == (size - 1) &&
      write(m_fd, &size, 1) == 1)
    return true;
  
  return false;
}

MemoryChunk
File::create_chunk(off_t offset, uint32_t length, int prot, int flags) const {
  if (!is_open())
    throw internal_error("File::get_chunk() called on a closed file");

  if (((prot & MemoryChunk::prot_read) && !is_readable()) ||
      ((prot & MemoryChunk::prot_write) && !is_writable()))
    throw storage_error("File::get_chunk() permission denied");

  // For some reason mapping beyond the extent of the file does not
  // cause mmap to complain, so we need to check manually here.
  if (offset < 0 || length == 0 || offset + length > size())
    return MemoryChunk();

  off_t align = offset % MemoryChunk::page_size();

  char* ptr = (char*)mmap(NULL, length + align, prot, flags, m_fd, offset - align);
  
  if (ptr == MAP_FAILED)
    return MemoryChunk();

  return MemoryChunk(ptr, ptr + align, ptr + align + length, prot, flags);
}

}

