#include <memory>
#include <vector>
#include <cppunit/Test.h>
#include <cppunit/TestFailure.h>
#include <cppunit/TestListener.h>

#include "torrent/utils/log_buffer.h"

class progress_listener : public CppUnit::TestListener {
public:
  typedef std::unique_ptr<torrent::log_buffer> log_buffer_p;
  typedef std::vector<std::tuple<CppUnit::TestFailure, log_buffer_p>> failure_list_type;

  progress_listener() : m_last_test_failed(false) {}
  virtual ~progress_listener() {}

  virtual void startTest(CppUnit::Test *test);
  virtual void addFailure(const CppUnit::TestFailure &failure);
  virtual void endTest(CppUnit::Test *test);

  // Called by a TestComposite just before running its child tests. 
  // virtual void startSuite(CppUnit::Test *suite);

  // Called by a TestComposite after running its child tests. 
  // virtual void endSuite(CppUnit::Test *suite);

  //Called by a TestRunner before running the test. 
  // virtual void startTestRun(CppUnit::Test *test, CppUnit::TestResult *event_manager);
  
  // Called by a TestRunner after running the test.
  // virtual void endTestRun(CppUnit::Test *test, CppUnit::TestResult *event_manager);

private:
  progress_listener(const progress_listener& rhs) = delete;
  void operator =(const progress_listener& rhs) = delete;

  failure_list_type m_failures;

  log_buffer_p      m_current_log_buffer;
  bool              m_last_test_failed;
};
