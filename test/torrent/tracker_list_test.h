#include <torrent/tracker.h>
#include <torrent/tracker_list.h>
#include <cppunit/extensions/HelperMacros.h>

#include "torrent/tracker_list.h"

class tracker_list_test : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(tracker_list_test);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_single_tracker);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}
  void tearDown() {}

  void test_basic();
  void test_single_tracker();
};

class TrackerTest : public torrent::Tracker {
public:
  // TODO: Clean up tracker related enums.
  TrackerTest(torrent::TrackerList* parent, const std::string& url) :
    torrent::Tracker(parent, url), m_requesting_state(-1) {}

  virtual bool        is_busy() const { return false; }

  virtual Type        type() const { return (Type)(TRACKER_DHT + 1); }

  int                 requesting_state() const { return m_requesting_state; }

private:
  virtual void        send_state(int state) { m_requesting_state = state; }
  virtual void        close() {}

  int                 m_requesting_state;
};

