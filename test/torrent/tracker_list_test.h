#include <torrent/tracker.h>
#include <torrent/tracker_list.h>
#include <rak/timer.h>
#include <cppunit/extensions/HelperMacros.h>

class tracker_list_test : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(tracker_list_test);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_enable);
  CPPUNIT_TEST(test_close);

  CPPUNIT_TEST(test_tracker_flags);
  CPPUNIT_TEST(test_find_url);
  CPPUNIT_TEST(test_can_scrape);

  CPPUNIT_TEST(test_single_success);
  CPPUNIT_TEST(test_single_failure);
  CPPUNIT_TEST(test_single_closing);

  CPPUNIT_TEST(test_multiple_success);

  CPPUNIT_TEST(test_scrape_success);
  CPPUNIT_TEST(test_scrape_failure);

  CPPUNIT_TEST(test_has_active);
  CPPUNIT_TEST(test_count_active);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}
  void tearDown() {}

  void test_basic();
  void test_enable();
  void test_close();

  void test_tracker_flags();
  void test_find_url();
  void test_can_scrape();

  void test_single_success();
  void test_single_failure();
  void test_single_closing();

  void test_multiple_success();

  void test_scrape_success();
  void test_scrape_failure();

  void test_has_active();
  void test_count_active();
};

class TrackerTest : public torrent::Tracker {
public:
  static const int flag_close_on_done = max_flag_size << 0;
  static const int flag_scrape_on_success = max_flag_size << 1;

  // TODO: Clean up tracker related enums.
  TrackerTest(torrent::TrackerList* parent, const std::string& url, int flags = torrent::Tracker::flag_enabled) :
    torrent::Tracker(parent, url, flags),
    m_busy(false),
    m_open(false),
    m_requesting_state(-1) { m_flags |= flag_close_on_done; }

  virtual bool        is_busy() const { return m_busy; }
  bool                is_open() const { return m_open; }

  virtual Type        type() const { return (Type)(TRACKER_DHT + 1); }

  int                 requesting_state() const { return m_requesting_state; }

  bool                trigger_success();
  bool                trigger_success(torrent::TrackerList::address_list* address_list);
  bool                trigger_failure();
  bool                trigger_scrape();

  void                set_close_on_done(bool state) { if (state) m_flags |= flag_close_on_done; else m_flags &= ~flag_close_on_done; }
  void                set_scrape_on_success(bool state) { if (state) m_flags |= flag_scrape_on_success; else m_flags &= ~flag_scrape_on_success; }
  void                set_can_scrape()              { m_flags |= flag_can_scrape; }

private:
  virtual void        send_state(int state) { m_busy = true; m_open = true; m_requesting_state = m_latest_event = state; }
  virtual void        send_scrape()         { m_busy = true; m_open = true; m_requesting_state = m_latest_event = torrent::Tracker::EVENT_SCRAPE; }
  virtual void        close()               { m_busy = false; m_open = false; m_requesting_state = -1; }

  bool                m_busy;
  bool                m_open;
  int                 m_requesting_state;
};
