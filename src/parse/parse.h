#ifndef LIBTORRENT_PARSE_H
#define LIBTORRENT_PARSE_H

namespace torrent {

class bencode;
class Content;
class TrackerControl;

void parse_tracker(const bencode& b, TrackerControl& tracker);
void parse_info(const bencode& b, Content& content);

}

#endif
