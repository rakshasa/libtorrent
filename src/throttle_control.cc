#include "config.h"

#include <sigc++/slot.h>

#include "torrent/exceptions.h"
#include "throttle_control.h"

namespace torrent {

ThrottleControl ThrottleControl::m_global;

ThrottleControl::ThrottleControl() :
  m_settings(2),
  m_taskUpdate(sigc::mem_fun(*this, &ThrottleControl::update)) {

  m_root.set_settings(&m_settings[SETTINGS_ROOT]);

  m_settings[SETTINGS_ROOT].constantRate = Throttle::UNLIMITED;
  m_settings[SETTINGS_PEER].constantRate = Throttle::UNLIMITED;
}

void
ThrottleControl::update() {
  m_root.update(ThrottleSettings::minPeriod / 1000000.0f, Throttle::UNLIMITED);

  // TODO: Remove this later
  if (m_taskUpdate.is_scheduled())
    throw internal_error("Duplicate ThrottleService in task schedule");

  // TODO: we lose some time, adjust.
  m_taskUpdate.insert(Timer::cache() + ThrottleSettings::minPeriod);
}

}
