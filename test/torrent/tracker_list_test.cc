#include "config.h"

#include "tracker_list_test.h"

#include <torrent/tracker.h>

CPPUNIT_TEST_SUITE_REGISTRATION(tracker_list_test);

class TrackerTest : public torrent::Tracker {
public:
  // TODO: Clean up tracker related enums.
  TrackerTest(torrent::TrackerList* parent, const std::string& url) :
    torrent::Tracker(parent, url), m_requesting_state(0) {}

  virtual bool        is_busy() const { return false; }

  virtual Type        type() const { return (Type)(TRACKER_DHT + 1); }

private:
  virtual void        send_state(int state) {  }
  virtual void        close() {}

  int                 m_requesting_state;
};

void
tracker_list_test::test_basic() {
  torrent::TrackerList tracker_list;

  tracker_list.insert(0, new TrackerTest(&tracker_list, ""));

  CPPUNIT_ASSERT(tracker_list[0]->parent() == &tracker_list);
  CPPUNIT_ASSERT(std::distance(tracker_list.begin_group(0), tracker_list.end_group(0)) == 1);
  CPPUNIT_ASSERT(tracker_list.find_usable(tracker_list.begin()) != tracker_list.end());
}

void
tracker_list_test::test_single_tracker() {
  torrent::TrackerList tracker_list;

  tracker_list.insert(0, new TrackerTest(&tracker_list, ""));
  tracker_list.send_state(1);
}
