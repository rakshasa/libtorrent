#include <memory>
#include <vector>
#include <cppunit/Test.h>
#include <cppunit/TestFailure.h>
#include <cppunit/TestListener.h>

#include "torrent/utils/log_buffer.h"

typedef std::unique_ptr<CppUnit::TestFailure> test_failure_p;

struct failure_type {
  test_failure_p test;
  torrent::log_buffer_p log;
};

// typedef std::tuple<CppUnit::TestFailure, torrent::log_buffer_p> failure_type;
typedef std::vector<failure_type> failure_list_type;

class progress_listener : public CppUnit::TestListener {
public:
  progress_listener() : m_last_test_failed(false) {}
  virtual ~progress_listener() {}

  virtual void startTest(CppUnit::Test *test);
  virtual void addFailure(const CppUnit::TestFailure &failure);
  virtual void endTest(CppUnit::Test *test);

  virtual void startSuite(CppUnit::Test *suite);
  // virtual void endSuite(CppUnit::Test *suite);

  //Called by a TestRunner before running the test.
  // virtual void startTestRun(CppUnit::Test *test, CppUnit::TestResult *event_manager);

  // Called by a TestRunner after running the test.
  // virtual void endTestRun(CppUnit::Test *test, CppUnit::TestResult *event_manager);

  const failure_list_type& failures() { return m_failures; }
  failure_list_type&& move_failures() { return std::move(m_failures); }

private:
  progress_listener(const progress_listener& rhs) = delete;
  void operator =(const progress_listener& rhs) = delete;

  failure_list_type m_failures;
  bool              m_last_test_failed;

  torrent::log_buffer_p m_current_log_buffer;
};
