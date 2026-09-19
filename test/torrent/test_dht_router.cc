#include "config.h"

#include "test/torrent/test_dht_router.h"

#include "dht/dht_router.h"
#include "dht/dht_tracker.h"
#include "torrent/hash_string.h"
#include "torrent/object.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_dht_router, "torrent");

namespace {

constexpr unsigned int announced_info_hashes = 4096;
constexpr unsigned int unknown_info_hash     = 5000;

torrent::HashString
info_hash_for(unsigned int index) {
  torrent::HashString hash;

  hash.clear(0);
  hash.data()[0] = static_cast<char>(index & 0xff);
  hash.data()[1] = static_cast<char>((index >> 8) & 0xff);

  return hash;
}

size_t
tracker_count(const torrent::DhtRouter& router) {
  return router.get_statistics().num_trackers;
}

} // namespace

void
test_dht_router::test_tracker_count_is_capped() {
  torrent::DhtRouter router(nullptr, torrent::Object::create_map());

  unsigned int created = 0;

  for (unsigned int index = 0; index < announced_info_hashes; index++) {
    auto hash = info_hash_for(index);

    if (router.get_tracker(hash, true) != nullptr)
      created++;
  }

  CPPUNIT_ASSERT(created < announced_info_hashes);
  CPPUNIT_ASSERT_EQUAL(torrent::DhtRouter::max_trackers, static_cast<size_t>(created));
}

void
test_dht_router::test_full_table_evicts_stale_trackers() {
  constexpr auto max_trackers = torrent::DhtRouter::max_trackers;
  constexpr auto after_evict  = max_trackers - torrent::DhtRouter::num_tracker_evict;
  constexpr auto evict_period = std::chrono::seconds(torrent::DhtRouter::timeout_tracker_evict);

  torrent::DhtRouter router(nullptr, torrent::Object::create_map());

  // Fill the table: the first half announced 20 minutes before the second.
  for (unsigned int index = 0; index < max_trackers; index++) {
    if (index == max_trackers / 2)
      m_main_thread->test_add_cached_time(std::chrono::minutes(20));

    router.get_tracker(info_hash_for(index), true)->add_peer(0x0a000001 + index, 6881);
  }

  CPPUNIT_ASSERT(router.get_tracker(info_hash_for(unknown_info_hash), true) == nullptr);

  m_main_thread->test_add_cached_time(evict_period);
  router.evict_stale_trackers();

  CPPUNIT_ASSERT_EQUAL(after_evict, tracker_count(router));

  for (unsigned int index = max_trackers / 2; index < max_trackers; index++)
    CPPUNIT_ASSERT(router.get_tracker(info_hash_for(index), false) != nullptr);

  CPPUNIT_ASSERT(router.get_tracker(info_hash_for(unknown_info_hash), true) != nullptr);

  // Refill, then check eviction waits for the period to pass again.
  for (unsigned int index = announced_info_hashes; tracker_count(router) < max_trackers; index++)
    router.get_tracker(info_hash_for(index), true);

  router.evict_stale_trackers();
  CPPUNIT_ASSERT_EQUAL(max_trackers, tracker_count(router));

  m_main_thread->test_add_cached_time(evict_period);
  router.evict_stale_trackers();
  CPPUNIT_ASSERT_EQUAL(after_evict, tracker_count(router));
}
