#include "helpers/test_fixture.h"

class test_transfer_list : public test_fixture {
  CPPUNIT_TEST_SUITE(test_transfer_list);

  CPPUNIT_TEST(test_hash_failed_without_promotion_redownloads);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_hash_failed_without_promotion_redownloads();
};
