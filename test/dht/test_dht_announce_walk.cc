#include "config.h"

#include "test/dht/test_dht_announce_walk.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <functional>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "dht/dht_bucket.h"
#include "dht/dht_router.h"
#include "dht/transactions/dht_search.h"
#include "torrent/hash_string.h"
#include "torrent/object.h"
#include "torrent/object_stream.h"
#include "torrent/runtime/network_manager.h"
#include "torrent/runtime/socket_manager.h"
#include "torrent/system/poll.h"
#include "torrent/tracker/dht_controller.h"

CPPUNIT_TEST_SUITE_REGISTRATION(TestDhtAnnounceWalk);

// The announce walk is driven over loopback UDP: a DhtRouter on 127.0.0.1 talks to fake nodes, each a
// plain UDP socket that answers get_peers (and find_node, ping, announce_peer) and records every query.

namespace {

constexpr uint16_t dht_port = 43841;

torrent::HashString
walk_target() {
  torrent::HashString hash;
  hash.clear(0);
  hash.data()[0] = static_cast<char>(0x80);
  return hash;
}

// A node ID whose XOR distance to the target is `distance`, in the last byte.
torrent::HashString
node_id_at(uint8_t distance) {
  auto id = walk_target();
  id.data()[19] = static_cast<char>(distance);
  return id;
}

struct Query {
  std::string method;
  std::string target;
  std::string token;
};

class FakeNode {
public:
  explicit FakeNode(uint8_t distance)
    : m_id(node_id_at(distance)),
      m_token("tk" + std::to_string(distance)) {

    m_fd = ::socket(AF_INET, SOCK_DGRAM, 0);

    sockaddr_in sa{};
    sa.sin_family      = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (m_fd == -1 || ::fcntl(m_fd, F_SETFL, O_NONBLOCK) != 0 ||
        ::bind(m_fd, reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) != 0)
      throw std::runtime_error("FakeNode: could not bind a loopback UDP socket");

    socklen_t len = sizeof(sa);
    ::getsockname(m_fd, reinterpret_cast<sockaddr*>(&sa), &len);
    m_port = ntohs(sa.sin_port);
  }

  ~FakeNode() { ::close(m_fd); }

  FakeNode(const FakeNode&) = delete;
  FakeNode& operator=(const FakeNode&) = delete;

  const torrent::HashString& id() const    { return m_id; }
  uint16_t                   port() const  { return m_port; }
  const std::string&         token() const { return m_token; }
  void                       set_token(std::string token) { m_token = std::move(token); }

  std::string compact() const {
    std::string node(m_id.data(), torrent::HashString::size_data);
    uint32_t    addr = htonl(INADDR_LOOPBACK);
    uint16_t    port = htons(m_port);

    node.append(reinterpret_cast<const char*>(&addr), 4);
    node.append(reinterpret_cast<const char*>(&port), 2);
    return node;
  }

  // Count the queries of `method` about the walk's target.
  unsigned int count(const std::string& method) const {
    unsigned int n = 0;

    for (const auto& q : queries)
      n += q.method == method && q.target == target_string();

    return n;
  }

  void service();

  std::string        nodes;    // compact nodes returned by find_node and get_peers
  bool               silent{false};
  std::vector<Query> queries;

private:
  static std::string target_string() { return std::string(walk_target().data(), torrent::HashString::size_data); }

  int                 m_fd{-1};
  uint16_t            m_port{0};
  torrent::HashString m_id;
  std::string         m_token;
};

void
FakeNode::service() {
  char buffer[2048];

  while (true) {
    sockaddr_in from{};
    socklen_t   from_len = sizeof(from);
    ssize_t     size     = ::recvfrom(m_fd, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&from), &from_len);

    if (size <= 0)
      return;

    torrent::Object message;

    try {
      torrent::object_read_bencode_c(buffer, buffer + size, &message);
    } catch (const std::exception&) {
      continue;
    }

    if (!message.is_map() || !message.has_key_string("q") || !message.has_key_map("a") || !message.has_key_string("t"))
      continue;

    const auto& args = message.get_key("a");

    Query query;
    query.method = message.get_key_string("q");

    if (args.has_key_string("info_hash"))
      query.target = args.get_key_string("info_hash");
    else if (args.has_key_string("target"))
      query.target = args.get_key_string("target");

    if (args.has_key_string("token"))
      query.token = args.get_key_string("token");

    queries.push_back(query);

    if (silent)
      continue;

    auto reply = torrent::Object::create_map();
    auto& r    = reply.insert_key("r", torrent::Object::create_map());

    r.insert_key("id", std::string(m_id.data(), torrent::HashString::size_data));

    if (query.method == "find_node" || query.method == "get_peers")
      r.insert_key("nodes", nodes);

    if (query.method == "get_peers")
      r.insert_key("token", m_token);

    reply.insert_key("t", message.get_key_string("t"));
    reply.insert_key("y", std::string("r"));

    std::ostringstream out;
    torrent::object_write_bencode(&out, &reply);

    auto data = out.str();
    ::sendto(m_fd, data.data(), data.size(), 0, reinterpret_cast<sockaddr*>(&from), from_len);
  }
}

// A DhtRouter on 127.0.0.1 whose table holds the given fake nodes.
class Walk {
public:
  explicit Walk(const std::vector<FakeNode*>& cached) {
    torrent::HashString self_id;
    self_id.clear(0x55);

    auto cache = torrent::Object::create_map();
    cache.insert_key("self_id", self_id.str());

    auto& cache_nodes = cache.insert_key("nodes", torrent::Object::create_map());

    for (auto node : cached) {
      auto& entry = cache_nodes.insert_key(node->id().str(), torrent::Object::create_map());
      entry.insert_key("i", int64_t{0x7f000001});
      entry.insert_key("p", int64_t{node->port()});
      entry.insert_key("t", int64_t{0});
    }

    m_router = std::make_unique<torrent::DhtRouter>(torrent::runtime::network_manager()->dht_controller(), cache);
    m_router->start(dht_port);
  }

  ~Walk() { m_router->stop(); }

  void announce() { m_router->announce(walk_target(), std::weak_ptr<torrent::TrackerDht>()); }

  // Run the router's socket and the fake nodes until `done` holds or `limit` passes.
  static bool pump(const std::vector<FakeNode*>& fakes, const std::function<bool()>& done, std::chrono::milliseconds limit) {
    auto deadline = std::chrono::steady_clock::now() + limit;

    while (std::chrono::steady_clock::now() < deadline) {
      torrent::this_thread::poll()->do_poll(std::chrono::milliseconds(5));

      for (auto fake : fakes)
        fake->service();

      if (done())
        return true;
    }

    return false;
  }

private:
  std::unique_ptr<torrent::DhtRouter> m_router;
};

unsigned int
announces(const std::vector<FakeNode*>& fakes) {
  unsigned int n = 0;

  for (auto fake : fakes)
    n += fake->count("announce_peer");

  return n;
}

// Three nodes in the table, all farther from the target than the eight nodes the first one returns.
struct EightCloserNodes {
  EightCloserNodes() {
    for (uint8_t distance : { 0x40, 0x41, 0x42 })
      cached.push_back(std::make_unique<FakeNode>(distance));

    for (uint8_t distance = 1; distance <= 8; distance++)
      closer.push_back(std::make_unique<FakeNode>(distance));

    for (auto& node : closer)
      cached[0]->nodes += node->compact();

    for (auto& node : cached)
      all.push_back(node.get());

    for (auto& node : closer)
      all.push_back(node.get());
  }

  std::vector<FakeNode*> cached_ptrs() const {
    std::vector<FakeNode*> ptrs;

    for (auto& node : cached)
      ptrs.push_back(node.get());

    return ptrs;
  }

  std::vector<std::unique_ptr<FakeNode>> cached;
  std::vector<std::unique_ptr<FakeNode>> closer;
  std::vector<FakeNode*>                 all;
};

} // namespace

void
TestDhtAnnounceWalk::setUp() {
  TestFixtureWithMainNetTrackerThread::setUp();

  torrent::runtime::socket_manager()->set_max_size_and_adjust(1024);
}

void
TestDhtAnnounceWalk::test_announce_reaches_the_eight_closest() {
  // BEP 5 stores an info-hash's peers at the eight closest nodes.
  CPPUNIT_ASSERT_EQUAL(torrent::DhtBucket::num_nodes, torrent::dht::DhtSearch::max_announce);
}

void
TestDhtAnnounceWalk::test_walk_sends_get_peers() {
  EightCloserNodes net;
  Walk             walk(net.cached_ptrs());

  walk.announce();
  Walk::pump(net.all, [&] { return announces(net.all) >= 8; }, std::chrono::milliseconds(3000));

  // Every node on the way, including the ones learned from a get_peers reply, is asked with get_peers.
  for (auto fake : net.all) {
    CPPUNIT_ASSERT_EQUAL(0u, fake->count("find_node"));
    CPPUNIT_ASSERT(fake->count("get_peers") >= 1);
  }
}

void
TestDhtAnnounceWalk::test_announce_peer_goes_to_eight_closest_with_their_tokens() {
  EightCloserNodes net;
  Walk             walk(net.cached_ptrs());

  walk.announce();
  Walk::pump(net.all, [&] { return announces(net.all) >= 8; }, std::chrono::milliseconds(3000));

  for (auto& fake : net.closer) {
    CPPUNIT_ASSERT_EQUAL(1u, fake->count("announce_peer"));

    for (const auto& q : fake->queries)
      if (q.method == "announce_peer")
        CPPUNIT_ASSERT_EQUAL(fake->token(), q.token);
  }

  for (auto& fake : net.cached)
    CPPUNIT_ASSERT_EQUAL(0u, fake->count("announce_peer"));
}

void
TestDhtAnnounceWalk::test_no_second_get_peers_round() {
  EightCloserNodes net;
  Walk             walk(net.cached_ptrs());

  walk.announce();
  Walk::pump(net.all, [&] { return announces(net.all) >= 8; }, std::chrono::milliseconds(3000));
  Walk::pump(net.all, [] { return false; }, std::chrono::milliseconds(200));

  // The tokens come from the walk, so each of the closest nodes is asked once before the announce.
  for (auto& fake : net.closer)
    CPPUNIT_ASSERT_EQUAL(1u, fake->count("find_node") + fake->count("get_peers"));
}

void
TestDhtAnnounceWalk::test_walk_continues_after_quick_timeout() {
  // The three closest nodes never answer; the fourth is asked once their quick timeout passes.
  FakeNode silent1(1), silent2(2), silent3(3), replying(0x40);
  silent1.silent = silent2.silent = silent3.silent = true;

  std::vector<FakeNode*> all{ &silent1, &silent2, &silent3, &replying };
  Walk                   walk(all);

  walk.announce();
  Walk::pump(all, [] { return false; }, std::chrono::milliseconds(200));

  CPPUNIT_ASSERT_EQUAL(1u, silent1.count("get_peers"));
  CPPUNIT_ASSERT_EQUAL(1u, silent2.count("get_peers"));
  CPPUNIT_ASSERT_EQUAL(1u, silent3.count("get_peers"));
  CPPUNIT_ASSERT_EQUAL(0u, replying.count("get_peers"));

  m_main_thread->test_add_cached_time(std::chrono::seconds(6));
  m_main_thread->test_process_events_without_cached_time();

  CPPUNIT_ASSERT(Walk::pump(all, [&] { return replying.count("get_peers") == 1; }, std::chrono::milliseconds(1000)));

  // Once the silent nodes time out for good, the walk ends and announces to the node that answered.
  m_main_thread->test_add_cached_time(std::chrono::seconds(40));
  m_main_thread->test_process_events_without_cached_time();

  CPPUNIT_ASSERT(Walk::pump(all, [&] { return replying.count("announce_peer") == 1; }, std::chrono::milliseconds(1000)));
  CPPUNIT_ASSERT_EQUAL(0u, silent1.count("announce_peer"));
}

void
TestDhtAnnounceWalk::test_oversized_token_keeps_walking() {
  // The only node in the table answers with a 65-byte token and one closer node. The token cannot be
  // used, but the closer node is still part of the walk and gets the announce.
  FakeNode far(0x40), near(1);
  far.set_token(std::string(65, 'x'));
  far.nodes = near.compact();

  std::vector<FakeNode*> all{ &far, &near };
  Walk                   walk({ &far });

  walk.announce();
  Walk::pump(all, [&] { return near.count("announce_peer") == 1; }, std::chrono::milliseconds(2000));

  CPPUNIT_ASSERT_EQUAL(1u, near.count("get_peers"));
  CPPUNIT_ASSERT_EQUAL(1u, near.count("announce_peer"));
  CPPUNIT_ASSERT_EQUAL(0u, far.count("announce_peer"));
}
