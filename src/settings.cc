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

// Time to wait before doing the next hash check if madvice WILL_NEED
// did not successfully retrive the whole chunk after 3 calls. Can be
// set to any positive number.
int Settings::hashForcedWait = 10000;

// Time to wait after HashQueue has called madvice telling the kernel
// to load into memory parts of the chunk. Seeks usually take atleast
// 6-7 ms. Can stay abit under that, but not to far.
int Settings::hashMadviceWait = 5000;

DownloadSettings* DownloadSettings::m_global = new DownloadSettings();

DownloadSettings::DownloadSettings() :
  minPeers(40),
  maxPeers(100),
  maxUploads(7),
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
