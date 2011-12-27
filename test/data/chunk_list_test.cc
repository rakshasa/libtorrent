#include "config.h"

#include "chunk_list_test.h"

#include "torrent/chunk_manager.h"
#include "torrent/exceptions.h"

CPPUNIT_TEST_SUITE_REGISTRATION(ChunkListTest);

namespace tr1 { using namespace std::tr1; }

#define SETUP_CHUNK_LIST()                                              \
  torrent::ChunkManager* chunk_manager = new torrent::ChunkManager;     \
  torrent::ChunkList* chunk_list = new torrent::ChunkList;              \
  chunk_list->set_manager(chunk_manager);                                \
  chunk_list->slot_create_chunk() = std::tr1::bind(&func_create_chunk, chunk_list, tr1::placeholders::_1, tr1::placeholders::_2); \
  chunk_list->slot_free_diskspace() = std::tr1::bind(&func_free_diskspace, chunk_list); \
  chunk_list->slot_storage_error() = std::tr1::bind(&func_storage_error, chunk_list, tr1::placeholders::_1); \
  chunk_list->set_chunk_size(1 << 16);                                  \
  chunk_list->resize(32);

#define CLEANUP_CHUNK_LIST()                    \
  delete chunk_list;                            \
  delete chunk_manager;

torrent::Chunk*
func_create_chunk(torrent::ChunkList* chunk_list, uint32_t index, int prot_flags) {
  if (index >= chunk_list->size())
    return NULL; // Throw instead.

  char* memory_part1 = (char*)mmap(NULL, 10, PROT_READ, MAP_ANON | MAP_PRIVATE, -1, 0);

  if (memory_part1 == MAP_FAILED)
    throw torrent::internal_error("func_create_chunk() failed: " + std::string(strerror(errno)));

  torrent::Chunk* chunk = new torrent::Chunk();
  chunk->push_back(torrent::ChunkPart::MAPPED_MMAP, torrent::MemoryChunk(memory_part1, memory_part1, memory_part1 + 10, torrent::MemoryChunk::prot_read, 0));

  return chunk;
}

uint64_t
func_free_diskspace(torrent::ChunkList* chunk_list) {
  return 0;
}

void
func_storage_error(torrent::ChunkList* chunk_list, const std::string& message) {
}

void
ChunkListTest::test_basic() {
  torrent::ChunkManager chunk_manager;
  torrent::ChunkList chunk_list;

  CPPUNIT_ASSERT(chunk_list.flags() == 0);
  CPPUNIT_ASSERT(chunk_list.chunk_size() == 0);

  chunk_list.set_chunk_size(1 << 16);
  chunk_list.set_manager(&chunk_manager);
  chunk_list.resize(32);

  CPPUNIT_ASSERT(chunk_list.size() == 32);
  CPPUNIT_ASSERT(chunk_list.chunk_size() == (1 << 16));

  for (unsigned int i = 0; i < 32; i++)
    CPPUNIT_ASSERT(chunk_list[i].index() == i);
}

void
ChunkListTest::test_get_release() {
  SETUP_CHUNK_LIST();

  CPPUNIT_ASSERT(!(*chunk_list)[0].is_valid());

  torrent::ChunkHandle handle_0 = chunk_list->get(0);

  CPPUNIT_ASSERT(handle_0.object() != NULL);
  CPPUNIT_ASSERT(handle_0.object()->index() == 0);
  CPPUNIT_ASSERT(handle_0.index() == 0);
  CPPUNIT_ASSERT(!handle_0.is_writable());
  CPPUNIT_ASSERT(!handle_0.is_blocking());

  CPPUNIT_ASSERT((*chunk_list)[0].is_valid());
  CPPUNIT_ASSERT((*chunk_list)[0].references() == 1);
  CPPUNIT_ASSERT((*chunk_list)[0].writable() == 0);
  CPPUNIT_ASSERT((*chunk_list)[0].blocking() == 0);

  chunk_list->release(&handle_0);

  torrent::ChunkHandle handle_1 = chunk_list->get(1, torrent::ChunkList::get_writable);

  CPPUNIT_ASSERT(handle_1.object() != NULL);
  CPPUNIT_ASSERT(handle_1.object()->index() == 1);
  CPPUNIT_ASSERT(handle_1.index() == 1);
  CPPUNIT_ASSERT(handle_1.is_writable());
  CPPUNIT_ASSERT(!handle_1.is_blocking());

  CPPUNIT_ASSERT((*chunk_list)[1].is_valid());
  CPPUNIT_ASSERT((*chunk_list)[1].references() == 1);
  CPPUNIT_ASSERT((*chunk_list)[1].writable() == 1);
  CPPUNIT_ASSERT((*chunk_list)[1].blocking() == 0);

  chunk_list->release(&handle_1);

  torrent::ChunkHandle handle_2 = chunk_list->get(2, torrent::ChunkList::get_blocking);

  CPPUNIT_ASSERT(handle_2.object() != NULL);
  CPPUNIT_ASSERT(handle_2.object()->index() == 2);
  CPPUNIT_ASSERT(handle_2.index() == 2);
  CPPUNIT_ASSERT(!handle_2.is_writable());
  CPPUNIT_ASSERT(handle_2.is_blocking());

  CPPUNIT_ASSERT((*chunk_list)[2].is_valid());
  CPPUNIT_ASSERT((*chunk_list)[2].references() == 1);
  CPPUNIT_ASSERT((*chunk_list)[2].writable() == 0);
  CPPUNIT_ASSERT((*chunk_list)[2].blocking() == 1);

  chunk_list->release(&handle_2);

  // Test ro->wr, etc.

  CLEANUP_CHUNK_LIST();
}

// Make sure we can't go into writable when blocking, etc.
void
ChunkListTest::test_blocking() {
  SETUP_CHUNK_LIST();

  torrent::ChunkHandle handle_0_ro = chunk_list->get(0, torrent::ChunkList::get_blocking);
  CPPUNIT_ASSERT(handle_0_ro.is_valid());

  // Test writable, etc, on blocking without get_nonblock using a
  // timer on other thread.
  // torrent::ChunkHandle handle_1 = chunk_list->get(0, torrent::ChunkList::get_writable);
  
  torrent::ChunkHandle handle_0_rw = chunk_list->get(0, torrent::ChunkList::get_writable | torrent::ChunkList::get_nonblock);
  CPPUNIT_ASSERT(!handle_0_rw.is_valid());
  CPPUNIT_ASSERT(handle_0_rw.error_number() == rak::error_number::e_again);

  chunk_list->release(&handle_0_ro);

  handle_0_rw = chunk_list->get(0, torrent::ChunkList::get_writable);
  CPPUNIT_ASSERT(handle_0_rw.is_valid());

  chunk_list->release(&handle_0_rw);

  CLEANUP_CHUNK_LIST();
}
