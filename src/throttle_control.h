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
