#include "config.h"

#include "torrent/exceptions.h"
#include "download/download_main.h"

#include "torrent/bencode.h"
#include "parse.h"
#include "general.h"

namespace torrent {

void parse_main(const Bencode& b, DownloadMain& download) {
  download.set_name(b["info"]["name"].as_string());
}

}
