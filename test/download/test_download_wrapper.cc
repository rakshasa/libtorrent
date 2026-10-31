#include "config.h"

#include "test_download_wrapper.h"

#include <cstdint>
#include <utility>

#include "download/download_wrapper.h"
#include "torrent/exceptions.h"
#include "torrent/path.h"
#include "torrent/data/file.h"
#include "torrent/data/file_list.h"

CPPUNIT_TEST_SUITE_REGISTRATION(test_download_wrapper);

namespace {

using window_type = std::pair<uint32_t, uint32_t>;

constexpr uint32_t default_chunk_size = 256 << 10;

// FileList::initialize() is protected.
class window_file_list : public torrent::FileList {
public:
  // Three files of 'file_chunks' chunks each.
  window_file_list(uint32_t file_chunks, uint32_t chunk_bytes = default_chunk_size) {
    uint64_t file_size = uint64_t{file_chunks} * chunk_bytes;

    initialize(3 * file_size, chunk_bytes);

    split_type files[] = {
      split_type(file_size, torrent::Path(), 0),
      split_type(file_size, torrent::Path(), 0),
      split_type(file_size, torrent::Path(), 0),
    };

    split(begin(), std::begin(files), std::end(files));
  }
};

window_type
window_range(const torrent::FileList& file_list, uint32_t first, uint32_t size) {
  return torrent::DownloadWrapper::priority_window_range(&file_list, first, size);
}

} // namespace

void
test_download_wrapper::test_window_range() {
  window_file_list file_list(10);

  CPPUNIT_ASSERT(file_list.size_chunks() == 30);
  CPPUNIT_ASSERT(window_range(file_list, 10, 5) == window_type(10, 15));
  CPPUNIT_ASSERT(window_range(file_list, 25, 20) == window_type(25, 30));
  CPPUNIT_ASSERT(window_range(file_list, 29, 1) == window_type(29, 30));

  CPPUNIT_ASSERT_THROW(window_range(file_list, 30, 1), torrent::input_error);
  CPPUNIT_ASSERT_THROW(window_range(file_list, 500, 1), torrent::input_error);
}

void
test_download_wrapper::test_window_range_clear() {
  window_file_list file_list(10);

  CPPUNIT_ASSERT(window_range(file_list, 10, 0) == window_type(0, 0));
  CPPUNIT_ASSERT(window_range(file_list, 500, 0) == window_type(0, 0));

  // Clearing never fails, even when the window could not be set.
  file_list.at(1)->set_priority(torrent::PRIORITY_OFF);
  CPPUNIT_ASSERT(window_range(file_list, 10, 0) == window_type(0, 0));
}

void
test_download_wrapper::test_window_range_cap() {
  constexpr uint32_t max_chunks = torrent::DownloadWrapper::max_priority_window / default_chunk_size;

  window_file_list file_list(max_chunks);

  CPPUNIT_ASSERT(window_range(file_list, 10, max_chunks) == window_type(10, 10 + max_chunks));
  CPPUNIT_ASSERT_THROW(window_range(file_list, 10, max_chunks + 1), torrent::input_error);

  // The cap applies to the window clamped to the end of the torrent.
  CPPUNIT_ASSERT(window_range(file_list, file_list.size_chunks() - 1, max_chunks + 1) ==
                 window_type(file_list.size_chunks() - 1, file_list.size_chunks()));

  // A chunk larger than the cap still allows a window of one chunk.
  window_file_list large_chunks(1, uint32_t{1} << 31);

  CPPUNIT_ASSERT(window_range(large_chunks, 1, 1) == window_type(1, 2));
  CPPUNIT_ASSERT_THROW(window_range(large_chunks, 1, 2), torrent::input_error);
}

void
test_download_wrapper::test_window_range_not_normal() {
  window_file_list file_list(10);

  CPPUNIT_ASSERT(torrent::DownloadWrapper::has_only_normal_priority(&file_list));

  // Any file that is not normal priority refuses the window, also outside of it.
  for (auto priority : { torrent::PRIORITY_OFF, torrent::PRIORITY_HIGH }) {
    file_list.at(2)->set_priority(priority);

    CPPUNIT_ASSERT(!torrent::DownloadWrapper::has_only_normal_priority(&file_list));
    CPPUNIT_ASSERT_THROW(window_range(file_list, 0, 5), torrent::input_error);
  }

  file_list.at(2)->set_priority(torrent::PRIORITY_NORMAL);
  CPPUNIT_ASSERT(window_range(file_list, 0, 5) == window_type(0, 5));
}
