#include "config.h"

#include "test/protocol/test_request_list.h"

#include "download/delegator.h"
#include "protocol/peer_chunks.h"
#include "protocol/request_list.h"
#include "test/helpers/network.h"
#include "test/helpers/test_main_thread.h"
#include "torrent/exceptions.h"
#include "torrent/peer/peer_info.h"

CPPUNIT_TEST_SUITE_REGISTRATION(TestRequestList);

static uint32_t
chunk_index_size([[maybe_unused]] uint32_t index) {
  return 1 << 10;
}

static void
transfer_list_void() {
  // std::cout << "list_void" << std::endl;
}

static void
transfer_list_completed(torrent::TransferList* transfer_list, uint32_t index) {
  torrent::TransferList::iterator itr = transfer_list->find(index);

  // std::cout << "list_completed:" << index << " found: " << (itr != transfer_list->end()) << std::endl;

  CPPUNIT_ASSERT(itr != transfer_list->end());

  transfer_list->erase(itr);
}

struct RequestListGuard {
  RequestListGuard(torrent::RequestList* rl) : request_list(rl) {}
  ~RequestListGuard() {
    if (request_list == nullptr || completed)
      return;

    request_list->clear();
    request_list->delegator()->transfer_list()->clear();
  }

  bool                  completed{false};
  torrent::RequestList* request_list;
};

#define SETUP_DELEGATOR(fpc_prefix)                                     \
  auto delegator = std::make_unique<torrent::Delegator>();              \
  delegator->slot_chunk_find() = std::bind(&fpc_prefix ## _find_peer_chunk, std::placeholders::_1, std::placeholders::_2); \
  delegator->slot_chunk_size() = std::bind(&chunk_index_size, std::placeholders::_1); \
  delegator->transfer_list()->slot_canceled()  = std::bind(&transfer_list_void); \
  delegator->transfer_list()->slot_queued()    = std::bind(&transfer_list_void); \
  delegator->transfer_list()->slot_completed() = std::bind(&transfer_list_completed, delegator->transfer_list(), std::placeholders::_1); \
  delegator->transfer_list()->slot_corrupt()   = std::bind(&transfer_list_void);

// Set bitfield size...
#define SETUP_PEER_CHUNKS()                                             \
  auto peer_info = std::make_unique<torrent::PeerInfo>(wrap_ai_get_first_sa("1.2.3.4", "5000").get()); \
  auto peer_chunks = std::make_unique<torrent::PeerChunks>();           \
  peer_chunks->set_peer_info(peer_info.get());

#define SETUP_REQUEST_LIST()                                    \
  auto request_list = std::make_unique<torrent::RequestList>(); \
  RequestListGuard request_list_guard(request_list.get());      \
  request_list->set_delegator(delegator.get());                 \
  request_list->set_peer_chunks(peer_chunks.get());

#define SETUP_ALL(fpc_prefix)                       \
  auto test_main_thread = TestMainThread::create(); \
  test_main_thread->init_thread();                  \
  test_main_thread->test_set_cached_time(0s);       \
  SETUP_DELEGATOR(basic);                           \
  SETUP_PEER_CHUNKS();                              \
  SETUP_REQUEST_LIST();

#define SETUP_ALL_WITH_3(fpc_prefix)                \
  SETUP_ALL(fpc_prefix);                            \
  auto pieces = request_list->delegate(3);          \
  CPPUNIT_ASSERT(pieces.size() == 3);               \
  const torrent::Piece* piece_1 = pieces[0];        \
  const torrent::Piece* piece_2 = pieces[1];        \
  const torrent::Piece* piece_3 = pieces[2];        \
  CPPUNIT_ASSERT(piece_1 && piece_2 && piece_3);

#define CLEAR_TRANSFERS(fpc_prefix)             \
  delegator->transfer_list()->clear();          \
  request_list_guard.completed = true;

//
//
//

#define VERIFY_QUEUE_SIZES(s_0, s_1, s_2, s_3)                          \
  CPPUNIT_ASSERT(request_list->queued_size() == s_0);                   \
  CPPUNIT_ASSERT(request_list->unordered_size() == s_1);                \
  CPPUNIT_ASSERT(request_list->stalled_size() == s_2);                  \
  CPPUNIT_ASSERT(request_list->choked_size() == s_3);

#define VERIFY_PIECE_IS_LEADER(piece)                                   \
  CPPUNIT_ASSERT(request_list->transfer() != NULL);                     \
  CPPUNIT_ASSERT(request_list->transfer()->is_leader());                \
  CPPUNIT_ASSERT(request_list->transfer()->peer_info() != NULL);        \
  CPPUNIT_ASSERT(request_list->transfer()->peer_info() == peer_info.get());

#define VERIFY_TRANSFER_COUNTER(transfer, count)                        \
  CPPUNIT_ASSERT(transfer != NULL);                                     \
  CPPUNIT_ASSERT(transfer->peer_info() != NULL);                        \
  CPPUNIT_ASSERT(transfer->peer_info()->transfer_counter() == count);


//
// Basic tests:
//

static uint32_t
basic_find_peer_chunk([[maybe_unused]] torrent::PeerChunks* peerChunk, [[maybe_unused]] bool highPriority) {
  static int next_index = 0;

  return next_index++;
}

void
TestRequestList::test_basic() {
  SETUP_ALL(basic);

  CPPUNIT_ASSERT(!request_list->is_downloading());
  CPPUNIT_ASSERT(!request_list->is_interested_in_active());

  VERIFY_QUEUE_SIZES(0, 0, 0, 0);

  CPPUNIT_ASSERT(request_list->calculate_pipe_size(1024 * 0) == 2);
  CPPUNIT_ASSERT(request_list->calculate_pipe_size(1024 * 10) == 12);

  CPPUNIT_ASSERT(request_list->transfer() == NULL);
}

void
TestRequestList::test_single_request() {
  SETUP_ALL(basic);

  auto pieces = request_list->delegate(1);
  CPPUNIT_ASSERT(pieces.size() == 1);
  const torrent::Piece* piece = pieces[0];
  // std::cout << piece->index() << ' ' << piece->offset() << ' ' << piece->length() << std::endl;
  // std::cout << peer_info->transfer_counter() << std::endl;

  CPPUNIT_ASSERT(request_list->downloading(*piece));

  VERIFY_PIECE_IS_LEADER(*piece);
  VERIFY_TRANSFER_COUNTER(request_list->transfer(), 1);

  request_list->transfer()->adjust_position(piece->length());

  CPPUNIT_ASSERT(request_list->transfer()->is_finished());

  request_list->finished();

  CPPUNIT_ASSERT(peer_info->transfer_counter() == 0);
}

void
TestRequestList::test_single_canceled() {
  SETUP_ALL(basic);

  auto pieces = request_list->delegate(1);
  CPPUNIT_ASSERT(pieces.size() == 1);
  const torrent::Piece* piece = pieces[0];
  // std::cout << piece->index() << ' ' << piece->offset() << ' ' << piece->length() << std::endl;
  // std::cout << peer_info->transfer_counter() << std::endl;

  CPPUNIT_ASSERT(request_list->downloading(*piece));

  VERIFY_PIECE_IS_LEADER(*piece); // REMOVE
  VERIFY_TRANSFER_COUNTER(request_list->transfer(), 1); // REMOVE

  request_list->transfer()->adjust_position(piece->length() / 2);

  CPPUNIT_ASSERT(!request_list->transfer()->is_finished());

  request_list->skipped();

  // The transfer remains in Block until it gets the block has a
  // transfer trigger finished.

  // TODO: We need to have a way of clearing disowned transfers, then
  // make transfer list delete Block's(?) with those before dtor...

  CPPUNIT_ASSERT(peer_info->transfer_counter() == 0);

  CLEAR_TRANSFERS();
}

void
TestRequestList::test_choke_normal() {
  SETUP_ALL_WITH_3(basic);
  VERIFY_QUEUE_SIZES(3, 0, 0, 0);

  request_list->choked();

  test_main_thread->test_set_cached_time(1s);
  test_main_thread->test_process_events_without_cached_time();
  VERIFY_QUEUE_SIZES(0, 0, 0, 3);

  test_main_thread->test_set_cached_time(1s + 6s);
  test_main_thread->test_process_events_without_cached_time();
  VERIFY_QUEUE_SIZES(0, 0, 0, 0);

  CLEAR_TRANSFERS();
}

void
TestRequestList::test_choke_unchoke_discard() {
  SETUP_ALL_WITH_3(basic);

  request_list->choked();

  test_main_thread->test_set_cached_time(5s);
  test_main_thread->test_process_events_without_cached_time();
  request_list->unchoked();

  test_main_thread->test_set_cached_time(5s + 5s);
  test_main_thread->test_process_events_without_cached_time();
  VERIFY_QUEUE_SIZES(0, 0, 0, 3);

  test_main_thread->test_set_cached_time(5s + 60s);
  test_main_thread->test_process_events_without_cached_time();
  VERIFY_QUEUE_SIZES(0, 0, 0, 0);

  CLEAR_TRANSFERS();
}

void
TestRequestList::test_choke_unchoke_transfer() {
  SETUP_ALL_WITH_3(basic);

  request_list->choked();
  test_main_thread->test_set_cached_time(5s);
  test_main_thread->test_process_events_without_cached_time();
  request_list->unchoked();

  test_main_thread->test_set_cached_time(5s + 5s);
  test_main_thread->test_process_events_without_cached_time();
  CPPUNIT_ASSERT(request_list->downloading(*piece_1));
  request_list->transfer()->adjust_position(piece_1->length());
  request_list->finished();

  test_main_thread->test_set_cached_time(10s + 50s);
  test_main_thread->test_process_events_without_cached_time();
  CPPUNIT_ASSERT(request_list->downloading(*piece_2));
  request_list->transfer()->adjust_position(piece_2->length());
  request_list->finished();

  test_main_thread->test_set_cached_time(60s + 50s);
  test_main_thread->test_process_events_without_cached_time();
  CPPUNIT_ASSERT(request_list->downloading(*piece_3));
  request_list->transfer()->adjust_position(piece_3->length());
  request_list->finished();

  VERIFY_QUEUE_SIZES(0, 0, 0, 0);

  CLEAR_TRANSFERS();
}
