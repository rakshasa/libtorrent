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

#ifndef LIBTORRENT_THROTTLE_CONTROL_H
#define LIBTORRENT_THROTTLE_CONTROL_H

#include <vector>

#include "settings.h"
#include "throttle.h"
#include "utils/task.h"

namespace torrent {

class ThrottleControl {
 public:
  typedef enum {
    SETTINGS_ROOT,
    SETTINGS_PEER
  } SettingsType;

  ThrottleControl();

  Throttle& root() { return m_root; }

  ThrottleSettings* settings(SettingsType type) {
    return &m_settings[type];
  }

  void update();

  Task& get_task_update() { return m_taskUpdate; }

  static ThrottleControl& global() { return m_global; }

 private:
  static ThrottleControl m_global;

  Throttle m_root;

  std::vector<ThrottleSettings> m_settings;

  Task     m_taskUpdate;
};

}

#endif
