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

// Only add if t is before the previous scheduled service.
void Service::insertService(Timer t, int type) {
  m_tasks.insert(std::find_if(m_tasks.begin(), m_tasks.end(),
			      gt(member(&Task::m_time), value(t))),
		 Task(t, type, this));
}

// This should really only be called when destroying the object.
void Service::removeService() {
  Tasks::iterator itr = m_tasks.begin();

  do {
    itr = std::find_if(itr, m_tasks.end(),
		       eq(member(&Task::m_service), value(this)));

    if (itr == m_tasks.end())
      break;
    else
      itr = m_tasks.erase(itr);
  } while(true);
}

// This should really only be called when destroying the object.
void Service::removeService(int type) {
  Tasks::iterator itr = m_tasks.begin();

  do {
    itr = std::find_if(itr, m_tasks.end(),
		 bool_and(eq(member(&Task::m_service), value(this)),
			  eq(member(&Task::m_arg), value(type))));

    if (itr == m_tasks.end())
      break;
    else
      itr = m_tasks.erase(itr);
  } while(true);
}

bool Service::inService(int type) {
  return std::find_if(m_tasks.begin(), m_tasks.end(),
		      bool_and(eq(member(&Task::m_service), value(this)),
			       eq(member(&Task::m_arg), value(type))))
    != m_tasks.end();
}

Timer Service::whenService(int type) {
  return std::find_if(m_tasks.begin(), m_tasks.end(),
		      bool_and(eq(member(&Task::m_service), value(this)),
			       eq(member(&Task::m_arg), value(type)))
		      )->m_time;
}  

void Service::runService() {
  if (Timer::cache().usec() < 0)
    throw internal_error("Service Timer Bork Bork Bork");

  while (!m_tasks.empty() &&
	 m_tasks.front().m_time < Timer::cache()) {
    Task k = m_tasks.front();

    m_tasks.pop_front();

    k.m_service->service(k.m_arg);
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

