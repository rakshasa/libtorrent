#include "config.h"

#include "chunk_list_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION(ChunkListTest);

void
ChunkListTest::test_basic() {
  torrent::ChunkList chunk_list;

  CPPUNIT_ASSERT(chunk_list.flags() == 0);
  CPPUNIT_ASSERT(chunk_list.chunk_size() == 0);

  // Verify chunk size needs to be set.
  
}
