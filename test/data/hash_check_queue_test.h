#include <map>
#include <vector>
#include <cppunit/extensions/HelperMacros.h>

#include "data/hash_check_queue.h"
#include "torrent/hash_string.h"


class HashCheckQueueTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(HashCheckQueueTest);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_single);
  CPPUNIT_TEST(test_multiple);
  CPPUNIT_TEST(test_erase);

  CPPUNIT_TEST(test_thread);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();
  void tearDown();

  void test_basic();
  void test_single();
  void test_multiple();
  void test_erase();

  void test_thread();
};

typedef std::map<int, torrent::HashString> done_chunks_type;
typedef std::vector<torrent::ChunkHandle> handle_list;

torrent::HashString hash_for_index(uint32_t index);

bool verify_hash(const done_chunks_type* done_chunks, int index, const torrent::HashString& hash);
