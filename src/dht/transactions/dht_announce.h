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
  DhtAnnounce(DhtServer* server, const HashString& infoHash, std::weak_ptr<TrackerDht> tracker);
  ~DhtAnnounce() override;

  bool                 is_announce() const override      { return true; }

  const auto&          tracker() const                   { return m_tracker; }

  using announce_target = std::pair<const DhtNode*, raw_string>;

  // Return the closest good nodes, with the tokens they returned during the walk, to announce to.
  std::vector<announce_target> start_announce();

  void                 set_token(const HashString& id, raw_string token);

  void                 receive_peers(raw_list peers);
  void                 update_status();

private:
  std::weak_ptr<TrackerDht> m_tracker;

  std::map<HashString, std::string> m_tokens;
};

} // namespace torrent::dht

#endif
