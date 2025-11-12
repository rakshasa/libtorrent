#include "config.h"

#include "dht_router.h"

#include <cassert>

#include "dht_bucket.h"
#include "dht_tracker.h"
#include "dht_transaction.h"
#include "torrent/exceptions.h"
#include "torrent/net/resolver.h"
#include "torrent/net/socket_address.h"
#include "torrent/tracker/dht_controller.h"
#include "torrent/utils/log.h"
#include "utils/sha1.h"

#define LT_LOG_THIS(log_fmt, ...)                                       \
  lt_log_print_hash(torrent::LOG_DHT_ROUTER, this->id(), "dht_router", log_fmt, __VA_ARGS__);

namespace torrent {

HashString DhtRouter::zero_id;

DhtRouter::DhtRouter(const Object& cache) :
  DhtNode(zero_id, sa_make_inet_any().get()), // actual ID is set later
  m_server(this),
  m_curToken(random()),
  m_prevToken(random()) {

  HashString ones_id;

  zero_id.clear();
  ones_id.clear(0xFF);

  if (cache.has_key("self_id")) {
    const std::string& id = cache.get_key_string("self_id");

    if (id.length() != HashString::size_data)
      throw bencode_error("Loading cache: Invalid ID.");

    assign(id.c_str());

  } else {
    long buffer[size_data];

    for (long* itr = buffer; itr != buffer + size_data; ++itr)
      *itr = random();

    Sha1 sha;
    sha.init();
    sha.update(buffer, sizeof(buffer));
    sha.final_c(data());
  }

  LT_LOG_THIS("creating", 0);

  set_bucket(new DhtBucket(zero_id, ones_id));
  m_routingTable.emplace(bucket()->id_range_end(), bucket());

  if (cache.has_key("nodes")) {
    const Object::map_type& nodes = cache.get_key_map("nodes");

    LT_LOG_THIS("adding nodes : size:%zu", nodes.size());

    for (const auto& [id, node] : nodes) {
      if (id.length() != HashString::size_data)
        throw bencode_error("Loading cache: Invalid node hash.");

      add_node_to_bucket(m_nodes.add_node(new DhtNode(id, node)));
    }
  }

  if (m_nodes.size() < num_bootstrap_complete) {
    m_contacts.emplace();

    if (cache.has_key("contacts")) {
      for (const auto& contact : cache.get_key_list("contacts")) {
        auto litr = contact.as_list().begin();
        auto host = litr->as_string();
        auto port = std::next(litr)->as_value();

        m_contacts->emplace_back(std::move(host), port);
      }
    }
  }
}

DhtRouter::~DhtRouter() {
  assert(!is_active() && "DhtRouter::~DhtRouter() called while still active.");

  for (auto& route : m_routingTable)
    delete route.second;

  for (auto& tracker : m_trackers)
    delete tracker.second;

  for (auto& node : m_nodes)
    delete node.second;
}

void
DhtRouter::start(int port) {
  LT_LOG_THIS("starting: port:%d", port);

  m_server.start(port);

  // Set timeout slot and schedule it to be called immediately for initial bootstrapping if
  // necessary.
  m_task_timeout.slot() = [this] { receive_timeout_bootstrap(); };

  this_thread::scheduler()->wait_for_ceil_seconds(&m_task_timeout, 1s);
}

void
DhtRouter::stop() {
  if (!is_active())
    return;

  LT_LOG_THIS("stopping", 0);

  this_thread::resolver()->cancel(this);
  this_thread::scheduler()->erase(&m_task_timeout);

  m_server.stop();
}

// Start a DHT get_peers and announce_peer request.
void
DhtRouter::announce(const HashString& info_hash, TrackerDht* tracker) {
  m_server.announce(*find_bucket(info_hash)->second, info_hash, tracker);
}

// Cancel any running requests from the given tracker.
// If info or tracker is not NULL, only cancel matching requests.
void
DhtRouter::cancel_announce(const HashString* info_hash, const TrackerDht* tracker) {
  m_server.cancel_announce(info_hash, tracker);
}

DhtTracker*
DhtRouter::get_tracker(const HashString& hash, bool create) {
  auto itr = m_trackers.find(hash);

  if (itr != m_trackers.end())
    return itr->second;

  if (!create)
    return NULL;

  auto [tr, ins] = m_trackers.emplace(hash, new DhtTracker());

  if (!ins)
    throw internal_error("DhtRouter::get_tracker did not actually insert tracker.");

  return tr->second;
}

bool
DhtRouter::want_node(const HashString& id) {
  // We don't want to add ourself.  Also, too many broken implementations
  // advertise an ID of 0, which causes collisions, so reject that.
  if (id == this->id() || id == zero_id)
    return false;

  // We are always interested in more nodes for our own bucket (causing it
  // to be split if full); in other buckets only if there's space.
  DhtBucket* b = find_bucket(id)->second;
  return b == bucket() || b->has_space();
}

DhtNode*
DhtRouter::get_node(const HashString& id) {
  DhtNodeList::accessor itr = m_nodes.find(&id);

  if (itr == m_nodes.end()) {
    if (id == this->id())
      return this;
    else
      return NULL;
  }

  return itr.node();
}

DhtRouter::DhtBucketList::iterator
DhtRouter::find_bucket(const HashString& id) {
  auto itr = m_routingTable.lower_bound(id);

#ifdef USE_EXTRA_DEBUG
  if (itr == m_routingTable.end())
    throw internal_error("DHT Buckets not covering entire ID space.");

  if (!itr->second->is_in_range(id))
    throw internal_error("DhtRouter::find_bucket, m_routingTable.lower_bound did not find correct bucket.");
#endif

  return itr;
}

void
DhtRouter::add_contact(const std::string& host, int port) {
  // Externally obtained nodes are added to the contact list, but only if
  // we're still bootstrapping. We don't contact external nodes after that.
  if (m_contacts.has_value()) {
    if (m_contacts->size() >= num_bootstrap_contacts)
      m_contacts->pop_front();

    m_contacts->emplace_back(host, port);
  }
}

void
DhtRouter::contact(const sockaddr* sa, int port) {
  if (!is_active())
    return;

  auto sa_tmp = sa_copy_unmapped(sa);

  if (sa_tmp->sa_family != AF_INET && sa_tmp->sa_family != AF_INET6)
    throw input_error("DhtRouter::contact() called with non-inet/inet6 address.");

  // Currently only IPv4 is supported.
  if (sa_tmp->sa_family != AF_INET)
    return;

  if (sap_is_any(sa_tmp))
    throw input_error("DhtRouter::contact() called with any address.");

  sap_set_port(sa_tmp, port);

  m_server.ping(zero_id, sa_tmp.get());
}

// Received a query from the given node. If it has previously replied
// to one of our queries, consider it alive and update the bucket mtime,
// otherwise if we could use it in a bucket, try contacting it.
DhtNode*
DhtRouter::node_queried(const HashString& id, const sockaddr* sa) {
  DhtNode* node = get_node(id);

  if (node == nullptr) {
    if (want_node(id))
      m_server.ping(id, sa);

    return nullptr;
  }

  // If we know the ID but the address is different, don't set the original node
  // active, but neither use this new address to prevent rogue nodes from polluting
  // our routing table with fake source addresses.
  if (!sa_equal_addr(node->address(), sa))
    return nullptr;

  node->queried();

  if (node->is_good())
    node->bucket()->touch();

  return node;
}

// Received a reply from a node we queried.
// Check that it matches the information we have, set that it has replied
// and update the bucket mtime.
DhtNode*
DhtRouter::node_replied(const HashString& id, const sockaddr* sa) {
  DhtNode* node = get_node(id);

  if (node == NULL) {
    if (!want_node(id))
      return NULL;

    // New node, create it. It's a good node (it replied!) so add it to a bucket.
    node = m_nodes.add_node(new DhtNode(id, sa));

    if (!add_node_to_bucket(node))   // deletes the node if it fails
      return NULL;
  }

  if (!sa_equal_addr(node->address(), sa))
    return NULL;

  node->replied();
  node->bucket()->touch();

  return node;
}

// A node has not replied to one of our queries.
DhtNode*
DhtRouter::node_inactive(const HashString& id, const sockaddr* sa) {
  DhtNodeList::accessor itr = m_nodes.find(&id);

  // If not found add it to some blacklist so we won't try contacting it again immediately?
  if (itr == m_nodes.end())
    return NULL;

  // Check source address. Normally node_inactive is called if we DON'T receive a reply,
  // however it can also be called if a node replied with an malformed response packet,
  // so check that the address matches so that a rogue node cannot cause other nodes
  // to be considered bad by sending malformed packets.
  if (!sa_equal_addr(itr.node()->address(), sa))
    return NULL;

  itr.node()->inactive();

  // Old node age normally implies no replies for many consecutive queries, however
  // after loading the node cache after a day or more we want to give each node a few
  // chances to reply again instead of removing all nodes instantly.
  if (itr.node()->is_bad() && itr.node()->age() >= timeout_remove_node) {
    delete_node(itr);
    return NULL;
  }

  return itr.node();
}

// We sent a query to the given node ID, but received a reply from a different
// node ID, that means the address of the original ID is invalid now.
void
DhtRouter::node_invalid(const HashString& id) {
  DhtNode* node = get_node(id);

  if (node == NULL || node == this)
    return;

  delete_node(m_nodes.find(&node->id()));
}

Object*
DhtRouter::store_cache(Object* container) const {
  container->insert_key("self_id", str());

  // Insert all nodes.
  Object& nodes = container->insert_key("nodes", Object::create_map());
  for (const auto& [id, node] : m_nodes) {
    if (!node->is_bad())
      node->store_cache(&nodes.insert_key(id->str(), Object::create_map()));
  }

  // Insert contacts, if we have any.
  if (m_contacts.has_value()) {
    Object& contacts = container->insert_key("contacts", Object::create_list());

    for (const auto& m_contact : *m_contacts) {
      Object::list_type& list = contacts.insert_back(Object::create_list()).as_list();
      list.emplace_back(m_contact.first);
      list.emplace_back(m_contact.second);
    }
  }

  return container;
}

tracker::DhtController::statistics_type
DhtRouter::get_statistics() const {
  tracker::DhtController::statistics_type stats;

  if (!m_server.is_active())
    stats.cycle = 0;
  else if (m_numRefresh < 2)  // still bootstrapping
    stats.cycle = 1;
  else
    stats.cycle = m_numRefresh;

  stats.queries_received = m_server.queries_received();
  stats.queries_sent     = m_server.queries_sent();
  stats.replies_received = m_server.replies_received();
  stats.errors_received  = m_server.errors_received();
  stats.errors_caught    = m_server.errors_caught();

  stats.num_nodes        = m_nodes.size();
  stats.num_buckets      = m_routingTable.size();

  stats.num_peers        = 0;
  stats.max_peers        = 0;
  stats.num_trackers     = m_trackers.size();

  for (const auto& [_, tracker] : m_trackers) {
    unsigned int peers = tracker->size();
    stats.num_peers += peers;
    stats.max_peers = std::max(peers, stats.max_peers);
  }

  return stats;
}

void
DhtRouter::receive_timeout_bootstrap() {
  // If we're still bootstrapping, restart the process every 60 seconds until
  // we have enough nodes in our routing table. After we have 32 nodes, we switch
  // to a less aggressive non-bootstrap mode of collecting nodes that contact us
  // and through doing normal torrent announces.
  if (m_nodes.size() < num_bootstrap_complete) {
    if (!m_contacts.has_value())
      throw internal_error("DhtRouter::receive_timeout_bootstrap called without contact list.");

    if (!m_nodes.empty() || !m_contacts->empty())
      bootstrap();

    // Retry in 60 seconds.
    this_thread::scheduler()->wait_for_ceil_seconds(&m_task_timeout, std::chrono::seconds(timeout_bootstrap_retry));

    m_numRefresh = 1;  // still bootstrapping

  } else {
    // We won't be needing external contacts after this.
    m_contacts.reset();

    m_task_timeout.slot() = [this] { receive_timeout(); };

    if (!m_numRefresh) {
      // If we're still in the startup, do the usual refreshing too.
      receive_timeout();

    } else {
      // Otherwise just set the 15 minute timer.
      this_thread::scheduler()->wait_for_ceil_seconds(&m_task_timeout, std::chrono::seconds(timeout_update));
    }

    m_numRefresh = 2;
  }
}

void
DhtRouter::receive_timeout() {
  this_thread::scheduler()->wait_for_ceil_seconds(&m_task_timeout, std::chrono::seconds(timeout_update));

  m_prevToken = m_curToken;
  m_curToken = random();

  // Do some periodic accounting, refreshing buckets and marking
  // bad nodes.

  // Update nodes.
  for (const auto& [id, node] : m_nodes) {
    if (!node->bucket())
      throw internal_error("DhtRouter::receive_timeout has node without bucket.");

    node->update();

    // Try contacting nodes we haven't received anything from for a while.
    // Don't contact repeatedly unresponsive nodes; we keep them in case they
    // do send a query, until we find a better node. However, give it a last
    // chance just before deleting it.
    if (node->is_questionable() && (!node->is_bad() || node->age() >= timeout_remove_node))
      m_server.ping(node->id(), node->address());
  }

  // If bucket isn't full yet or hasn't received replies/queries from
  // its nodes for a while, try to find new nodes now.
  for (const auto& route : m_routingTable) {
    route.second->update();

    if (!route.second->is_full() || route.second == bucket() || route.second->age() > timeout_bucket_bootstrap)
      bootstrap_bucket(route.second);
  }

  // Remove old peers and empty torrents from the tracker.
  for (auto itr = m_trackers.begin(); itr != m_trackers.end();) {
    auto tracker = itr->second;
    tracker->prune(timeout_peer_announce);

    if (tracker->empty()) {
      delete tracker;
      itr = m_trackers.erase(itr);
      continue;
    }
    ++itr;
  }

  m_server.update();

  m_numRefresh++;
}

char*
DhtRouter::generate_token(const sockaddr* sa, int token, char buffer[20]) {
  if (!sa_is_inet(sa))
    throw internal_error("DhtRouter::generate_token called with non-inet address.");

  uint32_t key = reinterpret_cast<const sockaddr_in*>(sa)->sin_addr.s_addr;

  Sha1 sha;
  sha.init();
  sha.update(&token, sizeof(token));
  sha.update(&key, 4);
  sha.final_c(buffer);

  return buffer;
}

bool
DhtRouter::token_valid(raw_string token, const sockaddr* sa) const {
  if (token.size() != size_token)
    return false;

  // Compare given token to the reference token.
  char reference[20];

  // First try current token.
  //
  // Else if token recently changed, some clients may be using the older one.
  // That way a token is valid for 15-30 minutes, instead of 0-15.
  return
    token == raw_string(generate_token(sa, m_curToken, reference), size_token) ||
    token == raw_string(generate_token(sa, m_prevToken, reference), size_token);
}

DhtNode*
DhtRouter::find_node(const sockaddr* sa) {
  for (const auto& [id, node] : m_nodes) {
    if (sa_equal_addr(node->address(), sa))
      return node;
  }

  return nullptr;
}

DhtRouter::DhtBucketList::iterator
DhtRouter::split_bucket(const DhtBucketList::iterator& itr, DhtNode* node) {
  // Split bucket. Current bucket keeps the upper half thus keeping the
  // map key valid, new bucket is the lower half of the original bucket.
  DhtBucket* newBucket = itr->second->split(id());

  // If our bucket has a child now (the new bucket), move ourself into it.
  if (bucket()->child() != NULL)
    set_bucket(bucket()->child());

  if (!bucket()->is_in_range(id()))
    throw internal_error("DhtRouter::split_bucket router ID ended up in wrong bucket.");

  // Insert new bucket with iterator hint = just before current bucket.
  auto other = m_routingTable.emplace_hint(itr, newBucket->id_range_end(), newBucket);

  // Check that the bucket we're not adding the node to isn't empty.
  if (other->second->is_in_range(node->id())) {
    if (itr->second->empty())
      bootstrap_bucket(itr->second);

  } else {
    if (other->second->empty())
      bootstrap_bucket(other->second);

    other = itr;
  }

  return other;
}

bool
DhtRouter::add_node_to_bucket(DhtNode* node) {
  auto itr = find_bucket(node->id());

  while (itr->second->is_full()) {
    // Bucket is full. If there are any bad nodes, remove the oldest.
    DhtBucket::iterator nodeItr = itr->second->find_replacement_candidate();
    if (nodeItr == itr->second->end())
      throw internal_error("DhtBucket::find_candidate returned no node.");

    if ((*nodeItr)->is_bad()) {
      delete_node(m_nodes.find(&(*nodeItr)->id()));

    } else {
      // Bucket is full of good nodes; if our own ID falls in
      // range then split the bucket else discard new node.
      if (itr->second != bucket()) {
        delete_node(m_nodes.find(&node->id()));
        return false;
      }

      itr = split_bucket(itr, node);
    }
  }

  itr->second->add_node(node);
  node->set_bucket(itr->second);
  return true;
}

void
DhtRouter::delete_node(const DhtNodeList::accessor& itr) {
  if (itr == m_nodes.end())
    throw internal_error("DhtRouter::delete_node called with invalid iterator.");

  if (itr.node()->bucket() != NULL)
    itr.node()->bucket()->remove_node(itr.node());

  delete itr.node();

  m_nodes.erase(itr);
}

void
DhtRouter::bootstrap() {
  if (!m_contacts.has_value())
    return;

  // Contact up to 8 nodes from the contact list (newest first).
  for (int count = 0; count < 8 && !m_contacts->empty(); count++) {
    // Currently discarding SOCK_DGRAM.
    auto f = [this](const auto& sa, int) {
      if (sa != nullptr)
        contact(sa.get(), m_contacts->back().second);
    };

    this_thread::resolver()->resolve_specific(this, m_contacts->back().first, AF_INET, f);

    m_contacts->pop_back();
  }

  // Abort unless we already found some nodes for a search.
  if (m_nodes.empty())
    return;

  bootstrap_bucket(bucket());

  // Aggressively ping all questionable nodes in our own bucket to weed
  // out bad nodes as early as possible and make room for fresh nodes.
  for (auto node : *bucket()) {
    if (!node->is_good())
      m_server.ping(node->id(), node->address());
  }

  // Also bootstrap a random bucket, if there are others.
  if (m_routingTable.size() < 2)
    return;

  auto itr = m_routingTable.begin();
  std::advance(itr, random() % m_routingTable.size());

  if (itr->second != bucket() && itr != m_routingTable.end())
    bootstrap_bucket(itr->second);
}

void
DhtRouter::bootstrap_bucket(const DhtBucket* bucket) {
  if (!m_server.is_active())
    return;

  // Do a search for a random ID, or the ID adjacent to our
  // own when bootstrapping our own bucket. We don't search for
  // our own exact ID to avoid receiving only our own node info
  // instead of closest nodes, from nodes that know us already.
  HashString contactId;

  if (bucket == this->bucket()) {
    contactId = id();
    contactId[torrent::HashString::size() - 1] ^= 1;
  } else {
    bucket->get_random_id(&contactId);
  }

  m_server.find_node(*bucket, contactId);
}

} // namespace torrent
