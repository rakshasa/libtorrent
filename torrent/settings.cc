#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "settings.h"

namespace torrent {

std::string Settings::httpName = "libtorrent/" VERSION;
std::string Settings::peerName = PEER_NAME;

int Settings::filesCheckWait = 5 * 1000;
int Settings::filesMode = 0666;
int Settings::dirMode = 0777;

int Settings::rateStart = 20 * 1000000;
int Settings::rateWindow = 60 * 1000000;
int Settings::rateQuick = 5 * 1000000;

DownloadSettings* DownloadSettings::m_global = new DownloadSettings();

DownloadSettings::DownloadSettings() :
  minPeers(30),
  maxPeers(50),
  maxUploads(7),
  chokeCycle(30 * 1000000),
  chokeGracePeriod(55 * 1000000)
{}

int ThrottleSettings::minPeriod = 1000000;
int ThrottleSettings::wakeupPoint = 2048;
int ThrottleSettings::starvePoint = 2048;
int ThrottleSettings::starveBuffer = 1024;

int ThrottleSettings::minChunk = 512;

ThrottleSettings::ThrottleSettings() :
  constantRate(-1)
{}

}
