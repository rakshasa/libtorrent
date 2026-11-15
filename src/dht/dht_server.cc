#include "config.h"

#include "dht/dht_server.h"

#include <algorithm>

#include "manager.h"
#include "dht/dht_bucket.h"
#include "dht/dht_router.h"
#include "dht/dht_transaction.h"
#include "dht/transactions/dht_announce.h"
#include "torrent/exceptions.h"
#include "torrent/object.h"
#include "torrent/object_static_map.h"
#include "torrent/object_stream.h"
#include "torrent/net/socket_address.h"
#include "torrent/runtime/runtime.h"
#include "torrent/utils/log.h"
#include "tracker/tracker_dht.h"

#define LT_LOG_THIS(log_fmt, ...)                                       \
  lt_log_print(LOG_DHT_SERVER, "dht_server : " log_fmt, __VA_ARGS__);

namespace torrent {

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
  { key_r_nodes6,   "r::nodes6*S" },
  { key_r_token,    "r::token*S" },
  { key_r_values,   "r::values*L" },

  { key_t,          "t*S" },
  { key_v,          "v*" },
  { key_y,          "y*S" },
};

DhtServer::DhtServer(DhtRouter* router)
  : m_router(router) {

  // router->id(),
  //   [this](const HashString& id, const sockaddr* sa, const DhtMessage& message) { process_query(id, sa, message); },
  //   [this](const DhtMessage& message, const sockaddr* sa, int code, const char* text) { create_error(message, sa, code, text); },
  //   [this](const HashString& id, const sockaddr* sa) { m_router->node_inactive(id, sa); },
  //   [this] { start_write(); }, [this] { event_write(); })

  reset_statistics();
}

DhtServer::~DhtServer() {
  stop();
}

void
DhtServer::start(int port) {
  m_udp_router.open(m_router->family(), port);
  m_router->set_address(m_udp_router.socket_address());
}

void
DhtServer::stop() {
  if (!is_active())
    return;

  LT_LOG_THIS("stopping", 0);
  LT_LOG_THIS("searches : count:%zu", m_searches.size());

  clear_transactions();

  m_udp_router.close();

  m_network_up = false;
}

void
DhtServer::reset_statistics() {
  m_queriesReceived = 0;
  m_queriesSent = 0;
  m_repliesReceived = 0;
  m_errorsReceived = 0;
  m_errorsCaught = 0;
}

// Ping a node whose ID we know.
void
DhtServer::ping(const HashString& id, const sockaddr* sa) {
  if (m_transactions.size() >= max_transactions)
    return;

  if (!sa_is_inet(sa) && !sa_is_inet6(sa))
    return;

  // No point pinging a node that we're already contacting otherwise.
  auto itr = m_transactions.lower_bound(DhtTransaction::key(sa, 0));

  if (itr == m_transactions.end() || !DhtTransaction::key_match(itr->first, sa))
    add_transaction(std::unique_ptr<DhtTransaction>(new DhtTransactionPing(id, sa)), packet_prio_low);
}

// Contact nodes in given bucket and ask for their nodes closest to target.
void
DhtServer::find_node(const DhtBucket& contacts, const HashString& target) {
  auto search = std::make_shared<dht::DhtSearch>(this, target);
  search->add_contacts(contacts);

  auto n = search->get_contact();

  while (n != search->end()) {
    add_transaction(std::unique_ptr<DhtTransaction>(new DhtTransactionFindNode(search, n)), packet_prio_low);
    n = search->get_contact();
  }

  // This shouldn't happen, it means we had no contactable nodes at all.
  if (!search->start())
    return;
    // throw internal_error("DhtServer::find_node search start failed, no contactable nodes.");  ///////////////// TMP

  m_searches.insert(search);
}

void
DhtServer::announce(const DhtBucket& contacts, const HashString& infoHash, std::weak_ptr<TrackerDht> tracker, bool primary) {
  auto announce = std::make_shared<dht::DhtAnnounce>(this, infoHash, tracker, primary);
  announce->add_contacts(contacts);

  auto n = announce->get_contact();

  while (n != announce->end()) {
    add_transaction(std::unique_ptr<DhtTransaction>(new DhtTransactionGetPeers(announce, n)), packet_prio_high);
    n = announce->get_contact();
  }

  // This can only happen if all nodes we know are bad.
  if (!announce->start())
    return;
    // throw internal_error("DhtServer::announce search start failed, no contactable nodes.");  ///////////////// TMP

  m_searches.insert(announce);
  announce->update_status();
}

void
DhtServer::cancel_announce(const HashString& info_hash, std::weak_ptr<TrackerDht> tracker) {
  auto itr = m_transactions.begin();

  // TODO: Verify this removes us from m_searches.

  while (itr != m_transactions.end()) {
    if (itr->second->is_search() && itr->second->as_search()->search()->is_announce()) {
      auto announce = dynamic_cast<dht::DhtAnnounce*>(itr->second->as_search()->search().get());

      if (announce == nullptr)
        throw internal_error("DhtServer::cancel_announce dynamic_cast to DhtAnnounce failed.");

      bool tracker_match = !tracker.owner_before(announce->tracker()) && !announce->tracker().owner_before(tracker);

      if (announce->target() == info_hash && (tracker.expired() || tracker_match)) {
        itr = erase_transaction(itr);
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
  m_network_up = false;
}

void
DhtServer::check_search_completed(std::shared_ptr<dht::DhtSearch> search) {
  if (!search->complete())
    return;

  // Removing search if has completed.

  auto itr = m_searches.find(search);

  if (itr == m_searches.end())
    return; // throw internal_error("DhtServer::mark_search_completed search not found.");

  // TODO: Verify we got ref_count == 2.

  m_searches.erase(itr);
}

void
DhtServer::check_search_trimming(std::shared_ptr<dht::DhtSearch> search) {
  // Make sure trimming search does not delete searches that have not completed.

  if (search->complete())
    return;

  // TODO: Should we erase if complete?

  auto itr = m_searches.lower_bound(search);

  if (itr != m_searches.end() && *itr == search)
    return;

  m_searches.insert(itr, search);
}

void
DhtServer::process_query(const HashString& id, const sockaddr* sa, const DhtMessage& msg) {
  m_queriesReceived++;
  m_network_up = true;

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
    throw dht::dht_error(dht_error_bad_method, "Unknown query type.");

  m_router->node_queried(id, sa);
  create_response(msg, sa, reply);
}

void
DhtServer::create_find_node_response(const DhtMessage& req, DhtMessage& reply) {
  raw_string target = req[key_a_target].as_raw_string();

  if (target.size() < HashString::size_data)
    throw dht::dht_error(dht_error_protocol, "target string too short");

  auto nodes_key = m_router->family() == AF_INET6 ? key_r_nodes6 : key_r_nodes;

  reply[nodes_key] = m_router->get_closest_nodes(*HashString::cast_from(target.data()));

  if (reply[nodes_key].as_raw_string().empty())
    throw dht::dht_error(dht_error_generic, "No nodes");
}

void
DhtServer::create_get_peers_response(const DhtMessage& req, const sockaddr* sa, DhtMessage& reply) {
  reply[key_r_token] = m_router->make_token(sa, reply.data_end);
  reply.data_end += reply[key_r_token].as_raw_string().size();

  raw_string info_hash_str = req[key_a_infoHash].as_raw_string();

  if (info_hash_str.size() < HashString::size_data)
    throw dht::dht_error(dht_error_protocol, "info hash too short");

  const HashString* info_hash = HashString::cast_from(info_hash_str.data());

  DhtTracker* tracker = m_router->get_tracker(*info_hash, false);

  // If we're not tracking or have no peers, send closest nodes.
  if (!tracker || tracker->empty()) {
    raw_string nodes = m_router->get_closest_nodes(*info_hash);

    if (nodes.empty())
      throw dht::dht_error(dht_error_generic, "No peers nor nodes");

    reply[m_router->family() == AF_INET6 ? key_r_nodes6 : key_r_nodes] = nodes;

  } else if (m_router->family() == AF_INET6) {
    reply[key_r_values] = tracker->get_peers6();

  } else {
    reply[key_r_values] = tracker->get_peers();
  }
}

void
DhtServer::create_announce_peer_response(const DhtMessage& req, const sockaddr* sa, [[maybe_unused]] DhtMessage& reply) {
  raw_string info_hash = req[key_a_infoHash].as_raw_string();

  if (info_hash.size() < HashString::size_data)
    throw dht::dht_error(dht_error_protocol, "info hash too short");

  if (!m_router->token_valid(req[key_a_token].as_raw_string(), sa))
    throw dht::dht_error(dht_error_protocol, "Token invalid.");

  if (!req[key_a_port].is_value())
    throw dht::dht_error(dht_error_protocol, "Port missing.");

  auto port = req[key_a_port].as_value();

  if (port <= 0 || port > 65535)
    throw dht::dht_error(dht_error_protocol, "Port invalid.");

  if (sa_is_inet(sa)) {
    DhtTracker* tracker = m_router->get_tracker(*HashString::cast_from(info_hash.data()), true);

    if (tracker == NULL)
      throw dht::dht_error(dht_error_generic, "Tracking too many info hashes");

    tracker->add_peer(reinterpret_cast<const sockaddr_in*>(sa)->sin_addr.s_addr, port);

  } else if (sa_is_inet6(sa)) {
    DhtTracker* tracker = m_router->get_tracker(*HashString::cast_from(info_hash.data()), true);

    if (tracker == NULL)
      throw dht::dht_error(dht_error_generic, "Tracking too many info hashes");

    tracker->add_peer6(reinterpret_cast<const sockaddr_in6*>(sa)->sin6_addr, port);

  } else {
    throw internal_error("DhtServer::create_announce_peer_response called with non-inet address.");
  }
}

void
DhtServer::process_response(DhtTransaction::key_type key, const HashString& id, const sockaddr* sa, const DhtMessage& response) {
  auto itr = m_transactions.find(key);
  if (itr == m_transactions.end())
    throw internal_error("DhtServer::process_response called with an invalid transaction key.");

  m_repliesReceived++;
  m_network_up = true;

  // Make sure transaction is erased even if an exception is thrown.
  try {
    auto& transaction = itr->second;

#ifdef USE_EXTRA_DEBUG
    int transactionId = static_cast<unsigned char>(response[key_t].as_raw_string().data()[0]);
    if (DhtTransaction::key(sa, transactionId) != transaction->key(transactionId))
      throw internal_error("DhtServer::process_response key mismatch.");
#endif

    // If we contact a node but its ID is not the one we expect, ignore the reply.
    if ((id != transaction->id() && transaction->id() != torrent::DhtRouter::zero_id))
      return;

    switch (transaction->type()) {
      case DhtTransaction::DHT_FIND_NODE:
        // the get_peers walk of an announce: the peers a node on the way holds go to the tracker at once
        if (transaction->as_find_node()->search()->is_announce() && response[key_r_values].is_raw_list()) {
          auto announce = dynamic_cast<dht::DhtAnnounce*>(transaction->as_find_node()->search().get());

          if (announce != nullptr)
            announce->receive_peers(response[key_r_values].as_raw_list());
        }

        parse_find_node_reply(transaction->as_find_node(), response);
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

  } catch (const std::exception&) {
    erase_transaction(itr);

    m_errorsCaught++;
    throw;
  }

  erase_transaction(itr);
}

void
DhtServer::process_error(DhtTransaction::key_type key) {
  auto itr = m_transactions.find(key);

  if (itr == m_transactions.end())
    throw internal_error("DhtServer::process_error called with an invalid transaction key.");

  m_repliesReceived++;
  m_errorsReceived++;
  m_network_up = true;

  // Don't mark node as good (because it replied) or bad (because it returned an error).
  // If it consistently returns errors for valid queries it's probably broken.  But a
  // few error messages are acceptable. So we do nothing and pretend the query never happened.

  erase_transaction(itr);
}

void
DhtServer::parse_find_node_reply(DhtTransactionSearch* transaction, const DhtMessage& response) {
  transaction->complete(true);

  add_search_contacts(transaction, response);
  find_node_next(transaction);
}

void
DhtServer::add_search_contacts(DhtTransactionSearch* transaction, const DhtMessage& response) {
  if (sizeof(const compact_node_info) != 26)
    throw internal_error("DhtServer::add_search_contacts(...) bad struct size.");

  if (m_router->family() == AF_INET6) {
    if (response[key_r_nodes6].is_raw_string()) {
      raw_string nodes = response[key_r_nodes6].as_raw_string();

      for (auto itr = nodes.data(); itr + 38 <= nodes.data() + nodes.size(); itr += 38) {
        const HashString* node_id = HashString::cast_from(itr);

        if (*node_id == m_router->id())
          continue;

        auto sin6 = sin6_make_n(itr + 20, *reinterpret_cast<const uint16_t*>(itr + 36));

        // a v4-mapped entry is an IPv4 node in disguise: not this table's
        if (sin6_is_v4mapped(sin6.get()) || sin6_is_any(sin6.get()) || sin6->sin6_port == 0)
          continue;

        transaction->search()->add_contact(*node_id, sa_from_in6(std::move(sin6)).get());
      }
    }

  } else {
    if (response[key_r_nodes].is_raw_string()) {
      raw_string nodes = response[key_r_nodes].as_raw_string();

      for (auto itr = nodes.data(); itr + 26 <= nodes.data() + nodes.size(); itr += 26) {
        const HashString* node_id = HashString::cast_from(itr);

        if (*node_id == m_router->id())
          continue;

        auto sin = sin_make_n(*reinterpret_cast<const uint32_t*>(itr + 20), *reinterpret_cast<const uint16_t*>(itr + 24));

        if (sin_is_any(sin.get()) || sin->sin_port == 0)
          continue;

        transaction->search()->add_contact(*node_id, sa_from_in(std::move(sin)).get());
      }
    }
  }
}

void
DhtServer::parse_get_peers_reply(DhtTransactionGetPeers* transaction, const DhtMessage& response) {
  auto announce = dynamic_cast<dht::DhtAnnounce*>(transaction->as_search()->search().get());

  if (announce == nullptr)
    throw internal_error("DhtServer::parse_get_peers_reply dynamic_cast to DhtAnnounce failed.");

  transaction->complete(true);

  if (response[key_r_values].is_raw_list())
    announce->receive_peers(response[key_r_values].as_raw_list());

  // Restrict the length of tokens. We echo them back in announce_peer, and the query has to fit in
  // a single packet.
  //
  // A reply with a longer token still carries nodes for the walk.
  if (response[key_r_token].is_raw_string() && response[key_r_token].as_raw_string().size() <= 64)
    announce->set_token(transaction->id(), response[key_r_token].as_raw_string());

  add_search_contacts(transaction, response);

  find_node_next(transaction);
}

void
DhtServer::find_node_next(DhtTransactionSearch* transaction) {
  int priority = packet_prio_low;

  if (transaction->search()->is_announce())
    priority = packet_prio_high;

  auto node = transaction->search()->get_contact();

  while (node != transaction->search()->end()) {
    if (transaction->search()->is_announce())
      add_transaction(std::unique_ptr<DhtTransaction>(new DhtTransactionGetPeers(transaction->search(), node)), priority);
    else
      add_transaction(std::unique_ptr<DhtTransaction>(new DhtTransactionFindNode(transaction->search(), node)), priority);

    node = transaction->search()->get_contact();
  }

  if (!transaction->search()->is_announce())
    return;

  auto announce = dynamic_cast<dht::DhtAnnounce*>(transaction->search().get());

  if (announce == nullptr)
    throw internal_error("DhtServer::find_node_next dynamic_cast to DhtAnnounce failed.");

  if (announce->complete()) {
    // We have found the 8 closest nodes to the info hash, announce to them.
    for (const auto& [node, token] : announce->start_announce())
      add_transaction(std::unique_ptr<DhtTransaction>(new DhtTransactionAnnouncePeer(node->id(), node->address(), announce->target(), token)), packet_prio_high);
  }

  announce->update_status();
}

void
DhtServer::create_query(transaction_itr itr, int tID, [[maybe_unused]] const sockaddr* sa, int priority) {
  if (itr->second->id() == m_router->id())
    throw internal_error("DhtServer::create_query trying to send to itself.");

  DhtMessage query;

  // Transaction ID is a bencode string.
  query[key_t] = raw_bencode(query.data_end, 3);
  *query.data_end++ = '1';
  *query.data_end++ = ':';
  *query.data_end++ = tID;

  auto& transaction = itr->second;

  query[key_q] = raw_string::from_c_str(queries[transaction->type()]);
  query[key_y] = raw_bencode::from_c_str("1:q");
  query[key_v] = raw_bencode("4:" PEER_VERSION, 6);
  query[key_a_id] = m_router->id_raw_string();

  switch (transaction->type()) {
    case DhtTransaction::DHT_PING:
      // nothing to do
      break;

    case DhtTransaction::DHT_FIND_NODE:
      // A peer search walks with get_peers (BEP 5): every node on the way may hold the info-hash's peers and
      // answers with them beside the closer nodes; a find_node walk only asked the last few nodes for peers.
      if (transaction->as_find_node()->search()->is_announce()) {
        query[key_q] = raw_string::from_c_str("get_peers");
        query[key_a_infoHash] = transaction->as_find_node()->search()->target_raw_string();
      } else {
        query[key_a_target] = transaction->as_find_node()->search()->target_raw_string();
      }
      break;

    case DhtTransaction::DHT_GET_PEERS:
      query[key_a_infoHash] = transaction->as_get_peers()->search()->target_raw_string();
      break;

    case DhtTransaction::DHT_ANNOUNCE_PEER:
      query[key_a_infoHash] = transaction->as_announce_peer()->info_hash_raw_string();
      query[key_a_token]    = transaction->as_announce_peer()->token();
      query[key_a_port]     = runtime::listen_port();
      break;
  }

  auto packet = std::make_shared<DhtTransactionPacket>(transaction->address(), query, tID, itr->first);

  transaction->set_packet(packet);
  // add_packet(packet, priority);
}

void
DhtServer::create_response(const DhtMessage& req, const sockaddr* sa, DhtMessage& reply) {
  reply[key_r_id] = m_router->id_raw_string();
  reply[key_t] = req[key_t];
  reply[key_y] = raw_bencode::from_c_str("1:r");
  reply[key_v] = raw_bencode("4:" PEER_VERSION, 6);

  // add_packet(std::make_shared<DhtTransactionPacket>(sa, reply), packet_prio_reply);
}

void
DhtServer::create_error(const DhtMessage& req, const sockaddr* sa, int num, const char* msg) {
  DhtMessage error;

  if (req[key_t].is_raw_string() && req[key_t].as_raw_string().size() < 67)
    error[key_t] = req[key_t];

  error[key_y] = raw_bencode::from_c_str("1:e");
  error[key_v] = raw_bencode("4:" PEER_VERSION, 6);
  error[key_e_0] = num;
  error[key_e_1] = raw_string::from_c_str(msg);

  // add_packet(std::make_shared<DhtTransactionPacket>(sa, error), packet_prio_reply);
}

int
DhtServer::add_transaction(std::shared_ptr<DhtTransaction> transaction, int priority) {
  if (!sa_is_inet(transaction->address()) && !sa_is_inet6(transaction->address()))
    return -1;

  // Try random transaction ID. This is to make it less likely that we reuse
  // a transaction ID from an earlier transaction which timed out and we forgot
  // about it, so that if the node replies after the timeout it's less likely
  // that we match the reply to the wrong transaction.
  //
  // If there's an existing transaction with the random ID we search for the next
  // unused one. Since normally only one or two transactions will be active per
  // node, a collision is extremely unlikely, and a linear search for the first
  // open one is the most efficient.
  unsigned int rnd = static_cast<uint8_t>(random());
  unsigned int id = rnd;

  auto insertItr = m_transactions.lower_bound(transaction->key(rnd));

  // If key matches, keep trying successive IDs.
  while (insertItr != m_transactions.end() && insertItr->first == transaction->key(id)) {
    ++insertItr;
    id = static_cast<uint8_t>(id + 1);

    // Give up after trying all possible IDs. This should never happen.
    if (id == rnd)
      return -1;

    // Transaction ID wrapped around, reset iterator.
    if (id == 0)
      insertItr = m_transactions.lower_bound(transaction->key(id));
  }

  // We know where to insert it, so pass that as hint.
  insertItr = m_transactions.insert(insertItr, std::make_pair(transaction->key(id), transaction));

  try {
    create_query(insertItr, id, transaction->address(), priority);

    auto packet = transaction->packet();

    m_udp_router.connect(insertItr->first, transaction->address_ptr(), {
      [packet](auto, auto& buffer) {
        if (packet->length() > buffer.reserved())
          throw internal_error("DhtServer query exceeds UDP router buffer.");
        buffer.write_len(packet->c_str(), packet->length());
      },
      [this](auto id, auto& buffer) {
        return process_queue(id, buffer);
      },
      [this](auto id, int errno_err, int gai_err) {
        // We don't actually use gai_err, but keeping it for compatibility with other UDP router implementations.
        process_error(id, errno_err, gai_err);
      },
      nullptr,
      nullptr,
      });

  } catch (const std::exception&) {
    m_transactions.erase(insertItr);
    throw;
  }

  return id;
}

// Transaction received no reply and timed out. Mark node as bad and remove
// transaction (except if it was only the quick timeout).
DhtServer::transaction_itr
DhtServer::failed_transaction(transaction_itr itr, bool quick, bool router_disconnected) {
  auto transaction = itr->second;

  // If it was a known node, remember that it didn't reply, unless the transaction
  // is only stalled (had quick timeout, but not full timeout).  Also if the
  // transaction still has an associated packet, the packet never got sent due to
  // throttling, so don't blame the remote node for not replying.
  // Finally, if we haven't received anything whatsoever so far, assume the entire
  // network is down and so we can't blame the node either.
  if (!quick && m_network_up && transaction->packet() == NULL && transaction->id() != torrent::DhtRouter::zero_id)
    m_router->node_inactive(transaction->id(), transaction->address());

  if (transaction->type() == DhtTransaction::DHT_FIND_NODE || transaction->type() == DhtTransaction::DHT_GET_PEERS) {
    if (quick)
      transaction->as_search()->set_stalled();
    else
      transaction->as_search()->complete(false);

    try {
      find_node_next(transaction->as_search());

    } catch (const std::exception&) {
      if (!quick) {
        erase_transaction(itr, router_disconnected);
      }

      throw;
    }
  }

  // don't actually delete the transaction until the final timeout
  if (quick)
    return ++itr;

  return erase_transaction(itr, router_disconnected);
}

DhtServer::transaction_itr
DhtServer::erase_transaction(transaction_itr itr, bool router_disconnected) {
  if (!router_disconnected)
    m_udp_router.disconnect(itr->first);

  return m_transactions.erase(itr);
}

void
DhtServer::clear_transactions() {
  while (!m_transactions.empty())
    erase_transaction(m_transactions.begin());
}

} // namespace torrent
