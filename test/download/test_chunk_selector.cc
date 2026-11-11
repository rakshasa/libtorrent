#include "config.h"

#include "test_chunk_selector.h"

#include <cstdlib>
#include <utility>

#include "download/chunk_selector.h"
#include "download/chunk_statistics.h"
#include "protocol/peer_chunks.h"

CPPUNIT_TEST_SUITE_REGISTRATION(test_chunk_selector);

namespace torrent {

// Exposes the protected setters used by DownloadWrapper.
class test_download_data : public download_data {
public:
  using download_data::mutable_completed_bitfield;
  using download_data::mutable_high_priority;
  using download_data::mutable_normal_priority;
};

} // namespace torrent

namespace {

using ratio_type = torrent::ChunkSelector::priority_ratio;

constexpr uint32_t size_chunks = 50000;
constexpr uint32_t window_last = 5000;
constexpr uint32_t picks       = 3000;

ratio_type
make_ratio(uint32_t high, uint32_t normal) {
  return {high, normal};
}

// Chunks [0, high_last) are high priority, the rest normal priority. The last 'completed'
// chunks are already downloaded. The peer has every chunk but the last one, so it is not a
// seeder and uses its own download cache.
//
// Returns the number of high and normal priority chunks picked.
std::pair<uint32_t, uint32_t>
pick_chunks(bool window, uint32_t high_last, uint32_t completed = 0) {
  ::srandom(1);

  torrent::test_download_data data;
  data.mutable_completed_bitfield()->set_size_bits(size_chunks);
  data.mutable_completed_bitfield()->allocate();
  data.mutable_completed_bitfield()->unset_all();

  for (uint32_t i = size_chunks - completed; i < size_chunks; i++)
    data.mutable_completed_bitfield()->set(i);

  data.mutable_high_priority()->insert(0, high_last);
  data.mutable_normal_priority()->insert(high_last, size_chunks);

  torrent::ChunkStatistics statistics;
  statistics.initialize(size_chunks);

  torrent::ChunkSelector selector(&data);
  selector.initialize(&statistics);
  selector.set_window_ratio(window);
  selector.update_priorities();

  torrent::PeerChunks peer_chunks;
  peer_chunks.bitfield()->set_size_bits(size_chunks);
  peer_chunks.bitfield()->allocate();
  peer_chunks.bitfield()->set_all();
  peer_chunks.bitfield()->unset(size_chunks - 1);
  CPPUNIT_ASSERT(!peer_chunks.is_seeder());

  uint32_t high = 0;
  uint32_t normal = 0;

  for (uint32_t i = 0; i < picks; i++) {
    auto pick = selector.find(&peer_chunks);

    CPPUNIT_ASSERT(pick.index != torrent::ChunkSelector::invalid_chunk);
    CPPUNIT_ASSERT(pick.high == (pick.index < high_last));

    selector.using_index(pick.index);
    (pick.high ? high : normal)++;
  }

  selector.cleanup();
  return {high, normal};
}

// The ratio is best-effort: the peer's cached picks are used up before switching, so allow a
// small margin around the expected count, below the gap between two neighbouring ratios.
void
assert_ratio(std::pair<uint32_t, uint32_t> counts, ratio_type ratio) {
  uint32_t expected_normal = picks * ratio.normal / (ratio.high + ratio.normal);

  CPPUNIT_ASSERT(counts.first + counts.second == picks);
  CPPUNIT_ASSERT(counts.second + picks / 100 >= expected_normal);
  CPPUNIT_ASSERT(counts.second <= expected_normal + picks / 100);
}

} // namespace

void
test_chunk_selector::test_ratio_default() {
  torrent::ChunkSelector selector(nullptr);

  CPPUNIT_ASSERT(!selector.has_window_ratio());
  CPPUNIT_ASSERT(torrent::ChunkSelector::default_ratio == make_ratio(3, 1));

  assert_ratio(pick_chunks(false, size_chunks / 2), torrent::ChunkSelector::default_ratio);
}

void
test_chunk_selector::test_window_ratio() {
  using torrent::ChunkSelector;

  // Completed chunks over the whole torrent, in 20% steps.
  CPPUNIT_ASSERT(ChunkSelector::window_ratio(0, 100) == make_ratio(1, 5));
  CPPUNIT_ASSERT(ChunkSelector::window_ratio(19, 100) == make_ratio(1, 5));
  CPPUNIT_ASSERT(ChunkSelector::window_ratio(20, 100) == make_ratio(1, 4));
  CPPUNIT_ASSERT(ChunkSelector::window_ratio(39, 100) == make_ratio(1, 4));
  CPPUNIT_ASSERT(ChunkSelector::window_ratio(40, 100) == make_ratio(1, 3));
  CPPUNIT_ASSERT(ChunkSelector::window_ratio(60, 100) == make_ratio(1, 2));
  CPPUNIT_ASSERT(ChunkSelector::window_ratio(79, 100) == make_ratio(1, 2));
  CPPUNIT_ASSERT(ChunkSelector::window_ratio(80, 100) == make_ratio(1, 1));
  CPPUNIT_ASSERT(ChunkSelector::window_ratio(100, 100) == make_ratio(1, 1));

  // No overflow on torrents with many chunks.
  CPPUNIT_ASSERT(ChunkSelector::window_ratio(0xffffffff, 0xffffffff) == make_ratio(1, 1));
  CPPUNIT_ASSERT(ChunkSelector::window_ratio(0, 0) == make_ratio(1, 1));
}

void
test_chunk_selector::test_window_ratio_picks() {
  // The window is the first 5000 chunks, the completed chunks are at the end of the normal
  // priority range.
  for (uint32_t step = 0; step <= 4; step++) {
    uint32_t completed = size_chunks / 5 * step;
    auto     ratio     = torrent::ChunkSelector::window_ratio(completed, size_chunks);

    CPPUNIT_ASSERT(ratio == make_ratio(1, 5 - step));
    assert_ratio(pick_chunks(true, window_last, completed), ratio);
  }
}

void
test_chunk_selector::test_ratio_fallback() {
  // Fewer high priority chunks than the ratio asks for: once they are taken, only normal
  // priority chunks are left and are picked without waiting.
  auto counts = pick_chunks(false, 100);

  CPPUNIT_ASSERT(counts.first == 100);
  CPPUNIT_ASSERT(counts.second == picks - 100);
}
