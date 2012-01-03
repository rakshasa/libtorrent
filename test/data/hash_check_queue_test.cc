#include "config.h"

#include <tr1/functional>

#include "utils/sha1.h"
#include "torrent/chunk_manager.h"
#include "torrent/hash_string.h"
#include "torrent/poll_select.h"
#include "thread_disk.h"

#include "chunk_list_test.h"
#include "hash_check_queue_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION(HashCheckQueueTest);

namespace tr1 { using namespace std::tr1; }

typedef std::map<int, torrent::HashString> done_chunks_type;

static void
chunk_done(done_chunks_type* done_chunks, const torrent::ChunkHandle& handle, torrent::HashQueueNode* node, const torrent::HashString& hash_value) {
  // Verify hash_value...

  (*done_chunks)[handle.index()] = hash_value;
  __sync_synchronize();
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

torrent::Poll* create_select_poll() { return torrent::PollSelect::create(256); }

void
HashCheckQueueTest::setUp() {
  torrent::Poll::slot_create_poll() = tr1::bind(&create_select_poll);
}

void
HashCheckQueueTest::tearDown() {
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

void
HashCheckQueueTest::test_thread() {
  SETUP_CHUNK_LIST();

  torrent::thread_disk thread_disk;
  torrent::HashCheckQueue& hash_queue = thread_disk.hash_queue();

  done_chunks_type done_chunks;
  hash_queue.slot_chunk_done() = tr1::bind(&chunk_done, &done_chunks, tr1::placeholders::_1, tr1::placeholders::_2, tr1::placeholders::_3);
  
  // Set poll...
  thread_disk.init_thread();
  thread_disk.start_thread();

  torrent::ChunkHandle handle_0 = chunk_list->get(0, torrent::ChunkList::get_blocking);

  hash_queue.push_back(handle_0, NULL);
  
  CPPUNIT_ASSERT(hash_queue.size() == 1);
  CPPUNIT_ASSERT(hash_queue.front().handle.is_blocking());
  CPPUNIT_ASSERT(hash_queue.front().handle.object() == &((*chunk_list)[0]));


  // Test that the timeout is properly zeroed on insert of new chunk
  // to check.

  __sync_synchronize();
  sleep(15);
  __sync_synchronize();

  CPPUNIT_ASSERT(done_chunks.find(0) != done_chunks.end());
  CPPUNIT_ASSERT(done_chunks[0] == hash_for_index(0));


  // thread_disk.stop_thread();

  // Wait for thread to stop.

  // Should not be needed...
  chunk_list->release(&handle_0);
  
  // Ensure proper cleanup of thread.
  CLEANUP_CHUNK_LIST();
}
