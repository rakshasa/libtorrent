#include "helpers/test_fixture.h"

#include "torrent/utils/thread_base.h"

class test_tracker_http : public test_fixture {
  CPPUNIT_TEST_SUITE(test_tracker_http);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST_SUITE_END();

public:
  void test_basic();
};
