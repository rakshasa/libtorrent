// libTorrent - BitTorrent library
// Copyright (C) 2005, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

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
