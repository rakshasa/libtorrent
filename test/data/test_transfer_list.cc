#include "config.h"

#include "test_transfer_list.h"

#include <cerrno>
#include <cstring>
#include <sys/mman.h>

#include "data/chunk.h"
#include "data/chunk_part.h"
#include "data/memory_chunk.h"
#include "test/helpers/network.h"
#include "torrent/data/block.h"
#include "torrent/data/block_list.h"
#include "torrent/data/block_transfer.h"
#include "torrent/data/transfer_list.h"
#include "torrent/exceptions.h"
#include "torrent/peer/peer_info.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_transfer_list, "data");

namespace {

constexpr uint32_t piece_length = 16;

void
build_chunk(torrent::Chunk* chunk) {
  auto* memory = static_cast<char*>(mmap(nullptr, piece_length, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0));

  if (memory == MAP_FAILED)
    throw torrent::internal_error("build_chunk() failed: " + std::string(std::strerror(errno)));

  std::memset(memory, 'x', piece_length);

  chunk->push_back(torrent::ChunkPart::MAPPED_MMAP,
                   torrent::MemoryChunk(memory, memory, memory + piece_length,
                                        torrent::MemoryChunk::prot_read | torrent::MemoryChunk::prot_write));
}

} // namespace

// On the first hash failure every block records its data for the first time, so nothing is promoted
// and there is no more-popular data set to swap in. Retrying would re-hash the identical bytes, so
// the blocks have to be re-downloaded instead.
void
test_transfer_list::test_hash_failed_without_promotion_redownloads() {
  torrent::Chunk chunk;
  build_chunk(&chunk);

  torrent::TransferList transfer_list;

  unsigned int completed_calls = 0;

  transfer_list.slot_queued()    = [](uint32_t) {};
  transfer_list.slot_canceled()  = [](uint32_t) {};
  transfer_list.slot_corrupt()   = [](torrent::PeerInfo*) {};
  transfer_list.slot_completed() = [&completed_calls](uint32_t) { completed_calls++; };

  struct clear_guard {
    ~clear_guard() { list->clear(); }
    torrent::TransferList* list;
  } guard{&transfer_list};

  auto* block_list = *transfer_list.insert(torrent::Piece(0, 0, piece_length), piece_length);

  torrent::PeerInfo peer(wrap_ai_get_first_sa("1.2.3.4", "5000").get());

  for (auto& block : *block_list) {
    auto* transfer = block.insert(&peer);

    CPPUNIT_ASSERT(block.transfering(transfer));
    transfer->adjust_position(transfer->piece().length());
    CPPUNIT_ASSERT(block.completed(transfer));
  }

  CPPUNIT_ASSERT_EQUAL(uint32_t(0), block_list->attempt());

  transfer_list.hash_failed(0, &chunk);

  CPPUNIT_ASSERT_EQUAL(0u, completed_calls);
  CPPUNIT_ASSERT_EQUAL(uint32_t(0), block_list->attempt());
}
