#include "config.h"

#include "dht/transactions/dht_announce.h"

#include <cassert>

#include "dht/dht_bucket.h"
#include "dht/dht_node.h"
#include "tracker/tracker_dht.h"

namespace torrent::dht {

DhtAnnounce::DhtAnnounce(DhtServer* server, const HashString& infoHash, TrackerDht* tracker)
  : DhtSearch(server, infoHash),
    m_tracker(tracker) {
}

DhtAnnounce::~DhtAnnounce() {
  assert(complete() && "DhtAnnounce::~DhtAnnounce called while announce not complete.");

  const char* failure = NULL;

  if (m_tracker->get_dht_state() != TrackerDht::state_announcing) {
    if (!m_contacted)
      failure = "No DHT nodes available for peer search.";
    else
      failure = "DHT search unsuccessful.";

  } else {
    if (!m_contacted)
      failure = "DHT search unsuccessful.";
    else if (m_replied == 0 && !m_tracker->has_peers())
      failure = "Announce failed";
  }

  if (failure != NULL)
    m_tracker->receive_failed(failure);
  else
    m_tracker->receive_success();
}

DhtSearch::const_accessor
DhtAnnounce::start_announce() {
  trim(true);

  if (empty())
    return end();

  if (!complete() || m_next != end() || size() > DhtBucket::num_nodes)
    throw internal_error("DhtSearch::start_announce called in inconsistent state.");

  m_contacted = m_pending = size();
  m_replied = 0;
  m_tracker->set_dht_state(TrackerDht::state_announcing);

  for (const auto& [node, _] : *this)
    set_node_active(node, true);

  return const_accessor(begin());
}

void
DhtAnnounce::receive_peers(raw_list peers) {
  m_tracker->receive_peers(peers);
}

void
DhtAnnounce::update_status() {
  m_tracker->receive_progress(m_replied, m_contacted);
}

} // namespace torrent::dht
