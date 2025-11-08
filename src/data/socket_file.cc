#include "config.h"

#include "socket_file.h"
#include "torrent/exceptions.h"
#include "torrent/utils/log.h"

#include <fcntl.h>
#include <rak/file_stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#define LT_LOG_ERROR(log_fmt, ...)                                      \
  lt_log_print(LOG_STORAGE, "socket_file->%i: " log_fmt, m_fd, __VA_ARGS__);

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
SocketFile::set_size(uint64_t size) const {
  if (!is_open())
    throw internal_error("SocketFile::set_size() called on a closed file");

  if (ftruncate(m_fd, size) == -1) {
    return false;
  }

  return true;
}

bool
SocketFile::allocate(uint64_t size, int flags) const {
  if (!is_open())
    throw internal_error("SocketFile::allocate() called on a closed file");

#if defined(USE_FALLOCATE)
  if (flags & flag_fallocate) {
    if (fallocate(m_fd, 0, 0, size) == -1) {
      LT_LOG_ERROR("fallocate failed : %s", strerror(errno));
      return false;
    }
  }

#elif defined(USE_POSIX_FALLOCATE)
  if (flags & flag_fallocate && flags & flag_fallocate_blocking) {
    if (posix_fallocate(m_fd, 0, size) == -1) {
      LT_LOG_ERROR("posix_fallocate failed : %s", strerror(errno));
      return false;
    }
  }

#elif defined(SYS_DARWIN)
  if (flags & flag_fallocate) {
    fstore_t fstore;
    fstore.fst_flags = F_ALLOCATECONTIG;
    fstore.fst_posmode = F_PEOFPOSMODE;
    fstore.fst_offset = 0;
    fstore.fst_length = size;
    fstore.fst_bytesalloc = 0;

    // This shouldn't really be something we fail the set_size
    // on.
    //
    // Yet is somehow fails with ENOSPC.
//     if (fcntl(m_fd, F_PREALLOCATE, &fstore) == -1)
//       throw internal_error("hack: fcntl failed" + std::string(strerror(errno)));

    if (fcntl(m_fd, F_PREALLOCATE, &fstore) == -1)
      LT_LOG_ERROR("fcntl(,F_PREALLOCATE,) failed : %s", strerror(errno));

    return true;
  }
#endif

  return true;
}

MemoryChunk
SocketFile::create_padding_chunk(uint32_t length, int prot, int flags) {
  flags |= MemoryChunk::map_anon;

  auto ptr = static_cast<char*>(mmap(NULL, length, prot, flags, -1, 0));

  if (ptr == MAP_FAILED)
    return MemoryChunk();

  return MemoryChunk(ptr, ptr, ptr + length, prot, flags);
}

MemoryChunk
SocketFile::create_chunk(uint64_t offset, uint32_t length, int prot, int flags) const {
  if (!is_open())
    throw internal_error("SocketFile::get_chunk() called on a closed file");

  // For some reason mapping beyond the extent of the file does not
  // cause mmap to complain, so we need to check manually here.
  if (length == 0 || offset > size() || offset + length > size())
    return MemoryChunk();

  uint32_t page_size = MemoryChunk::page_size();
  uint64_t align = offset % page_size;

  if (page_size < 4096 || page_size >= (1 << 18))
    throw internal_error("SocketFile::get_chunk() page size is out-of-bounds: " + std::to_string(page_size));

  if (align >= page_size)
    throw internal_error("SocketFile::get_chunk() alignment calculation error: " + std::to_string(align));

  auto ptr = static_cast<char*>(::mmap(nullptr, length + align, prot, flags, m_fd, offset - align));

  if (ptr == MAP_FAILED)
    return MemoryChunk();

  return MemoryChunk(ptr, ptr + align, ptr + align + length, prot, flags);
}

} // namespace torrent
