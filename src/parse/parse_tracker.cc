#include "config.h"

#include "torrent/exceptions.h"
#include "tracker/tracker_control.h"

#include "torrent/bencode.h"
#include "parse.h"

namespace torrent {

struct _add_tracker {
  _add_tracker(int group, TrackerControl* control) : m_group(group), m_control(control) {}

  void operator () (const Bencode& b) {
    if (!b.is_string())
      throw bencode_error("Tracker entry not a string");
    
    m_control->add_url(m_group, b.as_string());
  }

  int             m_group;
  TrackerControl* m_control;
};

struct _add_tracker_group {
  _add_tracker_group(TrackerControl* control) : m_group(0), m_control(control) {}

  void operator () (const Bencode& b) {
    if (!b.is_list())
      throw bencode_error("Tracker group list not a list");

    std::for_each(b.as_list().begin(), b.as_list().end(), _add_tracker(m_group++, m_control));
  }

  int             m_group;
  TrackerControl* m_control;
};

void parse_tracker(const Bencode& b, TrackerControl* control) {
  if (b.has_key("announce-list") && b["announce-list"].is_list())
    std::for_each(b["announce-list"].as_list().begin(), b["announce-list"].as_list().end(),
		  _add_tracker_group(control));

  else if (b.has_key("announce"))
    _add_tracker(0, control)(b["announce"]);

  else
    throw bencode_error("Could not find any trackers");
}

}
