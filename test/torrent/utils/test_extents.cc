#include "config.h"

#include "test_extents.h"

#include <torrent/utils/extents.h>
#include <torrent/utils/log.h>

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_extents, "torrent/utils");

#define TEST_EXTENT_BEGIN(name)                                 \
  lt_log_print(torrent::LOG_MOCK_CALLS, "extent: %s", name);

typedef torrent::extents<uint32_t, int> extent_type_1;

template <typename Extent>
bool
verify_extent_data(Extent& extent, const uint32_t* idx, const int* val) {
  while (*idx != *(idx + 1)) {
    for (auto i = *idx; i != *(idx + 1); i++) {
      lt_log_print(torrent::LOG_MOCK_CALLS, "extent: at %u", i);

      if (extent.at(i) != *val)
        return false;
    }

    idx++;
    val++;
  }

  return true;
}

static const uint32_t idx_empty[] = {0, 256, 256};
static const int      val_empty[] = {0};

static const uint32_t idx_basic_1[] = {0, 1, 255, 256, 256};
static const int      val_basic_1[] = {1, 0, 1};

void
test_extents::test_basic() {
  extent_type_1 extent_1;
  extent_1.insert(0, 255, int());

  { TEST_EXTENT_BEGIN("empty");
    CPPUNIT_ASSERT(verify_extent_data(extent_1, idx_empty, val_empty));

    CPPUNIT_ASSERT(extent_1.at(0) == int());
    CPPUNIT_ASSERT(extent_1.at(255) == int());
  };
  { TEST_EXTENT_BEGIN("borders");

    extent_1.insert(0, 0, 1);
    extent_1.insert(255, 255, 1);
    // This step shouldn't be needed.
    extent_1.insert(1, 254, int());

    CPPUNIT_ASSERT(extent_1.at(0) == 1);
    CPPUNIT_ASSERT(extent_1.at(255) == 1);

    CPPUNIT_ASSERT(verify_extent_data(extent_1, idx_basic_1, val_basic_1));
  };
}
