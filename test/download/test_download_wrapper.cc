#include "config.h"

#include "test_download_wrapper.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "download/download_wrapper.h"

CPPUNIT_TEST_SUITE_REGISTRATION(test_download_wrapper);

namespace {

using window_type = std::pair<uint32_t, uint32_t>;
using range_list  = std::vector<std::pair<uint32_t, uint32_t>>;

constexpr uint32_t chunk_size = 256 << 10;

torrent::download_data::priority_ranges
make_ranges(const range_list& list) {
  torrent::download_data::priority_ranges result;

  for (const auto& range : list)
    result.insert(range.first, range.second);

  return result;
}

range_list
to_list(const torrent::download_data::priority_ranges& ranges) {
  return range_list(ranges.begin(), ranges.end());
}

} // namespace

void
test_download_wrapper::test_seek_window() {
  CPPUNIT_ASSERT(torrent::DownloadWrapper::seek_window(10, 20, 100, chunk_size) == window_type(10, 30));
  CPPUNIT_ASSERT(torrent::DownloadWrapper::seek_window(90, 20, 100, chunk_size) == window_type(90, 100));
  CPPUNIT_ASSERT(torrent::DownloadWrapper::seek_window(100, 20, 100, chunk_size) == window_type(100, 100));
  CPPUNIT_ASSERT(torrent::DownloadWrapper::seek_window(500, 20, 100, chunk_size) == window_type(100, 100));
}

void
test_download_wrapper::test_seek_window_clear() {
  auto window = torrent::DownloadWrapper::seek_window(10, 0, 100, chunk_size);

  CPPUNIT_ASSERT_EQUAL(window.first, window.second);

  auto high = make_ranges({{0, 1}});
  torrent::DownloadWrapper::promote_seek_window(&high, make_ranges({{0, 100}}), window.first, window.second);

  CPPUNIT_ASSERT(to_list(high) == (range_list{{0, 1}}));
}

void
test_download_wrapper::test_seek_window_cap() {
  constexpr uint32_t max_chunks = torrent::DownloadWrapper::max_seek_window / chunk_size;

  CPPUNIT_ASSERT(torrent::DownloadWrapper::seek_window(10, 100000, 200000, chunk_size) == window_type(10, 10 + max_chunks));

  // A chunk larger than the cap still gives a window of one chunk.
  CPPUNIT_ASSERT(torrent::DownloadWrapper::seek_window(1, 5, 10, uint32_t{1} << 31) == window_type(1, 2));

  // No chunks yet (no metadata): an empty window.
  CPPUNIT_ASSERT(torrent::DownloadWrapper::seek_window(0, 5, 0, 0) == window_type(0, 0));
}

void
test_download_wrapper::test_promote_seek_window() {
  auto high = make_ranges({{0, 1}});
  torrent::DownloadWrapper::promote_seek_window(&high, make_ranges({{1, 100}}), 40, 60);

  CPPUNIT_ASSERT(to_list(high) == (range_list{{0, 1}, {40, 60}}));
}

// Chunks 10..20 belong to a PRIORITY_OFF file: they are in neither normal nor high.
void
test_download_wrapper::test_promote_seek_window_off() {
  auto normal = make_ranges({{0, 10}, {20, 30}});
  auto high   = torrent::download_data::priority_ranges();

  torrent::DownloadWrapper::promote_seek_window(&high, normal, 5, 25);
  CPPUNIT_ASSERT(to_list(high) == (range_list{{5, 10}, {20, 25}}));

  high.clear();
  torrent::DownloadWrapper::promote_seek_window(&high, normal, 12, 18);
  CPPUNIT_ASSERT(high.empty());
}
