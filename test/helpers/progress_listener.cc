#include "config.h"

#include "progress_listener.h"

#include <algorithm>
#include <iostream>
#include "torrent/utils/log.h"
#include "torrent/utils/log_buffer.h"

void
progress_listener::startTest(CppUnit::Test *test) {
  std::cout << test->getName() << std::flush;
  m_last_test_failed = false;

  torrent::log_cleanup();

  // TODO: Fix log_open_log_buffer so it cleans up on deleting
  // log_buffer, then delete before log_cleanup.
  m_current_log_buffer = torrent::log_open_log_buffer("test_output");
}

void
progress_listener::addFailure(const CppUnit::TestFailure &failure) {
  std::cout << " : " << (failure.isError() ? "error" : "assertion");

  m_last_test_failed = true;
  m_failures.push_back(std::move(failure_type{test_failure_p(failure.clone()), std::move(m_current_log_buffer)}));
}

void
progress_listener::endTest(CppUnit::Test *test) {
  std::cout << (m_last_test_failed ? "" : " : OK") << std::endl;

  m_current_log_buffer.reset();
  torrent::log_cleanup();
}

void
progress_listener::startSuite(CppUnit::Test *suite) {
  // TODO: When suite is a namespace handle differently.
  std::cout << std::endl << suite->getName() << " Suite:" << std::endl;
}
