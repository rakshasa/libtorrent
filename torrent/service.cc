#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "service.h"
#include "exceptions.h"

#include <algorithm>
#include <algo/algo.h>
#include <iostream>

using namespace algo;

namespace torrent {

Service::Tasks Service::m_tasks;

Service::~Service() {
  removeService();
}

void Service::service(int type) {
  throw internal_error("Pure virtual Service::service called");
}

// Don't add an element that has 't' less than Timer::cache during
// Service::runService. (in torrent::work)
void Service::insertService(Timer t, int type) {
  m_tasks.insert(std::find_if(m_tasks.begin(), m_tasks.end(),
			      gt(member(&Task::m_time), value(t))),
		 Task(t, type, this));
}

// This should really only be called when destroying the object.
void Service::removeService() {
  std::for_each(m_tasks.begin(), m_tasks.end(),
		if_on(eq(value(this), member(&Task::m_service)),

		      assign(member(&Task::m_ignore), value(true))));
}

// This should really only be called when destroying the object.
void Service::removeService(int type) {
  std::for_each(m_tasks.begin(), m_tasks.end(),
		if_on(bool_and(eq(value(this), member(&Task::m_service)),
			       eq(value(type), member(&Task::m_arg))),

		      assign(member(&Task::m_ignore), value(true))));
}

bool Service::inService(int type) {
  return std::find_if(m_tasks.begin(), m_tasks.end(),
		      bool_and(eq(member(&Task::m_service), value(this)),
			       bool_and(eq(member(&Task::m_arg), value(type)),
					bool_not(member(&Task::m_ignore)))))
    != m_tasks.end();
}

Timer Service::whenService(int type) {
  Tasks::iterator itr =
    std::find_if(m_tasks.begin(), m_tasks.end(),
		 bool_and(eq(member(&Task::m_service), value(this)),

			  bool_and(eq(member(&Task::m_arg), value(type)),
				   bool_not(member(&Task::m_ignore)))));

  return itr != m_tasks.end() ? itr->m_time : Timer();
}  

void Service::runService() {
  if (Timer::cache().usec() < 0)
    throw internal_error("Service Timer Bork Bork Bork");

  while (!m_tasks.empty() &&
	 m_tasks.front().m_time < Timer::cache()) {

    if (!m_tasks.front().m_ignore) {
      // Set ignore flag so {in,when}Service calls behave.
      m_tasks.front().m_ignore = true;
      m_tasks.front().m_service->service(m_tasks.front().m_arg);
    }

    m_tasks.pop_front();
  }
}

Timer Service::nextService() {
  if (!m_tasks.empty()) {
    Timer t = m_tasks.front().m_time - Timer::current();

    return t.usec() > 0 ? t : Timer(0);
  } else {
    return Timer((uint64_t)(1 << 30) * 1000000);
  }
}

} // namespace torrent

