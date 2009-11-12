#include "config.h"

#include "task_manager_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION(TaskManagerTest);

struct TMT_Data {
  TMT_Data(TaskManagerTest* m) : manager(m), counter(0) {}

  TaskManagerTest *manager;
  unsigned int counter;
};

void
TaskManagerTest::setUp() {

}

void
TaskManagerTest::tearDown() {
}

void*
TMT_Func(TMT_Data* data) {
  __sync_add_and_fetch(&data->counter, 1);

  return NULL;
}

void*
TMT_lock_func(TMT_Data* data) {
  __sync_add_and_fetch(&data->counter, 1);

  data->manager->m_manager.acquire_global_lock();
  usleep(10000);
  data->manager->m_manager.release_global_lock();

  __sync_add_and_fetch(&data->counter, 1);

  return NULL;
}

void*
TMT_waive_lock_func(TMT_Data* data) {
  __sync_add_and_fetch(&data->counter, 1);

  data->manager->m_manager.acquire_global_lock();
  usleep(20000);
  data->manager->m_manager.waive_global_lock();

  __sync_add_and_fetch(&data->counter, 1);

  return NULL;
}

inline TaskManagerTest::task_pair
TaskManagerTest::create_task(const char* name, torrent::Task::pt_func func) {
  TMT_Data* data = new TMT_Data(this);

  return task_pair(m_manager.insert(name, func, data), data);
}

void
TaskManagerTest::testInsert() {
  task_pair task1 = create_task("Test 1", (torrent::Task::pt_func)&TMT_Func);
  task_pair task2 = create_task("Test 2", (torrent::Task::pt_func)&TMT_Func);

  usleep(10000);

  CPPUNIT_ASSERT(task1.first != m_manager.end() && task2.first != m_manager.end());
  CPPUNIT_ASSERT(task1.second->counter == 1 && task2.second->counter == 1);

  // Clean up test, check that we've taken down the threads properly.

  //  delete data1;
  //  delete data2;
}

void
TaskManagerTest::testLock() {
  task_pair task1 = create_task("Test Lock 1", (torrent::Task::pt_func)&TMT_lock_func);
  task_pair task2 = create_task("Test Lock 2", (torrent::Task::pt_func)&TMT_lock_func);
  task_pair task3 = create_task("Test Lock 3", (torrent::Task::pt_func)&TMT_lock_func);

  usleep(100000);

  CPPUNIT_ASSERT(task1.second->counter == 2 &&
                 task2.second->counter == 2 &&
                 task3.second->counter == 2);
}

void
TaskManagerTest::testWaiveLock() {
  task_pair task1 = create_task("Test Waive Lock 1", (torrent::Task::pt_func)&TMT_waive_lock_func);
  usleep(5000);
  task_pair task2 = create_task("Test Waive Lock 2", (torrent::Task::pt_func)&TMT_lock_func);
  task_pair task3 = create_task("Test Waive Lock 3", (torrent::Task::pt_func)&TMT_lock_func);

  usleep(100000);

  CPPUNIT_ASSERT(task1.second->counter == 2 &&
                 task2.second->counter == 2 &&
                 task3.second->counter == 2);

  // The global lock needs to be released as 'Test Lock 1' doesn't
  // release it.
  m_manager.release_global_lock();
}

