#include "config.h"

#include "test/dht/test_dht_announce_walk.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "dht/dht_bucket.h"
#include "dht/dht_router.h"
#include "dht/dht_transaction.h"
#include "dht/transactions/dht_search.h"
#include "dht/udp_router.h"
#include "torrent/hash_string.h"
#include "torrent/net/socket_address.h"
#include "torrent/object.h"
#include "torrent/object_stream.h"
#include "torrent/runtime/network_manager.h"
#include "torrent/runtime/socket_manager.h"
#include "torrent/system/poll.h"
#include "torrent/system/callbacks.h"
#include "torrent/tracker/dht_controller.h"
#include "tracker/tracker_dht.h"
#include "tracker/udp_router.h"

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
  std::string transaction_id;
  std::string packet;
};

class FakeNode {
public:
  explicit FakeNode(uint8_t distance, uint32_t address = INADDR_LOOPBACK)
    : m_id(node_id_at(distance)),
      m_token("tk" + std::to_string(distance)) {

    m_fd = ::socket(AF_INET, SOCK_DGRAM, 0);

    sockaddr_in sa{};
    sa.sin_family      = AF_INET;
    sa.sin_addr.s_addr = htonl(address);

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

  ssize_t send_to_server(const std::string& data) const {
    return send_to_port(data, dht_port);
  }

  ssize_t send_to_port(const std::string& data, uint16_t port) const {
    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    server.sin_port = htons(port);
    return ::sendto(m_fd, data.data(), data.size(), 0, reinterpret_cast<sockaddr*>(&server), sizeof(server));
  }

  ssize_t receive(char* data, size_t size) const {
    return ::recv(m_fd, data, size, 0);
  }

  ssize_t reply_to_query(const Query& query) const {
    auto reply = torrent::Object::create_map();
    auto& result = reply.insert_key("r", torrent::Object::create_map());
    result.insert_key("id", m_id.str());
    result.insert_key("nodes", std::string());
    reply.insert_key("t", query.transaction_id);
    reply.insert_key("y", std::string("r"));
    std::ostringstream stream;
    torrent::object_write_bencode(&stream, &reply);
    return send_to_server(stream.str());
  }

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
  bool               silent_announce{false};
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
    query.transaction_id = message.get_key_string("t");
    query.packet.assign(buffer, size);

    if (args.has_key_string("info_hash"))
      query.target = args.get_key_string("info_hash");
    else if (args.has_key_string("target"))
      query.target = args.get_key_string("target");

    if (args.has_key_string("token"))
      query.token = args.get_key_string("token");

    queries.push_back(query);

    if (silent || (silent_announce && query.method == "announce_peer"))
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
  void announce(std::weak_ptr<torrent::TrackerDht> tracker, bool primary) { m_router->announce(walk_target(), std::move(tracker), primary); }
  void cancel() { m_router->cancel_announce(walk_target(), std::weak_ptr<torrent::TrackerDht>()); }
  unsigned int queries_sent() const { return m_router->get_statistics().queries_sent; }
  unsigned int replies_received() const { return m_router->get_statistics().replies_received; }

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

class FailureTracker : public torrent::TrackerDht {
public:
  explicit FailureTracker(std::shared_ptr<std::vector<std::string>> failures, std::shared_ptr<unsigned int> successes = {})
    : TrackerDht(torrent::TrackerInfo{}, torrent::tracker::TrackerState::flag_deleted) {
    m_slot_failure = [failures = std::move(failures)](std::string message) {
      failures->push_back(std::move(message));
    };
    m_slot_success = [successes = std::move(successes)](auto) {
      if (successes)
        ++*successes;
    };
  }
};

void
wait_for_tracker_events() {
  auto completed = std::make_shared<std::promise<void>>();
  auto future = completed->get_future();
  torrent::tracker_thread::callback([completed] { completed->set_value(); });
  CPPUNIT_ASSERT(future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
}

std::vector<std::string>
empty_announce_failures(bool primary) {
  auto cache = torrent::Object::create_map();
  torrent::runtime::network_manager()->dht_controller()->initialize(cache);

  auto failures = std::make_shared<std::vector<std::string>>();
  auto tracker = std::make_shared<FailureTracker>(failures);
  Walk walk({});
  walk.announce(tracker, primary);

  wait_for_tracker_events();
  return *failures;
}

} // namespace

void
TestDhtAnnounceWalk::setUp() {
  TestFixtureWithMainNetTrackerThread::setUp();

  torrent::runtime::socket_manager()->set_max_size_and_adjust(1024);
}

void
TestDhtAnnounceWalk::test_empty_announce_reports_primary_failure() {
  auto failures = empty_announce_failures(true);
  CPPUNIT_ASSERT_EQUAL(size_t{1}, failures.size());
  CPPUNIT_ASSERT_EQUAL(std::string("No DHT nodes available for peer search."), failures.front());
}

void
TestDhtAnnounceWalk::test_empty_announce_does_not_report_secondary_failure() {
  CPPUNIT_ASSERT(empty_announce_failures(false).empty());
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
  // The three closest nodes never answer; the fourth is asked once their first 15s timeout passes.
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

  m_main_thread->test_add_cached_time(std::chrono::seconds(16));
  m_main_thread->test_process_events_without_cached_time();

  CPPUNIT_ASSERT(Walk::pump(all, [&] { return replying.count("get_peers") == 1; }, std::chrono::milliseconds(1000)));
  CPPUNIT_ASSERT(Walk::pump(all, [&] { return walk.replies_received() >= 1; }, std::chrono::milliseconds(1000)));

  // Once the silent nodes time out for good, the walk ends and announces to the node that answered.
  m_main_thread->test_add_cached_time(std::chrono::seconds(40));
  m_main_thread->test_process_events_without_cached_time();
  m_main_thread->test_add_cached_time(std::chrono::seconds(31));
  m_main_thread->test_process_events_without_cached_time();
  m_main_thread->test_add_cached_time(std::chrono::seconds(46));
  m_main_thread->test_process_events_without_cached_time();

  Walk::pump(all, [&] { return replying.count("announce_peer") >= 1; }, std::chrono::milliseconds(1000));
  CPPUNIT_ASSERT_EQUAL(1u, replying.count("announce_peer"));
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

void
TestDhtAnnounceWalk::test_incoming_ping_after_invalid_datagram() {
  FakeNode node(1);
  Walk walk({});

  auto ping = torrent::Object::create_map();
  ping.insert_key("a", torrent::Object::create_map()).insert_key("id", node.id().str());
  ping.insert_key("q", std::string("ping"));
  ping.insert_key("t", std::string("xy"));
  ping.insert_key("y", std::string("q"));

  std::ostringstream stream;
  torrent::object_write_bencode(&stream, &ping);
  auto data = stream.str();

  CPPUNIT_ASSERT_EQUAL(ssize_t{7}, node.send_to_server("invalid"));
  CPPUNIT_ASSERT_EQUAL(static_cast<ssize_t>(data.size()), node.send_to_server(data));

  char buffer[2048];
  torrent::Object reply;
  CPPUNIT_ASSERT(Walk::pump({}, [&] {
    auto size = node.receive(buffer, sizeof(buffer));
    if (size < 0)
      return false;

    torrent::object_read_bencode_c(buffer, buffer + size, &reply);
    CPPUNIT_ASSERT(reply.get_key_string("y") != "e");
    return reply.get_key_string("y") == "r";
  }, std::chrono::milliseconds(1000)));

  CPPUNIT_ASSERT_EQUAL(std::string("r"), reply.get_key_string("y"));
  CPPUNIT_ASSERT_EQUAL(std::string("xy"), reply.get_key_string("t"));
}

void
TestDhtAnnounceWalk::test_incoming_get_peers_without_nodes() {
  FakeNode node(1);
  Walk walk({});

  auto query = torrent::Object::create_map();
  auto& args = query.insert_key("a", torrent::Object::create_map());
  args.insert_key("id", node.id().str());
  args.insert_key("info_hash", walk_target().str());
  query.insert_key("q", std::string("get_peers"));
  query.insert_key("t", std::string("xy"));
  query.insert_key("y", std::string("q"));

  std::ostringstream stream;
  torrent::object_write_bencode(&stream, &query);
  CPPUNIT_ASSERT(node.send_to_server(stream.str()) > 0);

  char buffer[2048];
  torrent::Object reply;
  CPPUNIT_ASSERT(Walk::pump({}, [&] {
    auto size = node.receive(buffer, sizeof(buffer));
    if (size < 0)
      return false;
    torrent::object_read_bencode_c(buffer, buffer + size, &reply);
    return true;
  }, std::chrono::milliseconds(1000)));

  CPPUNIT_ASSERT_EQUAL(std::string("e"), reply.get_key_string("y"));
  CPPUNIT_ASSERT_EQUAL(std::string("xy"), reply.get_key_string("t"));
  const auto& error = reply.get_key_list("e");
  CPPUNIT_ASSERT_EQUAL(size_t{2}, error.size());
  CPPUNIT_ASSERT_EQUAL(int64_t{201}, error.front().as_value());
  CPPUNIT_ASSERT_EQUAL(std::string("No peers nor nodes"), error.back().as_string());

  Walk::pump({}, [] { return false; }, std::chrono::milliseconds(100));
  CPPUNIT_ASSERT_EQUAL(ssize_t{-1}, node.receive(buffer, sizeof(buffer)));

  auto ping = torrent::Object::create_map();
  ping.insert_key("a", torrent::Object::create_map()).insert_key("id", node.id().str());
  ping.insert_key("q", std::string("ping"));
  ping.insert_key("t", std::string("xz"));
  ping.insert_key("y", std::string("q"));

  std::ostringstream ping_stream;
  torrent::object_write_bencode(&ping_stream, &ping);
  CPPUNIT_ASSERT(node.send_to_server(ping_stream.str()) > 0);
  CPPUNIT_ASSERT(Walk::pump({&node}, [&] { return walk.replies_received() == 1; }, std::chrono::milliseconds(1000)));

  CPPUNIT_ASSERT(node.send_to_server(stream.str()) > 0);
  CPPUNIT_ASSERT(Walk::pump({}, [&] {
    auto size = node.receive(buffer, sizeof(buffer));
    if (size < 0)
      return false;
    torrent::object_read_bencode_c(buffer, buffer + size, &reply);
    return reply.has_key_string("y") && reply.get_key_string("y") == "r";
  }, std::chrono::milliseconds(1000)));
  CPPUNIT_ASSERT_EQUAL(node.compact(), reply.get_key("r").get_key_string("nodes"));
  CPPUNIT_ASSERT_EQUAL(size_t{8}, reply.get_key("r").get_key_string("token").size());
}

void
TestDhtAnnounceWalk::test_incoming_malformed_query() {
  FakeNode node(1);
  Walk walk({});

  auto query = torrent::Object::create_map();
  query.insert_key("a", torrent::Object::create_map()).insert_key("id", node.id().str());
  query.insert_key("q", std::string("get_peers"));
  query.insert_key("t", std::string("xy"));
  query.insert_key("y", std::string("q"));

  std::ostringstream stream;
  torrent::object_write_bencode(&stream, &query);
  CPPUNIT_ASSERT(node.send_to_server(stream.str()) > 0);

  char buffer[2048];
  query.get_key("a").insert_key("info_hash", std::string("short"));
  std::ostringstream short_stream;
  torrent::object_write_bencode(&short_stream, &query);
  CPPUNIT_ASSERT(node.send_to_server(short_stream.str()) > 0);

  query.get_key("q") = torrent::Object(std::string("ping"));
  query.get_key("t") = torrent::Object(std::string("ok"));
  std::ostringstream ping_stream;
  torrent::object_write_bencode(&ping_stream, &query);
  CPPUNIT_ASSERT(node.send_to_server(ping_stream.str()) > 0);

  torrent::Object reply;
  CPPUNIT_ASSERT(Walk::pump({}, [&] {
    auto size = node.receive(buffer, sizeof(buffer));
    if (size < 0)
      return false;
    torrent::object_read_bencode_c(buffer, buffer + size, &reply);
    if (reply.get_key_string("y") == "q")
      return false;
    CPPUNIT_ASSERT_EQUAL(std::string("r"), reply.get_key_string("y"));
    CPPUNIT_ASSERT_EQUAL(std::string("ok"), reply.get_key_string("t"));
    return true;
  }, std::chrono::milliseconds(1000)));

  CPPUNIT_ASSERT_EQUAL(std::string("r"), reply.get_key_string("y"));
  CPPUNIT_ASSERT_EQUAL(std::string("ok"), reply.get_key_string("t"));
  Walk::pump({}, [&] {
    auto size = node.receive(buffer, sizeof(buffer));
    if (size >= 0) {
      torrent::object_read_bencode_c(buffer, buffer + size, &reply);
      CPPUNIT_ASSERT_EQUAL(std::string("q"), reply.get_key_string("y"));
    }
    return false;
  }, std::chrono::milliseconds(100));
}

void
TestDhtAnnounceWalk::test_malformed_response_reports_failed_search() {
  auto cache = torrent::Object::create_map();
  torrent::runtime::network_manager()->dht_controller()->initialize(cache);
  auto failures = std::make_shared<std::vector<std::string>>();
  auto tracker = std::make_shared<FailureTracker>(failures);
  FakeNode node(1);
  node.silent = true;
  Walk walk({&node});
  walk.announce(tracker, true);

  CPPUNIT_ASSERT(Walk::pump({&node}, [&] { return node.count("get_peers") == 1; }, std::chrono::milliseconds(1000)));
  auto reply = torrent::Object::create_map();
  reply.insert_key("r", torrent::Object::create_map()).insert_key("id", std::string("short"));
  reply.insert_key("t", node.queries.front().transaction_id);
  reply.insert_key("y", std::string("r"));
  std::ostringstream stream;
  torrent::object_write_bencode(&stream, &reply);
  CPPUNIT_ASSERT(node.send_to_server(stream.str()) > 0);
  Walk::pump({}, [] { return false; }, std::chrono::milliseconds(100));
  char buffer[2048];
  CPPUNIT_ASSERT(node.receive(buffer, sizeof(buffer)) < 0);
  wait_for_tracker_events();
  CPPUNIT_ASSERT(failures->empty());

  for (auto seconds : {16, 31, 46}) {
    m_main_thread->test_add_cached_time(std::chrono::seconds(seconds));
    m_main_thread->test_process_events_without_cached_time();
  }
  wait_for_tracker_events();
  CPPUNIT_ASSERT_EQUAL(size_t{1}, failures->size());
  CPPUNIT_ASSERT_EQUAL(std::string("DHT search: Invalid `id' length"), failures->front());
}

void
TestDhtAnnounceWalk::test_malformed_response_does_not_fail_successful_search() {
  auto cache = torrent::Object::create_map();
  torrent::runtime::network_manager()->dht_controller()->initialize(cache);
  auto failures = std::make_shared<std::vector<std::string>>();
  auto successes = std::make_shared<unsigned int>(0);
  auto tracker = std::make_shared<FailureTracker>(failures, successes);
  FakeNode bad(1), good(2);
  bad.silent = true;
  Walk walk({&bad, &good});
  walk.announce(tracker, true);
  CPPUNIT_ASSERT(Walk::pump({&bad, &good}, [&] { return bad.count("get_peers") == 1 && walk.replies_received() >= 1; }, std::chrono::milliseconds(1000)));

  auto reply = torrent::Object::create_map();
  reply.insert_key("r", torrent::Object::create_map()).insert_key("id", std::string("short"));
  reply.insert_key("t", bad.queries.front().transaction_id);
  reply.insert_key("y", std::string("r"));
  std::ostringstream stream;
  torrent::object_write_bencode(&stream, &reply);
  CPPUNIT_ASSERT(bad.send_to_server(stream.str()) > 0);
  Walk::pump({&bad, &good}, [] { return false; }, std::chrono::milliseconds(100));

  for (auto seconds : {16, 31, 46}) {
    m_main_thread->test_add_cached_time(std::chrono::seconds(seconds));
    m_main_thread->test_process_events_without_cached_time();
  }
  Walk::pump({&good}, [] { return false; }, std::chrono::milliseconds(100));
  wait_for_tracker_events();
  CPPUNIT_ASSERT(failures->empty());
  CPPUNIT_ASSERT_EQUAL(1u, *successes);
}

void
TestDhtAnnounceWalk::test_udp_router_keyed_dispatch() {
  FakeNode node(1);
  FakeNode other(2, 0x7f000002);
  unsigned int responses = 0;
  unsigned int errors = 0;
  torrent::dht::UdpRouter router;

  router.open(AF_INET, 0);
  sockaddr_in server{};
  socklen_t server_length = sizeof(server);
  CPPUNIT_ASSERT_EQUAL(0, ::getsockname(router.file_descriptor(), reinterpret_cast<sockaddr*>(&server), &server_length));

  sockaddr_in peer{};
  peer.sin_family = AF_INET;
  peer.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  peer.sin_port = htons(node.port());
  torrent::c_sa_shared_ptr address = torrent::sa_copy(reinterpret_cast<const sockaddr*>(&peer));
  auto key = torrent::DhtTransaction::key(address.get(), 7);

  auto send = [&](const FakeNode& sender, char type, const std::string& transaction_id) {
    auto packet = torrent::Object::create_map();
    if (type == 'r')
      packet.insert_key("r", torrent::Object::create_map()).insert_key("id", node.id().str());
    packet.insert_key("t", transaction_id);
    packet.insert_key("y", std::string(1, type));
    std::ostringstream stream;
    torrent::object_write_bencode(&stream, &packet);
    auto data = stream.str();
    CPPUNIT_ASSERT_EQUAL(static_cast<ssize_t>(data.size()), sender.send_to_port(data, ntohs(server.sin_port)));
    static_cast<torrent::system::Event&>(router).event_read();
  };

  auto prepare = [](torrent::dht::UdpRouter::key_type, torrent::dht::UdpRouter::buffer_type& buffer) { buffer.write_8('x'); };
  auto timeout = [](torrent::dht::UdpRouter::key_type, int, int) { CPPUNIT_FAIL("Unexpected router timeout"); };

  router.connect(key, address, {
    prepare,
    [&](torrent::dht::UdpRouter::key_type received_key, torrent::dht::UdpRouter::buffer_type&) {
      CPPUNIT_ASSERT_EQUAL(key, received_key);
      ++responses;
      return false;
    },
    timeout, {}
  });

  auto second_key = torrent::DhtTransaction::key(address.get(), 8);
  unsigned int second_responses = 0;
  router.connect(second_key, address, {
    prepare,
    [&](torrent::dht::UdpRouter::key_type received_key, torrent::dht::UdpRouter::buffer_type&) {
      CPPUNIT_ASSERT_EQUAL(second_key, received_key);
      ++second_responses;
      return false;
    },
    timeout, {}
  });

  send(node, 'r', std::string("\0\x09", 2));
  send(other, 'r', std::string("\0\x07", 2));
  CPPUNIT_ASSERT_EQUAL(0u, responses);
  CPPUNIT_ASSERT_EQUAL(0u, second_responses);
  send(node, 'r', std::string("\0\x08", 2));
  CPPUNIT_ASSERT_EQUAL(1u, second_responses);
  CPPUNIT_ASSERT_EQUAL(0u, responses);
  send(node, 'r', std::string("\0\x07", 2));
  CPPUNIT_ASSERT_EQUAL(1u, responses);
  router.connect(key, address, {
    prepare,
    [&](auto received_key, auto&) {
      CPPUNIT_ASSERT_EQUAL(key, received_key);
      ++errors;
      return false;
    },
    timeout, {}
  });
  send(node, 'e', std::string("\0\x07", 2));
  CPPUNIT_ASSERT_EQUAL(1u, errors);
  send(node, 'e', std::string("\0\x07", 2));
  CPPUNIT_ASSERT_EQUAL(1u, errors);

  const std::array<std::string, 4> transaction_ids{
    std::string("\x80", 1),
    std::string("\x01\x00", 2),
    std::string("\x01\x00\x00", 3),
    std::string("\x01\x00\x00\x00", 4),
  };
  std::vector<torrent::DhtTransaction::key_type> multi_byte_responses;
  auto no_failure = [](auto, int, int) {};

  for (const auto& transaction_id : transaction_ids) {
    auto transaction_key = torrent::DhtTransaction::key(address.get(), transaction_id.data(), transaction_id.size());
    router.connect(transaction_key, address, {
      prepare,
      [&](auto received_key, auto&) {
        multi_byte_responses.push_back(received_key);
        return false;
      },
      no_failure, {}
    });
  }

  for (const auto& transaction_id : transaction_ids)
    send(node, 'r', transaction_id);

  auto reentrant_key = torrent::DhtTransaction::key(address.get(), 0x40);
  unsigned int reentrant_responses = 0;
  router.connect(reentrant_key, address, {
    prepare,
    [&](auto, auto&) {
      for (uint64_t id = 0x10000; id < 0x10080; ++id)
        router.connect(id, address, {prepare, nullptr, no_failure, {}});

      ++reentrant_responses;
      return false;
    },
    no_failure, {}
  });
  send(node, 'r', std::string("\0\x40", 2));
  send(node, 'r', std::string("\0\x40", 2));

  router.close();
  CPPUNIT_ASSERT_EQUAL(transaction_ids.size(), multi_byte_responses.size());
  CPPUNIT_ASSERT_EQUAL(1u, reentrant_responses);

  for (size_t i = 0; i < transaction_ids.size(); ++i)
    CPPUNIT_ASSERT_EQUAL(torrent::DhtTransaction::key(address.get(), transaction_ids[i].data(), transaction_ids[i].size()), multi_byte_responses[i]);
}

void
TestDhtAnnounceWalk::test_retry_and_reply() {
  FakeNode node(1);
  node.silent = true;
  Walk walk({&node});
  walk.announce();

  CPPUNIT_ASSERT(Walk::pump({&node}, [&] { return node.count("get_peers") == 1; }, std::chrono::milliseconds(1000)));
  auto initial = node.queries.front();
  CPPUNIT_ASSERT_EQUAL(size_t{2}, initial.transaction_id.size());
  m_main_thread->test_add_cached_time(std::chrono::seconds(16));
  m_main_thread->test_process_events_without_cached_time();
  Walk::pump({&node}, [&] { return node.count("get_peers") >= 2; }, std::chrono::milliseconds(1000));
  CPPUNIT_ASSERT_EQUAL(2u, node.count("get_peers"));
  CPPUNIT_ASSERT_EQUAL(static_cast<unsigned int>(node.queries.size()), walk.queries_sent());
  auto retry = std::find_if(node.queries.begin() + 1, node.queries.end(), [](const auto& query) { return query.method == "get_peers"; });
  CPPUNIT_ASSERT(retry != node.queries.end());
  CPPUNIT_ASSERT_EQUAL(initial.transaction_id, retry->transaction_id);
  CPPUNIT_ASSERT_EQUAL(initial.packet, retry->packet);

  CPPUNIT_ASSERT(node.reply_to_query(initial) > 0);
  CPPUNIT_ASSERT(Walk::pump({}, [&] { return walk.replies_received() == 1; }, std::chrono::milliseconds(1000)));
  m_main_thread->test_add_cached_time(std::chrono::seconds(31));
  m_main_thread->test_process_events_without_cached_time();
  Walk::pump({&node}, [] { return false; }, std::chrono::milliseconds(100));
  CPPUNIT_ASSERT_EQUAL(2u, node.count("get_peers"));
}

void
TestDhtAnnounceWalk::test_announce_retry_preserves_query() {
  FakeNode node(1);
  std::string token(64, 'x');
  token[3] = '\0';
  node.set_token(token);
  node.silent_announce = true;
  Walk walk({&node});
  walk.announce();

  CPPUNIT_ASSERT(Walk::pump({&node}, [&] { return node.count("announce_peer") == 1; }, std::chrono::milliseconds(1000)));
  auto initial = std::find_if(node.queries.begin(), node.queries.end(), [](const auto& query) { return query.method == "announce_peer"; });
  CPPUNIT_ASSERT(initial != node.queries.end());
  auto packet = initial->packet;
  CPPUNIT_ASSERT_EQUAL(token, initial->token);

  m_main_thread->test_add_cached_time(std::chrono::seconds(16));
  m_main_thread->test_process_events_without_cached_time();
  CPPUNIT_ASSERT(Walk::pump({&node}, [&] { return node.count("announce_peer") == 2; }, std::chrono::milliseconds(1000)));

  auto retry = std::find_if(node.queries.rbegin(), node.queries.rend(), [](const auto& query) { return query.method == "announce_peer"; });
  CPPUNIT_ASSERT(retry != node.queries.rend());
  CPPUNIT_ASSERT_EQUAL(packet, retry->packet);
  CPPUNIT_ASSERT_EQUAL(token, retry->token);
}

void
TestDhtAnnounceWalk::test_sent_transaction_timeout_marks_node_inactive() {
  FakeNode node(1);
  node.silent = true;

  torrent::HashString self_id;
  self_id.clear(0x55);
  auto cache = torrent::Object::create_map();
  cache.insert_key("self_id", self_id.str());
  auto& entry = cache.insert_key("nodes", torrent::Object::create_map()).insert_key(node.id().str(), torrent::Object::create_map());
  entry.insert_key("i", int64_t{0x7f000001});
  entry.insert_key("p", int64_t{node.port()});
  entry.insert_key("t", int64_t{0});

  torrent::DhtRouter router(torrent::runtime::network_manager()->dht_controller(), cache);
  torrent::DhtServer server(&router);
  server.start(dht_port);
  auto address = torrent::sa_make_inet_h(INADDR_LOOPBACK, node.port());

  server.ping(node.id(), address.get());
  CPPUNIT_ASSERT(Walk::pump({&node}, [&] { return !node.queries.empty(); }, std::chrono::milliseconds(1000)));
  CPPUNIT_ASSERT(node.reply_to_query(node.queries.front()) > 0);
  CPPUNIT_ASSERT(Walk::pump({}, [&] { return server.replies_received() == 1; }, std::chrono::milliseconds(1000)));

  auto contact = router.get_node(node.id());
  CPPUNIT_ASSERT(contact != nullptr);

  for (unsigned int i = 1; i < torrent::DhtNode::max_failed_replies; i++)
    contact->inactive();

  CPPUNIT_ASSERT(!contact->is_bad());
  auto count = node.queries.size();
  server.ping(node.id(), address.get());
  CPPUNIT_ASSERT(Walk::pump({&node}, [&] { return node.queries.size() > count; }, std::chrono::milliseconds(1000)));

  for (auto seconds : {16, 31, 46}) {
    m_main_thread->test_add_cached_time(std::chrono::seconds(seconds));
    m_main_thread->test_process_events_without_cached_time();
  }

  CPPUNIT_ASSERT(contact->is_bad());
}

void
TestDhtAnnounceWalk::test_error_completes_transaction() {
  auto cache = torrent::Object::create_map();
  torrent::runtime::network_manager()->dht_controller()->initialize(cache);
  auto failures = std::make_shared<std::vector<std::string>>();
  auto tracker = std::make_shared<FailureTracker>(failures);
  FakeNode node(1);
  node.silent = true;
  Walk walk({&node});
  walk.announce(tracker, true);

  CPPUNIT_ASSERT(Walk::pump({&node}, [&] { return node.count("get_peers") == 1; }, std::chrono::milliseconds(1000)));
  auto error = torrent::Object::create_map();
  auto& error_list = error.insert_key("e", torrent::Object::create_list()).as_list();
  error_list.push_back(torrent::Object(int64_t{201}));
  error_list.push_back(torrent::Object(std::string("error")));
  error.insert_key("t", node.queries.front().transaction_id);
  error.insert_key("y", std::string("e"));
  std::ostringstream stream;
  torrent::object_write_bencode(&stream, &error);
  CPPUNIT_ASSERT(node.send_to_server(stream.str()) > 0);
  CPPUNIT_ASSERT(Walk::pump({}, [&] { return walk.replies_received() == 1; }, std::chrono::milliseconds(1000)));
  wait_for_tracker_events();
  CPPUNIT_ASSERT_EQUAL(size_t{1}, failures->size());
  CPPUNIT_ASSERT_EQUAL(std::string("DHT search: DHT node returned an error."), failures->front());

  m_main_thread->test_add_cached_time(std::chrono::seconds(16));
  m_main_thread->test_process_events_without_cached_time();
  Walk::pump({&node}, [] { return false; }, std::chrono::milliseconds(100));
  CPPUNIT_ASSERT_EQUAL(1u, node.count("get_peers"));
}

void
TestDhtAnnounceWalk::test_timeout_bucket_pointer_stability() {
  torrent::utils::list_buckets<int, 3> buckets;
  buckets.push_back(1, 10);
  int* entry = &buckets.back(1);
  buckets.push_back(0, 20);
  buckets.push_back(1, 30);
  buckets.push_back(2, 40);
  buckets.pop_front(0);
  buckets.pop_back(1);
  buckets.clear(2);
  CPPUNIT_ASSERT_EQUAL(entry, &buckets.front(1));
  CPPUNIT_ASSERT_EQUAL(10, *entry);
  CPPUNIT_ASSERT(buckets.empty(0));
  CPPUNIT_ASSERT(buckets.empty(2));
}

void
TestDhtAnnounceWalk::test_retry_deadline_order() {
  FakeNode node(1);
  torrent::dht::UdpRouter router;
  router.open(AF_INET, 0);

  sockaddr_in peer{};
  peer.sin_family = AF_INET;
  peer.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  peer.sin_port = htons(node.port());
  torrent::c_sa_shared_ptr address = torrent::sa_copy(reinterpret_cast<const sockaddr*>(&peer));
  auto first = torrent::DhtTransaction::key(address.get(), 1);
  auto second = torrent::DhtTransaction::key(address.get(), 2);
  unsigned int first_sends = 0;
  unsigned int second_sends = 0;

  auto connect = [&](auto key, unsigned int& sends) {
    router.connect(key, address, {
      [](auto, auto& buffer) { buffer.write_8('x'); },
      [](auto, auto&) { return true; },
      [](auto, int, int) { CPPUNIT_FAIL("Unexpected router timeout"); },
      {},
      [&sends](auto) { ++sends; }
    });
  };

  connect(first, first_sends);
  m_main_thread->test_add_cached_time(std::chrono::seconds(16));
  m_main_thread->test_process_events_without_cached_time();
  auto first_after_retry = first_sends;

  connect(second, second_sends);
  m_main_thread->test_add_cached_time(std::chrono::seconds(16));
  m_main_thread->test_process_events_without_cached_time();

  router.disconnect(first);
  router.disconnect(second);
  router.close();
  router.open(AF_INET, 0);
  m_main_thread->test_add_cached_time(std::chrono::seconds(120));
  m_main_thread->test_process_events_without_cached_time();

  unsigned int terminal_sends = 0;
  unsigned int terminal_errors = 0;
  int terminal_errno = 0;
  router.connect(first, address, {
    [](auto, auto& buffer) { buffer.write_8('x'); },
    [](auto, auto&) { return true; },
    [&](auto, int err, int) { ++terminal_errors; terminal_errno = err; },
    {},
    [&](auto) { ++terminal_sends; }
  });
  for (auto seconds : {16, 31, 46}) {
    m_main_thread->test_add_cached_time(std::chrono::seconds(seconds));
    m_main_thread->test_process_events_without_cached_time();
  }

  auto remove_on_timeout = torrent::DhtTransaction::key(address.get(), 3);
  unsigned int removed_sends = 0;
  router.connect(remove_on_timeout, address, {
    [](auto, auto& buffer) { buffer.write_8('x'); },
    [](auto, auto&) { return true; },
    [](auto, int, int) { CPPUNIT_FAIL("Unexpected router failure"); },
    [](auto, unsigned int) { return false; },
    [&](auto) { ++removed_sends; }
  });
  m_main_thread->test_add_cached_time(std::chrono::seconds(16));
  m_main_thread->test_process_events_without_cached_time();

  CPPUNIT_ASSERT(!router.has_connection_id(remove_on_timeout));
  router.close();
  CPPUNIT_ASSERT_EQUAL(2u, first_after_retry);
  CPPUNIT_ASSERT_EQUAL(2u, first_sends);
  CPPUNIT_ASSERT_EQUAL(2u, second_sends);
  CPPUNIT_ASSERT_EQUAL(3u, terminal_sends);
  CPPUNIT_ASSERT_EQUAL(1u, terminal_errors);
  CPPUNIT_ASSERT_EQUAL(ETIMEDOUT, terminal_errno);
  CPPUNIT_ASSERT_EQUAL(1u, removed_sends);
}

void
TestDhtAnnounceWalk::test_tracker_retry_deadline_order() {
  FakeNode node(1);
  torrent::tracker::UdpRouter router;
  router.open(AF_INET);
  CPPUNIT_ASSERT(router.is_open());

  auto address = torrent::sa_make_inet_h(INADDR_LOOPBACK, node.port());
  uint32_t first = 0;
  uint32_t second = 0;
  uint32_t third = 0;
  unsigned int first_sends = 0;
  unsigned int second_sends = 0;
  unsigned int third_sends = 0;

  auto connect = [&](uint32_t& id, unsigned int& sends) {
    router.connect(torrent::sa_copy(address.get()), {
      [](auto, auto& buffer) { buffer.write_8('x'); },
      [](auto, auto&) { return false; },
      [](auto, int, int) { CPPUNIT_FAIL("Unexpected tracker timeout"); },
      [&id](auto value) { id = value; },
      [&sends](auto) { ++sends; }
    });
  };

  connect(first, first_sends);
  m_main_thread->test_add_cached_time(std::chrono::seconds(16));
  m_main_thread->test_process_events_without_cached_time();
  auto first_after_retry = first_sends;

  connect(second, second_sends);
  m_main_thread->test_add_cached_time(std::chrono::seconds(16));
  m_main_thread->test_process_events_without_cached_time();
  CPPUNIT_ASSERT_EQUAL(2u, first_after_retry);
  CPPUNIT_ASSERT_EQUAL(2u, first_sends);
  CPPUNIT_ASSERT_EQUAL(2u, second_sends);

  router.disconnect(second);
  connect(third, third_sends);
  m_main_thread->test_add_cached_time(std::chrono::seconds(15));
  m_main_thread->test_process_events_without_cached_time();
  CPPUNIT_ASSERT_EQUAL(3u, first_sends);
  CPPUNIT_ASSERT_EQUAL(2u, second_sends);
  CPPUNIT_ASSERT_EQUAL(2u, third_sends);

  router.disconnect(first);
  router.disconnect(third);
  m_main_thread->test_add_cached_time(std::chrono::seconds(100));
  m_main_thread->test_process_events_without_cached_time();
  router.close();
}

void
TestDhtAnnounceWalk::test_late_reply_after_cancel() {
  FakeNode node(1);
  node.silent = true;
  Walk walk({&node});
  walk.announce();
  CPPUNIT_ASSERT(Walk::pump({&node}, [&] { return !node.queries.empty(); }, std::chrono::milliseconds(1000)));

  walk.cancel();
  CPPUNIT_ASSERT(node.reply_to_query(node.queries.front()) > 0);
  Walk::pump({}, [] { return false; }, std::chrono::milliseconds(100));
  CPPUNIT_ASSERT_EQUAL(0u, walk.replies_received());
}

void
TestDhtAnnounceWalk::test_late_reply_after_timeout() {
  FakeNode node(1);
  node.silent = true;
  Walk walk({&node});
  walk.announce();
  CPPUNIT_ASSERT(Walk::pump({&node}, [&] { return !node.queries.empty(); }, std::chrono::milliseconds(1000)));

  m_main_thread->test_add_cached_time(std::chrono::seconds(16));
  m_main_thread->test_process_events_without_cached_time();
  m_main_thread->test_add_cached_time(std::chrono::seconds(31));
  m_main_thread->test_process_events_without_cached_time();
  m_main_thread->test_add_cached_time(std::chrono::seconds(46));
  m_main_thread->test_process_events_without_cached_time();

  CPPUNIT_ASSERT(node.reply_to_query(node.queries.front()) > 0);
  Walk::pump({}, [] { return false; }, std::chrono::milliseconds(100));
  CPPUNIT_ASSERT_EQUAL(0u, walk.replies_received());
}
