#include "config.h"

#include "test/dht/test_dht_ipv6.h"

#include <arpa/inet.h>
#include <cstring>
#include <memory>

#include "dht/dht_bucket.h"
#include "dht/dht_node.h"
#include "dht/dht_router.h"
#include "dht/dht_tracker.h"
#include "dht/dht_transaction.h"
#include "dht/transactions/dht_search.h"
#include "net/address_list.h"
#include "torrent/net/socket_address.h"
#include "torrent/object.h"
#include "torrent/object_stream.h"

CPPUNIT_TEST_SUITE_REGISTRATION(TestDhtIpv6);

namespace {

torrent::sa_unique_ptr
make_inet6(const char* addr, uint16_t port) {
  auto sin6 = torrent::sin6_make();

  if (inet_pton(AF_INET6, addr, &sin6->sin6_addr) != 1)
    throw std::runtime_error("bad test address");

  sin6->sin6_port = htons(port);
  return torrent::sa_from_in6(std::move(sin6));
}

torrent::HashString
make_id(char fill, char first = 0) {
  torrent::HashString id;
  id.clear(fill);
  id.data()[0] = first ? first : fill;
  return id;
}

const unsigned char seed_addr[16] = { 0x2a, 0x01, 0x0e, 0x34, 0xec, 0x02, 0xe8, 0x60, 0x9c, 0x66, 0x1f, 0x03, 0xbd, 0xcb, 0x64, 0x28 };

} // namespace

void
TestDhtIpv6::test_node_compact_is_38_bytes() {
  torrent::DhtNode node(make_id('\x11'), make_inet6("2a01:e34:ec02:e860:9c66:1f03:bdcb:6428", 35730).get());

  char buffer[64];
  std::memset(buffer, 0, sizeof(buffer));

  char* end = node.store_compact(buffer);

  CPPUNIT_ASSERT_EQUAL(38, static_cast<int>(end - buffer));
  CPPUNIT_ASSERT(std::memcmp(buffer, make_id('\x11').data(), 20) == 0);
  CPPUNIT_ASSERT(std::memcmp(buffer + 20, seed_addr, 16) == 0);
  // 35730 = 0x8B92, network order
  CPPUNIT_ASSERT_EQUAL(0x8B, static_cast<int>(static_cast<unsigned char>(buffer[36])));
  CPPUNIT_ASSERT_EQUAL(0x92, static_cast<int>(static_cast<unsigned char>(buffer[37])));
}

void
TestDhtIpv6::test_node_compact_v4_is_still_26_bytes() {
  torrent::DhtNode node(make_id('\x22'), torrent::sa_make_inet_h(0x01020304, 6881).get());

  char buffer[64];
  char* end = node.store_compact(buffer);

  CPPUNIT_ASSERT_EQUAL(26, static_cast<int>(end - buffer));
  CPPUNIT_ASSERT_EQUAL(0x01, static_cast<int>(static_cast<unsigned char>(buffer[20])));
  CPPUNIT_ASSERT_EQUAL(0x04, static_cast<int>(static_cast<unsigned char>(buffer[23])));
  CPPUNIT_ASSERT_EQUAL(0x1A, static_cast<int>(static_cast<unsigned char>(buffer[24])));
  CPPUNIT_ASSERT_EQUAL(0xE1, static_cast<int>(static_cast<unsigned char>(buffer[25])));
}

void
TestDhtIpv6::test_node_cache_round_trip_v6() {
  auto sa = make_inet6("2a01:e34:ec02:e860:9c66:1f03:bdcb:6428", 35730);
  torrent::DhtNode node(make_id('\x33'), sa.get());

  torrent::Object cache = torrent::Object::create_map();
  node.store_cache(&cache);

  CPPUNIT_ASSERT(cache.has_key_string("i6"));
  CPPUNIT_ASSERT(!cache.has_key("i"));

  torrent::DhtNode loaded(make_id('\x33').str(), cache);

  CPPUNIT_ASSERT(torrent::sa_is_inet6(loaded.address()));
  CPPUNIT_ASSERT(torrent::sa_equal(loaded.address(), sa.get()));
}

void
TestDhtIpv6::test_node_cache_round_trip_v4() {
  auto sa = torrent::sa_make_inet_h(0x0a000001, 6881);
  torrent::DhtNode node(make_id('\x44'), sa.get());

  torrent::Object cache = torrent::Object::create_map();
  node.store_cache(&cache);

  CPPUNIT_ASSERT(cache.has_key_value("i"));
  CPPUNIT_ASSERT(!cache.has_key("i6"));

  torrent::DhtNode loaded(make_id('\x44').str(), cache);

  CPPUNIT_ASSERT(torrent::sa_is_inet(loaded.address()));
  CPPUNIT_ASSERT(torrent::sa_equal(loaded.address(), sa.get()));
}

void
TestDhtIpv6::test_tracker_v6_value_is_18_bytes_network_order() {
  torrent::DhtTracker tracker;

  in6_addr addr;
  std::memcpy(&addr, seed_addr, 16);

  tracker.add_peer6(addr, 1000);

  CPPUNIT_ASSERT_EQUAL(size_t{1}, tracker.size());

  torrent::raw_list peers = tracker.get_peers6();
  CPPUNIT_ASSERT_EQUAL(uint32_t{21}, peers.size());

  auto data = reinterpret_cast<const unsigned char*>(peers.data());

  CPPUNIT_ASSERT_EQUAL(static_cast<unsigned char>('1'), data[0]);
  CPPUNIT_ASSERT_EQUAL(static_cast<unsigned char>('8'), data[1]);
  CPPUNIT_ASSERT_EQUAL(static_cast<unsigned char>(':'), data[2]);
  CPPUNIT_ASSERT(std::memcmp(data + 3, seed_addr, 16) == 0);
  CPPUNIT_ASSERT_EQUAL(static_cast<unsigned char>(0x03), data[19]);
  CPPUNIT_ASSERT_EQUAL(static_cast<unsigned char>(0xE8), data[20]);

  // An IPv4 query never sees IPv6 values.
  CPPUNIT_ASSERT(tracker.get_peers().empty());
}

void
TestDhtIpv6::test_tracker_v6_same_address_updates_port() {
  torrent::DhtTracker tracker;

  in6_addr addr;
  std::memcpy(&addr, seed_addr, 16);

  tracker.add_peer6(addr, 1000);
  tracker.add_peer6(addr, 2000);
  tracker.add_peer6(addr, 0);    // port 0 is ignored

  CPPUNIT_ASSERT_EQUAL(size_t{1}, tracker.size());

  auto data = reinterpret_cast<const unsigned char*>(tracker.get_peers6().data());
  CPPUNIT_ASSERT_EQUAL(static_cast<unsigned char>(0x07), data[19]);
  CPPUNIT_ASSERT_EQUAL(static_cast<unsigned char>(0xD0), data[20]);
}

void
TestDhtIpv6::test_values_mixed_v4_and_v6() {
  std::string values;
  values += "6:";
  values += std::string("\x01\x02\x03\x04\x1a\xe1", 6);
  values += "18:";
  values += std::string(reinterpret_cast<const char*>(seed_addr), 16);
  values += std::string("\x8b\x92", 2);

  torrent::AddressList list;
  list.parse_address_bencode(torrent::raw_list(values.data(), values.size()));

  CPPUNIT_ASSERT_EQUAL(size_t{2}, list.size());
  CPPUNIT_ASSERT_EQUAL(static_cast<int>(AF_INET), static_cast<int>(list[0].inet.sin_family));
  CPPUNIT_ASSERT_EQUAL(uint16_t{6881}, ntohs(list[0].inet.sin_port));
  CPPUNIT_ASSERT_EQUAL(static_cast<int>(AF_INET6), static_cast<int>(list[1].inet6.sin6_family));
  CPPUNIT_ASSERT(std::memcmp(&list[1].inet6.sin6_addr, seed_addr, 16) == 0);
  CPPUNIT_ASSERT_EQUAL(uint16_t{35730}, ntohs(list[1].inet6.sin6_port));
}

void
TestDhtIpv6::test_values_truncated_or_odd_entries_are_skipped() {
  // an entry of another length is skipped, a truncated one ends the list, a bad prefix ends it too
  std::string values;
  values += "4:abcd";
  values += "6:";
  values += std::string("\x0a\x00\x00\x01\x00\x50", 6);
  values += "18:";
  values += std::string(reinterpret_cast<const char*>(seed_addr), 10);

  torrent::AddressList list;
  list.parse_address_bencode(torrent::raw_list(values.data(), values.size()));

  CPPUNIT_ASSERT_EQUAL(size_t{1}, list.size());
  CPPUNIT_ASSERT_EQUAL(static_cast<int>(AF_INET), static_cast<int>(list[0].inet.sin_family));

  std::string junk("99999999999999999999:x");
  torrent::AddressList none;
  none.parse_address_bencode(torrent::raw_list(junk.data(), junk.size()));
  CPPUNIT_ASSERT(none.empty());

  std::string no_colon("18");
  none.parse_address_bencode(torrent::raw_list(no_colon.data(), no_colon.size()));
  CPPUNIT_ASSERT(none.empty());
}

void
TestDhtIpv6::test_transaction_key_v6() {
  auto one = make_inet6("2a01:e34:ec02:e860:9c66:1f03:bdcb:6428", 35730);
  auto two = make_inet6("2a01:e34:ec02:e860:9c66:1f03:bdcb:6429", 35730);

  auto key_one = torrent::DhtTransaction::key(one.get(), 7);
  auto key_two = torrent::DhtTransaction::key(two.get(), 7);

  CPPUNIT_ASSERT(key_one != key_two);
  CPPUNIT_ASSERT_EQUAL(uint64_t{7}, key_one & 0xff);
  CPPUNIT_ASSERT(torrent::DhtTransaction::key_match(key_one, one.get()));
  CPPUNIT_ASSERT(!torrent::DhtTransaction::key_match(key_one, two.get()));
}

void
TestDhtIpv6::test_router_v6_token() {
  torrent::Object cache = torrent::Object::create_map();
  torrent::DhtRouter router(nullptr, cache, AF_INET6);

  CPPUNIT_ASSERT_EQUAL(static_cast<int>(AF_INET6), router.family());

  auto one = make_inet6("2a01:e34:ec02:e860:9c66:1f03:bdcb:6428", 35730);
  auto two = make_inet6("2a01:e34:ec02:e860:9c66:1f03:bdcb:6429", 35730);

  char buffer_one[20];
  char buffer_two[20];
  auto token_one = router.make_token(one.get(), buffer_one);
  auto token_two = router.make_token(two.get(), buffer_two);

  CPPUNIT_ASSERT(router.token_valid(token_one, one.get()));
  CPPUNIT_ASSERT(!router.token_valid(token_one, two.get()));
  CPPUNIT_ASSERT(!(token_one == token_two));
}

void
TestDhtIpv6::test_router_v6_cache_keeps_families_apart() {
  torrent::Object cache = torrent::Object::create_map();

  auto self = make_id('\x55');
  cache.insert_key("self_id", self.str());

  auto& nodes = cache.insert_key("nodes", torrent::Object::create_map());
  auto& v4 = nodes.insert_key(make_id('\x60').str(), torrent::Object::create_map());
  v4.insert_key("i", int64_t{0x0a000001});
  v4.insert_key("p", int64_t{6881});
  v4.insert_key("t", int64_t{0});

  auto& nodes6 = cache.insert_key("nodes6", torrent::Object::create_map());
  auto& v6 = nodes6.insert_key(make_id('\x70').str(), torrent::Object::create_map());
  v6.insert_key("i6", std::string(reinterpret_cast<const char*>(seed_addr), 16));
  v6.insert_key("p", int64_t{35730});
  v6.insert_key("t", int64_t{0});

  torrent::DhtRouter router4(nullptr, cache, AF_INET);
  torrent::DhtRouter router6(nullptr, cache, AF_INET6);

  // the same node ID, each family its own table
  CPPUNIT_ASSERT(router4.id() == self);
  CPPUNIT_ASSERT(router6.id() == self);
  CPPUNIT_ASSERT_EQUAL(1u, router4.get_statistics().num_nodes);
  CPPUNIT_ASSERT_EQUAL(1u, router6.get_statistics().num_nodes);
  CPPUNIT_ASSERT(router4.get_node(make_id('\x60')) != nullptr);
  CPPUNIT_ASSERT(router4.get_node(make_id('\x70')) == nullptr);
  CPPUNIT_ASSERT(router6.get_node(make_id('\x70')) != nullptr);
  CPPUNIT_ASSERT(torrent::sa_is_inet6(router6.get_node(make_id('\x70'))->address()));

  // each writes back only its own family's nodes
  torrent::Object out = torrent::Object::create_map();
  router4.store_cache(&out);
  router6.store_cache(&out);

  CPPUNIT_ASSERT_EQUAL(size_t{1}, out.get_key_map("nodes").size());
  CPPUNIT_ASSERT_EQUAL(size_t{1}, out.get_key_map("nodes6").size());
  CPPUNIT_ASSERT(out.get_key_map("nodes6").begin()->second.has_key_string("i6"));
  CPPUNIT_ASSERT_EQUAL(self.str(), out.get_key_string("self_id"));
}

void
TestDhtIpv6::test_bucket_full_cache_holds_eight_nodes() {
  torrent::HashString begin;
  torrent::HashString end;
  begin.clear();
  end.clear(0xFF);

  torrent::DhtBucket bucket(begin, end);
  std::vector<std::unique_ptr<torrent::DhtNode>> nodes;

  // more good IPv4 nodes than a reply carries: the reply still holds eight (8 x 26 bytes)
  for (int i = 0; i < 12; i++) {
    nodes.push_back(std::make_unique<torrent::DhtNode>(make_id('\x10', static_cast<char>(0x10 + i)),
                                                       torrent::sa_make_inet_h(0x0a000001 + i, 6881).get()));
    bucket.add_node(nodes.back().get());
  }

  CPPUNIT_ASSERT_EQUAL(uint32_t{8 * 26}, bucket.full_bucket().size());

  torrent::DhtBucket bucket6(begin, end);
  std::vector<std::unique_ptr<torrent::DhtNode>> nodes6;

  for (int i = 0; i < 12; i++) {
    char addr[64];
    snprintf(addr, sizeof(addr), "2a01:e34::%x", i + 1);
    nodes6.push_back(std::make_unique<torrent::DhtNode>(make_id('\x20', static_cast<char>(0x20 + i)), make_inet6(addr, 6881).get()));
    bucket6.add_node(nodes6.back().get());
  }

  CPPUNIT_ASSERT_EQUAL(uint32_t{8 * 38}, bucket6.full_bucket().size());
}

void
TestDhtIpv6::test_message_reads_nodes_and_nodes6() {
  // a find_node reply carrying both families (BEP 32): each key reaches its own slot
  std::string id(20, '\x42');
  std::string node4 = std::string(20, '\x61') + std::string("\x01\x02\x03\x04\x1a\xe1", 6);
  std::string node6 = std::string(20, '\x62') + std::string(reinterpret_cast<const char*>(seed_addr), 16) + std::string("\x8b\x92", 2);

  std::string msg = "d1:rd2:id20:" + id + "5:nodes26:" + node4 + "6:nodes638:" + node6 + "e1:t1:x1:y1:re";

  torrent::DhtMessage message;
  torrent::static_map_read_bencode(msg.data(), msg.data() + msg.size(), message);

  CPPUNIT_ASSERT(message[torrent::key_r_nodes].is_raw_string());
  CPPUNIT_ASSERT_EQUAL(uint32_t{26}, message[torrent::key_r_nodes].as_raw_string().size());
  CPPUNIT_ASSERT(message[torrent::key_r_nodes6].is_raw_string());
  CPPUNIT_ASSERT_EQUAL(uint32_t{38}, message[torrent::key_r_nodes6].as_raw_string().size());
  CPPUNIT_ASSERT(message[torrent::key_r_id].is_raw_string());

  // a reply with nodes6 alone leaves nodes empty, not malformed
  std::string only6 = "d1:rd2:id20:" + id + "6:nodes638:" + node6 + "e1:t1:x1:y1:re";
  torrent::DhtMessage message6;
  torrent::static_map_read_bencode(only6.data(), only6.data() + only6.size(), message6);

  CPPUNIT_ASSERT(!message6[torrent::key_r_nodes].is_raw_string());
  CPPUNIT_ASSERT(message6[torrent::key_r_nodes6].is_raw_string());
}

void
TestDhtIpv6::test_announce_reaches_the_eight_closest() {
  // BEP 5: peers are stored at the eight nodes closest to the info-hash; announcing to three missed a small
  // swarm's peers that qBittorrent found (half-1, 2026-09-25)
  CPPUNIT_ASSERT_EQUAL(torrent::DhtBucket::num_nodes, torrent::dht::DhtSearch::max_announce);
}

void
TestDhtIpv6::test_bootstrap_retries_within_seconds() {
  // a fresh table held 2 nodes for the first minute when bootstrapping retried once a minute (2026-09-25)
  CPPUNIT_ASSERT(torrent::DhtRouter::timeout_bootstrap_retry <= 10);
}
