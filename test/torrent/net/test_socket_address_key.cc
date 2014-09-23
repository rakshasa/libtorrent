#include "config.h"

#include lt_tr1_functional
#include <sys/types.h>
#include <sys/socket.h>

#include "test_socket_address_key.h"

#include "torrent/utils/net.h"
#include "torrent/net/socket_address_key.h"

CPPUNIT_TEST_SUITE_REGISTRATION(test_socket_address_key);

// TODO: Move into a test utilities header:

typedef std::function<struct addrinfo* ()> addrinfo_ftor;

static torrent::socket_address_key
test_create_valid(const char* hostname, addrinfo_ftor ftor) {
  struct addrinfo* addr_info;

  try {
    addr_info = ftor();
  } catch (torrent::address_info_error& e) {
    CPPUNIT_ASSERT_MESSAGE("Caught address_info_error for '" + std::string(hostname) + "'", false);
  }

  CPPUNIT_ASSERT_MESSAGE("test_create_valid could not find '" + std::string(hostname) + "'",
                         addr_info != NULL);

  torrent::socket_address_key sock_key = torrent::socket_address_key::from_sockaddr(addr_info->ai_addr);

  CPPUNIT_ASSERT_MESSAGE("test_create_valid failed to create valid socket_address_key for '" + std::string(hostname) + "'",
                         sock_key.is_valid());

  return sock_key;
}

static bool
test_create_throws(const char* hostname, addrinfo_ftor ftor) {
  try {
    ftor();

    return false;
  } catch (torrent::address_info_error& e) {
    return true;
  }
}

static torrent::socket_address_key
test_create_inet(const char* hostname) {
  return test_create_valid(hostname, std::bind(&torrent::address_info_lookup, hostname, AF_INET, 0));
}

static bool
test_create_inet_throws(const char* hostname) {
  return test_create_throws(hostname, std::bind(&torrent::address_info_lookup, hostname, AF_INET, 0));
}

static torrent::socket_address_key
test_create_inet6(const char* hostname) {
  return test_create_valid(hostname, std::bind(&torrent::address_info_lookup, hostname, AF_INET6, 0));
}

static bool
test_create_inet6_throws(const char* hostname) {
  return test_create_throws(hostname, std::bind(&torrent::address_info_lookup, hostname, AF_INET6, 0));
}

//
// Basic tests:
//

void
test_socket_address_key::test_basic() {
  CPPUNIT_ASSERT(test_create_inet("1.1.1.1").is_valid());
  CPPUNIT_ASSERT(test_create_inet_throws("1.1.1.300"));

  CPPUNIT_ASSERT(test_create_inet6("ff01::1").is_valid());
  CPPUNIT_ASSERT(test_create_inet6("2001:0db8:85a3:0000:0000:8a2e:0370:7334").is_valid());
  CPPUNIT_ASSERT(test_create_inet6("2001:db8:a::123").is_valid());

  CPPUNIT_ASSERT(test_create_inet6_throws("2001:db8:a::22123"));
}


// Test lexical comparison:
