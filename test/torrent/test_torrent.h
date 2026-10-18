#include "helpers/test_fixture.h"

class test_torrent : public test_fixture {
  CPPUNIT_TEST_SUITE(test_torrent);
  CPPUNIT_TEST(test_cleanup_removes_downloads);
  CPPUNIT_TEST(test_download_add_zero_info_hash);
  CPPUNIT_TEST(test_download_find_unknown_hash);
  CPPUNIT_TEST(test_download_find_wrong_size);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() override;
  void tearDown() override;

  void test_cleanup_removes_downloads();
  void test_download_add_zero_info_hash();
  void test_download_find_unknown_hash();
  void test_download_find_wrong_size();
};
