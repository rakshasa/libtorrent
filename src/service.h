#ifndef LIBTORRENT_SERVICE_H
#define LIBTORRENT_SERVICE_H

#include "timer.h"
#include <deque>

namespace torrent {

// TODO: Since this list is heavly used and results in
// many cache misses it should convert to using deque. Removed
// services should be flagged since we can't yank them out.

class Service {
public:
  struct Task {
    Task(Timer t, int arg, Service* s) :
      m_time(t),
      m_service(s),
      m_arg(arg),
      m_ignore(false) {}

    Timer m_time;
    Service* m_service;

    int m_arg;
    bool m_ignore;
  };

  typedef std::deque<Task> Tasks;

  virtual ~Service();

  // Debuging, should be pure virtual
  virtual void service(int type);
  
  // Only add if t is before the previous scheduled service.
  void insert_service(Timer t, int type);

  // This should really only be called when destroying the object.
  void remove_service();
  void remove_service(int type);

  bool in_service(int type);
  
  // Don't call this unless you have checked inService first.
  Timer when_service(int type);

  static void perform_service();
  static Timer next_service();

protected:
  Service() {}

private:

  // Disable copying
  Service(const Service&);
  void operator = (const Service&);

  static Tasks m_tasks;
};

}

#endif // LIBTORRENT_SERVICE_H
