#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "torrent/exceptions.h"
#include "throttle_control.h"

namespace torrent {

ThrottleControl ThrottleControl::m_global;

ThrottleControl::ThrottleControl() :
  m_settings(2) {

  m_root.set_settings(&m_settings[SETTINGS_ROOT]);

  m_settings[SETTINGS_ROOT].constantRate = Throttle::UNLIMITED;
  m_settings[SETTINGS_PEER].constantRate = Throttle::UNLIMITED;
}

void ThrottleControl::service(int type) {
  m_root.update(ThrottleSettings::minPeriod / 1000000.0f, Throttle::UNLIMITED);

  // TODO: Remove this later
  if (in_service(0))
    throw internal_error("Duplicate ThrottleService in service");

  // TODO: we lose some time, adjust.
  insert_service(Timer::cache() + ThrottleSettings::minPeriod, 0);
}

}
