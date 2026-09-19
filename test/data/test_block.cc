#include "config.h"

#include "test_block.h"

#include "test/helpers/network.h"
#include "torrent/data/block_list.h"
#include "torrent/data/block_transfer.h"
#include "torrent/peer/peer_info.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_block, "data");

void
test_block::test_completed_skips_erased_not_stalled_accounting() {
  torrent::BlockList block_list(torrent::Piece(0, 0, 16), 16);
  torrent::Block& block = block_list[0];

  torrent::PeerInfo leader_peer(wrap_ai_get_first_sa("1.2.3.4", "5000").get());
  torrent::PeerInfo trailing_peer(wrap_ai_get_first_sa("4.3.2.1", "5000").get());

  torrent::BlockTransfer* leader = block.insert(&leader_peer);
  torrent::BlockTransfer* trailing = block.insert(&trailing_peer);

  CPPUNIT_ASSERT(leader != nullptr);
  CPPUNIT_ASSERT(trailing != nullptr);
  CPPUNIT_ASSERT_EQUAL(static_cast<torrent::Block::size_type>(2), block.size_not_stalled());

  CPPUNIT_ASSERT(block.transfering(leader));
  CPPUNIT_ASSERT(!block.transfering(trailing));
  CPPUNIT_ASSERT(trailing->is_not_leader());

  block.transfer_dissimilar(trailing);

  CPPUNIT_ASSERT(trailing->is_erased());
  CPPUNIT_ASSERT(!trailing->is_valid());
  CPPUNIT_ASSERT_EQUAL(static_cast<torrent::Block::size_type>(1), block.size_not_stalled());

  leader->adjust_position(leader->piece().length());

  CPPUNIT_ASSERT(block.completed(leader));
  CPPUNIT_ASSERT(block.is_finished());
}

void
test_block::test_invalidate_transfer_without_connection() {
  torrent::PeerInfo peer(wrap_ai_get_first_sa("1.2.3.4", "5000").get());

  CPPUNIT_ASSERT(peer.connection() == nullptr);
  CPPUNIT_ASSERT_EQUAL(static_cast<uint32_t>(0), peer.transfer_counter());

  {
    torrent::BlockList block_list(torrent::Piece(0, 0, 16), 16);
    torrent::Block&    block = block_list[0];

    CPPUNIT_ASSERT(block.insert(&peer) != nullptr);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint32_t>(1), peer.transfer_counter());
  }

  CPPUNIT_ASSERT_EQUAL(static_cast<uint32_t>(0), peer.transfer_counter());
}
