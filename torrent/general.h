#ifndef LIBTORRENT_GENERAL_H
#define LIBTORRENT_GENERAL_H

#include <string>
#include <vector>

namespace torrent {

class BitField;
class bencode;

std::string generateId();
std::string generateKey();

std::string calcHash(const bencode& b);

std::vector<std::string> partitionLine(char*& pos, char* end);

} // namespace torrent

#endif // LIBTORRENT_TORRENT_H
