#ifndef LIBTORRENT_PARSE_INFO_H
#define LIBTORRENT_PARSE_INFO_H

namespace torrent {

class bencode;
class Content;

void parse_info(const bencode& bencode, Content& content);

}

#endif
