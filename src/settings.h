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

#ifndef LIBTORRENT_SETTINGS_H
#define LIBTORRENT_SETTINGS_H

#include <string>
#include <inttypes.h>

namespace torrent {

class Settings {
 public:
  static std::string httpName;
  static std::string peerName;

  static int filesCheckWait;
  static int filesMode;
  static int dirMode;

  static int rateStart;
  static int rateWindow;
  static int rateQuick;
  static int rateSample;
};

class DownloadSettings {
 public:
  DownloadSettings();

  int minPeers;
  int maxPeers;
  uint32_t maxAvailable;

  int chokeCycle;
  int chokeGracePeriod;

  // How long before we stall a download.
  int stallTimeout;

  // Time to wait after a choke before we consider it final.
  int cancelTimeout;

  int32_t endgameBorder;
  uint32_t endgameRate;

  static DownloadSettings& global() { return *m_global; }

 private:
  static DownloadSettings* m_global;
};

class ThrottleSettings { 
 public:
  ThrottleSettings();

  int constantRate;

  static int minPeriod;
  static int wakeupPoint;

  // If greater than 'starvePoint' bytes was left then limit the amount
  // allocated.
  static int starvePoint;

  // Each turn give spent + starveBuffer bytes if not starved.
  static int starveBuffer;

  // Min size of chunks to send out.
  static int minChunkMask;
};

}

#endif // LIBTORRENT_SETTINGS_H
