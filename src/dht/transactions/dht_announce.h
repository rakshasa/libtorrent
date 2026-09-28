#ifndef LIBTORRENT_DHT_TRANSACTIONS_DHT_ANNOUNCE_H
#define LIBTORRENT_DHT_TRANSACTIONS_DHT_ANNOUNCE_H

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

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

  using announce_target = std::pair<const DhtNode*, raw_string>;

  // Return the closest good nodes, with the tokens they returned during the walk, to announce to.
  std::vector<announce_target> start_announce();

  void                 set_token(const HashString& id, raw_string token);

  void                 receive_peers(raw_list peers);
  void                 update_status();

private:
  std::weak_ptr<TrackerDht> m_tracker;
  std::map<HashString, std::string> m_tokens;
  bool                      m_primary;
};

} // namespace torrent::dht

#endif
