#include "config.h"

#include "test_peer_info.h"

#include <cstdio>
#include <cstring>
#include <new>

#include "test/helpers/network.h"
#include "torrent/peer/peer_info.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_peer_info, "torrent");

// Six log sites print id_hex() with %40s, and the handshake writes only the 40 hex characters, so
// the buffer has to carry its terminator from the moment the object exists.
void
test_peer_info::test_id_hex_is_terminated_on_construction() {
  alignas(torrent::PeerInfo) unsigned char storage[sizeof(torrent::PeerInfo)];

  std::memset(storage, 'A', sizeof(storage));

  auto* peer_info = new (storage) torrent::PeerInfo(wrap_ai_get_first_sa("1.2.3.4", "5000").get());

  CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), std::strlen(peer_info->id_hex()));

  char formatted[128];

  CPPUNIT_ASSERT_EQUAL(40, std::snprintf(formatted, sizeof(formatted), "%40s", peer_info->id_hex()));

  peer_info->~PeerInfo();
}
