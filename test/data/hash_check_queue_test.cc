#include "config.h"

#include <tr1/functional>

#include "utils/sha1.h"
#include "torrent/chunk_manager.h"
#include "torrent/hash_string.h"

#include "chunk_list_test.h"
#include "hash_check_queue_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION(HashCheckQueueTest);

namespace tr1 { using namespace std::tr1; }

typedef std::map<int, torrent::HashString> done_chunks_type;

static void
chunk_done(done_chunks_type* done_chunks, const torrent::ChunkHandle& handle, torrent::HashQueueNode* node, const torrent::HashString& hash_value) {
  // Verify hash_value...

  (*done_chunks)[handle.index()] = hash_value;
}

torrent::HashString
hash_for_index(uint32_t index) {
  char buffer[10];
  std::memset(buffer, index, 10);

  torrent::Sha1 sha1;
  torrent::HashString hash;
  sha1.init();
  sha1.update(buffer, 10);
  sha1.final_c(hash.data());

  return hash;
}

void
HashCheckQueueTest::test_basic() {
  // torrent::HashCheckQueue hash_queue;

  // Test done_chunks here?..
}

void
HashCheckQueueTest::test_single() {
  SETUP_CHUNK_LIST();
  torrent::HashCheckQueue hash_queue;

  done_chunks_type done_chunks;
  hash_queue.slot_chunk_done() = tr1::bind(&chunk_done, &done_chunks, tr1::placeholders::_1, tr1::placeholders::_2, tr1::placeholders::_3);
  
  torrent::ChunkHandle handle_0 = chunk_list->get(0, torrent::ChunkList::get_blocking);

  hash_queue.push_back(handle_0, NULL);
  
  CPPUNIT_ASSERT(hash_queue.size() == 1);
  CPPUNIT_ASSERT(hash_queue.front().handle.is_blocking());
  CPPUNIT_ASSERT(hash_queue.front().handle.object() == &((*chunk_list)[0]));

  hash_queue.perform();

  CPPUNIT_ASSERT(done_chunks.find(0) != done_chunks.end());
  CPPUNIT_ASSERT(done_chunks[0] == hash_for_index(0));

  // Should not be needed...
  chunk_list->release(&handle_0);
  
  CLEANUP_CHUNK_LIST();
}
