#import "helpers/test_fixture.h"

class test_chunk_list : public test_fixture {
  CPPUNIT_TEST_SUITE(test_chunk_list);

  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_get_release);
  CPPUNIT_TEST(test_blocking);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_basic();
  void test_get_release();
  void test_blocking();
};

#include "data/chunk_list.h"

torrent::Chunk* func_create_chunk(uint32_t index, int prot_flags);
uint64_t        func_free_diskspace(torrent::ChunkList* chunk_list);
void            func_storage_error(torrent::ChunkList* chunk_list, const std::string& message);

#define SETUP_CHUNK_LIST()                                              \
  torrent::ChunkManager* chunk_manager = new torrent::ChunkManager;     \
  torrent::ChunkList* chunk_list = new torrent::ChunkList;              \
  chunk_list->set_manager(chunk_manager);                               \
  chunk_list->slot_create_chunk() = std::bind(&func_create_chunk, std::placeholders::_1, std::placeholders::_2); \
  chunk_list->slot_free_diskspace() = std::bind(&func_free_diskspace, chunk_list); \
  chunk_list->slot_storage_error() = std::bind(&func_storage_error, chunk_list, std::placeholders::_1); \
  chunk_list->set_chunk_size(1 << 16);                                  \
  chunk_list->resize(32);

#define CLEANUP_CHUNK_LIST()                    \
  delete chunk_list;                            \
  delete chunk_manager;
