#include "config.h"

#include "torrent/exceptions.h"
#include "tracker/tracker_control.h"

#include "utils/bencode.h"
#include "parse.h"

namespace torrent {

void parse_tracker(const Bencode& b, TrackerControl& tracker) {
  // TODO: Add multi-tracker support here.

  if (!b.has_key("announce") || !b["announce"].is_string())
    throw bencode_error("Tracker info not found or invalid");

  tracker.add_url(b["announce"].as_string());
}

}
