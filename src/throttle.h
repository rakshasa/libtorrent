#ifndef LIBTORRENT_THROTTLE_H
#define LIBTORRENT_THROTTLE_H

#include "utils/rate.h"
#include <list>

namespace torrent {

// Top down, torrent::work inserts a class into service. Do insertWrite on every eligble
// node.

class ThrottleSettings;
class SocketBase;

class Throttle {
 public:
  typedef std::list<Throttle*> Children;

  static const int UNLIMITED = -1;

  Throttle();
  ~Throttle();

  // Include in the calculation when parent delegates bandwidth.
  void activate();
  void idle();

  bool in_active() { return m_inActive; }

  void set_snub(bool s) { m_snub = s; }
  bool get_snub() { return m_snub; }

  Rate& up() { return m_up; }
  Rate& down() { return m_down; }
  
  int left();
  
  void spent(unsigned int bytes);

  // Called when we want to allocate more bandwidth.
  int update(float period, int bytes);

  const Children& children() { return m_children; }
  const Children& active() { return m_active; }

  void set_parent(Throttle* parent);
  void set_settings(ThrottleSettings* settings);
  void set_socket(SocketBase* socket);
  
 private:
  // Disable ctor
  Throttle(const Throttle&);
  void operator = (const Throttle&);

  Throttle* m_parent;

  Children m_children;
  Children m_active;

  bool m_inActive;
  bool m_snub;

  int m_activeSize;

  int m_left;
  int m_spent;

  Rate m_up;
  Rate m_down;

  ThrottleSettings* m_settings;
  SocketBase* m_socket;
};

}

#endif
