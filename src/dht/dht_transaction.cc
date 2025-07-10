#include "config.h"

#include "dht/dht_transaction.h"

#include <cassert>

#include "dht/dht_bucket.h"
#include "torrent/exceptions.h"
#include "torrent/object_stream.h"
#include "tracker/tracker_dht.h"

namespace torrent {

template<>
const DhtMessage::key_list_type DhtMessage::base_type::keys;

DhtSearch::DhtSearch(const HashString& target, const DhtBucket& contacts)
  : base_type(dht_compare_closer(target)),
    m_next(end()),
    m_target(target) {

  add_contacts(contacts);
}

DhtSearch::~DhtSearch() {
  // Make sure transactions were destructed first. Since it is the destruction
  // of a transaction that triggers this destructor, that should always be the
  // case.
  assert(!m_pending && "DhtSearch::~DhtSearch called with pending transactions.");
  assert(m_concurrency == 3 && "DhtSearch::~DhtSearch called with invalid concurrency limit.");
}

bool
DhtSearch::add_contact(const HashString& id, const rak::socket_address* sa) {
  auto n = std::make_unique<DhtNode>(id, sa);
  bool added = emplace(std::move(n), this).second;

  if (added)
    m_restart = true;

  return added;
}

void
DhtSearch::add_contacts(const DhtBucket& contacts) {
  DhtBucketChain chain(&contacts);

  // Add max_contacts=18 closest nodes, and fill up so we also have at least 8 good nodes.
  int needClosest = max_contacts - size();
  int needGood = DhtBucket::num_nodes;

  for (DhtBucket::const_iterator itr = chain.bucket()->begin(); needClosest > 0 || needGood > 0; ++itr) {
    while (itr == chain.bucket()->end()) {
      if (!chain.next())
        return;

      itr = chain.bucket()->begin();
    }

    if ((!(*itr)->is_bad() || needClosest > 0) && add_contact((*itr)->id(), (*itr)->address())) {
      needGood -= !(*itr)->is_bad();
      needClosest--;
    }
  }
}

// Check if a node has been contacted yet.  This is the case if it is not currently
// being contacted, nor has it been found to be good or bad.
bool
DhtSearch::node_uncontacted(const std::unique_ptr<DhtNode>& node) {
  return !node->is_active() && !node->is_good() && !node->is_bad();
}

// After more contacts have been added, discard least closest nodes
// except if node has a transaction pending.
void
DhtSearch::trim(bool final) {

  // We keep:
  // - the max_contacts=18 closest good or unknown nodes and all nodes closer
  //   than them (to see if further searches find closer ones)
  // - for announces, also the 3 closest good nodes (i.e. nodes that have
  //   replied) to have at least that many for the actual announce
  // - any node that currently has transactions pending
  //
  // However, after exhausting all search nodes, we only keep good nodes.
  //
  // For our purposes, the node status is as follows:
  // node is bad (contacted but hasn't replied) if is_bad()
  // node is good (contacted and replied) if is_good()
  // node is currently being contacted if is_active()
  // node is new and unknown otherwise

  int needClosest = final ? 0 : max_contacts;
  int needGood = is_announce() ? max_announce : 0;

  // We're done if we can't find any more nodes to contact.
  m_next = end();

  for (accessor itr = base_type::begin(); itr != end(); ) {
    // If we have all we need, delete current node unless it is
    // currently being contacted.
    if (!itr.node()->is_active() && needClosest <= 0 && (!itr.node()->is_good() || needGood <= 0)) {
      erase(itr++);
      continue;
    }

    // Otherwise adjust needed counts appropriately.
    needClosest--;
    needGood -= itr.node()->is_good();

    // Remember the first uncontacted node as the closest one to contact next.
    if (m_next == end() && node_uncontacted(itr.node()))
      m_next = const_accessor(itr);

    ++itr;
  }

  m_restart = false;
}

DhtSearch::const_accessor
DhtSearch::get_contact() {
  if (m_pending >= m_concurrency)
    return end();

  if (m_restart)
    trim(false);

  const_accessor ret = m_next;
  if (ret == end())
    return ret;

  set_node_active(ret.node(), true);
  m_pending++;
  m_contacted++;

  // Find next node to contact: any node we haven't contacted yet.
  while (++m_next != end()) {
    if (node_uncontacted(m_next.node()))
      break;
  }

  return ret;
}

void
DhtSearch::node_status(const std::unique_ptr<DhtNode>& n, bool success) {
  if (!n->is_active())
    throw internal_error("DhtSearch::node_status called for invalid/inactive node.");

  if (success) {
    n->set_good();
    m_replied++;

  } else {
    n->set_bad();
  }

  m_pending--;
  set_node_active(n, false);
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
DhtTransactionPacket::build_buffer(const DhtMessage& msg) {
  char buffer[1500];  // If the message would exceed an Ethernet frame, something went very wrong.
  object_buffer_t result = static_map_write_bencode_c(object_write_to_buffer, NULL, std::make_pair(buffer, buffer + sizeof(buffer)), msg);

  m_length = result.second - buffer;
  m_data   = std::make_unique<char[]>(m_length);
  memcpy(m_data.get(), buffer, m_length);
}

DhtTransaction::DhtTransaction(int quick_timeout, int timeout, const HashString& id, const rak::socket_address* sa)
  : m_id(id),
    m_hasQuickTimeout(quick_timeout > 0),
    m_sa(*sa),
    m_timeout(this_thread::cached_seconds().count() + timeout),
    m_quickTimeout(this_thread::cached_seconds().count() + quick_timeout) {
}

DhtTransaction::~DhtTransaction() {
  if (m_packet != NULL)
    m_packet->set_failed();
}

void
DhtTransactionSearch::set_stalled() {
  if (!m_hasQuickTimeout)
    throw internal_error("DhtTransactionSearch::set_stalled called on already stalled transaction.");

  m_hasQuickTimeout = false;
  m_search->m_concurrency++;
}

void
DhtTransactionSearch::complete(bool success) {
  if (m_node == m_search->end())
    throw internal_error("DhtTransactionSearch::complete called multiple times.");

  if (m_node.search() != m_search)
    throw internal_error("DhtTransactionSearch::complete called for node from wrong search.");

  if (!m_hasQuickTimeout)
    m_search->m_concurrency--;

  m_search->node_status(m_node.node(), success);
  m_node = m_search->end();
}

DhtTransactionSearch::~DhtTransactionSearch() {
  if (m_node != m_search->end())
    complete(false);

  if (m_search->complete())
    delete m_search;
}

DhtTransaction::transaction_type
DhtTransactionPing::type() const {
  return DHT_PING;
}

DhtTransaction::transaction_type
DhtTransactionFindNode::type() const {
  return DHT_FIND_NODE;
}

DhtTransaction::transaction_type
DhtTransactionGetPeers::type() const {
  return DHT_GET_PEERS;
}

DhtTransaction::transaction_type
DhtTransactionAnnouncePeer::type() const {
  return DHT_ANNOUNCE_PEER;
}

} // namespace torrent
