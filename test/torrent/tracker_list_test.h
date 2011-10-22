#include <torrent/tracker.h>
#include <torrent/tracker_list.h>
#include <cppunit/extensions/HelperMacros.h>

class tracker_list_test : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(tracker_list_test);
  CPPUNIT_TEST(test_basic);

  CPPUNIT_TEST(test_single_success);
  CPPUNIT_TEST(test_single_failure);
  CPPUNIT_TEST(test_single_closing);

  CPPUNIT_TEST(test_multiple_success);

  CPPUNIT_TEST(test_has_active);
  CPPUNIT_TEST(test_count_active);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}
  void tearDown() {}

  void test_basic();

  void test_single_success();
  void test_single_failure();
  void test_single_closing();

  void test_multiple_success();

  void test_has_active();
  void test_count_active();
};

class TrackerTest : public torrent::Tracker {
public:
  // TODO: Clean up tracker related enums.
  TrackerTest(torrent::TrackerList* parent, const std::string& url) :
    torrent::Tracker(parent, url),
    m_busy(false),
    m_open(false),
    m_close_on_done(true),
    m_requesting_state(-1) {}

  virtual bool        is_busy() const { return m_busy; }
  bool                is_open() const { return m_open; }

  virtual Type        type() const { return (Type)(TRACKER_DHT + 1); }

  int                 requesting_state() const { return m_requesting_state; }

  bool                trigger_success();
  bool                trigger_success(torrent::TrackerList::address_list* address_list);
  bool                trigger_failure();

  void                set_close_on_done(bool state) { m_close_on_done = state; }

private:
  virtual void        send_state(int state) { m_busy = true; m_open = true; m_requesting_state = state; m_latest_event = state; }
  virtual void        close()               { m_busy = false; m_open = false; m_requesting_state = 0; }

  bool                m_busy;
  bool                m_open;
  bool                m_close_on_done;
  int                 m_requesting_state;
};
