#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "exceptions.h"
#include "settings.h"
#include "socket_base.h"
#include "throttle.h"
#include <numeric>
#include <algo/algo.h>

using namespace algo;

namespace torrent {

Throttle::Throttle() :
  m_parent(NULL),
  m_inActive(false),
  m_activeSize(0),
  m_left(0),
  m_spent(0),
  m_settings(NULL),
  m_socket(NULL) {
}

Throttle::~Throttle() {
  set_parent(NULL);

  while (!m_children.empty())
    m_children.front()->set_parent(NULL);
}

// Include in the calculation when parent delegates bandwidth.

// TODO: active should allocate a resonable amount of bytes for immidiate download.
// We can't do that in active, as it will be called regulary.
void Throttle::activate() {
  if (m_parent == NULL || m_inActive)
    return;

  m_inActive = true;

  m_parent->m_activeSize++;
  m_parent->m_active.insert(m_parent->m_active.end(), this);

  m_left = m_parent->m_left == UNLIMITED
    ? UNLIMITED
    : (m_parent->m_left / m_parent->m_activeSize);
}

void Throttle::idle() {
  if (m_parent == NULL || !m_inActive)
    return;

  m_inActive = false;

  m_parent->m_activeSize--;
  m_parent->m_active.erase(std::find(m_parent->m_active.begin(), m_parent->m_active.end(), this));
  
  m_spent = m_left = 0;
}

int Throttle::left() {
  if (m_left == UNLIMITED)
    return std::numeric_limits<int>::max() / 2;

  // TODO: Give chunks.
  return m_left;
}

void Throttle::spent(unsigned int bytes) {
  if (m_left == UNLIMITED)
    return;

  m_left -= bytes;
  m_spent += bytes;

  if (m_left < 0)
    throw internal_error("Throttle::spendt(...) error, used to many bytes");
}

// Called when we want to allocate more bandwidth.
// Returns the number of bytes actually taken.
int Throttle::update(float period, int bytes) {
  m_spent = 0;

  // Find out how much we are allowed. Int max if unlimited.
  // TODO: Adjust quickly if we are behind
  if (m_settings->constantRate != UNLIMITED)
    bytes = std::min((unsigned int)bytes,
		     (unsigned int)(m_settings->constantRate * period));

  // Make sure we always build up enough to be able to get past wakeupPoint.
  if (bytes < ThrottleSettings::wakeupPoint) {
    
    if (m_left + bytes > ThrottleSettings::wakeupPoint)
      bytes = std::max(ThrottleSettings::wakeupPoint - m_left, 0);

    m_left += bytes;

  } else {
    m_left = bytes;
  }

  // Not needed unless i fscked up something here.
  if (m_active.size() != (unsigned)m_activeSize)
    throw internal_error("Throttle::update(...) has invalid m_activeSize");

  if (m_socket &&
      (m_left >= ThrottleSettings::wakeupPoint || m_left == UNLIMITED))
    m_socket->insertWrite();

  // TODO: This needs to be alot more intelligent.

  if (m_left == UNLIMITED) {
    for (Children::iterator itr = m_active.begin(); itr != m_active.end(); ++itr)
      (*itr)->update(period, UNLIMITED);

  } else if (!m_active.empty()) {
    // The list should be well sorted already, so it propably won't spend
    // much time. (Besides, the list propably won't ever be larger than 20
    // even if you are sharing on a fat pipe)

    m_active.sort(lt(member(&Throttle::m_spent),
 		     member(&Throttle::m_spent)));

    int size = m_activeSize;
    int left = m_left;

    // Preliminary algorithm, this might not work well with low throttle.

    for (Children::iterator itr = m_active.begin(); itr != m_active.end(); ++itr, --size) {
      int allowed = left / size;
      int check = allowed;
      
      if ((*itr)->m_left > ThrottleSettings::starvePoint)
	// We didn't spend everything last turn, adjust to a more resonable value.
	allowed = std::min(allowed, (*itr)->m_spent + ThrottleSettings::starveBuffer);
      
      if (allowed > check)
	throw internal_error("Throttle::update(...) allowed borked big");

      left -= check = (*itr)->update(period, allowed);

      if (left < 0)
	throw internal_error("Throttle::update(...) 'left' less than zero");

      else if (check < 0 || check > allowed)
	throw internal_error("Throttle::update(...) update return value borked");
    }
  }

  return bytes;
}

void Throttle::set_parent(Throttle* parent) {
  if (m_parent) {
    idle();

    m_parent->m_children.erase(std::find(m_parent->m_children.begin(),
					 m_parent->m_children.begin(),
					 this));
  }

  m_parent = parent;

  if (parent) {
    m_parent->m_children.insert(m_parent->m_children.end(), this);
  }
}

void Throttle::set_settings(ThrottleSettings* settings) {
  m_settings = settings;
}

void Throttle::set_socket(SocketBase* socket) {
  m_socket = socket;
}

}
