#include <cppunit/extensions/HelperMacros.h>

#include "torrent/task_manager.h"

struct TMT_Data;

class TaskManagerTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(TaskManagerTest);
  CPPUNIT_TEST(testInsert);
  CPPUNIT_TEST(testLock);
  CPPUNIT_TEST(testWaiveLock);
  CPPUNIT_TEST_SUITE_END();

public:
  typedef std::pair<torrent::TaskManager::iterator, TMT_Data*> task_pair;

  void setUp();
  void tearDown();

  void testInsert();
  void testLock();
  void testWaiveLock();

  inline task_pair
  create_task(const char* name, torrent::Task::pt_func func);

  torrent::TaskManager m_manager;
};
