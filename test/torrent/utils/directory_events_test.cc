#include "config.h"

#include <cstdlib>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <vector>

#include <torrent/exceptions.h>
#include <torrent/utils/directory_events.h>

#include "directory_events_test.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(utils_directory_events_test, "torrent/utils");

namespace {

constexpr int added_flags = torrent::directory_events::flag_on_added |
                            torrent::directory_events::flag_on_updated;

torrent::directory_events::slot_string
noop_slot() {
  return [](const std::string&) {};
}

void
assert_mkdir(const std::string& path) {
  CPPUNIT_ASSERT_MESSAGE(("Could not create test directory: " + path).c_str(), ::mkdir(path.c_str(), 0700) == 0);
}

void
assert_write_file(const std::string& path) {
  int fd = ::open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0600);
  CPPUNIT_ASSERT_MESSAGE(("Could not create test file: " + path).c_str(), fd != -1);
  CPPUNIT_ASSERT(::write(fd, "x", 1) == 1);
  CPPUNIT_ASSERT(::close(fd) == 0);
}

void
assert_conflicting_watch(torrent::directory_events* events, const std::string& path, int flags) {
  try {
    events->notify_on(path, flags, noop_slot());
    CPPUNIT_ASSERT_MESSAGE(("Expected conflicting watch for: " + path).c_str(), false);
  } catch (const torrent::input_error&) {
  }
}

} // namespace

void
utils_directory_events_test::setUp() {
  TestFixtureWithMainThread::setUp();

  char  path_template[] = "/tmp/libtorrent-directory-events.XXXXXX";
  char* path            = ::mkdtemp(path_template);

  CPPUNIT_ASSERT(path != nullptr);

  m_root = path;
  m_link = m_root + "-link";
  assert_mkdir(m_root + "/child");
  assert_mkdir(m_root + "/other");
  CPPUNIT_ASSERT_MESSAGE(("Could not create test symlink: " + m_link).c_str(), ::symlink(m_root.c_str(), m_link.c_str()) == 0);
}

void
utils_directory_events_test::tearDown() {
  if (!m_root.empty()) {
    ::unlink((m_root + "/symlink-callback.torrent").c_str());
    ::unlink(m_link.c_str());
    ::rmdir((m_root + "/child").c_str());
    ::rmdir((m_root + "/other").c_str());
    ::rmdir(m_root.c_str());
    m_link.clear();
    m_root.clear();
  }

  TestFixtureWithMainThread::tearDown();
}

void
utils_directory_events_test::test_ready_conflicts_with_added_same_directory() {
#ifdef USE_INOTIFY
  torrent::directory_events events;
  CPPUNIT_ASSERT(events.open());

  events.notify_on(m_root, torrent::directory_events::flag_on_ready, noop_slot());
  assert_conflicting_watch(&events, m_root + "/.", added_flags);

  events.close();
#endif
}

void
utils_directory_events_test::test_symlink_watch_uses_configured_callback_path() {
#ifdef USE_INOTIFY
  torrent::directory_events events;
  std::vector<std::string> paths;
  CPPUNIT_ASSERT(events.open());

  events.notify_on(m_link, added_flags, [&paths](const std::string& path) {
    paths.push_back(path);
  });

  assert_write_file(m_link + "/symlink-callback.torrent");

  for (int i = 0; i < 100 && paths.empty(); i++) {
    events.event_read();
    ::usleep(1000);
  }

  CPPUNIT_ASSERT(!paths.empty());
  CPPUNIT_ASSERT_EQUAL(m_link + "/symlink-callback.torrent", paths.front());

  events.close();
#endif
}

void
utils_directory_events_test::test_added_conflicts_with_ready_same_directory_normalized() {
#ifdef USE_INOTIFY
  torrent::directory_events events;
  CPPUNIT_ASSERT(events.open());

  events.notify_on(m_root + "/child/..", added_flags, noop_slot());
  assert_conflicting_watch(&events, m_root, torrent::directory_events::flag_on_ready);

  events.close();
#endif
}

void
utils_directory_events_test::test_ready_allows_added_child() {
#ifdef USE_INOTIFY
  torrent::directory_events events;
  CPPUNIT_ASSERT(events.open());

  events.notify_on(m_root, torrent::directory_events::flag_on_ready, noop_slot());
  events.notify_on(m_root + "/child", added_flags, noop_slot());

  events.close();
#endif
}

void
utils_directory_events_test::test_added_allows_ready_parent() {
#ifdef USE_INOTIFY
  torrent::directory_events events;
  CPPUNIT_ASSERT(events.open());

  events.notify_on(m_root + "/child", added_flags, noop_slot());
  events.notify_on(m_root, torrent::directory_events::flag_on_ready, noop_slot());

  events.close();
#endif
}

void
utils_directory_events_test::test_ready_allows_added_sibling() {
#ifdef USE_INOTIFY
  torrent::directory_events events;
  CPPUNIT_ASSERT(events.open());

  events.notify_on(m_root + "/child", torrent::directory_events::flag_on_ready, noop_slot());
  events.notify_on(m_root + "/other", added_flags, noop_slot());

  events.close();
#endif
}
