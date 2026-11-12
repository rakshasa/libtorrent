#include "config.h"

#include "test/protocol/test_download_slot_rotation.h"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <set>
#include <vector>

#include "protocol/peer_connection_base.h"
#include "torrent/download/choke_queue.h"
#include "torrent/download/group_entry.h"

CPPUNIT_TEST_SUITE_REGISTRATION(test_download_slot_rotation);

using namespace std::chrono_literals;

namespace {

// A connection that is never opened: the rotation reads only its download rate and its download choke state.
class slot_connection : public torrent::PeerConnectionBase {
public:
  void initialize_custom() override {}
  void update_interested() override {}
  bool receive_keepalive() override { return true; }
  void event_read() override {}
  void event_write() override {}

  void received(uint32_t bytes) { m_peer_chunks.download_throttle()->rate()->insert(bytes); }

  // What receive_download_choke(true) leaves when the peer still unchokes us: NOT_INTERESTED on its way.
  void initalize_withhold_state() {
    m_down.mark_interested();
    m_down.withhold_interest_and_schedule();
  }

  void interested(bool v)       {
    if (v)
      m_down.mark_interested();
    else
      m_down.mark_not_interested();

    m_down.clear_pending_interest();
    m_down.clear_interest_withheld();
  }

  bool is_interested() const    { return m_down.is_interested(); }
  bool sends_interested() const { return m_down.has_pending_interest(); }

  // What the rig's slot does for a slot taken back from a peer that still unchokes us, as receive_download_choke does.
  void choked_by_queue() {
    if (!m_down.is_unchoked())
      return;

    m_down.withhold_interest_and_schedule();
  }
  void remote_unchoked()        { m_down.mark_unchoked(); m_down.mark_interested(); }
};

// The entry's list moves are for choke_queue and PeerConnectionBase; the test's slot does what
// PeerConnectionBase::receive_download_choke does with them.
class slot_entry : public torrent::group_entry {
public:
  using torrent::group_entry::connection_choked;
  using torrent::group_entry::connection_queued;
  using torrent::group_entry::connection_unchoked;
  using torrent::group_entry::connection_unqueued;
};

struct rig {
  explicit rig(uint32_t slots) {
    entry.set_max_slots(slots);
    queue.group_container().push_back(&entry);
    queue.set_heuristics(torrent::HEURISTICS_DOWNLOAD_LEECH);
    queue.set_slot_unchoke([this](int n) { unchoked_total += n; });
    queue.set_slot_can_unchoke([] { return 0; });
    queue.set_slot_connection([this](torrent::PeerConnectionBase* pcb, bool choke) {
      pcb->down_choke()->set_unchoked(!choke);
      pcb->down_choke()->set_time_last_choke(torrent::this_thread::cached_time());

      if (choke) {
        static_cast<slot_connection*>(pcb)->choked_by_queue();
        entry.connection_choked(pcb);
        entry.connection_queued(pcb);
        queue.modify_currently_unchoked(-1);
        queue.modify_currently_queued(1);
        choked.push_back(pcb);
      } else {
        entry.connection_unqueued(pcb);
        entry.connection_unchoked(pcb);
        queue.modify_currently_unchoked(1);
        queue.modify_currently_queued(-1);
        unchoked.push_back(pcb);
      }
      return true;
    });
  }

  ~rig() {
    // Empty the counters choke_queue's destructor checks.
    queue.modify_currently_unchoked(-static_cast<int>(queue.size_unchoked()));
    queue.modify_currently_queued(-static_cast<int>(queue.size_queued()));
  }

  // A slot holder: unchoked `held` ago, `bytes` received in this second.
  slot_connection* holder(uint32_t bytes, std::chrono::microseconds held) {
    auto c = add();
    c->remote_unchoked();
    c->received(bytes);
    c->down_choke()->set_queued(true);
    c->down_choke()->set_unchoked(true);
    c->down_choke()->set_time_last_choke(torrent::this_thread::cached_time() - held);
    entry.connection_unchoked(c);
    queue.modify_currently_unchoked(1);
    return c;
  }

  // A peer that unchoked us and waits for a slot; `last` is when it last got or lost one, 0 for never.
  slot_connection* waiting(std::chrono::microseconds last) {
    auto c = add();
    c->remote_unchoked();
    c->down_choke()->set_queued(true);
    c->down_choke()->set_time_last_choke(last);
    entry.connection_queued(c);
    queue.modify_currently_queued(1);
    return c;
  }

  bool holds(torrent::PeerConnectionBase* c) {
    return std::find(entry.unchoked()->begin(), entry.unchoked()->end(), c) != entry.unchoked()->end();
  }

  slot_connection* add() {
    connections.push_back(std::make_unique<slot_connection>());
    connections.back()->down_choke()->set_entry(&entry);
    return connections.back().get();
  }

  slot_entry                                    entry;
  torrent::choke_queue                          queue;
  std::vector<std::unique_ptr<slot_connection>> connections;
  std::vector<torrent::PeerConnectionBase*>     choked;
  std::vector<torrent::PeerConnectionBase*>     unchoked;
  int                                           unchoked_total{};
};

} // namespace

void
test_download_slot_rotation::test_count_follows_max_alternate() {
  CPPUNIT_ASSERT(torrent::choke_queue::rotate_count(1) == 1);
  CPPUNIT_ASSERT(torrent::choke_queue::rotate_count(8) == 1);
  CPPUNIT_ASSERT(torrent::choke_queue::rotate_count(15) == 2);
  CPPUNIT_ASSERT(torrent::choke_queue::rotate_count(25) == 4);
  CPPUNIT_ASSERT(torrent::choke_queue::rotate_count(30) == 4);
  CPPUNIT_ASSERT(torrent::choke_queue::rotate_count(50) == 5);
  CPPUNIT_ASSERT(torrent::choke_queue::rotate_count(100) == 10);
}

void
test_download_slot_rotation::test_cycle_rotates_full_torrent() {
  m_main_thread->test_set_cached_time(10000s);

  rig t(9);  // rotate_count(9) == 2

  std::vector<slot_connection*> holders;

  for (uint32_t i = 0; i < 7; i++)
    holders.push_back(t.holder(64 * 1024 * (i + 1), 120s));

  auto silent = t.holder(0, 120s);      // unchoked us, sends nothing
  auto fresh  = t.holder(0, 5s);        // got its slot 5 s ago: spared
  auto old    = t.waiting(9000s);       // lost a slot at 9000 s
  auto never  = t.waiting(0s);          // never had one
  auto newer  = t.waiting(9900s);       // lost one at 9900 s

  CPPUNIT_ASSERT(t.queue.cycle(~uint32_t()) == 0);

  CPPUNIT_ASSERT(!t.holds(silent) && !t.holds(holders[0]));  // the two slowest that were measured
  CPPUNIT_ASSERT(t.holds(fresh));
  CPPUNIT_ASSERT(t.holds(never) && t.holds(old) && !t.holds(newer));
  CPPUNIT_ASSERT(t.entry.unchoked()->size() == 9);
  CPPUNIT_ASSERT(t.entry.queued()->size() == 3);
  CPPUNIT_ASSERT(t.choked.size() == 2 && t.unchoked.size() == 2);
  CPPUNIT_ASSERT(t.choked[0] == silent);                     // the slots went back first

  // Still interested in the two: they wait in the queue, no NOT_INTERESTED goes out.
  CPPUNIT_ASSERT(silent->is_interested() && !silent->sends_interested() && !silent->down().is_interest_withheld());
  CPPUNIT_ASSERT(holders[0]->is_interested() && !holders[0]->sends_interested());
}

void
test_download_slot_rotation::test_cycle_limited_by_the_queue() {
  m_main_thread->test_set_cached_time(10000s);

  rig t(4);  // rotate_count(4) == 1, and one waits

  auto a = t.holder(4000, 120s);
  auto b = t.holder(3000, 120s);
  auto c = t.holder(2000, 120s);
  auto d = t.holder(1000, 120s);
  auto w = t.waiting(0s);

  t.queue.cycle(~uint32_t());

  CPPUNIT_ASSERT(t.holds(a) && t.holds(b) && t.holds(c) && t.holds(w) && !t.holds(d));
  CPPUNIT_ASSERT(t.choked.size() == 1 && t.unchoked.size() == 1);
}

void
test_download_slot_rotation::test_cycle_spares_the_newcomer() {
  m_main_thread->test_set_cached_time(10000s);

  rig t(2);  // rotate_count(2) == 1

  auto a = t.holder(1000, 120s);
  auto b = t.holder(10, 120s);
  auto w = t.waiting(0s);

  t.queue.cycle(~uint32_t());

  CPPUNIT_ASSERT(t.holds(a) && t.holds(w) && !t.holds(b));

  // The next cycle 20 s later: the newcomer is inside its grace and keeps its slot; the other measured holder goes, and
  // the peer whose slot was taken back 20 s ago has waited longest.
  m_main_thread->test_set_cached_time(10020s);
  t.queue.cycle(~uint32_t());

  CPPUNIT_ASSERT(t.holds(w) && t.holds(b) && !t.holds(a));
}

void
test_download_slot_rotation::test_cycle_picks_the_oldest() {
  m_main_thread->test_set_cached_time(10000s);

  // Wherever the random place falls, the same four, oldest first. The queue holds the waits out of order.
  for (unsigned int seed = 1; seed <= 64; seed++) {
    ::srandom(seed);

    rig t(30);  // rotate_count(30) == 4

    for (uint32_t i = 0; i < 30; i++)
      t.holder(64 * 1024 * (i + 1), 120s);

    std::vector<slot_connection*> waiting;

    for (uint32_t i = 0; i < 40; i++)
      waiting.push_back(t.waiting(std::chrono::seconds{1000 + (i * 7) % 40}));

    auto never = t.waiting(0s);

    t.queue.cycle(~uint32_t());

    CPPUNIT_ASSERT(t.unchoked.size() == 4);
    CPPUNIT_ASSERT(t.unchoked[0] == never);

    // (i * 7) % 40 is 0, 1 and 2 at i = 0, 23 and 6.
    CPPUNIT_ASSERT(t.unchoked[1] == waiting[0]);
    CPPUNIT_ASSERT(t.unchoked[2] == waiting[23]);
    CPPUNIT_ASSERT(t.unchoked[3] == waiting[6]);
  }
}

void
test_download_slot_rotation::test_cycle_never_picks_back_the_slots_it_took() {
  m_main_thread->test_set_cached_time(10000s);

  // Two wait, two go out: wherever the list starts, even on the two just taken out, the two that waited get the slots.
  for (unsigned int seed = 1; seed <= 64; seed++) {
    ::srandom(seed);

    rig t(9);  // rotate_count(9) == 2

    for (uint32_t i = 0; i < 9; i++)
      t.holder(64 * 1024 * (i + 1), 120s);

    auto a = t.waiting(9000s);
    auto b = t.waiting(0s);

    t.queue.cycle(~uint32_t());

    CPPUNIT_ASSERT(t.unchoked.size() == 2);
    CPPUNIT_ASSERT(t.unchoked[0] == b && t.unchoked[1] == a);
    CPPUNIT_ASSERT(t.entry.unchoked()->size() == 9 && t.entry.queued()->size() == 2);
  }
}

void
test_download_slot_rotation::test_cycle_equal_waits_start_at_a_random_place() {
  m_main_thread->test_set_cached_time(10000s);

  // All never had a slot: over the seeds each is picked at some time, the last to come too.
  std::set<size_t> picked;

  for (unsigned int seed = 1; seed <= 256; seed++) {
    ::srandom(seed);

    rig t(2);  // rotate_count(2) == 1

    t.holder(1000, 120s);
    t.holder(10, 120s);

    std::vector<torrent::PeerConnectionBase*> waiting;

    for (uint32_t i = 0; i < 8; i++)
      waiting.push_back(t.waiting(0s));

    t.queue.cycle(~uint32_t());

    CPPUNIT_ASSERT(t.unchoked.size() == 1);

    auto itr = std::find(waiting.begin(), waiting.end(), t.unchoked[0]);

    CPPUNIT_ASSERT(itr != waiting.end());
    picked.insert(std::distance(waiting.begin(), itr));
  }

  CPPUNIT_ASSERT(picked.size() == 8);
}

void
test_download_slot_rotation::test_cycle_all_holders_in_grace() {
  m_main_thread->test_set_cached_time(10000s);

  rig t(2);

  t.holder(1000, 5s);
  t.holder(10, 20s);
  t.waiting(0s);

  t.queue.cycle(~uint32_t());

  CPPUNIT_ASSERT(t.choked.empty() && t.unchoked.empty());
}

void
test_download_slot_rotation::test_free_slot_is_left_to_the_cycle() {
  m_main_thread->test_set_cached_time(10000s);

  rig t(9);

  for (uint32_t i = 0; i < 8; i++)
    t.holder(0, 120s);

  auto w = t.waiting(0s);

  // A free slot: the cycle hands it out as before and takes none back.
  t.queue.cycle(~uint32_t());

  CPPUNIT_ASSERT(t.holds(w));
  CPPUNIT_ASSERT(t.choked.empty() && t.unchoked.size() == 1);
}

void
test_download_slot_rotation::test_unlimited_is_left_alone() {
  m_main_thread->test_set_cached_time(10000s);

  rig t(torrent::group_entry::unlimited);

  auto h = t.holder(0, 120s);
  auto w = t.waiting(0s);

  t.queue.cycle(~uint32_t());

  CPPUNIT_ASSERT(t.holds(h) && t.holds(w));
  CPPUNIT_ASSERT(t.choked.empty());
}

void
test_download_slot_rotation::test_nothing_waiting() {
  m_main_thread->test_set_cached_time(10000s);

  rig t(2);

  t.holder(0, 120s);
  t.holder(0, 120s);

  t.queue.cycle(~uint32_t());

  CPPUNIT_ASSERT(t.choked.empty() && t.unchoked.empty());
}

void
test_download_slot_rotation::test_balance_does_not_rotate() {
  m_main_thread->test_set_cached_time(10000s);

  rig t(2);

  t.holder(0, 120s);
  t.holder(0, 120s);
  t.waiting(0s);

  t.queue.balance();

  CPPUNIT_ASSERT(t.choked.empty() && t.unchoked.empty());
}

void
test_download_slot_rotation::test_interest_restored_after_remote_choke() {
  slot_connection c;
  c.initalize_withhold_state();

  CPPUNIT_ASSERT(c.down().is_interest_withheld());
  CPPUNIT_ASSERT(c.restore_down_interest(false));
  CPPUNIT_ASSERT(c.is_interested() && c.sends_interested());
  CPPUNIT_ASSERT(!c.down().is_interest_withheld());

  // Once only: another choke later finds nothing withheld.
  CPPUNIT_ASSERT(!c.restore_down_interest(false));
}

void
test_download_slot_rotation::test_interest_not_restored_when_done() {
  slot_connection c;
  c.initalize_withhold_state();

  CPPUNIT_ASSERT(!c.restore_down_interest(true));
  CPPUNIT_ASSERT(!c.is_interested());
  CPPUNIT_ASSERT(!c.down().is_interest_withheld());
}

void
test_download_slot_rotation::test_interest_kept_after_rotation() {
  slot_connection c;
  c.initalize_withhold_state();
  c.keep_down_interest();

  CPPUNIT_ASSERT(c.is_interested() && !c.sends_interested() && !c.down().is_interest_withheld());

  // A peer we are not interested in for its pieces stays so.
  slot_connection d;
  d.interested(false);
  d.keep_down_interest();

  CPPUNIT_ASSERT(!d.is_interested() && !d.sends_interested());
}

void
test_download_slot_rotation::test_interest_untouched_when_not_withheld() {
  slot_connection c;
  c.interested(false);  // not interested because it has nothing we want

  CPPUNIT_ASSERT(!c.restore_down_interest(false));
  CPPUNIT_ASSERT(!c.is_interested() && !c.sends_interested());
}

void
test_download_slot_rotation::test_interest_notification_transitions() {
  torrent::protocol::StateDown state;

  state.mark_interested_and_schedule_if_changed();
  CPPUNIT_ASSERT(state.is_interested() && state.has_pending_interest());

  state.mark_interested_and_schedule_if_changed();
  CPPUNIT_ASSERT(state.is_interested() && !state.has_pending_interest());

  state.mark_interested_and_schedule();
  state.withhold_interest_and_schedule();
  CPPUNIT_ASSERT(!state.is_interested() && state.is_interest_withheld() && state.has_pending_interest());

  state.withhold_interest_and_schedule();
  CPPUNIT_ASSERT(!state.is_interested() && !state.is_interest_withheld() && !state.has_pending_interest());

  state.mark_not_interested_and_schedule();
  CPPUNIT_ASSERT(!state.is_interested() && state.has_pending_interest());
}
