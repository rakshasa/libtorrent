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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <inttypes.h>
#include "settings.h"

namespace torrent {

std::string Settings::httpName = "libtorrent/" VERSION;
std::string Settings::peerName = PEER_NAME;

int Settings::filesCheckWait = 5 * 1000;
int Settings::filesMode = 0666;
int Settings::dirMode = 0777;

int Settings::rateStart = 20 * 1000000;
int Settings::rateWindow = 60 * 1000000;
int Settings::rateQuick = 10 * 1000000;
int Settings::rateSample = 1000000;

int Settings::hashWait = 100000;
int Settings::hashWillneed = 10 << 20; // Bytes to preload, rounded up to the nearest chunk.
int Settings::hashTries = 5;

DownloadSettings* DownloadSettings::m_global = new DownloadSettings();

DownloadSettings::DownloadSettings() :
  minPeers(40),
  maxPeers(100),
  maxAvailable(1000),          // Max number of unconnected peers to keep that it has received from the tracker.
  chokeCycle(30 * 1000000),
  chokeGracePeriod(55 * 1000000),
  stallTimeout(160 * 1000000), // Less than 0.1 kb/s
  cancelTimeout(10 * 1000000), // choke/unchoke should be in the same packet, especially if the peer is lagged.
  endgameBorder(0),
  endgameRate(10000)           // Only include stalled peers if the endgame mode if we're slow.
{}

int ThrottleSettings::minPeriod = 1000000;
int ThrottleSettings::wakeupPoint = 2048;
int ThrottleSettings::starvePoint = 2048;
int ThrottleSettings::starveBuffer = 1024;

// Send out 512 byte chunks
int ThrottleSettings::minChunkMask = ~((1 << 9) - 1);

ThrottleSettings::ThrottleSettings() :
  constantRate(-1)
{}

}
