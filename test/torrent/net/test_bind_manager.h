#include "helpers/test_fixture.h"

class test_bind_manager : public test_fixture {
  CPPUNIT_TEST_SUITE(test_bind_manager);

  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_backlog);
  CPPUNIT_TEST(test_flags);

  CPPUNIT_TEST(test_add_bind);
  CPPUNIT_TEST(test_add_bind_error);
  CPPUNIT_TEST(test_add_bind_priority);
  CPPUNIT_TEST(test_add_bind_v4mapped);
  CPPUNIT_TEST(test_remove_bind);

  CPPUNIT_TEST(test_connect_socket);
  CPPUNIT_TEST(test_connect_socket_v4bound);
  CPPUNIT_TEST(test_connect_socket_v6bound);
  CPPUNIT_TEST(test_connect_socket_v4only);
  CPPUNIT_TEST(test_connect_socket_v6only);
  CPPUNIT_TEST(test_connect_socket_block_connect);
  CPPUNIT_TEST(test_error_connect_socket);

  CPPUNIT_TEST(test_listen_socket_randomize);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_basic();
  void test_backlog();
  void test_flags();

  void test_add_bind();
  void test_add_bind_error();
  void test_add_bind_priority();
  void test_add_bind_v4mapped();
  void test_remove_bind();

  void test_connect_socket();
  void test_connect_socket_v4bound();
  void test_connect_socket_v6bound();
  void test_connect_socket_v4only();
  void test_connect_socket_v6only();
  void test_connect_socket_block_connect();
  void test_error_connect_socket();

  void test_listen_socket_randomize();
};
