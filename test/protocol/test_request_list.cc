#include "config.h"

#include "test_request_list.h"

#include "torrent/exceptions.h"
#include "torrent/peer/peer_info.h"
#include "download/delegator.h"
#include "protocol/peer_chunks.h"
#include "protocol/request_list.h"
#include "rak/socket_address.h"

CPPUNIT_TEST_SUITE_REGISTRATION(TestRequestList);

static uint32_t
find_peer_chunk(torrent::PeerChunks* peerChunk, bool highPriority) {
  return 0;
}

static uint32_t
chunk_index_size(uint32_t index) {
  return 1 << 18;
}

static void
transfer_list_void() {
}

// Move to support header:
#define SETUP_DELEGATOR()                                               \
  torrent::Delegator delegator;                                         \
  delegator.slot_chunk_find() = std::tr1::bind(&find_peer_chunk, std::tr1::placeholders::_1, std::tr1::placeholders::_2); \
  delegator.slot_chunk_size() = std::tr1::bind(&chunk_index_size, std::tr1::placeholders::_1); \
  delegator.transfer_list()->slot_canceled()  = std::tr1::bind(&transfer_list_void); \
  delegator.transfer_list()->slot_queued()    = std::tr1::bind(&transfer_list_void); \
  delegator.transfer_list()->slot_completed() = std::tr1::bind(&transfer_list_void); \
  delegator.transfer_list()->slot_corrupt()   = std::tr1::bind(&transfer_list_void);

// Set bitfield size...
#define SETUP_PEER_CHUNKS()                                     \
  rak::socket_address peer_info_address;                        \
  torrent::PeerInfo peer_info(peer_info_address.c_sockaddr());  \
  torrent::PeerChunks peer_chunks;                              \
  peer_chunks.set_peer_info(&peer_info);

#define SETUP_REQUEST_LIST()                    \
  torrent::RequestList request_list;            \
  request_list.set_delegator(&delegator);       \
  request_list.set_peer_chunks(&peer_chunks);

void
TestRequestList::test_basic() {
  SETUP_DELEGATOR();
  SETUP_PEER_CHUNKS();
  SETUP_REQUEST_LIST();

  CPPUNIT_ASSERT(!request_list.is_downloading());
  CPPUNIT_ASSERT(!request_list.is_interested_in_active());

  CPPUNIT_ASSERT(!request_list.has_index(0));
  CPPUNIT_ASSERT(!request_list.has_index(1));
  CPPUNIT_ASSERT(!request_list.has_index(1024));

  CPPUNIT_ASSERT(request_list.queued_empty());
  CPPUNIT_ASSERT(request_list.queued_size() == 0);
  CPPUNIT_ASSERT(request_list.canceled_empty());
  CPPUNIT_ASSERT(request_list.canceled_size() == 0);

  CPPUNIT_ASSERT(request_list.calculate_pipe_size(1024 * 0) == 2);
  CPPUNIT_ASSERT(request_list.calculate_pipe_size(1024 * 10) == 12);

  CPPUNIT_ASSERT(request_list.transfer() == NULL);
}

