#include <cppunit/extensions/HelperMacros.h>

#include "protocol/request_list.h"

class TestRequestList : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(TestRequestList);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}
  void tearDown() {}

  void test_basic();
};

// torrent::Chunk* func_create_chunk(uint32_t index, int prot_flags);

#define SETUP_CHUNK_LIST()                                              \
  torrent::ChunkManager* chunk_manager = new torrent::ChunkManager;     \
  torrent::ChunkList* chunk_list = new torrent::ChunkList;              \
  chunk_list->set_manager(chunk_manager);                               \
  chunk_list->slot_create_chunk() = tr1::bind(&func_create_chunk, tr1::placeholders::_1, tr1::placeholders::_2); \
  chunk_list->resize(32);

#define CLEANUP_CHUNK_LIST()                    \
  delete chunk_list;                            \
  delete chunk_manager;
