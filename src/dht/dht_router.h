#ifndef LIBTORRENT_DHT_ROUTER_H
#define LIBTORRENT_DHT_ROUTER_H

#include "dht/dht_node.h"
#include "dht/dht_hash_map.h"
#include "dht/dht_server.h"
#include "rak/socket_address.h"
#include "torrent/hash_string.h"
#include "torrent/object.h"
#include "torrent/net/types.h"
#include "torrent/tracker/dht_controller.h"
#include "torrent/utils/scheduler.h"

#include <optional>

namespace torrent {

class DhtBucket;
class DhtTracker;
class TrackerDht;

// Main DHT class, maintains the routing table of known nodes and talks to the
// DhtServer object that handles the actual communication.

class DhtRouter : public DhtNode {
public:
  // How many bytes to return and verify from the 20-byte SHA token.
  static constexpr unsigned int size_token = 8;

  static constexpr unsigned int timeout_bootstrap_retry  =          60;  // Retry initial bootstrapping every minute.
  static constexpr unsigned int timeout_update           =     15 * 60;  // Regular housekeeping updates every 15 minutes.
  static constexpr unsigned int timeout_bucket_bootstrap =     15 * 60;  // Bootstrap idle buckets after 15 minutes.
  static constexpr unsigned int timeout_remove_node      = 4 * 60 * 60;  // Remove unresponsive nodes after 4 hours.
  static constexpr unsigned int timeout_peer_announce    =     30 * 60;  // Remove peers which haven't reannounced for 30 minutes.

  // A node ID of all zero.
  static HashString zero_id;

  DhtRouter(const Object& cache, const rak::socket_address* sa);
  ~DhtRouter();

  void                start(int port);
  void                stop();

  bool                is_active()                        { return m_server.is_active(); }

  // Pass NULL to cancel_announce to cancel all announces for the tracker.
  void                announce(const HashString& info_hash, TrackerDht* tracker);
  void                cancel_announce(const HashString* info_hash, const TrackerDht* tracker);

  // Returns NULL if not tracking the torrent unless create is true.
  DhtTracker*         get_tracker(const HashString& hash, bool create);

  // Check if we are interested in inserting a new node of the given ID
  // into our table (i.e. if we have space or bad nodes in the corresponding bucket).
  bool                want_node(const HashString& id);

  // Add the given host to the list of potential contacts if we haven't
  // completed the bootstrap process, or contact the given address directly.
  void                add_contact(const std::string& host, int port);
  void                contact(const sockaddr* sa, int port);

  // Retrieve node of given ID in constant time. Return NULL if not found, unless
  // it's our own ID in which case it returns the DhtRouter object.
  DhtNode*            get_node(const HashString& id);

  // Search for node with given address in O(n), disregarding the port.
  DhtNode*            find_node(const rak::socket_address* sa);

  // Whenever a node queries us, replies, or is confirmed inactive (no reply) or
  // invalid (reply with wrong ID), we need to update its status.
  DhtNode*            node_queried(const HashString& id, const rak::socket_address* sa);
  DhtNode*            node_replied(const HashString& id, const rak::socket_address* sa);
  DhtNode*            node_inactive(const HashString& id, const rak::socket_address* sa);
  void                node_invalid(const HashString& id);

  // Store compact node information (26 bytes) for nodes closest to the
  // given ID in the given buffer, return new buffer end.
  raw_string          get_closest_nodes(const HashString& id)  { return find_bucket(id)->second->full_bucket(); }

  // Store DHT cache in the given container.
  Object*             store_cache(Object* container) const;

  // Create and verify a token. Tokens are valid between 15-30 minutes from creation.
  raw_string          make_token(const rak::socket_address* sa, char* buffer);
  bool                token_valid(raw_string token, const rak::socket_address* sa);

  tracker::DhtController::statistics_type get_statistics() const;
  void                                    reset_statistics()  { m_server.reset_statistics(); }

  void                set_upload_throttle(ThrottleList* t)    { m_server.set_upload_throttle(t); }
  void                set_download_throttle(ThrottleList* t)  { m_server.set_download_throttle(t); }

private:
  // Hostname and port of potential bootstrap nodes.
  using contact_t = std::pair<std::string, int>;

  // Number of nodes we need to consider the bootstrap process complete.
  static constexpr unsigned int num_bootstrap_complete = 32;

  // Maximum number of potential contacts to keep until bootstrap complete.
  static constexpr unsigned int num_bootstrap_contacts = 64;

  using DhtBucketList = std::map<const HashString, DhtBucket*>;

  DhtBucketList::iterator find_bucket(const HashString& id);

  bool                add_node_to_bucket(DhtNode* node);
  void                delete_node(const DhtNodeList::accessor& itr);

  void                store_closest_nodes(const HashString& id, DhtBucket* bucket);

  DhtBucketList::iterator split_bucket(const DhtBucketList::iterator& itr, DhtNode* node);

  void                bootstrap();
  void                bootstrap_bucket(const DhtBucket* bucket);

  void                receive_timeout();
  void                receive_timeout_bootstrap();

  // buffer needs to hold an SHA1 hash (20 bytes), not just the token (8 bytes)
  char*               generate_token(const rak::socket_address* sa, int token, char buffer[20]);

  utils::SchedulerEntry m_task_timeout;

  DhtServer           m_server{nullptr};
  DhtNodeList         m_nodes;
  DhtBucketList       m_routingTable;
  DhtTrackerList      m_trackers;

  std::optional<std::deque<contact_t>> m_contacts;

  int                 m_numRefresh{0};

  bool                m_networkUp;

  // Secret keys used for generating announce tokens.
  int                 m_curToken;
  int                 m_prevToken;
};

inline raw_string
DhtRouter::make_token(const rak::socket_address* sa, char* buffer) {
  return raw_string(generate_token(sa, m_curToken, buffer), size_token);
}

} // namespace torrent

#endif
