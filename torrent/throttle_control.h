#ifndef LIBTORRENT_THROTTLE_CONTROL_H
#define LIBTORRENT_THROTTLE_CONTROL_H

#include "service.h"
#include "settings.h"
#include "throttle.h"
#include <vector>

namespace torrent {

class ThrottleControl : public Service {
 public:
  typedef enum {
    SETTINGS_ROOT
  } SettingsType;

  ThrottleControl();

  Throttle& root() { return m_root; }

  ThrottleSettings* settings(SettingsType type) {
    return &m_settings[type];
  }

  void service(int type);

 private:
  Throttle m_root;

  std::vector<ThrottleSettings> m_settings;
};

}

#endif
