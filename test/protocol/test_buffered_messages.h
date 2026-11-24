#include "test/helpers/test_main_thread.h"

class test_buffered_messages : public TestFixtureWithMainThread {
  CPPUNIT_TEST_SUITE(test_buffered_messages);

  CPPUNIT_TEST(test_empty_socket_empty_buffer);
  CPPUNIT_TEST(test_empty_socket_buffered_message);
  CPPUNIT_TEST(test_empty_socket_partial_message);

  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() override;
  void tearDown() override;

  void test_empty_socket_empty_buffer();
  void test_empty_socket_buffered_message();
  void test_empty_socket_partial_message();

private:
  int m_fds[2]{-1, -1};
};
