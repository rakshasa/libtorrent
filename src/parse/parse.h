#ifndef LIBTORRENT_PARSE_H
#define LIBTORRENT_PARSE_H

namespace torrent {

class Bencode;
class Content;
class DownloadMain;
class TrackerControl;

void parse_main(const Bencode& b, DownloadMain& download);
void parse_tracker(const Bencode& b, TrackerControl& tracker);
void parse_info(const Bencode& b, Content& content);

}

#endif
