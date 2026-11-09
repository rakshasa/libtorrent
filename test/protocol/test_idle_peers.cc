#include "config.h"

#include "test/protocol/test_idle_peers.h"

#include <netinet/in.h>
#include <arpa/inet.h>

#include "download/download_main.h"
#include "protocol/peer_connection_base.h"
#include "torrent/download_info.h"
#include "torrent/peer/peer_info.h"
#include "torrent/peer/peer_list.h"

CPPUNIT_TEST_SUITE_REGISTRATION(test_idle_peers);

using namespace std::chrono_literals;

namespace {

// A connection that is never opened: the idle clock reads only its own state, which a test sets directly.
class idle_connection : public torrent::PeerConnectionBase {
public:
  void initialize_custom() override {}
  void update_interested() override {}
  bool receive_keepalive() override { return true; }
  void event_read() override {}
  void event_write() override {}

  void start(std::chrono::seconds now) { m_idle_seen = now; }

  void state(bool interested, bool remote_unchoked, bool local_unchoked) {
    m_down_interested = interested;
    m_down_unchoked   = remote_unchoked;
    m_down_choke.set_unchoked(local_unchoked);
  }

  void wants_ours(bool v) { m_up_choke.set_queued(v); }
  void unchoke_received()  { m_idle_unchoked = true; }  // what the UNCHOKE message handler sets

  void received(uint32_t bytes) { m_peer_chunks.download_throttle()->rate()->insert(bytes); }
  void sent(uint32_t bytes)     { m_peer_chunks.upload_throttle()->rate()->insert(bytes); }
};

// PeerList's connect and disconnect calls are for libtorrent's own classes; a test reaches them through a subclass.
class peer_list : public torrent::PeerList {
public:
  using torrent::PeerList::connected;
  using torrent::PeerList::disconnected;
  using torrent::PeerList::set_info;
};

sockaddr_in
address(const char* ip, uint16_t port) {
  sockaddr_in sa{};
  sa.sin_family = AF_INET;
  sa.sin_port   = htons(port);
  inet_pton(AF_INET, ip, &sa.sin_addr);
  return sa;
}

constexpr auto timeout = torrent::DownloadMain::idle_peer_timeout;

} // namespace

void
test_idle_peers::test_clock_counts_while_choked() {
  idle_connection c;
  c.start(1000s);
  c.state(true, false, false);

  CPPUNIT_ASSERT(c.update_idle_time(1120s) == 120s);
  CPPUNIT_ASSERT(c.update_idle_time(1240s) == 240s);
  CPPUNIT_ASSERT(c.update_idle_time(1360s) == 360s);
  CPPUNIT_ASSERT(c.idle_time() >= timeout);
}

void
test_idle_peers::test_clock_counts_unchoked_silent() {
  idle_connection c;
  c.start(1000s);
  c.state(true, true, true);  // it unchoked us and we ask it: it answers nothing

  CPPUNIT_ASSERT(c.update_idle_time(1240s) == 240s);
  CPPUNIT_ASSERT(c.update_idle_time(1360s) >= timeout);
}

void
test_idle_peers::test_clock_reset_by_piece_data() {
  idle_connection c;
  c.start(1000s);
  c.state(true, false, false);

  CPPUNIT_ASSERT(c.update_idle_time(1240s) == 240s);

  c.received(16384);

  CPPUNIT_ASSERT(c.update_idle_time(1360s) == 0s);
  CPPUNIT_ASSERT(c.update_idle_time(1480s) == 120s);
}

void
test_idle_peers::test_clock_reset_by_upload() {
  idle_connection c;
  c.start(1000s);
  c.state(true, false, false);

  CPPUNIT_ASSERT(c.update_idle_time(1240s) == 240s);

  c.sent(16384);  // it keeps us choked, but takes our pieces: ratio on a private tracker

  CPPUNIT_ASSERT(c.update_idle_time(1360s) == 0s);
}

void
test_idle_peers::test_clock_reset_by_unchoke() {
  idle_connection c;
  c.start(1000s);
  c.state(true, false, false);

  CPPUNIT_ASSERT(c.update_idle_time(1240s) == 240s);

  c.unchoke_received();  // a seed that rotates its slots unchoked us for a while and choked us again

  CPPUNIT_ASSERT(c.update_idle_time(1360s) == 0s);
  CPPUNIT_ASSERT(c.update_idle_time(1480s) == 120s);
}

void
test_idle_peers::test_clock_reset_when_not_interested() {
  idle_connection c;
  c.start(1000s);
  c.state(true, false, false);

  CPPUNIT_ASSERT(c.update_idle_time(1240s) == 240s);

  c.state(false, false, false);  // it has nothing we want

  CPPUNIT_ASSERT(c.update_idle_time(1360s) == 0s);
  CPPUNIT_ASSERT(c.update_idle_time(1480s) == 0s);
}

void
test_idle_peers::test_clock_paused_while_our_slots_are_full() {
  idle_connection c;
  c.start(1000s);
  c.state(true, false, false);

  CPPUNIT_ASSERT(c.update_idle_time(1120s) == 120s);

  c.state(true, true, false);  // it unchoked us, our download slots are taken: the wait is ours

  CPPUNIT_ASSERT(c.update_idle_time(1240s) == 120s);
  CPPUNIT_ASSERT(c.update_idle_time(1960s) == 120s);

  c.state(true, true, true);  // now we ask it, and it sends nothing

  CPPUNIT_ASSERT(c.update_idle_time(2080s) == 240s);
}

void
test_idle_peers::test_clock_paused_while_it_wants_our_pieces() {
  idle_connection c;
  c.start(1000s);
  c.state(true, false, false);

  CPPUNIT_ASSERT(c.update_idle_time(1120s) == 120s);

  c.wants_ours(true);  // a leecher that keeps us choked and waits for our upload slots: ratio to be had

  CPPUNIT_ASSERT(c.update_idle_time(1240s) == 120s);
  CPPUNIT_ASSERT(c.update_idle_time(2200s) == 120s);

  c.wants_ours(false);

  CPPUNIT_ASSERT(c.update_idle_time(2320s) == 240s);
}

void
test_idle_peers::test_idle_drop_refused_within_window() {
  torrent::DownloadInfo info;
  peer_list             list;
  list.set_info(&info);

  auto first = address("198.18.1.1", 40001);
  auto peer  = list.connected(reinterpret_cast<sockaddr*>(&first), torrent::PeerList::connect_incoming);
  CPPUNIT_ASSERT(peer != nullptr);

  list.disconnected(peer, torrent::PeerList::disconnect_set_time | torrent::PeerList::disconnect_idle);
  CPPUNIT_ASSERT(peer->is_idle_dropped());

  // the same address from another port (an incoming connection) and on its listening port (one we dial)
  auto again = address("198.18.1.1", 40002);
  CPPUNIT_ASSERT(list.connected(reinterpret_cast<sockaddr*>(&again), torrent::PeerList::connect_incoming) == nullptr);
  CPPUNIT_ASSERT(list.connected(reinterpret_cast<sockaddr*>(&first), 0) == nullptr);

  // another address is not affected
  auto other = address("198.18.1.2", 40001);
  CPPUNIT_ASSERT(list.connected(reinterpret_cast<sockaddr*>(&other), torrent::PeerList::connect_incoming) != nullptr);

  m_main_thread->test_add_cached_time(std::chrono::seconds(torrent::PeerList::idle_refuse_seconds - 1));
  CPPUNIT_ASSERT(list.connected(reinterpret_cast<sockaddr*>(&again), torrent::PeerList::connect_incoming) == nullptr);

  m_main_thread->test_add_cached_time(1s);
  auto back = list.connected(reinterpret_cast<sockaddr*>(&again), torrent::PeerList::connect_incoming);
  CPPUNIT_ASSERT(back == peer);
  CPPUNIT_ASSERT(!back->is_idle_dropped());
}

void
test_idle_peers::test_plain_disconnect_not_refused() {
  torrent::DownloadInfo info;
  peer_list             list;
  list.set_info(&info);

  auto first = address("198.18.1.1", 40001);
  auto peer  = list.connected(reinterpret_cast<sockaddr*>(&first), torrent::PeerList::connect_incoming);
  CPPUNIT_ASSERT(peer != nullptr);

  list.disconnected(peer, torrent::PeerList::disconnect_set_time);
  CPPUNIT_ASSERT(!peer->is_idle_dropped());

  auto again = address("198.18.1.1", 40002);
  CPPUNIT_ASSERT(list.connected(reinterpret_cast<sockaddr*>(&again), torrent::PeerList::connect_incoming) == peer);
}

// StallWatch: the download's received total, read once every 30 seconds.

namespace {

constexpr uint64_t KiB = 1024;
constexpr uint64_t tick = 30;

// Feeds `ticks` ticks at `rate` bytes a second; returns whether the last one left the download stalled.
bool
feed(torrent::StallWatch& w, uint64_t& total, uint64_t rate, int ticks) {
  bool stalled = false;
  for (int i = 0; i != ticks; i++) {
    total += rate * tick;
    stalled = w.update(total);
  }
  return stalled;
}

} // namespace

void
test_idle_peers::test_stall_steady_rate() {
  torrent::StallWatch w;
  uint64_t total = 0;

  w.update(total);
  CPPUNIT_ASSERT(!feed(w, total, 1024 * KiB, 30));
}

void
test_idle_peers::test_stall_nothing_received() {
  torrent::StallWatch w;
  uint64_t total = 0;

  w.update(total);                                  // the first look only primes
  CPPUNIT_ASSERT(!feed(w, total, 0, 1));            // one starved tick is not a stall
  CPPUNIT_ASSERT(feed(w, total, 0, 1));             // two in a row are
}

void
test_idle_peers::test_stall_far_below_best() {
  torrent::StallWatch w;
  uint64_t total = 0;

  w.update(total);
  CPPUNIT_ASSERT(!feed(w, total, 4096 * KiB, 4));
  CPPUNIT_ASSERT(!feed(w, total, 300 * KiB, 1));    // under a tenth of the best tick, once
  CPPUNIT_ASSERT(feed(w, total, 300 * KiB, 1));     // and again: stalled though far above the floor
  CPPUNIT_ASSERT(!feed(w, total, 500 * KiB, 1));    // a tenth or more ends it
}

void
test_idle_peers::test_stall_one_bad_tick() {
  torrent::StallWatch w;
  uint64_t total = 0;

  w.update(total);
  CPPUNIT_ASSERT(!feed(w, total, 1024 * KiB, 3));
  CPPUNIT_ASSERT(!feed(w, total, 0, 1));
  CPPUNIT_ASSERT(!feed(w, total, 1024 * KiB, 1));
  CPPUNIT_ASSERT(!feed(w, total, 0, 1));
}

void
test_idle_peers::test_stall_slow_but_steady() {
  torrent::StallWatch w;
  uint64_t total = 0;

  w.update(total);
  CPPUNIT_ASSERT(!feed(w, total, 3 * KiB, 30));     // above 2 KiB/s and at its own best: never stalled
  CPPUNIT_ASSERT(feed(w, total, 1 * KiB, 2));       // under 2 KiB/s
}

void
test_idle_peers::test_stall_forgets_old_best() {
  torrent::StallWatch w;
  uint64_t total = 0;

  w.update(total);
  CPPUNIT_ASSERT(!feed(w, total, 4096 * KiB, 1));
  CPPUNIT_ASSERT(feed(w, total, 100 * KiB, 2));     // a fortieth of the best tick
  CPPUNIT_ASSERT(feed(w, total, 100 * KiB, 18));    // the best tick is still among the last 20 looked at
  CPPUNIT_ASSERT(!feed(w, total, 100 * KiB, 1));    // 20 ticks later it is gone: 100 KiB/s is the download's own rate
}
