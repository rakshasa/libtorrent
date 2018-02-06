#include "config.h"

#include "tracker_webseed_test.h"

#include "tracker/tracker_webseed.h"
#include "torrent/tracker_list.h"

CPPUNIT_TEST_SUITE_REGISTRATION(TrackerWebseedTest);

void
TrackerWebseedTest::setUp() {
}

void
TrackerWebseedTest::tearDown() {
}

void
TrackerWebseedTest::test_basic() {

  torrent::TrackerList parent;
  std::string url;
  int flags;

  torrent::TrackerWebseed tracker(&parent, url, flags);

  CPPUNIT_ASSERT(tracker.is_busy() == false);
  CPPUNIT_ASSERT(tracker.is_usable() == true);

  tracker.close();
  tracker.disown();

  CPPUNIT_ASSERT(tracker.type() == torrent::Tracker::TRACKER_WEBSEED);

}
