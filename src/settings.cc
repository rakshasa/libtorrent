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

DownloadSettings* DownloadSettings::m_global = new DownloadSettings();

DownloadSettings::DownloadSettings() :
  minPeers(40),
  maxPeers(100),
  maxUploads(7),
  chokeCycle(30 * 1000000),
  chokeGracePeriod(55 * 1000000),
  stallTimeout(40 * 1000000), // Less than 0.5 kb/s
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
