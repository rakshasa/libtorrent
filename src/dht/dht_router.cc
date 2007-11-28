// libTorrent - BitTorrent library
// Copyright (C) 2005-2007, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#include "config.h"
#include "globals.h"

#include <sstream>
#include <rak/functional.h>

#include "torrent/dht_manager.h"
#include "torrent/exceptions.h"
#include "utils/sha1.h"
#include "manager.h"

#include "dht_bucket.h"
#include "dht_router.h"
#include "dht_tracker.h"
#include "dht_transaction.h"

namespace torrent {

HashString DhtRouter::zero_id;

DhtRouter::DhtRouter(const Object& cache, const rak::socket_address* sa) :
  DhtNode(zero_id, sa),  // actual ID is set later
  m_server(this),
  m_contacts(NULL),
  m_numRefresh(0),
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

  set_bucket(new DhtBucket(zero_id, ones_id));
  m_routingTable.insert(std::make_pair(bucket()->id_range_end(), bucket()));

  if (cache.has_key("nodes")) {
    const Object::map_type& nodes = cache.get_key_map("nodes");

    for (Object::map_type::const_iterator itr = nodes.begin(); itr != nodes.end(); ++itr) {
      if (itr->first.length() != HashString::size_data)
        throw bencode_error("Loading cache: Invalid node hash.");

      add_node_to_bucket(m_nodes.add_node(new DhtNode(itr->first, itr->second)));
    }
  }

  if (m_nodes.size() < num_bootstrap_complete) {
    m_contacts = new std::deque<contact_t>;

    if (cache.has_key("contacts")) {
      const Object::list_type& contacts = cache.get_key_list("contacts");

      for (Object::list_type::const_iterator itr = contacts.begin(); itr != contacts.end(); ++itr) {
        Object::list_type::const_iterator litr = itr->as_list().begin();
        const std::string& host = litr->as_string();
        int port = (++litr)->as_value();
        m_contacts->push_back(std::make_pair(host, port));
      }
    }
  }
}

DhtRouter::~DhtRouter() {
  stop();
  delete m_contacts;
  std::for_each(m_routingTable.begin(), m_routingTable.end(), rak::on(rak::mem_ref(&DhtBucketList::value_type::second), rak::call_delete<DhtBucket>()));
  std::for_each(m_trackers.begin(), m_trackers.end(), rak::on(rak::mem_ref(&DhtTrackerList::value_type::second), rak::call_delete<DhtTracker>()));
  std::for_each(m_nodes.begin(), m_nodes.end(), rak::on(rak::mem_ref(&DhtNodeList::value_type::second), rak::call_delete<DhtNode>()));
}

void
DhtRouter::start(int port) {
  m_server.start(port);

  // Set timeout slot and schedule it to be called immediately for initial bootstrapping if necessary.
  m_taskTimeout.set_slot(rak::mem_fn(this, &DhtRouter::receive_timeout_bootstrap));
  priority_queue_insert(&taskScheduler, &m_taskTimeout, (cachedTime + rak::timer::from_seconds(1)).round_seconds());
}

void
DhtRouter::stop() {
  priority_queue_erase(&taskScheduler, &m_taskTimeout);
  m_server.stop();
}

// Start a DHT get_peers and announce_peer request.
void
DhtRouter::announce(DownloadInfo* info, TrackerDht* tracker) {
  m_server.announce(*find_bucket(info->hash())->second, info->hash(), tracker);
}

// Cancel any running requests from the given tracker.
// If info or tracker is not NULL, only cancel matching requests.
void
DhtRouter::cancel_announce(DownloadInfo* info, const TrackerDht* tracker) {
  m_server.cancel_announce(info, tracker);
}

DhtTracker*
DhtRouter::get_tracker(const HashString& hash, bool create) {
  DhtTrackerList::accessor itr = m_trackers.find(hash);

  if (itr != m_trackers.end())
    return itr.tracker();

  if (!create)
    return NULL;

  std::pair<DhtTrackerList::accessor, bool> res = m_trackers.insert(std::make_pair(hash, new DhtTracker()));

  if (!res.second)
    throw internal_error("DhtRouter::get_tracker did not actually insert tracker.");

  return res.first.tracker();
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
  DhtBucketList::iterator itr = m_routingTable.upper_bound(id);

#ifdef USE_EXTRA_DEBUG
  if (itr == m_routingTable.end())
    throw internal_error("DHT Buckets not covering entire ID space.");

  if (!itr->second->is_in_range(id))
    throw internal_error("DhtRouter::find_bucket, m_routingTable.upper_bound did not find correct bucket.");
#endif

  return itr;
}

void
DhtRouter::add_contact(const std::string& host, int port) {
  // Externally obtained nodes are added to the contact list, but only if
  // we're still bootstrapping. We don't contact external nodes after that.
  if (m_contacts != NULL) {
    if (m_contacts->size() >= num_bootstrap_contacts)
      m_contacts->pop_front();

    m_contacts->push_back(std::make_pair(host, port)); 
  }
}

void
DhtRouter::contact(const rak::socket_address* sa, int port) {
  if (is_active()) {
    rak::socket_address sa_port = *sa;
    sa_port.set_port(port);
    m_server.ping(zero_id, &sa_port);
  }
}

// Received a query from the given node. If it has previously replied
// to one of our queries, consider it alive and update the bucket mtime,
// otherwise if we could use it in a bucket, try contacting it.
DhtNode*
DhtRouter::node_queried(const HashString& id, const rak::socket_address* sa) {
  DhtNode* node = get_node(id);

  if (node == NULL) {
    if (want_node(id))
      m_server.ping(id, sa);

    return NULL;
  }

  // If we know the ID but the address is different, don't set the original node
  // active, but neither use this new address to prevent rogue nodes from polluting
  // our routing table with fake source addresses.
  if (node->address()->sa_inet()->address_n() != sa->sa_inet()->address_n())
    return NULL;

  node->queried();
  if (node->is_good())
    node->bucket()->touch();

  return node;
}

// Received a reply from a node we queried.
// Check that it matches the information we have, set that it has replied
// and update the bucket mtime.
DhtNode*
DhtRouter::node_replied(const HashString& id, const rak::socket_address* sa) {
  DhtNode* node = get_node(id);

  if (node == NULL) {
    if (!want_node(id))
      return NULL;

    // New node, create it. It's a good node (it replied!) so add it to a bucket.
    node = m_nodes.add_node(new DhtNode(id, sa));

    if (!add_node_to_bucket(node))   // deletes the node if it fails
      return NULL;
  }

  if (node->address()->sa_inet()->address_n() != sa->sa_inet()->address_n())
    return NULL;

  node->replied();
  node->bucket()->touch();

  return node;
}

// A node has not replied to one of our queries.
DhtNode*
DhtRouter::node_inactive(const HashString& id, const rak::socket_address* sa) {
  DhtNodeList::accessor itr = m_nodes.find(&id);

  // If not found add it to some blacklist so we won't try contacting it again immediately?
  if (itr == m_nodes.end())
    return NULL;

  // Check source address. Normally node_inactive is called if we DON'T receive a reply,
  // however it can also be called if a node replied with an malformed response packet,
  // so check that the address matches so that a rogue node cannot cause other nodes
  // to be considered bad by sending malformed packets.
  if (itr.node()->address()->sa_inet()->address_n() != sa->sa_inet()->address_n())
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

char*
DhtRouter::store_closest_nodes(const HashString& id, char* buffer, char* bufferEnd) {
  DhtBucketChain chain(find_bucket(id)->second);

  do {
    for (DhtBucket::const_iterator itr = chain.bucket()->begin(); itr != chain.bucket()->end() && buffer != bufferEnd; ++itr) {
      if (!(*itr)->is_bad()) {
        buffer = (*itr)->store_compact(buffer);

        if (buffer > bufferEnd)
          throw internal_error("DhtRouter::store_closest_nodes wrote past buffer end.");
      }
    }
  } while (buffer != bufferEnd && chain.next() != NULL);

  return buffer;
}

Object*
DhtRouter::store_cache(Object* container) const {
  container->insert_key("self_id", str());

  // Insert all nodes.
  Object& nodes = container->insert_key("nodes", Object(Object::TYPE_MAP));
  for (DhtNodeList::const_accessor itr = m_nodes.begin(); itr != m_nodes.end(); ++itr) {
    if (!itr.node()->is_bad())
      itr.node()->store_cache(&nodes.insert_key(itr.id().str(), Object(Object::TYPE_MAP)));
  }

  // Insert contacts, if we have any.
  if (m_contacts != NULL) {
    Object& contacts = container->insert_key("contacts", Object(Object::TYPE_LIST));

    for (std::deque<contact_t>::const_iterator itr = m_contacts->begin(); itr != m_contacts->end(); ++itr) {
      Object::list_type& list = contacts.insert_back(Object(Object::TYPE_LIST)).as_list();
      list.push_back(itr->first);
      list.push_back(itr->second);
    }
  }

  return container;
}

DhtManager::statistics_type
DhtRouter::get_statistics() const {
  DhtManager::statistics_type stats(*m_server.upload_throttle()->rate(), *m_server.download_throttle()->rate());

  if (!m_server.is_active())
    stats.cycle = 0;
  else if (m_numRefresh < 2)  // still bootstrapping
    stats.cycle = 1;
  else
    stats.cycle = m_numRefresh;

  stats.queries_received = m_server.queries_received();
  stats.queries_sent     = m_server.queries_sent();
  stats.replies_received = m_server.replies_received();

  stats.num_nodes        = m_nodes.size();
  stats.num_buckets      = m_routingTable.size();

  stats.num_peers        = 0;
  stats.max_peers        = 0;
  stats.num_trackers     = m_trackers.size();

  for (DhtTrackerList::const_accessor itr = m_trackers.begin(); itr != m_trackers.end(); ++itr) {
    unsigned int peers = itr.tracker()->size();
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
    if (m_contacts == NULL)
      throw internal_error("DhtRouter::receive_timeout_bootstrap called without contact list.");

    if (!m_nodes.empty() || !m_contacts->empty())
      bootstrap();

    // Retry in 60 seconds.
    priority_queue_insert(&taskScheduler, &m_taskTimeout, (cachedTime + rak::timer::from_seconds(timeout_bootstrap_retry)).round_seconds());
    m_numRefresh = 1;  // still bootstrapping

  } else {
    // We won't be needing external contacts after this.
    delete m_contacts;
    m_contacts = NULL;

    m_taskTimeout.set_slot(rak::mem_fn(this, &DhtRouter::receive_timeout));

    if (!m_numRefresh) {
      // If we're still in the startup, do the usual refreshing too.
      receive_timeout();

    } else {
      // Otherwise just set the 15 minute timer.
      priority_queue_insert(&taskScheduler, &m_taskTimeout, (cachedTime + rak::timer::from_seconds(timeout_update)).round_seconds());
    }

    m_numRefresh = 2;
  }
}

void
DhtRouter::receive_timeout() {
  priority_queue_insert(&taskScheduler, &m_taskTimeout, (cachedTime + rak::timer::from_seconds(timeout_update)).round_seconds());

  m_prevToken = m_curToken;
  m_curToken = random();

  // Do some periodic accounting, refreshing buckets and marking
  // bad nodes.

  // Update nodes.
  for (DhtNodeList::accessor itr = m_nodes.begin(); itr != m_nodes.end(); ++itr) {
    if (!itr.node()->bucket())
      throw internal_error("DhtRouter::receive_timeout has node without bucket.");

    itr.node()->update();

    // Try contacting nodes we haven't received anything from for a while.
    // Don't contact repeatedly unresponsive nodes; we keep them in case they
    // do send a query, until we find a better node. However, give it a last
    // chance just before deleting it.
    if (itr.node()->is_questionable() && (!itr.node()->is_bad() || itr.node()->age() >= timeout_remove_node))
      m_server.ping(itr.node()->id(), itr.node()->address());
  }

  // If bucket isn't full yet or hasn't received replies/queries from
  // its nodes for a while, try to find new nodes now.
  for (DhtBucketList::const_iterator itr = m_routingTable.begin(); itr != m_routingTable.end(); ++itr) {
    itr->second->update();

    if (!itr->second->is_full() || itr->second->age() > timeout_bucket_bootstrap)
      bootstrap_bucket(itr->second);
  }

  // Remove old peers and empty torrents from the tracker.
  for (DhtTrackerList::accessor itr = m_trackers.begin(); itr != m_trackers.end(); ) {
    itr.tracker()->prune(timeout_peer_announce);

    if (itr.tracker()->empty()) {
      delete itr.tracker();
      m_trackers.erase(itr++);

    } else {
      ++itr;
    }
  }

  m_server.update();

  m_numRefresh++;
}

void
DhtRouter::generate_token(const rak::socket_address* sa, int token, char buffer[20]) {
  Sha1 sha;
  uint32_t key = sa->sa_inet()->address_n();

  sha.init();
  sha.update(&token, sizeof(token));
  sha.update(&key, 4);
  sha.final_c(buffer);
}

std::string
DhtRouter::make_token(const rak::socket_address* sa) {
  char token[20];

  generate_token(sa, m_curToken, token);

  return std::string(token, size_token);
}

bool
DhtRouter::token_valid(const std::string& token, const rak::socket_address* sa) {
  if (token.length() != size_token)
    return false;

  // Compare given token to the reference token.
  char reference[20];

  // First try current token.
  generate_token(sa, m_curToken, reference);

  if (std::memcmp(reference, token.c_str(), size_token) == 0)
    return true;

  // If token recently changed, some clients may be using the older one.
  // That way a token is valid for 15-30 minutes, instead of 0-15.
  generate_token(sa, m_prevToken, reference);

  return std::memcmp(reference, token.c_str(), size_token) == 0;
}

DhtNode*
DhtRouter::find_node(const rak::socket_address* sa) {
  for (DhtNodeList::accessor itr = m_nodes.begin(); itr != m_nodes.end(); ++itr)
    if (itr.node()->address()->sa_inet()->address_n() == sa->sa_inet()->address_n())
      return itr.node();

  return NULL;
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
  DhtBucketList::iterator other = m_routingTable.insert(itr, std::make_pair(newBucket->id_range_end(), newBucket));

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
  DhtBucketList::iterator itr = find_bucket(node->id());

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

struct contact_node_t {
  contact_node_t(DhtRouter* router, int port) : m_router(router), m_port(port) { }

  void operator() (const sockaddr* sa, int err)
    { if (sa != NULL) m_router->contact(rak::socket_address::cast_from(sa), m_port); }

  DhtRouter* m_router;
  int        m_port;
};

void
DhtRouter::bootstrap() {
  // Contact up to 8 nodes from the contact list (newest first).
  for (int count = 0; count < 8 && !m_contacts->empty(); count++) {
    manager->connection_manager()->resolver()(m_contacts->back().first.c_str(), (int)rak::socket_address::pf_inet, SOCK_DGRAM,
                                              contact_node_t(this, m_contacts->back().second));
    m_contacts->pop_back();
  }

  // Abort unless we already found some nodes for a search.
  if (m_nodes.empty())
    return;

  bootstrap_bucket(bucket());

  // Aggressively ping all questionable nodes in our own bucket to weed
  // out bad nodes as early as possible and make room for fresh nodes.
  for (DhtBucket::iterator itr = bucket()->begin(); itr != bucket()->end(); ++itr)
    if (!(*itr)->is_good())
      m_server.ping((*itr)->id(), (*itr)->address());

  // Also bootstrap a random bucket, if there are others.
  if (m_routingTable.size() < 2)
    return;

  DhtBucketList::iterator itr = m_routingTable.begin();
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
    contactId[contactId.size() - 1] ^= 1;
  } else {
    bucket->get_random_id(&contactId);
  }

  m_server.find_node(*bucket, contactId);
}

}
