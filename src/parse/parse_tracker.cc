#include "config.h"

#include "torrent/exceptions.h"
#include "tracker/tracker_control.h"

#include "bencode.h"
#include "parse.h"

namespace torrent {

void parse_tracker(const bencode& b, TrackerControl& tracker) {
  // TODO: Add multi-tracker support here.

  if (!b.hasKey("announce") || !b["announce"].isString())
    throw bencode_error("Tracker info not found or invalid");

  tracker.add_url(b["announce"].asString());
}

}
