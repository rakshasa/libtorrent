#ifndef LIBTORRENT_SERVICE_H
#define LIBTORRENT_SERVICE_H

#include "timer.h"
#include <list>

namespace torrent {

class Service {
public:
  struct Task {
    Task(Timer t, int arg, Service* s) :
      m_time(t),
      m_arg(arg),
      m_service(s) {}

    Timer m_time;
    int m_arg;
    Service* m_service;
  };

  typedef std::list<Task> Tasks;

  virtual ~Service();

  // Debuging, should be pure virtual
  virtual void service(int type);
  
  // Only add if t is before the previous scheduled service.
  void insertService(Timer t, int type);

  // This should really only be called when destroying the object.
  void removeService();
  void removeService(int type);

  bool inService(int type);
  
  // Don't call this unless you have checked inService first.
  Timer whenService(int type);

  static void runService();
  static Timer nextService();

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
