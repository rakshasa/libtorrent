#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <inttypes.h>

#include "general.h"
#include "bitfield.h"
#include "exceptions.h"
#include "download.h"
#include "peer_handshake.h"
#include "settings.h"

#include <stdlib.h>
#include <iomanip>
#include <openssl/sha.h>
#include <sys/time.h>

namespace torrent {

std::string generateId() {
  std::string id = Settings::peerName;

  for (int i = id.length(); i < 20; ++i)
    id += random();

  return id;
}

std::string calcHash(const bencode& b) {
  std::stringstream str;
  str << b;

  return std::string((const char*)SHA1((const unsigned char*)(str.str().c_str()), str.str().length(), NULL), 20);
}

std::vector<std::string> partitionLine(char*& pos, char* end) {
  std::vector<std::string> l;
  std::string s;

  while (pos != end && *pos != '\n') {

    if ((*pos == ' ' || *pos == '\r' || *pos == '\t') &&
	s.length()) {
      l.push_back(s);
      s = std::string();
    } else {
      s += *pos;
    }

    ++pos;
  }

  return pos != end ? l : std::vector<std::string>();
}

}
