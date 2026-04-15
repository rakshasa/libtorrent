#ifndef LIBTORRENT_DHT_TRANSACTIONS_DHT_ANNOUNCE_H
#define LIBTORRENT_DHT_TRANSACTIONS_DHT_ANNOUNCE_H

#include <memory>

#include "dht/transactions/dht_search.h"
#include "torrent/common.h"

// DhtAnnounce is a derived class used for searches that will eventually lead to an announce to the
// closest nodes.

namespace torrent {

class DhtBucket;
class TrackerDht;

}

namespace torrent::dht {

class DhtAnnounce : public DhtSearch {
public:
  DhtAnnounce(DhtServer* server, const HashString& infoHash, TrackerDht* tracker);
  ~DhtAnnounce() override;

  bool                 is_announce() const override      { return true; }

  const TrackerDht*    tracker() const                   { return m_tracker; }

  // Start announce and return final set of nodes in get_contact() calls.
  // This resets DhtSearch's completed() function, which now
  // counts announces instead.
  const_accessor       start_announce();

  void                 receive_peers(raw_list peers);
  void                 update_status();

private:
  TrackerDht*          m_tracker;
};

} // namespace torrent::dht

#endif
