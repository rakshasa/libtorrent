#ifndef LIBTORRENT_GENERAL_H
#define LIBTORRENT_GENERAL_H

#include <string>
#include <stdint.h>
#include <vector>

namespace torrent {

class BitField;
class bencode;

std::string generateId();

std::string calcHash(const bencode& b);

std::vector<std::string> partitionLine(char*& pos, char* end);

} // namespace torrent

#endif // LIBTORRENT_TORRENT_H
