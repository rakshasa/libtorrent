#include "config.h"

#include "dht/transactions/dht_announce.h"

#include <cassert>

#include "dht/dht_bucket.h"
#include "dht/dht_node.h"
#include "net/address_list.h"
#include "tracker/tracker_dht.h"

namespace torrent::dht {

DhtAnnounce::DhtAnnounce(DhtServer* server, const HashString& infoHash, std::weak_ptr<TrackerDht> tracker)
  : DhtSearch(server, infoHash),
    m_tracker(tracker) {
}

DhtAnnounce::~DhtAnnounce() {
  assert(complete() && "DhtAnnounce::~DhtAnnounce called while announce not complete.");

  TrackerDht::add_event(m_tracker, [contacted = m_contacted, replied = m_replied](TrackerDht* tracker) {
      const char* failure = nullptr;

      if (tracker->dht_state() != TrackerDht::state_announcing) {
        if (!contacted)
          failure = "No DHT nodes available for peer search.";
        else
          failure = "DHT search unsuccessful.";

      } else {
        if (!contacted)
          failure = "DHT search unsuccessful.";
        else if (replied == 0) // && !tracker->has_peers_unsafe())
          failure = "Announce failed";
      }

      if (failure != nullptr)
        tracker->receive_failed(failure);
      else
        tracker->receive_success();
    });
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

  TrackerDht::add_event(m_tracker, [](TrackerDht* tracker) {
      tracker->set_dht_announce_state();
    });

  for (const auto& [node, _] : *this)
    set_node_active(node, true);

  return const_accessor(begin());
}

void
DhtAnnounce::receive_peers(raw_list peers) {
  AddressList address_list;
  address_list.parse_address_bencode(peers);

  TrackerDht::add_event(m_tracker, [address_list = std::move(address_list)](TrackerDht* tracker) mutable {
      tracker->receive_peers(std::move(address_list));
    });
}

void
DhtAnnounce::update_status() {
  TrackerDht::add_event(m_tracker, [contacted = m_contacted, replied = m_replied](TrackerDht* tracker) {
      tracker->receive_progress(replied, contacted);
    });
}

} // namespace torrent::dht
