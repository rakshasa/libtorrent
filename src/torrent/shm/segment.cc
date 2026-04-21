#include "config.h"

#include "torrent/shm/segment.h"

#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <sys/mman.h>

#include "torrent/exceptions.h"

namespace torrent::shm {

void
Segment::create(uint32_t size) {
  if (m_addr != nullptr)
    throw torrent::internal_error("Segment::create() segment already created");

  if (m_size != 0)
    throw torrent::internal_error("Segment::create() segment size already set");

  if (size == 0 || (size % page_size) != 0)
    throw std::invalid_argument("Segment::create() size must be non-zero and a multiple of page size");

  void* addr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

  if (addr == MAP_FAILED) {
    if (errno == ENOMEM)
      throw torrent::internal_error("mmap() failed with ENOMEM: not enough shared memory available (try deleting unused shared memory segments with 'ipcrm -m <id>')");

    throw torrent::internal_error("mmap() failed: " + std::string(std::strerror(errno)));
  }

  std::memset(addr, 0, size);

  m_size = size;
  m_addr = addr;
}

void
Segment::destroy() {
  if (m_addr == nullptr)
    return;

  if (munmap(m_addr, m_size) == -1)
    throw torrent::internal_error("munmap() failed: " + std::string(std::strerror(errno)));

  m_size = 0;
  m_addr = nullptr;
}

} // namespace torrent::shm
