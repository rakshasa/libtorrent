#include "config.h"

#include "test/protocol/test_idle_peers.h"

#include <arpa/inet.h>
#include <netinet/in.h>

#include "download/available_list.h"
#include "download/download_main.h"
#include "download/stall_watch.h"
#include "protocol/peer_connection_state.h"
#include "torrent/download_info.h"
#include "torrent/peer/connection_list.h"
#include "torrent/peer/peer_info.h"
#include "torrent/peer/peer_list.h"

CPPUNIT_TEST_SUITE_REGISTRATION(test_idle_peers);

using namespace std::chrono_literals;

namespace {

// One connection's idle clock, on its own state: what a test sets is what the keepalive pass reads.
struct idle_clock {
  torrent::protocol::StateUp   up;
  torrent::protocol::StateDown down;
  uint64_t                     down_total{};
  uint64_t                     up_total{};

  // A keepalive pass at `now`.
  std::chrono::seconds look(std::chrono::seconds now) {
    down.update_idle_time(up, down_total, up_total, now);
    return down.idle_time();
  }

  bool check(std::chrono::seconds now, std::chrono::seconds limit) {
    look(now);
    return down.is_idle_for(limit);
  }

  // The first pass, with the state as set.
  void start(std::chrono::seconds now) { look(now); }

  void state(bool interested, bool remote_unchoked, bool local_unchoked) {
    if (interested)
      down.mark_interested();
    else
      down.mark_not_interested();

    if (!remote_unchoked)
      down.mark_choked();
    else if (!down.is_unchoked())
      down.mark_unchoked(); // what the UNCHOKE message handler calls

    down.choke.set_unchoked(local_unchoked);
  }

  void wants_ours(bool v) { up.choke.set_queued(v); }

  // It unchoked us and choked us again between two passes.
  void unchoke_received() {
    down.mark_unchoked();
    down.mark_choked();
  }

  void received(uint32_t bytes) { down_total += bytes; }
  void sent(uint32_t bytes) { up_total += bytes; }
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
  idle_clock c;
  c.state(true, false, false);
  c.start(1000s);

  CPPUNIT_ASSERT(c.look(1120s) == 120s);
  CPPUNIT_ASSERT(c.look(1240s) == 240s);
  CPPUNIT_ASSERT(c.look(1360s) == 360s);
  CPPUNIT_ASSERT(c.down.idle_time() >= timeout);
}

void
test_idle_peers::test_clock_counts_unchoked_silent() {
  idle_clock c;
  c.state(true, true, true); // it unchoked us and we ask it: it answers nothing
  c.start(1000s);

  CPPUNIT_ASSERT(c.look(1240s) == 240s);
  CPPUNIT_ASSERT(c.look(1360s) >= timeout);
}

void
test_idle_peers::test_clock_reset_by_piece_data() {
  idle_clock c;
  c.state(true, false, false);
  c.start(1000s);

  CPPUNIT_ASSERT(c.look(1240s) == 240s);

  c.received(16384);

  CPPUNIT_ASSERT(c.look(1360s) == 0s);
  CPPUNIT_ASSERT(c.look(1480s) == 120s);
}

void
test_idle_peers::test_clock_reset_by_upload() {
  idle_clock c;
  c.state(true, false, false);
  c.start(1000s);

  CPPUNIT_ASSERT(c.look(1240s) == 240s);

  c.sent(16384); // it keeps us choked, but takes our pieces: ratio on a private tracker

  CPPUNIT_ASSERT(c.look(1360s) == 0s);
}

void
test_idle_peers::test_clock_reset_by_unchoke() {
  idle_clock c;
  c.state(true, false, false);
  c.start(1000s);

  CPPUNIT_ASSERT(c.look(1240s) == 240s);

  c.unchoke_received(); // a seed that rotates its slots unchoked us for a while and choked us again

  CPPUNIT_ASSERT(c.look(1360s) == 0s);
  CPPUNIT_ASSERT(c.look(1480s) == 120s);
}

void
test_idle_peers::test_clock_reset_when_not_interested() {
  idle_clock c;
  c.state(true, false, false);
  c.start(1000s);

  CPPUNIT_ASSERT(c.look(1240s) == 240s);

  c.state(false, false, false); // it has nothing we want

  CPPUNIT_ASSERT(c.look(1360s) == 0s);
  CPPUNIT_ASSERT(c.look(1480s) == 0s);
}

void
test_idle_peers::test_clock_paused_while_our_slots_are_full() {
  idle_clock c;
  c.state(true, false, false);
  c.start(1000s);

  CPPUNIT_ASSERT(c.look(1120s) == 120s);

  c.state(true, true, false); // it unchoked us, our download slots are taken: the wait is ours

  CPPUNIT_ASSERT(c.look(1240s) == 0s); // the unchoke
  CPPUNIT_ASSERT(c.look(1960s) == 0s);

  c.state(true, true, true); // now we ask it, and it sends nothing

  CPPUNIT_ASSERT(c.look(2080s) == 0s); // the wait began after the last look
  CPPUNIT_ASSERT(c.look(2200s) == 120s);
}

void
test_idle_peers::test_clock_paused_while_it_wants_our_pieces() {
  idle_clock c;
  c.state(true, false, false);
  c.start(1000s);

  CPPUNIT_ASSERT(c.look(1120s) == 120s);

  c.wants_ours(true); // a leecher that keeps us choked and waits for our upload slots: ratio to be had

  CPPUNIT_ASSERT(c.look(1240s) == 120s);
  CPPUNIT_ASSERT(c.look(2200s) == 120s);

  c.wants_ours(false);

  CPPUNIT_ASSERT(c.look(2320s) == 120s);
  CPPUNIT_ASSERT(c.look(2440s) == 240s);
}

void
test_idle_peers::test_clock_skips_interval_before_wait() {
  constexpr auto stalled = torrent::DownloadMain::idle_peer_timeout_stalled;

  idle_clock     c;
  c.state(false, false, false);
  c.start(1000s);
  CPPUNIT_ASSERT(!c.check(1120s, stalled));

  c.state(true, false, false); // it got a piece we want just before the look
  CPPUNIT_ASSERT(!c.check(1240s, stalled));
  CPPUNIT_ASSERT(c.down.idle_time() == 0s);
  CPPUNIT_ASSERT(c.check(1360s, stalled));
}

void
test_idle_peers::test_closes_itself_at_limit() {
  idle_clock c;
  c.state(true, false, false);
  c.start(1000s);

  CPPUNIT_ASSERT(!c.check(1120s, timeout));
  CPPUNIT_ASSERT(!c.check(1240s, timeout));
  CPPUNIT_ASSERT(c.check(1360s, timeout));
}

void
test_idle_peers::test_zero_limit_closes_nothing() {
  idle_clock c;
  c.state(true, false, false);
  c.start(1000s);

  CPPUNIT_ASSERT(!c.check(5000s, 0s));
  CPPUNIT_ASSERT(c.down.idle_time() == 4000s); // the clock still runs, so a later limit sees the real idle time
  CPPUNIT_ASSERT(c.check(5120s, timeout));
}

void
test_idle_peers::test_limit_from_download_state() {
  using torrent::DownloadMain;

  CPPUNIT_ASSERT(DownloadMain::idle_peer_limit_for(true, false, 50, 50) == DownloadMain::idle_peer_timeout);
  CPPUNIT_ASSERT(DownloadMain::idle_peer_limit_for(true, true, 120, 50) == DownloadMain::idle_peer_timeout_stalled);

  // below min the download dials by itself; stopped, done or fetching metadata: nothing is closed
  CPPUNIT_ASSERT(DownloadMain::idle_peer_limit_for(true, true, 49, 50) == 0s);
  CPPUNIT_ASSERT(DownloadMain::idle_peer_limit_for(false, false, 120, 50) == 0s);
}

void
test_idle_peers::test_idle_close_not_redialled() {
  torrent::DownloadInfo info;
  peer_list             list;
  list.set_info(&info);

  CPPUNIT_ASSERT(!torrent::ConnectionList::redial_wanted(torrent::ConnectionList::disconnect_idle, true, false, 16384));

  auto a    = address("198.18.1.1", 40001);
  auto peer = list.connected(reinterpret_cast<sockaddr*>(&a), 0); // we dialled it
  CPPUNIT_ASSERT(peer != nullptr);

  list.disconnected(peer, torrent::PeerList::disconnect_set_time | torrent::PeerList::disconnect_idle);
  CPPUNIT_ASSERT(list.available_list()->empty());
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

constexpr uint64_t KiB  = 1024;
constexpr uint64_t tick = 30;

// Feeds `ticks` ticks at `rate` bytes a second; returns whether the last one left the download stalled.
bool
feed(TestMainThread* thread, torrent::StallWatch& w, uint64_t& total, uint64_t rate, int ticks) {
  bool stalled = false;
  for (int i = 0; i != ticks; i++) {
    thread->test_add_cached_time(std::chrono::seconds(tick));
    total += rate * tick;
    stalled = w.update(total);
  }
  return stalled;
}

} // namespace

void
test_idle_peers::test_stall_steady_rate() {
  torrent::StallWatch w;
  uint64_t            total = 0;

  w.update(total);
  CPPUNIT_ASSERT(!feed(m_main_thread.get(), w, total, 1024 * KiB, 30));
}

void
test_idle_peers::test_stall_nothing_received() {
  torrent::StallWatch w;
  uint64_t            total = 0;

  w.update(total);                                            // the first look only primes
  CPPUNIT_ASSERT(!feed(m_main_thread.get(), w, total, 0, 1)); // one starved tick is not a stall
  CPPUNIT_ASSERT(feed(m_main_thread.get(), w, total, 0, 1));  // two in a row are
}

void
test_idle_peers::test_stall_far_below_average() {
  torrent::StallWatch w;
  uint64_t            total = 0;

  w.update(total);
  CPPUNIT_ASSERT(!feed(m_main_thread.get(), w, total, 4096 * KiB, 20));
  CPPUNIT_ASSERT(!feed(m_main_thread.get(), w, total, 300 * KiB, 1)); // under a tenth of the average, once
  CPPUNIT_ASSERT(feed(m_main_thread.get(), w, total, 300 * KiB, 1));  // and again: stalled though far above the floor
  CPPUNIT_ASSERT(!feed(m_main_thread.get(), w, total, 500 * KiB, 1)); // a tenth or more ends it
}

void
test_idle_peers::test_stall_one_bad_tick() {
  torrent::StallWatch w;
  uint64_t            total = 0;

  w.update(total);
  CPPUNIT_ASSERT(!feed(m_main_thread.get(), w, total, 1024 * KiB, 3));
  CPPUNIT_ASSERT(!feed(m_main_thread.get(), w, total, 0, 1));
  CPPUNIT_ASSERT(!feed(m_main_thread.get(), w, total, 1024 * KiB, 1));
  CPPUNIT_ASSERT(!feed(m_main_thread.get(), w, total, 0, 1));
}

void
test_idle_peers::test_stall_slow_but_steady() {
  torrent::StallWatch w;
  uint64_t            total = 0;

  w.update(total);
  CPPUNIT_ASSERT(!feed(m_main_thread.get(), w, total, 3 * KiB, 30)); // above 2 KiB/s and at its own average
  CPPUNIT_ASSERT(feed(m_main_thread.get(), w, total, 1 * KiB, 2));   // under 2 KiB/s
}

void
test_idle_peers::test_stall_forgets_old_rate() {
  torrent::StallWatch w;
  uint64_t            total = 0;

  w.update(total);
  CPPUNIT_ASSERT(!feed(m_main_thread.get(), w, total, 4096 * KiB, 20));
  CPPUNIT_ASSERT(feed(m_main_thread.get(), w, total, 100 * KiB, 2));   // a fortieth of the average
  CPPUNIT_ASSERT(feed(m_main_thread.get(), w, total, 100 * KiB, 10));  // the fast ticks still weigh in the ten minutes
  CPPUNIT_ASSERT(!feed(m_main_thread.get(), w, total, 100 * KiB, 10)); // gone: 100 KiB/s is the download's own rate
}

void
test_idle_peers::test_stall_fast_tick() {
  torrent::StallWatch w;
  uint64_t            total = 0;

  // 30 GiB in one tick is over Rate::insert()'s bound in bytes, not in KiB.
  w.update(total);
  CPPUNIT_ASSERT(!feed(m_main_thread.get(), w, total, 1024 * 1024 * KiB, 2));
  CPPUNIT_ASSERT(!feed(m_main_thread.get(), w, total, 1024 * 1024 * KiB, 1));
}
