#include "config.h"

#include "test/dht/test_dht_bucket.h"

#include <limits>
#include <memory>
#include <vector>

#include "dht/dht_bucket.h"
#include "dht/dht_node.h"
#include "torrent/object.h"

CPPUNIT_TEST_SUITE_REGISTRATION(TestDhtBucket);

namespace {

std::unique_ptr<torrent::DhtNode>
make_cached_node(unsigned int index, unsigned int last_seen) {
  torrent::Object cache = torrent::Object::create_map();

  cache.insert_key("i", int64_t{0x01020300} + index);
  cache.insert_key("p", int64_t{6881});
  cache.insert_key("t", int64_t{last_seen});

  std::string id(torrent::HashString::size_data, '\x11');
  id[0] = static_cast<char>(0x20 + index);

  return std::make_unique<torrent::DhtNode>(id, cache);
}

std::unique_ptr<torrent::DhtBucket>
make_bucket() {
  torrent::HashString begin;
  torrent::HashString end;

  begin.clear();
  end.clear(0xFF);

  return std::make_unique<torrent::DhtBucket>(begin, end);
}

} // namespace

void
TestDhtBucket::test_empty_bucket_has_no_candidate() {
  auto bucket = make_bucket();

  CPPUNIT_ASSERT(bucket->find_replacement_candidate() == bucket->end());
}

void
TestDhtBucket::test_oldest_node_is_the_candidate() {
  m_main_thread->test_set_cached_time(std::chrono::microseconds(0));

  auto bucket = make_bucket();
  std::vector<std::unique_ptr<torrent::DhtNode>> nodes;

  for (unsigned int i = 0; i != torrent::DhtBucket::num_nodes; i++) {
    nodes.push_back(make_cached_node(i, 500 + (i == 3 ? 0 : 100 + i)));
    bucket->add_node(nodes.back().get());
  }

  auto candidate = bucket->find_replacement_candidate();

  CPPUNIT_ASSERT(candidate != bucket->end());
  CPPUNIT_ASSERT_EQUAL(500u, (*candidate)->last_seen());
}

// A cache file may name nodes whose last-seen time is the same sentinel value
// find_replacement_candidate starts its search from. A full bucket must still
// offer a node to replace, as the caller treats end() as unreachable.
void
TestDhtBucket::test_full_bucket_of_max_last_seen_nodes_has_a_candidate() {
  m_main_thread->test_set_cached_time(std::chrono::microseconds(0));

  auto bucket = make_bucket();
  std::vector<std::unique_ptr<torrent::DhtNode>> nodes;

  for (unsigned int i = 0; i != torrent::DhtBucket::num_nodes; i++) {
    nodes.push_back(make_cached_node(i, std::numeric_limits<unsigned int>::max()));
    bucket->add_node(nodes.back().get());
  }

  CPPUNIT_ASSERT(bucket->is_full());
  CPPUNIT_ASSERT(bucket->find_replacement_candidate() != bucket->end());
}
