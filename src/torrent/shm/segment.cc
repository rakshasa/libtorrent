#include "config.h"

#include "torrent/shm/segment.h"

#include <cerrno>
#include <unistd.h>
#include <sys/shm.h>

#include "torrent/exceptions.h"

namespace torrent::shm {

void
Segment::create(uint32_t size) {
  if (m_shm_id != -1)
    throw torrent::internal_error("Segment::create() segment already created");

  if (m_size != 0)
    throw torrent::internal_error("Segment::create() segment size already set");

  if (size == 0 || (size % page_size) != 0)
    throw std::invalid_argument("Segment::create() size must be non-zero and a multiple of page size");

  auto fd = shmget(IPC_PRIVATE, size, IPC_CREAT | 0600);

  if (fd == -1) {
    if (errno == ENOMEM)
      throw torrent::internal_error("shmget() failed with ENOMEM: not enough shared memory available (try deleting unused shared memory segments with 'ipcrm -m <id>')");

    throw torrent::internal_error("shmget() failed: " + std::string(strerror(errno)));
  }

  m_shm_id = fd;
  m_size   = size;

  try {
    attach();
    unlink();
  } catch (...) {
    // If we don't unlink the shared memory it will stick around after all processes are done.
    unlink();
    throw;
  }
}

void
Segment::unlink() {
  if (m_shm_id == -1)
    return;

  if (shmctl(m_shm_id, IPC_RMID, nullptr) == -1)
    throw torrent::internal_error("shmctl(IPC_RMID) failed: " + std::string(strerror(errno)));
}

void
Segment::attach() {
  if (m_addr != nullptr)
    throw torrent::internal_error("Segment::attach() segment already attached");

  void* addr = shmat(m_shm_id, nullptr, 0);

  if (addr == (void*)-1)
    throw torrent::internal_error("shmat() failed: " + std::string(strerror(errno)));

  m_addr = addr;
}

void
Segment::detach() {
  if (m_addr == nullptr)
    return;

  if (shmdt(m_addr) == -1)
    throw torrent::internal_error("shmdt() failed: " + std::string(strerror(errno)));
}

} // namespace torrent::shm
