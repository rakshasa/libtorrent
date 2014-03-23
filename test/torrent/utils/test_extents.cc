#include "config.h"

#include "test_extents.h"

#include <inttypes.h>
#include <iostream>
#include <torrent/utils/extents.h>

CPPUNIT_TEST_SUITE_REGISTRATION(ExtentsTest);

void
ExtentsTest::setUp() {
}

void
ExtentsTest::tearDown() {
}

typedef torrent::extents<uint32_t, int, 8, 16, 4> extent_type_1;

// typedef torrent::extents<uint32_t, int, 0, 256, 16> extent_type_3;

template <typename Extent>
bool
verify_extent_data(Extent& extent, const uint32_t* idx, const int* val) {
  while (*idx != *(idx + 1)) {
    if (!extent.is_equal_range(*idx, *(idx + 1) - 1, *val)) {
      // std::cout << *idx << ' ' << *(idx + 1) << ' ' << *val << std::endl;
      // std::cout << extent.at(*idx) << std::endl;
      // std::cout << extent.at(*(idx + 1)) << std::endl;
      return false;
    }

    idx++;
    val++;
  }

  return true;
}

static const uint32_t idx_empty[] = {0, 256, 256};
static const int      val_empty[] = {0, 1};

static const uint32_t idx_basic_1[] = {0, 1, 255, 256, 256};
static const int      val_basic_1[] = {1, 0, 1};

// static const uint32_t idx_basic_2[] = {0, 1, 16, 255, 256, 256};
// static const int      val_basic_2[] = {1, 0, 2, 1};

void
ExtentsTest::test_basic() {
  extent_type_1 extent_1;

  // Test empty.
  CPPUNIT_ASSERT(verify_extent_data(extent_1, idx_empty, val_empty));

  CPPUNIT_ASSERT(extent_1.at(0) == int());
  CPPUNIT_ASSERT(extent_1.at(255) == int());

  extent_1.insert(0, 0, 1);
  extent_1.insert(255, 0, 1);

  CPPUNIT_ASSERT(extent_1.at(0) == 1);
  CPPUNIT_ASSERT(extent_1.at(255) == 1);

  CPPUNIT_ASSERT(verify_extent_data(extent_1, idx_basic_1, val_basic_1));

  // extent_1.insert(38, 3, 2);

  // CPPUNIT_ASSERT(verify_extent_data(extent_1, idx_basic_2, val_basic_2));
}
