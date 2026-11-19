#ifndef LIBTORRENT_TEST_TORRENT_NET_TEST_SOCKET_ADDRESS_KEY_H
#define LIBTORRENT_TEST_TORRENT_NET_TEST_SOCKET_ADDRESS_KEY_H

#include "helpers/test_fixture.h"

class test_socket_address_key : public test_fixture {
  CPPUNIT_TEST_SUITE(test_socket_address_key);

  CPPUNIT_TEST(test_natural_alignment);
  CPPUNIT_TEST(test_ipv4_construction_and_compare);
  CPPUNIT_TEST(test_ipv6_construction_and_compare);
  CPPUNIT_TEST(test_null_sockaddr);
  CPPUNIT_TEST(test_padding_zero_initialized);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_natural_alignment();
  void test_ipv4_construction_and_compare();
  void test_ipv6_construction_and_compare();
  void test_null_sockaddr();
  void test_padding_zero_initialized();
};

#endif
