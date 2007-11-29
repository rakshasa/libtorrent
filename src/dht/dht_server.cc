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

#include <algorithm>
#include <sstream>

#include "net/throttle_manager.h"
#include "torrent/exceptions.h"
#include "torrent/connection_manager.h"
#include "torrent/object.h"
#include "torrent/object_stream.h"
#include "torrent/poll.h"
#include "tracker/tracker_dht.h"

#include "dht_bucket.h"
#include "dht_router.h"
#include "dht_transaction.h"

#include "manager.h"

namespace torrent {

const char* DhtServer::queries[] = {
  "ping",
  "find_node",
  "get_peers",
  "announce_peer",
};

// Error in DHT protocol, avoids std::string ctor from communication_error
class dht_error : public network_error {
public:
  dht_error(int code, const char* message) : m_message(message), m_code(code) {}

  virtual int          code() const throw()   { return m_code; }
  virtual const char*  what() const throw()   { return m_message; }

private:
  const char*  m_message;
  int          m_code;
};

DhtServer::DhtServer(DhtRouter* router) :
  m_router(router),

  m_uploadThrottle(60),
  m_downloadThrottle(60),

  m_networkUp(false) {

  get_fd().clear();
  reset_statistics();

  // Reserve a socket for the DHT server, even though we don't
  // actually open it until the server is started, which may not
  // happen until the first non-private torrent is started.
  manager->connection_manager()->inc_socket_count();
}

DhtServer::~DhtServer() {
  stop();

  std::for_each(m_highQueue.begin(), m_highQueue.end(), rak::call_delete<DhtTransactionPacket>());
  std::for_each(m_lowQueue.begin(), m_lowQueue.end(), rak::call_delete<DhtTransactionPacket>());

  manager->connection_manager()->dec_socket_count();
}

void
DhtServer::start(int port) {
  try {
    if (!get_fd().open_datagram() || !get_fd().set_nonblock())
      throw resource_error("Could not allocate datagram socket.");

    if (!get_fd().set_reuse_address(true))
      throw resource_error("Could not set listening port to reuse address.");

    rak::socket_address sa = *m_router->address();
    sa.set_port(port);

    if (!get_fd().bind(sa))
      throw resource_error("Could not bind datagram socket.");

  } catch (torrent::base_error& e) {
    get_fd().close();
    get_fd().clear();
    throw;
  }

  m_taskTimeout.set_slot(rak::mem_fn(this, &DhtServer::receive_timeout));

  m_uploadThrottle.set_list_iterator(manager->upload_throttle()->throttle_list()->end());
  m_uploadThrottle.slot_activate(rak::make_mem_fun(static_cast<SocketBase*>(this), &SocketBase::receive_throttle_up_activate));

  m_downloadThrottle.set_list_iterator(manager->download_throttle()->throttle_list()->end());
  manager->download_throttle()->throttle_list()->insert(&m_downloadThrottle);

  manager->poll()->open(this);
  manager->poll()->insert_read(this);
  manager->poll()->insert_error(this);
}

void
DhtServer::stop() {
  if (!is_active())
    return;

  clear_transactions();

  priority_queue_erase(&taskScheduler, &m_taskTimeout);

  manager->upload_throttle()->throttle_list()->erase(&m_uploadThrottle);
  manager->download_throttle()->throttle_list()->erase(&m_downloadThrottle);

  manager->poll()->remove_read(this);
  manager->poll()->remove_write(this);
  manager->poll()->remove_error(this);
  manager->poll()->close(this);

  get_fd().close();
  get_fd().clear();

  m_networkUp = false;
}

void
DhtServer::reset_statistics() { 
  m_queriesReceived = 0;
  m_queriesSent = 0;
  m_repliesReceived = 0;

  m_uploadThrottle.rate()->set_total(0);
  m_downloadThrottle.rate()->set_total(0);
}

// Ping a node whose ID we know.
void
DhtServer::ping(const HashString& id, const rak::socket_address* sa) {
  // No point pinging a node that we're already contacting otherwise.
  transaction_itr itr = m_transactions.lower_bound(DhtTransaction::key(sa, 0));
  if (itr == m_transactions.end() || !DhtTransaction::key_match(itr->first, sa))
    add_transaction(new DhtTransactionPing(id, sa), packet_prio_low);
}

// Contact nodes in given bucket and ask for their nodes closest to target.
void
DhtServer::find_node(const DhtBucket& contacts, const HashString& target) {
  DhtSearch* search = new DhtSearch(target, contacts);

  DhtSearch::const_accessor n;
  while ((n = search->get_contact()) != search->end())
    add_transaction(new DhtTransactionFindNode(n), packet_prio_low);

  // This shouldn't happen, it means we had no contactable nodes at all.
  if (!search->start())
    delete search;
}

void
DhtServer::announce(const DhtBucket& contacts, const HashString& infoHash, TrackerDht* tracker) {
  DhtAnnounce* announce = new DhtAnnounce(infoHash, tracker, contacts);

  DhtSearch::const_accessor n;
  while ((n = announce->get_contact()) != announce->end())
    add_transaction(new DhtTransactionFindNode(n), packet_prio_high);

  // This can only happen if all nodes we know are bad.
  if (!announce->start())
    delete announce;
  else
    announce->update_status();
}

void
DhtServer::cancel_announce(DownloadInfo* info, const TrackerDht* tracker) {
  transaction_itr itr = m_transactions.begin();

  while (itr != m_transactions.end()) {
    if (itr->second->is_search() && itr->second->as_search()->search()->is_announce()) {
      DhtAnnounce* announce = static_cast<DhtAnnounce*>(itr->second->as_search()->search());

      if ((info == NULL || announce->target() == info->hash()) && (tracker == NULL || announce->tracker() == tracker)) {
        delete itr->second;
        m_transactions.erase(itr++);
        continue;
      }
    }

    ++itr;
  }
}

void
DhtServer::update() {
  // Reset this every 15 minutes. It'll get set back to true if we receive
  // any valid packets. This allows detecting when the entire network goes
  // down, and prevents all nodes from getting removed as unresponsive.
  m_networkUp = false;
}

void
DhtServer::process_query(const Object& transactionId, const HashString& id, const rak::socket_address* sa, Object& request) {
  m_queriesReceived++;
  m_networkUp = true;

  std::string& query = request.get_key_string("q");

  Object& arg = request.get_key("a");

  // Construct reply.
  Object reply(Object::TYPE_MAP);

  if (query == "find_node")
    create_find_node_response(arg, reply);

  else if (query == "get_peers")
    create_get_peers_response(arg, sa, reply);

  else if (query == "announce_peer")
    create_announce_peer_response(arg, sa, reply);

  else if (query != "ping")
    throw dht_error(dht_error_bad_method, "Unknown query type.");

  m_router->node_queried(id, sa);
  create_response(transactionId, sa, reply);
}

void
DhtServer::create_find_node_response(const Object& arg, Object& reply) {
  const std::string& target = arg.get_key_string("target");

  if (target.length() < HashString::size_data)
    throw dht_error(dht_error_protocol, "target string too short");

  char compact[sizeof(compact_node_info) * DhtBucket::num_nodes];
  char* end = m_router->store_closest_nodes(*HashString::cast_from(target), compact, compact + sizeof(compact));

  if (end == compact)
    throw dht_error(dht_error_generic, "No nodes");

  reply.insert_key("nodes", std::string(compact, end));
}

void
DhtServer::create_get_peers_response(const Object& arg, const rak::socket_address* sa, Object& reply) {
  reply.insert_key("token", m_router->make_token(sa));

  const std::string& info_hash_str = arg.get_key_string("info_hash");

  if (info_hash_str.length() < HashString::size_data)
    throw dht_error(dht_error_protocol, "info hash too short");

  const HashString* info_hash = HashString::cast_from(info_hash_str);

  DhtTracker* tracker = m_router->get_tracker(*info_hash, false);

  // If we're not tracking or have no peers, send closest nodes.
  if (!tracker || tracker->empty()) {
    char compact[sizeof(compact_node_info) * DhtBucket::num_nodes];
    char* end = m_router->store_closest_nodes(*info_hash, compact, compact + sizeof(compact));

    if (end == compact)
      throw dht_error(dht_error_generic, "No peers nor nodes");

    reply.insert_key("nodes", std::string(compact, end));

  } else {
    Object& values = reply.insert_key("values", Object(Object::TYPE_LIST));
    values.insert_back(tracker->get_peers());
  }
}

void
DhtServer::create_announce_peer_response(const Object& arg, const rak::socket_address* sa, Object& reply) {
  const std::string& info_hash = arg.get_key_string("info_hash");

  if (info_hash.length() < HashString::size_data)
    throw dht_error(dht_error_protocol, "info hash too short");

  if (!m_router->token_valid(arg.get_key_string("token"), sa))
    throw dht_error(dht_error_protocol, "Token invalid.");

  DhtTracker* tracker = m_router->get_tracker(*HashString::cast_from(info_hash), true);
  tracker->add_peer(sa->sa_inet()->address_n(), arg.get_key_value("port"));
}

void
DhtServer::process_response(int transactionId, const HashString& id, const rak::socket_address* sa, Object& request) {
  transaction_itr itr = m_transactions.find(DhtTransaction::key(sa, transactionId));

  // Response to a transaction we don't have in our table. At this point it's
  // impossible to tell whether it used to be a valid transaction but timed out
  // the node did not return the ID we sent it, or it returned it with a
  // different address than we sent it o. Best we can do is ignore the reply,
  // since the protocol doesn't call for returning errors in responses.
  if (itr == m_transactions.end())
    return;

  m_repliesReceived++;
  m_networkUp = true;

  DhtTransaction* transaction = itr->second;
#ifdef USE_EXTRA_DEBUG
  if (DhtTransaction::key(sa, transactionId) != transaction->key(transactionId))
    throw internal_error("DhtServer::process_response key mismatch.");
#endif

  // If we contact a node but its ID is not the one we expect, ignore the reply
  // to prevent interference from rogue nodes.
  if ((id != transaction->id() && transaction->id() != m_router->zero_id))
    return;

  const Object& response = request.get_key("r");

  switch (transaction->type()) {
    case DhtTransaction::DHT_FIND_NODE:
      parse_find_node_reply(transaction->as_find_node(), response.get_key_string("nodes"));
      break;

    case DhtTransaction::DHT_GET_PEERS:
      parse_get_peers_reply(transaction->as_get_peers(), response);
      break;

    // Nothing to do for DHT_PING and DHT_ANNOUNCE_PEER
    default:
      break;
  }

  // Mark node responsive only if all processing was successful, without errors.
  m_router->node_replied(id, sa);

  delete itr->second;
  m_transactions.erase(itr);
}

void
DhtServer::process_error(int transactionId, const rak::socket_address* sa, Object& request) {
  transaction_itr itr = m_transactions.find(DhtTransaction::key(sa, transactionId));

  if (itr == m_transactions.end())
    return;

  m_repliesReceived++;
  m_networkUp = true;

  // Don't mark node as good (because it replied) or bad (because it returned an error).
  // If it consistently returns errors for valid queries it's probably broken.  But a
  // few error messages are acceptable. So we do nothing and pretend the query never happened.

  delete itr->second;
  m_transactions.erase(itr);
}

void
DhtServer::parse_find_node_reply(DhtTransactionSearch* transaction, const std::string& nodes) {
  transaction->complete(true);

  if (sizeof(const compact_node_info) != 26)
    throw internal_error("DhtServer::parse_find_node_reply(...) bad struct size.");

  node_info_list list;
  std::copy(reinterpret_cast<const compact_node_info*>(nodes.c_str()),
            reinterpret_cast<const compact_node_info*>(nodes.c_str() + nodes.size() - nodes.size() % sizeof(compact_node_info)),
            std::back_inserter(list));

  for (node_info_list::iterator itr = list.begin(); itr != list.end(); ++itr) {
    if (itr->id() != m_router->id()) {
      rak::socket_address sa = itr->address();
      transaction->search()->add_contact(itr->id(), &sa);
    }
  }

  find_node_next(transaction);
}

void
DhtServer::parse_get_peers_reply(DhtTransactionGetPeers* transaction, const Object& response) {
  DhtAnnounce* announce = static_cast<DhtAnnounce*>(transaction->as_search()->search());

  transaction->complete(true);

  if (response.has_key_list("values"))
    announce->receive_peers((*response.get_key_list("values").begin()).as_string());

  if (response.has_key_string("token"))
    add_transaction(new DhtTransactionAnnouncePeer(transaction->id(), transaction->address(), announce->target(), response.get_key_string("token")), packet_prio_low);

  announce->update_status();
}

void
DhtServer::find_node_next(DhtTransactionSearch* transaction) {
  int priority = packet_prio_low;
  if (transaction->search()->is_announce())
    priority = packet_prio_high;

  DhtSearch::const_accessor node;
  while ((node = transaction->search()->get_contact()) != transaction->search()->end())
    add_transaction(new DhtTransactionFindNode(node), priority);

  if (!transaction->search()->is_announce())
    return;

  DhtAnnounce* announce = static_cast<DhtAnnounce*>(transaction->search());
  if (announce->complete()) {
    // We have found the 8 closest nodes to the info hash. Retrieve peers
    // from them and announce to them.
    for (node = announce->start_announce(); node != announce->end(); ++node)
      add_transaction(new DhtTransactionGetPeers(node), packet_prio_high);
  }

  announce->update_status();
}

void
DhtServer::add_packet(DhtTransactionPacket* packet, int priority) {
  switch (priority) {
    // High priority packets are for important queries, and quite small.
    // They're added to front of high priority queue and thus will be the
    // next packets sent.
    case packet_prio_high:
      m_highQueue.push_front(packet);
      break;

    // Low priority query packets are added to the back of the high priority
    // queue and will be sent when all high priority packets have been transmitted.
    case packet_prio_low:
      m_highQueue.push_back(packet);
      break;

    // Reply packets will be processed after all of our own packets have been send.
    case packet_prio_reply:
      m_lowQueue.push_back(packet);
      break;

    default:
      throw internal_error("DhtServer::add_packet called with invalid priority.");
  }
}

void
DhtServer::create_query(transaction_itr itr, int tID, const rak::socket_address* sa, int priority) {
  if (itr->second->id() == m_router->id())
    throw internal_error("DhtServer::create_query trying to send to itself.");

  Object query(Object::TYPE_MAP);

  DhtTransaction* transaction = itr->second;
  char trans_id = tID;
  query.insert_key("t", std::string(&trans_id, 1));
  query.insert_key("y", "q");
  query.insert_key("q", queries[transaction->type()]);
  query.insert_key("v", PEER_VERSION);

  Object& q = query.insert_key("a", Object(Object::TYPE_MAP));
  q.insert_key("id", m_router->str());

  switch (transaction->type()) {
    case DhtTransaction::DHT_PING:
      // nothing to do
      break;

    case DhtTransaction::DHT_FIND_NODE:
      q.insert_key("target", transaction->as_find_node()->search()->target().str());
      break;

    case DhtTransaction::DHT_GET_PEERS:
      q.insert_key("info_hash", transaction->as_get_peers()->search()->target().str());
      break;

    case DhtTransaction::DHT_ANNOUNCE_PEER:
      q.insert_key("info_hash", transaction->as_announce_peer()->info_hash().str());
      q.insert_key("token", transaction->as_announce_peer()->token());
      q.insert_key("port", manager->connection_manager()->listen_port());
      break;
  }

  DhtTransactionPacket* packet = new DhtTransactionPacket(transaction->address(), query, tID, transaction);
  transaction->set_packet(packet);
  add_packet(packet, priority);

  m_queriesSent++;
}

void
DhtServer::create_response(const Object& transactionId, const rak::socket_address* sa, Object& r) {
  Object reply(Object::TYPE_MAP);
  r.insert_key("id", m_router->str());

  reply.insert_key("t", transactionId);
  reply.insert_key("y", "r");
  reply.insert_key("r", r);
  reply.insert_key("v", PEER_VERSION);

  add_packet(new DhtTransactionPacket(sa, reply), packet_prio_reply);
}

void
DhtServer::create_error(const Object* transactionId, const rak::socket_address* sa, int num, const std::string& msg) {
  Object error(Object::TYPE_MAP);

  if (transactionId != NULL)
    error.insert_key("t", *transactionId);

  error.insert_key("y", "e");
  error.insert_key("v", PEER_VERSION);

  Object& e = error.insert_key("e", Object(Object::TYPE_LIST));
  e.insert_back(num);
  e.insert_back(msg);

  add_packet(new DhtTransactionPacket(sa, error), packet_prio_reply);
}

int
DhtServer::add_transaction(DhtTransaction* transaction, int priority) {
  // Try random transaction ID. This is to make it less likely that we reuse
  // a transaction ID from an earlier transaction which timed out and we forgot
  // about it, so that if the node replies after the timeout it's less likely
  // that we match the reply to the wrong transaction.
  //
  // If there's an existing transaction with the random ID we search for the next
  // unused one. Since normally only one or two transactions will be active per
  // node, a collision is extremely unlikely, and a linear search for the first
  // open one is the most efficient.
  unsigned int rnd = (uint8_t)random();
  unsigned int id = rnd;

  transaction_itr insertItr = m_transactions.lower_bound(transaction->key(rnd));

  // If key matches, keep trying successive IDs.
  while (insertItr != m_transactions.end() && insertItr->first == transaction->key(id)) {
    ++insertItr;
    id = (uint8_t)(id + 1);

    // Give up after trying all possible IDs. This should never happen.
    if (id == rnd) {
      delete transaction;
      return -1;
    }

    // Transaction ID wrapped around, reset iterator.
    if (id == 0)
      insertItr = m_transactions.lower_bound(transaction->key(id));
  }

  // We know where to insert it, so pass that as hint.
  insertItr = m_transactions.insert(insertItr, std::make_pair(transaction->key(id), transaction));

  create_query(insertItr, id, transaction->address(), priority);

  start_write();

  return id;
}

// Transaction received no reply and timed out. Mark node as bad and remove
// transaction (except if it was only the quick timeout).
DhtServer::transaction_itr
DhtServer::failed_transaction(transaction_itr itr, bool quick) {
  DhtTransaction* transaction = itr->second;

  // If it was a known node, remember that it didn't reply, unless the transaction
  // is only stalled (had quick timeout, but not full timeout).  Also if the
  // transaction still has an associated packet, the packet never got sent due to
  // throttling, so don't blame the remote node for not replying.
  // Finally, if we haven't received anything whatsoever so far, assume the entire
  // network is down and so we can't blame the node either.
  if (!quick && m_networkUp && transaction->packet() == NULL && transaction->id() != m_router->zero_id)
    m_router->node_inactive(transaction->id(), transaction->address());

  if (transaction->type() == DhtTransaction::DHT_FIND_NODE) {
    if (quick)
      transaction->as_find_node()->set_stalled();
    else
      transaction->as_find_node()->complete(false);

    find_node_next(transaction->as_find_node());
  }

  if (quick) {
    return ++itr;         // don't actually delete the transaction until the final timeout

  } else {
    delete itr->second;
    m_transactions.erase(itr++);
    return itr;
  }
}

void
DhtServer::clear_transactions() {
  std::for_each(m_transactions.begin(), m_transactions.end(),
                rak::on(rak::mem_ref(&transaction_map::value_type::second),
                        rak::call_delete<DhtTransaction>()));
  m_transactions.clear();
}

void
DhtServer::event_read() {
  uint32_t total = 0;
  std::istringstream sstream;

  sstream.imbue(std::locale::classic());

  while (true) {
    rak::socket_address sa;
    int type = '?';
    const Object* transactionId = NULL;
    const HashString* nodeId = NULL;

    try {
      char buffer[2048];
      int32_t read = read_datagram(buffer, sizeof(buffer), &sa);

      if (read < 0)
        break;

      total += read;
      sstream.str(std::string(buffer, read));

      Object request;
      sstream >> request;

      // If it's not a valid bencode dictionary at all, it's probably not a DHT
      // packet at all, so we don't throw an error to prevent bounce loops.
      if (sstream.fail() || !request.is_map())
        continue;

      if (!request.has_key("t"))
        throw dht_error(dht_error_protocol, "No transaction ID");

      transactionId = &request.get_key("t");

      if (!request.has_key_string("y"))
        throw dht_error(dht_error_protocol, "No message type");

      if (request.get_key_string("y").length() != 1)
        throw dht_error(dht_error_bad_method, "Unsupported message type");

      type = request.get_key_string("y")[0];

      // Queries and replies have node ID in different dictionaries.
      if (type == 'r' || type == 'q') {
        const std::string& nodeIdStr = request.get_key(type == 'q' ? "a" : "r").get_key_string("id");

        if (nodeIdStr.length() < HashString::size_data)
          throw dht_error(dht_error_protocol, "`id' value too short");

        nodeId = HashString::cast_from(nodeIdStr);
      }

      // Sanity check the returned transaction ID.
      if ((type == 'r' || type == 'e') && 
          (!transactionId->is_string() || transactionId->as_string().length() != 1))
        throw dht_error(dht_error_protocol, "Invalid transaction ID type/length.");

      // Stupid broken implementations.
      if (nodeId != NULL && *nodeId == m_router->id())
        throw dht_error(dht_error_protocol, "Send your own ID, not mine");

      switch (type) {
        case 'q':
          process_query(*transactionId, *nodeId, &sa, request);
          break;

        case 'r':
          process_response(((unsigned char*)transactionId->as_string().c_str())[0], *nodeId, &sa, request);
          break;

        case 'e':
          process_error(((unsigned char*)transactionId->as_string().c_str())[0], &sa, request);
          break;

        default:
          throw dht_error(dht_error_bad_method, "Unknown message type.");
      }

    // If node was querying us, reply with error packet, otherwise mark the node as "query failed",
    // so that if it repeatedly sends malformed replies we will drop it instead of propagating it
    // to other nodes.
    } catch (bencode_error& e) {
      if ((type == 'r' || type == 'e') && nodeId != NULL)
        m_router->node_inactive(*nodeId, &sa);
      else
        create_error(transactionId, &sa, dht_error_protocol, std::string("Malformed packet: ") + e.what());

    } catch (dht_error& e) {
      if ((type == 'r' || type == 'e') && nodeId != NULL)
        m_router->node_inactive(*nodeId, &sa);
      else
        create_error(transactionId, &sa, e.code(), e.what());

    } catch (network_error& e) {

    }
  }

  manager->download_throttle()->throttle_list()->node_used_unthrottled(total);
  m_downloadThrottle.rate()->insert(total);

  start_write();
}

bool
DhtServer::process_queue(packet_queue& queue, uint32_t* quota) {
  uint32_t used = 0;

  while (!queue.empty()) {
    DhtTransactionPacket* packet = queue.front();

    // Make sure its transaction hasn't timed out yet, if it has/had one
    // and don't bother sending non-transaction packets (replies) after 
    // more than 15 seconds in the queue.
    if (packet->has_failed() || packet->age() > 15) {
      delete packet;
      queue.pop_front();
      continue;
    }

    if (packet->length() > *quota) {
      manager->upload_throttle()->throttle_list()->node_used(&m_uploadThrottle, used);
      return false;
    }

    queue.pop_front();

    try {
      int written = write_datagram(packet->c_str(), packet->length(), packet->address());

      if (written == -1)
        throw network_error();

      used += written;
      *quota -= written;

      if ((unsigned int)written != packet->length())
        throw network_error();

    } catch (network_error& e) {
      // Couldn't write packet, maybe something wrong with node address or routing, so mark node as bad.
      if (packet->has_transaction()) {
        transaction_itr itr = m_transactions.find(packet->transaction()->key(packet->id()));
        if (itr == m_transactions.end())
          throw internal_error("DhtServer::process_queue could not find transaction.");

        failed_transaction(itr, false);
      }
    }

    if (packet->has_transaction())
      packet->transaction()->set_packet(NULL);

    delete packet;
  }

  manager->upload_throttle()->throttle_list()->node_used(&m_uploadThrottle, used);
  return true;
}

void
DhtServer::event_write() {
  if (m_highQueue.empty() && m_lowQueue.empty())
    throw internal_error("DhtServer::event_write called but both write queues are empty.");

  if (!manager->upload_throttle()->throttle_list()->is_throttled(&m_uploadThrottle))
    throw internal_error("DhtServer::event_write called while not in throttle list.");

  uint32_t quota = manager->upload_throttle()->throttle_list()->node_quota(&m_uploadThrottle);

  if (quota == 0 || !process_queue(m_highQueue, &quota) || !process_queue(m_lowQueue, &quota)) {
    manager->poll()->remove_write(this);
    manager->upload_throttle()->throttle_list()->node_deactivate(&m_uploadThrottle);

  } else if (m_highQueue.empty() && m_lowQueue.empty()) {
    manager->poll()->remove_write(this);
    manager->upload_throttle()->throttle_list()->erase(&m_uploadThrottle);
  }
}

void
DhtServer::event_error() {
}

void
DhtServer::start_write() {
  if ((!m_highQueue.empty() || !m_lowQueue.empty()) && !manager->upload_throttle()->throttle_list()->is_throttled(&m_uploadThrottle)) {
    manager->upload_throttle()->throttle_list()->insert(&m_uploadThrottle);
    manager->poll()->insert_write(this);
  }

  if (!m_taskTimeout.is_queued() && !m_transactions.empty())
    priority_queue_insert(&taskScheduler, &m_taskTimeout, (cachedTime + rak::timer::from_seconds(5)).round_seconds());
}

void
DhtServer::receive_timeout() {
  transaction_itr itr = m_transactions.begin();
  while (itr != m_transactions.end()) {
    if (itr->second->has_quick_timeout() && itr->second->quick_timeout() < cachedTime.seconds()) {
      itr = failed_transaction(itr, true);

    } else if (itr->second->timeout() < cachedTime.seconds()) {
      itr = failed_transaction(itr, false);

    } else {
      ++itr;
    }
  }

  start_write();
}

}
