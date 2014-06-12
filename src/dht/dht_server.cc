// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
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
#include <cstdio>
#include <rak/functional.h>

#include "torrent/exceptions.h"
#include "torrent/connection_manager.h"
#include "torrent/download_info.h"
#include "torrent/object.h"
#include "torrent/object_stream.h"
#include "torrent/poll.h"
#include "torrent/object_static_map.h"
#include "torrent/throttle.h"
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

// List of all possible keys we need/support in a DHT message.
// Unsupported keys we receive are dropped (ignored) while decoding.
// See torrent/object_static_map.h for how this works.
template <>
const DhtMessage::key_list_type DhtMessage::base_type::keys = {
  { key_a_id,       "a::id*S" },
  { key_a_infoHash, "a::info_hash*S" },
  { key_a_port,     "a::port", },
  { key_a_target,   "a::target*S" },
  { key_a_token,    "a::token*S" },

  { key_e_0,        "e[]*" },
  { key_e_1,        "e[]*" },

  { key_q,          "q*S" },

  { key_r_id,       "r::id*S" },
  { key_r_nodes,    "r::nodes*S" },
  { key_r_token,    "r::token*S" },
  { key_r_values,   "r::values*L" },

  { key_t,          "t*S" },
  { key_v,          "v*" },
  { key_y,          "y*S" },
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

  m_uploadNode(60),
  m_downloadNode(60),

  m_uploadThrottle(manager->upload_throttle()->throttle_list()),
  m_downloadThrottle(manager->download_throttle()->throttle_list()),

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

  m_taskTimeout.slot() = std::tr1::bind(&DhtServer::receive_timeout, this);

  m_uploadNode.set_list_iterator(m_uploadThrottle->end());
  m_uploadNode.slot_activate() = std::tr1::bind(&SocketBase::receive_throttle_up_activate, static_cast<SocketBase*>(this));

  m_downloadNode.set_list_iterator(m_downloadThrottle->end());
  m_downloadThrottle->insert(&m_downloadNode);

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

  m_uploadThrottle->erase(&m_uploadNode);
  m_downloadThrottle->erase(&m_downloadNode);

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
  m_errorsReceived = 0;
  m_errorsCaught = 0;

  m_uploadNode.rate()->set_total(0);
  m_downloadNode.rate()->set_total(0);
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
DhtServer::process_query(const HashString& id, const rak::socket_address* sa, const DhtMessage& msg) {
  m_queriesReceived++;
  m_networkUp = true;

  raw_string query = msg[key_q].as_raw_string();

  // Construct reply.
  DhtMessage reply;

  if (query == raw_string::from_c_str("find_node"))
    create_find_node_response(msg, reply);

  else if (query == raw_string::from_c_str("get_peers"))
    create_get_peers_response(msg, sa, reply);

  else if (query == raw_string::from_c_str("announce_peer"))
    create_announce_peer_response(msg, sa, reply);

  else if (query != raw_string::from_c_str("ping"))
    throw dht_error(dht_error_bad_method, "Unknown query type.");

  m_router->node_queried(id, sa);
  create_response(msg, sa, reply);
}

void
DhtServer::create_find_node_response(const DhtMessage& req, DhtMessage& reply) {
  raw_string target = req[key_a_target].as_raw_string();

  if (target.size() < HashString::size_data)
    throw dht_error(dht_error_protocol, "target string too short");

  reply[key_r_nodes] = m_router->get_closest_nodes(*HashString::cast_from(target.data()));

  if (reply[key_r_nodes].as_raw_string().empty())
    throw dht_error(dht_error_generic, "No nodes");
}

void
DhtServer::create_get_peers_response(const DhtMessage& req, const rak::socket_address* sa, DhtMessage& reply) {
  reply[key_r_token] = m_router->make_token(sa, reply.data_end);
  reply.data_end += reply[key_r_token].as_raw_string().size();

  raw_string info_hash_str = req[key_a_infoHash].as_raw_string();

  if (info_hash_str.size() < HashString::size_data)
    throw dht_error(dht_error_protocol, "info hash too short");

  const HashString* info_hash = HashString::cast_from(info_hash_str.data());

  DhtTracker* tracker = m_router->get_tracker(*info_hash, false);

  // If we're not tracking or have no peers, send closest nodes.
  if (!tracker || tracker->empty()) {
    raw_string nodes = m_router->get_closest_nodes(*info_hash);

    if (nodes.empty())
      throw dht_error(dht_error_generic, "No peers nor nodes");

    reply[key_r_nodes] = nodes;

  } else {
    reply[key_r_values] = tracker->get_peers();
  }
}

void
DhtServer::create_announce_peer_response(const DhtMessage& req, const rak::socket_address* sa, DhtMessage& reply) {
  raw_string info_hash = req[key_a_infoHash].as_raw_string();

  if (info_hash.size() < HashString::size_data)
    throw dht_error(dht_error_protocol, "info hash too short");

  if (!m_router->token_valid(req[key_a_token].as_raw_string(), sa))
    throw dht_error(dht_error_protocol, "Token invalid.");

  DhtTracker* tracker = m_router->get_tracker(*HashString::cast_from(info_hash.data()), true);
  tracker->add_peer(sa->sa_inet()->address_n(), req[key_a_port].as_value());
}

void
DhtServer::process_response(const HashString& id, const rak::socket_address* sa, const DhtMessage& response) {
  int transactionId = (unsigned char)response[key_t].as_raw_string().data()[0];
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

  // Make sure transaction is erased even if an exception is thrown.
  try {
    DhtTransaction* transaction = itr->second;
#ifdef USE_EXTRA_DEBUG
    if (DhtTransaction::key(sa, transactionId) != transaction->key(transactionId))
      throw internal_error("DhtServer::process_response key mismatch.");
#endif

    // If we contact a node but its ID is not the one we expect, ignore the reply
    // to prevent interference from rogue nodes.
    if ((id != transaction->id() && transaction->id() != m_router->zero_id))
      return;

    switch (transaction->type()) {
      case DhtTransaction::DHT_FIND_NODE:
        parse_find_node_reply(transaction->as_find_node(), response[key_r_nodes].as_raw_string());
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

  } catch (std::exception& e) {
    delete itr->second;
    m_transactions.erase(itr);

    m_errorsCaught++;
    throw;
  }

  delete itr->second;
  m_transactions.erase(itr);
}

void
DhtServer::process_error(const rak::socket_address* sa, const DhtMessage& error) {
  int transactionId = (unsigned char)error[key_t].as_raw_string().data()[0];
  transaction_itr itr = m_transactions.find(DhtTransaction::key(sa, transactionId));

  if (itr == m_transactions.end())
    return;

  m_repliesReceived++;
  m_errorsReceived++;
  m_networkUp = true;

  // Don't mark node as good (because it replied) or bad (because it returned an error).
  // If it consistently returns errors for valid queries it's probably broken.  But a
  // few error messages are acceptable. So we do nothing and pretend the query never happened.

  delete itr->second;
  m_transactions.erase(itr);
}

void
DhtServer::parse_find_node_reply(DhtTransactionSearch* transaction, raw_string nodes) {
  transaction->complete(true);

  if (sizeof(const compact_node_info) != 26)
    throw internal_error("DhtServer::parse_find_node_reply(...) bad struct size.");

  node_info_list list;
  std::copy(reinterpret_cast<const compact_node_info*>(nodes.data()),
            reinterpret_cast<const compact_node_info*>(nodes.data() + nodes.size() - nodes.size() % sizeof(compact_node_info)),
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
DhtServer::parse_get_peers_reply(DhtTransactionGetPeers* transaction, const DhtMessage& response) {
  DhtAnnounce* announce = static_cast<DhtAnnounce*>(transaction->as_search()->search());

  transaction->complete(true);

  if (response[key_r_values].is_raw_list())
    announce->receive_peers(response[key_r_values].as_raw_list());

  if (response[key_r_token].is_raw_string())
    add_transaction(new DhtTransactionAnnouncePeer(transaction->id(),
                                                   transaction->address(),
                                                   announce->target(),
                                                   response[key_r_token].as_raw_string()),
                    packet_prio_low);

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

  DhtMessage query;

  // Transaction ID is a bencode string.
  query[key_t] = raw_bencode(query.data_end, 3);
  *query.data_end++ = '1';
  *query.data_end++ = ':';
  *query.data_end++ = tID;

  DhtTransaction* transaction = itr->second;
  query[key_q] = raw_string::from_c_str(queries[transaction->type()]);
  query[key_y] = raw_bencode::from_c_str("1:q");
  query[key_v] = raw_bencode("4:" PEER_VERSION, 6);
  query[key_a_id] = m_router->id_raw_string();

  switch (transaction->type()) {
    case DhtTransaction::DHT_PING:
      // nothing to do
      break;

    case DhtTransaction::DHT_FIND_NODE:
      query[key_a_target] = transaction->as_find_node()->search()->target_raw_string();
      break;

    case DhtTransaction::DHT_GET_PEERS:
      query[key_a_infoHash] = transaction->as_get_peers()->search()->target_raw_string();
      break;

    case DhtTransaction::DHT_ANNOUNCE_PEER:
      query[key_a_infoHash] = transaction->as_announce_peer()->info_hash_raw_string();
      query[key_a_token] = transaction->as_announce_peer()->token();
      query[key_a_port] = manager->connection_manager()->listen_port();
      break;
  }

  DhtTransactionPacket* packet = new DhtTransactionPacket(transaction->address(), query, tID, transaction);
  transaction->set_packet(packet);
  add_packet(packet, priority);

  m_queriesSent++;
}

void
DhtServer::create_response(const DhtMessage& req, const rak::socket_address* sa, DhtMessage& reply) {
  reply[key_r_id] = m_router->id_raw_string();
  reply[key_t] = req[key_t];
  reply[key_y] = raw_bencode::from_c_str("1:r");
  reply[key_v] = raw_bencode("4:" PEER_VERSION, 6);

  add_packet(new DhtTransactionPacket(sa, reply), packet_prio_reply);
}

void
DhtServer::create_error(const DhtMessage& req, const rak::socket_address* sa, int num, const char* msg) {
  DhtMessage error;

  if (req[key_t].is_raw_bencode() || req[key_t].is_raw_string())
    error[key_t] = req[key_t];

  error[key_y] = raw_bencode::from_c_str("1:e");
  error[key_v] = raw_bencode("4:" PEER_VERSION, 6);
  error[key_e_0] = num;
  error[key_e_1] = raw_string::from_c_str(msg);

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

    try {
      find_node_next(transaction->as_find_node());

    } catch (std::exception& e) {
      if (!quick) {
        delete itr->second;
        m_transactions.erase(itr);
      }

      throw;
    }
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
  for (transaction_map::iterator itr = m_transactions.begin(), last = m_transactions.end(); itr != last; itr++)
    delete itr->second;

  m_transactions.clear();
}

void
DhtServer::event_read() {
  uint32_t total = 0;

  while (true) {
    Object request;
    rak::socket_address sa;
    int type = '?';
    DhtMessage message;
    const HashString* nodeId = NULL;

    try {
      char buffer[2048];
      int32_t read = read_datagram(buffer, sizeof(buffer), &sa);

      if (read < 0)
        break;

      total += read;

      // If it's not a valid bencode dictionary at all, it's probably not a DHT
      // packet at all, so we don't throw an error to prevent bounce loops.
      try {
        static_map_read_bencode(buffer, buffer + read, message);
      } catch (bencode_error& e) {
        continue;
      }

      if (!message[key_t].is_raw_string())
        throw dht_error(dht_error_protocol, "No transaction ID");

      if (!message[key_y].is_raw_string())
        throw dht_error(dht_error_protocol, "No message type");

      if (message[key_y].as_raw_string().size() != 1)
        throw dht_error(dht_error_bad_method, "Unsupported message type");

      type = message[key_y].as_raw_string().data()[0];

      // Queries and replies have node ID in different dictionaries.
      if (type == 'r' || type == 'q') {
        if (!message[type == 'q' ? key_a_id : key_r_id].is_raw_string())
          throw dht_error(dht_error_protocol, "Invalid `id' value");

        raw_string nodeIdStr = message[type == 'q' ? key_a_id : key_r_id].as_raw_string();

        if (nodeIdStr.size() < HashString::size_data)
          throw dht_error(dht_error_protocol, "`id' value too short");

        nodeId = HashString::cast_from(nodeIdStr.data());
      }

      // Sanity check the returned transaction ID.
      if ((type == 'r' || type == 'e') && 
          (!message[key_t].is_raw_string() || message[key_t].as_raw_string().size() != 1))
        throw dht_error(dht_error_protocol, "Invalid transaction ID type/length.");

      // Stupid broken implementations.
      if (nodeId != NULL && *nodeId == m_router->id())
        throw dht_error(dht_error_protocol, "Send your own ID, not mine");

      switch (type) {
        case 'q':
          process_query(*nodeId, &sa, message);
          break;

        case 'r':
          process_response(*nodeId, &sa, message);
          break;

        case 'e':
          process_error(&sa, message);
          break;

        default:
          throw dht_error(dht_error_bad_method, "Unknown message type.");
      }

    // If node was querying us, reply with error packet, otherwise mark the node as "query failed",
    // so that if it repeatedly sends malformed replies we will drop it instead of propagating it
    // to other nodes.
    } catch (bencode_error& e) {
      if ((type == 'r' || type == 'e') && nodeId != NULL) {
        m_router->node_inactive(*nodeId, &sa);
      } else {
        snprintf(message.data_end, message.data + message.data_size - message.data_end - 1, "Malformed packet: %s", e.what());
        message.data[message.data_size - 1] = '\0';
        create_error(message, &sa, dht_error_protocol, message.data_end);
      }

    } catch (dht_error& e) {
      if ((type == 'r' || type == 'e') && nodeId != NULL)
        m_router->node_inactive(*nodeId, &sa);
      else
        create_error(message, &sa, e.code(), e.what());

    } catch (network_error& e) {

    }
  }

  m_downloadThrottle->node_used_unthrottled(total);
  m_downloadNode.rate()->insert(total);

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
      m_uploadThrottle->node_used(&m_uploadNode, used);
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

  m_uploadThrottle->node_used(&m_uploadNode, used);
  return true;
}

void
DhtServer::event_write() {
  if (m_highQueue.empty() && m_lowQueue.empty())
    throw internal_error("DhtServer::event_write called but both write queues are empty.");

  if (!m_uploadThrottle->is_throttled(&m_uploadNode))
    throw internal_error("DhtServer::event_write called while not in throttle list.");

  uint32_t quota = m_uploadThrottle->node_quota(&m_uploadNode);

  if (quota == 0 || !process_queue(m_highQueue, &quota) || !process_queue(m_lowQueue, &quota)) {
    manager->poll()->remove_write(this);
    m_uploadThrottle->node_deactivate(&m_uploadNode);

  } else if (m_highQueue.empty() && m_lowQueue.empty()) {
    manager->poll()->remove_write(this);
    m_uploadThrottle->erase(&m_uploadNode);
  }
}

void
DhtServer::event_error() {
}

void
DhtServer::start_write() {
  if ((!m_highQueue.empty() || !m_lowQueue.empty()) && !m_uploadThrottle->is_throttled(&m_uploadNode)) {
    m_uploadThrottle->insert(&m_uploadNode);
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
