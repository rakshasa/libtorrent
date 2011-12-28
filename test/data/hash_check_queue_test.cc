#include "config.h"

#include <tr1/functional>

#include "torrent/chunk_manager.h"

#include "chunk_list_test.h"
#include "hash_check_queue_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION(HashCheckQueueTest);

namespace tr1 { using namespace std::tr1; }

void
HashCheckQueueTest::test_basic() {
  // torrent::HashCheckQueue hash_queue;

  
}

void
HashCheckQueueTest::test_single() {
  SETUP_CHUNK_LIST();
  torrent::HashCheckQueue hash_queue;
  
  torrent::ChunkHandle handle_0 = chunk_list->get(0, torrent::ChunkList::get_blocking);

  hash_queue.push_back(handle_0, NULL);
  
  CPPUNIT_ASSERT(hash_queue.size() == 1);
  CPPUNIT_ASSERT(hash_queue.front().handle.is_blocking());
  CPPUNIT_ASSERT(hash_queue.front().handle.object() == &((*chunk_list)[0]));

  chunk_list->release(&handle_0);
  
  CLEANUP_CHUNK_LIST();
}
