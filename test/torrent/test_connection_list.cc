#include "config.h"

#include "test_connection_list.h"

#include <net/address_list.h>
#include <torrent/exceptions.h>
#include <torrent/peer/connection_list.h>

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_connection_list, "torrent");

// Only the mutators that never dereference the download are covered here;
// insert and the erase paths that free a connection need a real DownloadMain.

void
test_connection_list::test_basic() {
  torrent::ConnectionList connection_list(nullptr);

  CPPUNIT_ASSERT(connection_list.empty());
  CPPUNIT_ASSERT(connection_list.size() == 0);
  CPPUNIT_ASSERT(connection_list.change_counter() == 0);
}

void
test_connection_list::test_set_size() {
  torrent::ConnectionList connection_list(nullptr);

  connection_list.set_min_size(10);

  CPPUNIT_ASSERT(connection_list.min_size() == 10);
  CPPUNIT_ASSERT(connection_list.change_counter() == 0);

  CPPUNIT_ASSERT_THROW(connection_list.set_min_size((1 << 16) + 1), torrent::input_error);
  CPPUNIT_ASSERT(connection_list.change_counter() == 0);
}

void
test_connection_list::test_set_difference() {
  torrent::ConnectionList connection_list(nullptr);
  torrent::AddressList    address_list;

  connection_list.set_difference(&address_list);

  CPPUNIT_ASSERT(connection_list.change_counter() == 1);

  connection_list.set_difference(&address_list);

  CPPUNIT_ASSERT(connection_list.change_counter() == 2);
}

void
test_connection_list::test_erase_missing() {
  torrent::ConnectionList connection_list(nullptr);

  CPPUNIT_ASSERT_THROW(connection_list.erase(static_cast<torrent::Peer*>(nullptr), 0),
                       torrent::internal_error);
  CPPUNIT_ASSERT(connection_list.change_counter() == 0);
}
