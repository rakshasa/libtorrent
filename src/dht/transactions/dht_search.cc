#include "config.h"

#include "dht/transactions/dht_search.h"

#include <cassert>

#include "dht/dht_node.h"
#include "dht/dht_server.h"

namespace torrent::dht {

bool
dht_compare_closer::operator () (const std::unique_ptr<DhtNode>& one, const std::unique_ptr<DhtNode>& two) const {
  return DhtSearch::is_closer(one->id(), two->id(), m_target);
}

DhtSearch::DhtSearch(DhtServer* server, const HashString& target)
  : base_type(dht_compare_closer(target)),
    m_next(end()),
    m_server(server),
    m_target(target) {

  // TODO: Must be done manually to ensure we got a shared_ptr.
  // add_contacts(contacts);
}

DhtSearch::~DhtSearch() {
  // Make sure transactions were destructed first. Since it is the destruction
  // of a transaction that triggers this destructor, that should always be the
  // case.
  assert(!m_pending && "DhtSearch::~DhtSearch called with pending transactions.");
  assert(m_concurrency == 3 && "DhtSearch::~DhtSearch called with invalid concurrency limit.");

  // TODO: Hack.
  // for (auto& itr : *this)
  //   itr.second->server()->check_search_trimming(itr.second);
}

// TODO: Check if DhtSearch gets stored somewhere, and DhtBucket seems the most likely candidate.
//
// This seems to be storing self in the map.

//
// TODO: Remove self stored in the map, and when getting accessor from map, also pass the (self) DhtSearch.
//

bool
DhtSearch::add_contact(const HashString& id, const sockaddr* sa) {
  auto n     = std::make_unique<DhtNode>(id, sa);
  bool added = emplace(std::move(n), shared_from_this()).second;

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

  for (auto itr = chain.bucket()->begin(); needClosest > 0 || needGood > 0; ++itr) {
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
DhtSearch::trim(bool is_final) {
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

  int needClosest = is_final ? 0 : max_contacts;
  int needGood = is_announce() ? max_announce : 0;

  // We're done if we can't find any more nodes to contact.
  m_next = end();

  for (accessor itr = base_type::begin(); itr != end(); ) {
    // If we have all we need, delete current node unless it is
    // currently being contacted.
    if (!itr.node()->is_active() && needClosest <= 0 && (!itr.node()->is_good() || needGood <= 0)) {
      // TODO: Temporary hack.
        // TODO: Add server function to add it to m_search if not complete.
        // TODO: We should replace m_pending with a vector of txs.

      // TODO: We're somehow storying self in the map, and erasing causes crash as m_pending == 2.

      // itr.search()->server()->check_search_trimming(itr.search());

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

bool
DhtSearch::is_closer(const HashString& one, const HashString& two, const HashString& target) {
  for (unsigned int i=0; i<torrent::HashString::size(); i++)
    if (one[i] != two[i])
      return static_cast<uint8_t>(one[i] ^ target[i]) < static_cast<uint8_t>(two[i] ^ target[i]);

  return false;
}

// TODO: Change m_pending to a vector of weak_ref.

DhtSearch::const_accessor
DhtSearch::get_contact() {
  if (m_pending >= m_concurrency)
    return end();

  if (m_restart)
    trim(false);

  auto ret = m_next;

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

void
DhtSearch::set_node_active(const std::unique_ptr<DhtNode>& n, bool active) {
  n->m_last_seen = active;
}

} // namespace torrent::dht
