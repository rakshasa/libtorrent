#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "throttle_control.h"

namespace torrent {

ThrottleControl::ThrottleControl() :
  m_settings(1) {

  m_root.set_settings(&m_settings[SETTINGS_ROOT]);

  m_settings[SETTINGS_ROOT].unlimited = false;
  m_settings[SETTINGS_ROOT].constantRate = 5000;
}

void ThrottleControl::service(int type) {
  m_root.update(ThrottleSettings::minPeriod, Throttle::UNLIMITED);

  // TODO: we lose some time, adjust.
  insertService(Timer::cache() + ThrottleSettings::minPeriod, 0);
}

}
