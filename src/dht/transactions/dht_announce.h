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
  // A primary announce reports progress, the announcing state and success or failure to the tracker; a
  // secondary one (the other address family's DHT, BEP 32) delivers peers only, so one family's empty table
  // never fails a tracker the other family serves.
  DhtAnnounce(DhtServer* server, const HashString& infoHash, std::weak_ptr<TrackerDht> tracker, bool primary = true);
  ~DhtAnnounce() override;

  bool                 is_announce() const override      { return true; }

  const auto&          tracker() const                   { return m_tracker; }
  bool                 is_primary() const                { return m_primary; }

  // Start announce and return final set of nodes in get_contact() calls.
  // This resets DhtSearch's completed() function, which now
  // counts announces instead.
  const_accessor       start_announce();

  void                 receive_peers(raw_list peers);
  void                 update_status();

private:
  std::weak_ptr<TrackerDht> m_tracker;
  bool                      m_primary;
};

} // namespace torrent::dht

#endif
