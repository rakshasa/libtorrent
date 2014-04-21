#include "config.h"

#include "test_queue_buckets.h"

#include "utils/instrumentation.h"
#include "utils/queue_buckets.h"

CPPUNIT_TEST_SUITE_REGISTRATION(TestQueueBuckets);

struct test_constants {
  static const int bucket_count = 2;

  static const torrent::instrumentation_enum instrumentation_added[bucket_count];
  static const torrent::instrumentation_enum instrumentation_moved[bucket_count];
  static const torrent::instrumentation_enum instrumentation_removed[bucket_count];
  static const torrent::instrumentation_enum instrumentation_total[bucket_count];

  template <typename Type>
  static void destroy(Type& obj);
};

const torrent::instrumentation_enum test_constants::instrumentation_added[bucket_count] = {
  torrent::INSTRUMENTATION_TRANSFER_REQUESTS_QUEUED_ADDED,
  torrent::INSTRUMENTATION_TRANSFER_REQUESTS_UNORDERED_ADDED
};
const torrent::instrumentation_enum test_constants::instrumentation_moved[bucket_count] = {
  torrent::INSTRUMENTATION_TRANSFER_REQUESTS_QUEUED_MOVED,
  torrent::INSTRUMENTATION_TRANSFER_REQUESTS_UNORDERED_MOVED
};
const torrent::instrumentation_enum test_constants::instrumentation_removed[bucket_count] = {
  torrent::INSTRUMENTATION_TRANSFER_REQUESTS_QUEUED_REMOVED,
  torrent::INSTRUMENTATION_TRANSFER_REQUESTS_UNORDERED_REMOVED
};
const torrent::instrumentation_enum test_constants::instrumentation_total[bucket_count] = {
  torrent::INSTRUMENTATION_TRANSFER_REQUESTS_QUEUED_TOTAL,
  torrent::INSTRUMENTATION_TRANSFER_REQUESTS_UNORDERED_TOTAL
};

typedef torrent::queue_buckets<int, test_constants> buckets_type;

static int items_destroyed = 0;

template <>
void
test_constants::destroy<int>(__UNUSED int& obj) {
  items_destroyed++;
}

struct test_queue_bucket_compare {
  test_queue_bucket_compare(int v) : m_v(v) {}

  bool operator () (int obj) {
    return m_v == obj;
  }

  int m_v;
};

#define FILL_BUCKETS(s_0, s_1)                  \
  for (int i = 0; i < s_0; i++)                 \
    buckets.push_back(0, i);                    \
  for (int i = 0; i < s_1; i++)                 \
    buckets.push_back(1, s_0 + i);

#define VERIFY_QUEUE_SIZES(s_0, s_1)            \
  CPPUNIT_ASSERT(buckets.queue_size(0) == s_0); \
  CPPUNIT_ASSERT(buckets.queue_size(1) == s_1);

#define VERIFY_INSTRUMENTATION(a_0, m_0, r_0, t_0, a_1, m_1, r_1, t_1)            \
  CPPUNIT_ASSERT(torrent::instrumentation_values[test_constants::instrumentation_added[0]] == a_0); \
  CPPUNIT_ASSERT(torrent::instrumentation_values[test_constants::instrumentation_moved[0]] == m_0); \
  CPPUNIT_ASSERT(torrent::instrumentation_values[test_constants::instrumentation_removed[0]] == r_0); \
  CPPUNIT_ASSERT(torrent::instrumentation_values[test_constants::instrumentation_total[0]] == t_0); \
  CPPUNIT_ASSERT(torrent::instrumentation_values[test_constants::instrumentation_added[1]] == a_1); \
  CPPUNIT_ASSERT(torrent::instrumentation_values[test_constants::instrumentation_moved[1]] == m_1); \
  CPPUNIT_ASSERT(torrent::instrumentation_values[test_constants::instrumentation_removed[1]] == r_1); \
  CPPUNIT_ASSERT(torrent::instrumentation_values[test_constants::instrumentation_total[1]] == t_1);

#define VERIFY_ITEMS_DESTROYED(count)           \
  CPPUNIT_ASSERT(items_destroyed == count);     \
  items_destroyed = 0;

//
// Basic tests:
//

void
TestQueueBuckets::test_basic() {
  torrent::instrumentation_initialize();

  buckets_type buckets;

  VERIFY_QUEUE_SIZES(0, 0);
  CPPUNIT_ASSERT(buckets.empty());

  buckets.push_front(0, int());
  VERIFY_QUEUE_SIZES(1, 0);
  buckets.push_back(0, int());
  VERIFY_QUEUE_SIZES(2, 0);
  VERIFY_INSTRUMENTATION(2, 0, 0, 2, 0, 0, 0, 0);  

  CPPUNIT_ASSERT(!buckets.empty());

  buckets.push_front(1, int());
  VERIFY_QUEUE_SIZES(2, 1);
  buckets.push_back(1, int());
  VERIFY_QUEUE_SIZES(2, 2);
  VERIFY_INSTRUMENTATION(2, 0, 0, 2, 2, 0, 0, 2);  

  CPPUNIT_ASSERT(!buckets.empty());

  buckets.pop_front(0);
  VERIFY_QUEUE_SIZES(1, 2);
  buckets.pop_back(0);
  VERIFY_QUEUE_SIZES(0, 2);
  VERIFY_INSTRUMENTATION(2, 0, 2, 0, 2, 0, 0, 2);  

  CPPUNIT_ASSERT(!buckets.empty());

  buckets.pop_front(1);
  VERIFY_QUEUE_SIZES(0, 1);
  buckets.pop_back(1);
  VERIFY_QUEUE_SIZES(0, 0);
  VERIFY_INSTRUMENTATION(2, 0, 2, 0, 2, 0, 2, 0);  

  CPPUNIT_ASSERT(buckets.empty());
}

void
TestQueueBuckets::test_erase() {
  items_destroyed = 0;
  torrent::instrumentation_initialize();

  buckets_type buckets;

  FILL_BUCKETS(10, 5);

  VERIFY_QUEUE_SIZES(10, 5);
  VERIFY_INSTRUMENTATION(10, 0, 0, 10, 5, 0, 0, 5);  
  
  buckets.destroy(0, buckets.begin(0) + 3, buckets.begin(0) + 7);
  VERIFY_ITEMS_DESTROYED(4);
  VERIFY_QUEUE_SIZES(6, 5);
  VERIFY_INSTRUMENTATION(10, 0, 4, 6, 5, 0, 0, 5);  

  buckets.destroy(1, buckets.begin(1) + 0, buckets.begin(1) + 5);
  VERIFY_ITEMS_DESTROYED(5);
  VERIFY_QUEUE_SIZES(6, 0);
  VERIFY_INSTRUMENTATION(10, 0, 4, 6, 5, 0, 5, 0);
}

static buckets_type::const_iterator
bucket_queue_find_in_queue(const buckets_type& buckets, int idx, int value) {
  return torrent::queue_bucket_find_if_in_queue(buckets, idx, test_queue_bucket_compare(value));
}

static std::pair<int, buckets_type::const_iterator>
bucket_queue_find_in_any(const buckets_type& buckets, int value) {
  return torrent::queue_bucket_find_if_in_any(buckets, test_queue_bucket_compare(value));
}

void
TestQueueBuckets::test_find() {
  items_destroyed = 0;
  torrent::instrumentation_initialize();

  buckets_type buckets;

  FILL_BUCKETS(10, 5);

  CPPUNIT_ASSERT(bucket_queue_find_in_queue(buckets, 0, 0)  == buckets.begin(0));
  CPPUNIT_ASSERT(bucket_queue_find_in_queue(buckets, 0, 10) == buckets.end(0));
  CPPUNIT_ASSERT(bucket_queue_find_in_queue(buckets, 1, 10) == buckets.begin(1));
  
  CPPUNIT_ASSERT(bucket_queue_find_in_any(buckets, 0).first == 0);
  CPPUNIT_ASSERT(bucket_queue_find_in_any(buckets, 0).second == buckets.begin(0));
  CPPUNIT_ASSERT(bucket_queue_find_in_any(buckets, 10).first == 1);
  CPPUNIT_ASSERT(bucket_queue_find_in_any(buckets, 10).second == buckets.begin(1));
  CPPUNIT_ASSERT(bucket_queue_find_in_any(buckets, 20).first == 2);
  CPPUNIT_ASSERT(bucket_queue_find_in_any(buckets, 20).second == buckets.end(1));
}

void
TestQueueBuckets::test_destroy_range() {
  items_destroyed = 0;
  torrent::instrumentation_initialize();

  buckets_type buckets;

  FILL_BUCKETS(10, 5);

  VERIFY_QUEUE_SIZES(10, 5);
  VERIFY_INSTRUMENTATION(10, 0, 0, 10, 5, 0, 0, 5);  
  
  buckets.destroy(0, buckets.begin(0) + 3, buckets.begin(0) + 7);
  VERIFY_ITEMS_DESTROYED(4);
  VERIFY_QUEUE_SIZES(6, 5);
  VERIFY_INSTRUMENTATION(10, 0, 4, 6, 5, 0, 0, 5);  

  buckets.destroy(1, buckets.begin(1) + 0, buckets.begin(1) + 5);
  VERIFY_ITEMS_DESTROYED(5);
  VERIFY_QUEUE_SIZES(6, 0);
  VERIFY_INSTRUMENTATION(10, 0, 4, 6, 5, 0, 5, 0);
}

void
TestQueueBuckets::test_move_range() {
  items_destroyed = 0;
  torrent::instrumentation_initialize();

  buckets_type buckets;

  FILL_BUCKETS(10, 5);
  torrent::instrumentation_reset();

  buckets.move_to(0, buckets.begin(0) + 3, buckets.begin(0) + 7, 1);
  VERIFY_ITEMS_DESTROYED(0);

  VERIFY_QUEUE_SIZES(6, 9);
  VERIFY_INSTRUMENTATION(0, 4, 0, 6, 4, 0, 0, 9);  
}
