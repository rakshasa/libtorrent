#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "exceptions.h"
#include "settings.h"
#include "socket_base.h"
#include "throttle.h"
#include <algo/algo.h>

using namespace algo;

namespace torrent {

Throttle::Throttle() :
  m_parent(NULL),
  m_activeSize(0),
  m_left(0),
  m_settings(NULL),
  m_socket(NULL) {
}

Throttle::~Throttle() {
  idle();

  while (!m_active.empty())
    m_active.front()->idle();
}

// Include in the calculation when parent delegates bandwidth.
void Throttle::activate() {
  if (m_parent == NULL || m_activeItr != m_parent->m_active.end())
    return;

  m_parent->m_activeSize++;

  m_activeItr = m_parent->m_active.insert(m_parent->m_active.end(), this);
}

void Throttle::idle() {
  if (m_parent == NULL || m_activeItr == m_parent->m_active.end())
    return;

  m_parent->m_activeSize--;
  m_parent->m_active.erase(m_activeItr);
  
  m_activeItr = m_parent->m_active.end();
}

void Throttle::spent(unsigned int bytes) {
  if (m_left < 0)
    return;

  m_left -= bytes;

  if (m_left < 0)
    throw internal_error("Throttle::spendt(...) error, used to many bytes");
}

// Called when we want to allocate more bandwidth.
// If bytes == -1 then unlimited.
void Throttle::update(float period, int bytes) {
  // Find out how much we are allowed. Int max if unlimited.
  if (m_settings->unlimited)
    m_left = bytes;

  else
    m_left = std::min<unsigned int>(bytes,
				    // TODO: Adjust quickly if we are behind
				    (unsigned int)(m_settings->constantRate * period));

  // Not needed unless i fscked up something here.
  if (!m_active.empty() && m_activeSize <= 0)
    throw internal_error("Throttle::update(...) has valid m_childrenSize");

  if (m_socket &&
      (m_left >= ThrottleSettings::wakeupPoint || m_left == UNLIMITED))
    m_socket->insertWrite();

  // TODO: This needs to be alot more intelligent.

  // Divide the bandwidth amongst children if present.
  for (Children::iterator itr = m_active.begin(); ++itr != m_active.end(); ++itr) {
    (*itr)->update(period,
		   m_left == UNLIMITED ? UNLIMITED : m_left / m_activeSize);
  }
}

void Throttle::set_parent(Throttle* parent) {
  if (m_parent) {
    m_parent->m_children.erase(m_childrenItr);
    m_parent->m_childrenSize--;
  }

  m_parent = parent;

  if (parent) {
    m_childrenItr = m_parent->m_children.insert(m_parent->m_children.end(), this);
    m_parent->m_childrenSize++;
  }
}

void Throttle::set_settings(ThrottleSettings* settings) {
  m_settings = settings;
}

}
