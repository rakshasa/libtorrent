#ifndef LIBTORRENT_GENERAL_H
#define LIBTORRENT_GENERAL_H

#include <string>
#include <vector>

namespace torrent {

class BitField;
class Bencode;

std::string generateId();
std::string generateKey();

std::string calcHash(const Bencode& b);

} // namespace torrent

#endif // LIBTORRENT_TORRENT_H
