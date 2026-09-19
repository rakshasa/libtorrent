#include "config.h"

#include "test_udp_router.h"

#include <random>
#include <type_traits>

#include "tracker/udp_router.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_udp_router, "tracker");

using namespace torrent::tracker;

namespace {

// Mirrors the construction in udp_router.cc, which is not reachable from here.
UdpRouter::random_engine
make_random_engine() {
  std::random_device rd;
  std::seed_seq      seed{rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd()};

  UdpRouter::random_engine engine;
  engine.seed(seed);

  return engine;
}

} // namespace

// Transaction ids are the only thing separating a tracker's own responses from
// responses forged for a different tracker, so one observed id must not give up
// the ids that follow it. That rules out any engine whose state is small enough
// to search.
void
test_udp_router::test_random_engine_state() {
  using base_type = std::remove_cvref_t<decltype(std::declval<UdpRouter::random_engine&>().base())>;

  CPPUNIT_ASSERT(base_type::state_size * base_type::word_size >= 256);
}

// Two routers in the same process must not issue the same sequence, which they
// would if the engine were seeded from a fixed or near-fixed value.
void
test_udp_router::test_random_engine_seeding() {
  auto first  = make_random_engine();
  auto second = make_random_engine();

  bool differs = false;

  for (int i = 0; i < 8; i++)
    differs = differs || first() != second();

  CPPUNIT_ASSERT(differs);
}
