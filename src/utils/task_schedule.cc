#include "config.h"

#include <algorithm>

#include "task.h"
#include "task_schedule.h"

namespace torrent {

TaskSchedule::Container TaskSchedule::m_container;

void
TaskSchedule::perform(Timer t) {
  while (!m_container.empty() && m_container.front()->get_time() <= t) {
    Task* u = m_container.front();

    u->remove();
    u->get_slot()();
  }
}

Timer
TaskSchedule::get_timeout() {
  if (!m_container.empty())
    return std::max(m_container.front()->get_time() - Timer::current(), Timer());
  else
    return Timer((int64_t)(1 << 30) * 1000000);
}

struct task_comp {
  task_comp(Timer t) : m_time(t) {}

  bool operator () (Task* t) {
    return m_time <= t->get_time();
  }

  Timer m_time;
};

TaskSchedule::iterator
TaskSchedule::insert(Task* t) {
  iterator itr = std::find_if(m_container.begin(), m_container.end(), task_comp(t->get_time()));

  return m_container.insert(itr, t);
}
			       
}
