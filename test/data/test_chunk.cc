#include "config.h"

#include "test_chunk.h"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>
#include <sys/mman.h>

#include "data/chunk.h"
#include "data/chunk_part.h"
#include "data/memory_chunk.h"
#include "torrent/exceptions.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_chunk, "data");

namespace {

constexpr uint32_t chunk_length = 4096;

char*
map_anonymous() {
  auto* memory = static_cast<char*>(mmap(nullptr, chunk_length, PROT_READ | PROT_WRITE,
                                         MAP_ANON | MAP_PRIVATE, -1, 0));

  if (memory == MAP_FAILED)
    throw torrent::internal_error("mmap() failed: " + std::string(std::strerror(errno)));

  std::memset(memory, 'x', chunk_length);

  return memory;
}

// A shared mapping over a zero-length file: every page in the mapping has no
// backing store, so touching it raises SIGBUS. The si_code reported for this
// differs between platforms, so from_buffer() keys its recovery off the guard
// it armed rather than off a specific code.
char*
map_without_backing_store() {
  char  path[] = "/tmp/libtorrent_test_chunk_XXXXXX";
  int   fd     = mkstemp(path);

  if (fd < 0)
    throw torrent::internal_error("mkstemp() failed: " + std::string(std::strerror(errno)));

  unlink(path);

  auto* memory = static_cast<char*>(mmap(nullptr, chunk_length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));

  close(fd);

  if (memory == MAP_FAILED)
    throw torrent::internal_error("mmap() failed: " + std::string(std::strerror(errno)));

  return memory;
}

bool
is_blocked(int signum) {
  sigset_t current;

  sigemptyset(&current);
  sigprocmask(SIG_BLOCK, nullptr, &current);

  return sigismember(&current, signum) == 1;
}

} // namespace

// 'position + length' is computed in uint32_t, so a range that wraps compares
// small and passes the check. The iterator built from it then hands out the
// rest of the chunk, which is more than 'length' bytes of caller buffer.
void
test_chunk::test_buffer_rejects_wrapping_range() {
  auto* memory = map_anonymous();

  torrent::Chunk chunk;
  chunk.push_back(torrent::ChunkPart::MAPPED_MMAP,
                  torrent::MemoryChunk(memory, memory, memory + chunk_length,
                                       torrent::MemoryChunk::prot_read | torrent::MemoryChunk::prot_write));

  constexpr uint32_t position = 2048;
  constexpr uint32_t length   = ~uint32_t() - 1791;

  // The range wraps to 256, which is inside the chunk.
  CPPUNIT_ASSERT_EQUAL(uint32_t{256}, uint32_t(position + length));

  char buffer[chunk_length];
  std::memset(buffer, 0, sizeof(buffer));

  CPPUNIT_ASSERT_THROW(chunk.to_buffer(buffer, position, length), torrent::internal_error);
  CPPUNIT_ASSERT_THROW(chunk.from_buffer(buffer, position, length), torrent::internal_error);
  CPPUNIT_ASSERT_THROW(chunk.compare_buffer(buffer, position, length), torrent::internal_error);

  chunk.clear();
}

// The SIGBUS handler is installed with a full sa_mask, so the whole signal set
// is blocked while it runs. Leaving the handler with a non-signal jump does not
// restore the mask, and the thread stays unable to receive signals afterwards.
void
test_chunk::test_from_buffer_bus_error_keeps_signal_mask() {
  auto* memory = map_without_backing_store();

  torrent::Chunk chunk;
  chunk.push_back(torrent::ChunkPart::MAPPED_MMAP,
                  torrent::MemoryChunk(memory, memory, memory + chunk_length,
                                       torrent::MemoryChunk::prot_read | torrent::MemoryChunk::prot_write));

  CPPUNIT_ASSERT(!is_blocked(SIGBUS));
  CPPUNIT_ASSERT(!is_blocked(SIGTERM));

  char buffer[chunk_length];
  std::memset(buffer, 'x', sizeof(buffer));

  CPPUNIT_ASSERT_THROW(chunk.from_buffer(buffer, 0, chunk_length), torrent::storage_error);

  CPPUNIT_ASSERT(!is_blocked(SIGBUS));
  CPPUNIT_ASSERT(!is_blocked(SIGTERM));

  chunk.clear();
}
