#include "config.h"

#include "test_file_list.h"

#include <limits>

#include "data/socket_file.h"
#include "torrent/data/file_list.h"
#include "torrent/exceptions.h"
#include "manager.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_file_list, "data");

namespace torrent {

// file_list.cc's FileList::close()/create_chunk_part(), compiled into the
// same translation unit as initialize(), reference the process-wide
// 'manager' singleton and SocketFile's chunk-creation helpers. Nothing else
// in this test binary links those in, and initialize()'s guard throws
// before either would ever run, so a link-only stub is enough here.
Manager* manager = nullptr;

MemoryChunk
SocketFile::create_padding_chunk(uint32_t, int, int) {
  throw internal_error("stub SocketFile::create_padding_chunk() called");
}

MemoryChunk
SocketFile::create_chunk(uint64_t, uint32_t, int, int) const {
  throw internal_error("stub SocketFile::create_chunk() called");
}

} // namespace torrent

namespace {

constexpr uint32_t chunk_size = 1u << 20;
constexpr uint64_t max_u64    = std::numeric_limits<uint64_t>::max();

// FileList::initialize() is protected; expose it for direct testing the way
// data/test_file_manager.cc exposes File::set_frozen_path via a derived type.
class accessible_file_list : public torrent::FileList {
public:
  using torrent::FileList::initialize;
};

} // namespace

// 'torrentSize + chunkSize - 1' is computed in uint64_t, so a torrentSize
// within 'chunkSize - 1' of UINT64_MAX wraps that sum. The division that
// follows then yields a small quotient that passes the chunk-count guard it
// should have failed. This is deep in the wrap: unambiguous overflow.
void
test_file_list::test_initialize_rejects_max_u64() {
  accessible_file_list file_list;
  CPPUNIT_ASSERT_THROW(file_list.initialize(max_u64, chunk_size), torrent::input_error);
}

// Exactly where the sum first wraps: 'torrentSize + (chunkSize - 1) == 2^64'.
void
test_file_list::test_initialize_rejects_wrap_boundary() {
  accessible_file_list file_list;
  CPPUNIT_ASSERT_THROW(file_list.initialize(max_u64 - chunk_size + 2, chunk_size), torrent::input_error);
}

// One below the wrap: the sum is exactly UINT64_MAX, so the old expression
// does not overflow here and already rejected this size correctly. Pinning
// it keeps the wrap boundary precise rather than resting on one large value.
void
test_file_list::test_initialize_rejects_just_below_wrap_boundary() {
  accessible_file_list file_list;
  CPPUNIT_ASSERT_THROW(file_list.initialize(max_u64 - chunk_size + 1, chunk_size), torrent::input_error);
}
